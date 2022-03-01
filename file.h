// Copyright Rob Cusimano

#pragma once

#ifdef _WIN32
#include <windows.h> // For FlushFileBuffers
#include <io.h>
#else
#include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <string.h>

#include <string>
#include <string_view>
#include <exception>
#include <vector>

//#include <iostream>
//#define DBG(x) std::cout << #x ": " << (x) << "\n"

namespace file {
class raw;
class file;
class lines_iterator;
struct eof_sentinel {};

enum class mode {
    read,
    write,
    append,
};

enum class seek_mode {
    set,
    cur,
    end,
};

template <class T = file>
T open(const std::string& path, mode mode = mode::read) {
    return T(path, mode);
}

/*
 * An unbuffered file. All reads and writes are sent directly to the OS via system calls.
 */
class raw {
public:
    raw(const std::string& path, mode mode);
    raw(const raw& other) = delete;
    raw& operator=(const raw& other) = delete;
    raw(raw&& other);
    raw& operator=(raw&& other);
    ~raw();

    // Returns true if the file is open and in read mode
    bool can_read() const;
    // Returns true if the file is open and in write or append mode
    bool can_write() const;
    // Returns the mode of the file
    enum mode mode() const;

    // Reads count bytes into buffer. Returns the number of bytes read
    size_t read(void* buffer, size_t count) const;
    // Writes count bytes to the file from buffer
    // Returns the number of bytes written
    size_t write(const void* buffer, size_t count) const;

    // Closes the file.
    void close();
    // Returns true if the file is closed
    bool closed() const;

    // Seek to a specific byte offset in the file
    int64_t seek(int64_t offset, seek_mode mode) const;
    // Return the current byte offset in the file 
    int64_t tell() const;
    // If in write or append mode tell the OS to sync its caches to the underlying storage device 
    void sync() const;

    // Returns the size of the opened file
    int64_t size() const;
    // Returns the filesystem block size of the opened file
    int64_t block_size() const;

private:
    void run_stat();

    int fd_;
    enum mode mode_;
    int64_t size_;
    int64_t block_size_;
};

/*
 * A buffered file. All reads and writes are buffered and flushed automatically when the buffer 
 * if full or the file is closed.
 */
class file {
public:
    file(const std::string& path, mode mode);
    file(const raw& other) = delete;
    file& operator=(const file& other) = delete;
    file(file&& other);
    file& operator=(file&& other);
    ~file();
    
    // Returns true if the file is open and in read mode
    bool can_read() const;
    // Returns true if the file is open and in write or append mode
    bool can_write() const;
    // Returns the mode of the file
    enum mode mode() const;

    // Reads count bytes into buffer. Returns the number of bytes read
    size_t read(void* buffer, size_t count);
    // Reads count bytes and returns then as a std::string. 
    // If count is less than 0 reads all remaining bytes in file.
    std::string read(int64_t count = -1);
    // Reads count bytes and returns them as a std::vector.
    // If count is less than 0 reads all remaining bytes in file.
    std::vector<uint8_t> read_bytes(int64_t count = -1);
    // Reads bytes until a newline ('\n') is read or end of file is reached.
    // line is filled in with all read bytes *except* the trailing newline, if any.
    // Returns true if any bytes were read.
    bool read_line(std::string& line);
    // Reads bytes until the provided vec is at capacity or end of file is reached.
    // Returns the number of bytes read.
    size_t read_into_capacity(std::vector<uint8_t>& vec);

    // Writes count bytes to the file from buffer
    // Returns the number of bytes written
    size_t write(const void* buffer, size_t count);
    // Writes the provided string_view sv to the file
    // Returns the number of bytes written
    size_t write(std::string_view sv);
    // Flushes the internal buffer to the underlying file
    void flush();

    // Flushes the internal buffer to the underlying file, if applicable. and closes the file.
    void close();
    // Returns true if the file is closed
    bool closed() const;

    // Seek to a specific byte offset in the file
    int64_t seek(int64_t offset, seek_mode mode);
    // Return the current byte offset in the file 
    int64_t tell() const;
    // If in write or append mode tell the OS to sync its caches to the underlying storage device 
    void sync() const;

    // Returns the size of the opened file
    int64_t size() const;
    // Returns the filesystem block size of the opened file
    int64_t block_size() const;

    struct lines_range {
        lines_iterator begin();
        eof_sentinel end();

        file& f_;
    };

    // An input range over the lines of this file
    lines_range lines();

private:
    raw file_;

    char* buffer_ = nullptr;
    size_t buf_cap_ = 0; // capacity of buffer
    size_t buf_size_ = 0; // read size 
    size_t buf_i_ = 0; // current "pointer" for reading/writing
};

class lines_iterator {
public:
    using value_type = const std::string;
    using pointer = const std::string*;
    using reference = const std::string&;
    using difference_type = ptrdiff_t;
    using iterator_category = std::input_iterator_tag;

    explicit lines_iterator(file& f) : f_(f) {
        eof_ = !f_.read_line(line_);
    }

    bool operator==(eof_sentinel) {
        return eof_;
    }

    bool operator!=(eof_sentinel) {
        return !eof_;
    }

    reference operator*() { return line_; }
    
    lines_iterator& operator++() {
        eof_ = !f_.read_line(line_);
        return *this;
    }

    lines_iterator operator++(int) { 
        auto old = *this; 
        eof_ = !f_.read_line(line_);
        return old;
    }

private:
    file& f_;
    std::string line_;
    bool eof_ = false;
};

inline void strerror_r_thrower(int, const char* buf) {
    throw std::runtime_error(buf);
}

inline void strerror_r_thrower(char* ret, const char*) {
    throw std::runtime_error(ret);
}

inline void throw_errno_exception(int err) {
    char errorstr[256] = "";
#ifdef _WIN32
    strerror_s(errorstr, 256, err);
    throw std::runtime_error(errorstr);
#else
    // Depending on compiler strerror_r either returns an int or a char* error message, ignoring the buffer
    auto ret = strerror_r(err, errorstr, 256);
    strerror_r_thrower(ret, errorstr);
#endif
}

raw::raw(const std::string& path, enum mode mode) : mode_(mode) {
    int flags = 0;
#ifdef _WIN32
    int p_mode = 0;
    switch (mode) {
        case mode::read:
            flags |= _O_RDONLY | _O_BINARY;
            p_mode = _S_IREAD;
            break;
        case mode::write:
            flags |= _O_WRONLY | _O_TRUNC | _O_CREAT | _O_BINARY;
            p_mode = _S_IWRITE;
            break;
        case mode::append:
            flags |= _O_WRONLY | _O_APPEND | _O_CREAT | _O_BINARY;
            p_mode = _S_IWRITE;
            break;
    }
    
#pragma warning(push)
#pragma warning(disable: 4996) // Disable deprecation warnings for _open
    fd_ = ::_open(path.c_str(), flags, p_mode);
#pragma warning(pop)
#else
    mode_t p_mode = 0;
    switch (mode) {
        case mode::read:
            flags |= O_RDONLY;
            break;
        case mode::write:
            flags |= O_WRONLY | O_TRUNC | O_CREAT;
            p_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
            break;
        case mode::append:
            flags |= O_WRONLY | O_APPEND | O_CREAT;
            p_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
            break;
    }
    
    fd_ = ::open(path.c_str(), flags, p_mode);
#endif
    if (fd_ < 0) {
        throw_errno_exception(errno);
    }
    run_stat();
}

raw::raw(raw&& other) : 
fd_(other.fd_),
mode_(other.mode_),
size_(other.size_),
block_size_(other.block_size_)  {
    other.fd_ = -1;
}

raw& raw::operator=(raw&& other) {
    if (this != &other) {
        this->close();
        fd_ = other.fd_;
        mode_ = other.mode_;
        size_ = other.size_;
        block_size_ = other.block_size_;
        other.fd_ = -1;
    }
    return *this;
}

raw::~raw() {
    close();
}

bool raw::can_read() const {
    return !closed() && mode_ == mode::read;
}

bool raw::can_write() const {
    return !closed() && (mode_ == mode::write || mode_ == mode::append);
}

mode raw::mode() const {
    return mode_;
}

size_t raw::read(void* buffer, size_t count) const {
#ifdef _WIN32
    int bytes_read = ::_read(fd_, buffer, static_cast<unsigned int>(count));
#else
    ssize_t bytes_read = ::read(fd_, buffer, count);
#endif
    if (bytes_read < 0) {
        throw_errno_exception(errno);
    }

    return static_cast<size_t>(bytes_read);
}

size_t raw::write(const void* buffer, size_t count) const {
#ifdef _WIN32
    int bytes_written = ::_write(fd_, buffer, static_cast<unsigned int>(count));
#else
    ssize_t bytes_written = ::write(fd_, buffer, count);
#endif
    if (bytes_written < 0) {
        throw_errno_exception(errno);
    }

    return static_cast<size_t>(bytes_written);
}

void raw::close() {
    if (fd_ >= 0) {
#ifdef _WIN32
        ::_close(fd_);
#else
        ::close(fd_);
#endif
    }
    fd_ = -1; 
    size_ = 0;
    block_size_ = 0;
}

bool raw::closed() const {
    return fd_ == -1;
}

int64_t raw::tell() const {
    return seek(0, seek_mode::cur);
}

int64_t raw::seek(int64_t offset, seek_mode mode) const {
    if (closed()) {
        throw std::runtime_error("Can't seek closed file.");
    }

    int whence = 0;
    switch (mode) {
        case seek_mode::set:
            whence = SEEK_SET;
            break;
        case seek_mode::cur:
            whence = SEEK_CUR;
            break;
        case seek_mode::end:
            whence = SEEK_END;
            break;
    }
#ifdef _WIN32
    int64_t ret = ::_lseeki64(fd_, offset, whence);
#else
    int64_t ret = ::lseek(fd_, offset, whence);
#endif

    if (ret == -1) {
        throw_errno_exception(errno);
    }
    return ret;
} 

void raw::sync() const {
    if (closed()) {
        throw std::runtime_error("Can't sync closed file.");
    }

    if (!can_write()) {
        throw std::runtime_error("File not opened for writing cannot be synced.");
    }

#ifdef _WIN32
    bool ret = FlushFileBuffers((HANDLE)_get_osfhandle(fd_));
    if (!ret) {
        throw std::runtime_error("Couldn't sync file");
    }
#else
    int ret = ::fsync(fd_);
    if (ret < 0) {
        throw_errno_exception(errno);
    }
#endif
}

int64_t raw::size() const {
    return size_;
}

int64_t raw::block_size() const {
    return block_size_;
}

void raw::run_stat() {
    if (fd_ < 0) {
        throw std::runtime_error("Can't stat closed file.");
    }

#ifdef _WIN32 
    struct _stat st;
    int ret = ::_fstat(fd_, &st);
#else
    struct stat st;
    int ret = ::fstat(fd_, &st);
#endif
    if (ret < 0) {
        throw_errno_exception(errno);
    }

    size_ = st.st_size;
#ifdef _WIN32
    block_size_ = 4096;
#else
    block_size_ = st.st_blksize;
#endif
}

file::file(const std::string& path, enum mode mode) : file_(path, mode) {
    int64_t block_s = file_.block_size();
    buf_cap_ = (block_s > 0) ? block_s : 4096;
    buffer_ = static_cast<char*>(malloc(buf_cap_));
    if (!buffer_) {
        throw std::bad_alloc();
    }
}

file::file(file&& other) : 
file_(std::move(other.file_)),
buffer_(other.buffer_),
buf_cap_(other.buf_cap_),
buf_i_(other.buf_i_)  {
    other.buffer_ = nullptr;
    other.buf_cap_ = 0;
    other.buf_size_ = 0;
    other.buf_i_ = 0;
}

file& file::operator=(file&& other) {
    if (this != &other) {
        this->close();
        free(buffer_);

        file_ = std::move(other.file_);
        buffer_ = other.buffer_;
        buf_cap_ = other.buf_cap_;
        buf_size_ = other.buf_size_;
        buf_i_ = other.buf_i_;

        other.buffer_ = nullptr;
        other.buf_cap_ = 0;
        other.buf_size_ = 0;
        other.buf_i_ = 0;
    }
    return *this;
}

file::~file() {
    close();
    free(buffer_);
}

bool file::can_read() const {
    return file_.can_read();
}

bool file::can_write() const {
    return file_.can_write();
}

mode file::mode() const {
    return file_.mode();
}

size_t file::read(void* buffer, size_t count) {
    if (closed()) {
        throw std::runtime_error("Can't read from closed file");
    }
    if (mode() != mode::read) {
        throw std::runtime_error("File not opened for reading");
    }

    size_t read = 0;
    while (true) {
        size_t available = buf_size_ - buf_i_;
        size_t copy_size = (std::min)(count - read, available);

        memcpy(buffer, buffer_ + buf_i_, copy_size);
        buf_i_ += copy_size;
        read += copy_size;

        if (read == count) {
            return read;
        }

        buf_size_ = file_.read(buffer_, buf_cap_);
        buf_i_ = 0;

        if (buf_size_ == 0) {
            return read;
        }
    }
}

std::string file::read(int64_t count) {
    if (closed()) {
        throw std::runtime_error("Can't read from closed file");
    }
    if (mode() != mode::read) {
        throw std::runtime_error("File not opened for reading");
    }

    if (count < 0) {
        size_t available = buf_size_ - buf_i_;
        count = size() - tell() + available;
        if (count < 0) {
            throw std::runtime_error("File offset beyond end of file");
        }
    }

    std::string ret;
    ret.reserve(static_cast<size_t>(count));

    size_t read = 0;
    while (true) {
        size_t available = buf_size_ - buf_i_;
        size_t copy_size = (std::min)(static_cast<size_t>(count) - read, available);
        ret.append(buffer_ + buf_i_, copy_size);
    
        buf_i_ += copy_size;
        read += copy_size;

        if (read == static_cast<size_t>(count)) {
            return ret;
        }

        buf_size_ = file_.read(buffer_, buf_cap_);
        buf_i_ = 0;

        if (buf_size_ == 0) {
            return ret;
        }
    }
}

std::vector<uint8_t> file::read_bytes(int64_t count) {
    if (closed()) {
        throw std::runtime_error("Can't read from closed file");
    }
    if (mode() != mode::read) {
        throw std::runtime_error("File not opened for reading");
    }

    if (count < 0) {
        size_t available = buf_size_ - buf_i_;
        count = size() - tell() + available;
        if (count < 0) {
            throw std::runtime_error("File offset beyond end of file");
        }
    }

    std::vector<uint8_t> ret;
    ret.reserve(static_cast<size_t>(count));

    size_t read = 0;
    while (true) {
        size_t available = buf_size_ - buf_i_;
        size_t copy_size = (std::min)(static_cast<size_t>(count) - read, available);
        ret.insert(ret.end(), buffer_ + buf_i_, buffer_ + buf_i_ + copy_size);
    
        buf_i_ += copy_size;
        read += copy_size;

        if (read == static_cast<size_t>(count)) {
            return ret;
        }

        buf_size_ = file_.read(buffer_, buf_cap_);
        buf_i_ = 0;

        if (buf_size_ == 0) {
            return ret;
        }
    }
}

size_t file::read_into_capacity(std::vector<uint8_t>& vec) {
    if (closed()) {
        throw std::runtime_error("Can't read from closed file");
    }
    if (mode() != mode::read) {
        throw std::runtime_error("File not opened for reading");
    }

    size_t read = 0;
    while (true) {
        size_t available = buf_size_ - buf_i_;
        size_t copy_size = (std::min)(vec.capacity() - vec.size(), available);

        vec.insert(vec.end(), buffer_ + buf_i_, buffer_ + buf_i_ + copy_size);
        buf_i_ += copy_size;
        read += copy_size;

        if (vec.capacity() == vec.size()) {
            return read;
        }

        buf_size_ = file_.read(buffer_, buf_cap_);
        buf_i_ = 0;

        if (buf_size_ == 0) {
            return read;
        }
    }
}

bool file::read_line(std::string& line) {
    if (closed()) {
        throw std::runtime_error("Can't read from closed file");
    }
    if (mode() != mode::read) {
        throw std::runtime_error("File not opened for reading");
    }

    size_t start = buf_i_;
    line.clear();

    while(true) {
        if (buf_i_ == buf_size_) {
            line.append(buffer_ + start, buf_i_ - start);
            buf_size_ = file_.read(buffer_, buf_cap_);

            buf_i_ = 0;
            start = 0;

            if (buf_size_ == 0) {
                return !line.empty();
            }
        }

        if (buffer_[buf_i_] == '\n') {
            size_t end = buf_i_ - start;
            end -= (buf_i_ > 0 && buffer_[buf_i_ - 1] == '\r') ? 1 : 0;
            line.append(buffer_ + start, end);
            buf_i_++;
            break;
        }

        buf_i_++;
    }

    return true;
}

size_t file::write(const void* buffer, size_t count) {
    if (closed()) {
        throw std::runtime_error("Can't write to closed file");
    }
    if (mode() != mode::write || mode() != mode::append) {
        throw std::runtime_error("File not opened for writing");
    }

    size_t written = 0;
    while (written != count) {
        size_t cap_remaining = buf_cap_ - buf_i_;
        size_t copy_size = (std::min)(count - written, cap_remaining);
        memcpy(buffer_ + buf_i_, static_cast<const char*>(buffer) + written, copy_size);
        buf_i_ += copy_size;
        written += copy_size;

        if (buf_i_ == buf_cap_) {
            flush();
        }
    }
    return written;
}

size_t file::write(std::string_view sv) {
    return write(sv.data(), sv.size());
}

void file::flush() {
    if (closed()) {
        throw std::runtime_error("Can't flush to closed file");
    }
    if (mode() != mode::write || mode() != mode::append) {
        throw std::runtime_error("File not opened for writing");
    }

    file_.write(buffer_, buf_i_);
    buf_i_ = 0; 
}

void file::close() {
    if (can_write()) {
        flush();
    }
    file_.close();
}

bool file::closed() const {
    return file_.closed();
}

int64_t file::seek(int64_t offset, seek_mode mode) {
    int64_t ret = file_.seek(offset, mode);
    buf_i_ = 0;
    buf_size_ = 0;
    return ret;
}

int64_t file::tell() const {
    return file_.tell();
}

void file::sync() const {
    file_.sync();
}

int64_t file::size() const {
    return file_.size();
}

int64_t file::block_size() const {
    return file_.block_size();
}

file::lines_range file::lines() {
    return {*this};
}

lines_iterator file::lines_range::begin() {
    return lines_iterator(f_);
}

eof_sentinel file::lines_range::end() {
    return {};
}

} // namespace file

