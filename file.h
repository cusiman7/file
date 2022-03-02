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
#include <utility>

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

enum class error {
    // our enums
    unknown,
    // errno enums
    access, // Requested access is not allowed
    bad_file, // File is not open or doesn't allow the attempted operation
    does_not_exist, // The file does not exist
    exists, // File already exists
    file_limit, // The process has reached the per-process limit on number of open files
    interrupted, // Interrupted while waiting for device
    invalid_argument, // An invalid or out-of-range argument was provided
    io, // A low level I/O error occurred
    no_mem, // Cannot allocate memory
    no_space, // The device containing the file has no room for data
};

template <class T, class E>
class result;

template <class T = file>
result<T, error> open(const std::string& path, mode mode = mode::read) {
    return T::open(path, mode);
}

template <class E>
class result_error {
public:
    result_error(const result_error&) = default;
    result_error(result_error&&) = default;
    template <class Err = E>
    result_error(Err&& err) : value_(std::forward<Err>(err)) {}

    template <class Err>
    result_error(const result_error<Err>& other) : value_(other.value()) {}
    template <class Err>
    result_error(result_error<Err>&& other) : value_(std::move(other).value()) {}

    result_error& operator=(const result_error&) = default;
    result_error& operator=(result_error&&) = default;

    template <class Err = E>
    result_error& operator=(const result_error<Err>& other) {
        value_ = other.value();
    }
    template <class Err = E>
    result_error& operator=(result_error<Err>&& other) {
        value_ = std::move(other).value();
    }

    const E& value() const& { return value_; }
    E& value() & { return value_; }
    const E&& value() const&& { return std::move(value_); }
    E&& value() && { return std::move(value_); }

    friend void swap(result_error& a, result_error& b) {
        using std::swap;
        swap(a.value_, b.value_);
    }
private:
    E value_;
};

template <typename E> result_error(E) -> result_error<E>;  // Constructor template deduction guide

template <class T, class E>
class result {
public:
    result() : ok_(true) {
        new (&value_) T();
    }
    result(const T& t) : ok_(true) {
        new (&value_) T(t);
    }
    template <class Err = E>
    result(const result_error<Err>& err) : ok_(false) {
        new(&err_) E(err.value());
    }
    template <class Err = E>
    result(result_error<Err>&& err) : ok_(false) {
        new(&err_) E(std::forward<Err>(err.value()));
    }
    template <class U = T>
    explicit result(U&& u) : ok_(true) {
        new (&value_) T(std::forward<U>(u));
    }
    result(const result& other) : ok_(other.ok_) {
        if (ok_) {
            new (&value_) T(other.value_);
        } else {
            new (&err_) E(other.err_);
        }
    }
    result(result&& other) : ok_(other.ok_) {
        if (ok_) {
            new (&value_) T(other.value_);
        } else {
            new (&err_) E(other.err_);
        }
    }
    result& operator=(const result& other) {
        if (this != &other) {
            if (ok_ && other.ok_) {
                value_ = other.value_;
            } else if (!ok_ && !other.ok_) {
                err_ = other.err_;
            } else if (ok_ && !other.ok_) {
                value_.~T();
                ok_ = other.ok_;
                new (&err_) E(other.err_);
            } else if (!ok_ && other.ok_) {
                err_.~E();
                ok_ = other.ok_;
                new (&value_) T(other.value_);
            }
        }
        return *this;
    }
    result& operator=(result&& other) {
        if (this != &other) {
            if (ok_ && other.ok_) {
                value_ = std::move(other.value_);
            } else if (!ok_ && !other.ok_) {
                err_ = std::move(other.err_);
            } else if (ok_ && !other.ok_) {
                value_.~T();
                ok_ = other.ok_;
                new (&err_) E(std::move(other.err_));
            } else if (!ok_ && other.ok_) {
                err_.~E();
                ok_ = other.ok_;
                new (&value_) T(std::move(other.value_));
            }
        }
        return *this;
    }

    friend void swap(result& a, result& b) {
        using std::swap;
        if (a.ok_ && b.ok_) {
            swap(a.value_, b.value_);
        } else if (!a.ok_ && !b.ok_) {
            swap(a.err_, b.err_);
        } else if (a.ok_ && !b.ok_) {
            T tmp_value(std::move(a.value_));
            a.value_.~T();

            new(&a.err_) E(std::move(b.err_));
            b.err_.~E();

            new(&b.value) T(std::move(tmp_value));

            a.ok_ = false;
            b.ok_ = true;
        } else if (!a.ok_ && b.ok_) {
            swap(b, a);
        }
    }

    bool is_ok() const { return ok_; }
    bool is_error() const { return !ok_; }

    const T& value() const& { return value_; }
    T& value() & { return value_; } 
    const T&& value() const&& { return std::move(value_); } 
    T&& value() && { return std::move(value_); } 
    const E& error() const& { return err_; }
    E& error() & { return err_; }
    const E&& error() const&& { return std::move(err_); }
    E&& error() && { return std::move(err_); }
    
    ~result() {
        if (ok_) {
            value_.~T();
        } else {
            err_.~E();
        }
    }
    
private:
    bool ok_;
    union {
        T value_;
        E err_;
    }; 
};

template <class E>
class result<void, E> {
public:
    result() : ok_(true) {}
    template <class Err = E>
    result(const result_error<Err>& err) : ok_(false) {
        new(&err_) E(err.value());
    }
    template <class Err = E>
    result(result_error<Err>&& err) : ok_(false) {
        new(&err_) E(std::forward<Err>(err.value()));
    }
    result(const result& other) : ok_(other.ok) {
        if (!ok_) {
            new (&err_) E(other.err_);
        }
    }
    result(result&& other) : ok_(other.ok) {
        if (!ok_) {
            new (&err_) E(std::move(other.err_));
        }
    }
    result& operator=(const result& other) {
        if (this != &other) {
            if (!ok_ && !other.ok_) {
                err_ = other.err_;
            } else if (ok_ && !other.ok_) {
                ok_ = other.ok_;
                new (&err_) E(other.err_);
            } else if (!ok_ && other.ok_) {
                err_.~E();
                ok_ = other.ok_;
            }
        }
        return *this;
    }
    result& operator=(result&& other) {
        if (this != &other) {
            if (!ok_ && !other.ok_) {
                err_ = std::move(other.err_);
            } else if (ok_ && !other.ok_) {
                ok_ = other.ok_;
                new (&err_) E(std::move(other.err_));
            } else if (!ok_ && other.ok_) {
                err_.~E();
                ok_ = other.ok_;
            }
        }
        return *this;
    }

    friend void swap(result& a, result& b) {
        using std::swap;
        if (!a.ok_ && !b.ok_) {
            swap(a.err_, b.err_);
        } else if (a.ok_ && !b.ok_) {
            new(&a.err_) E(std::move(b.err_));
            b.err_.~E();

            a.ok_ = false;
            b.ok_ = true;
        } else if (!a.ok_ && b.ok_) {
            swap(b, a);
        }
    }

    bool is_ok() const { return ok_; }
    bool is_error() const { return !ok_; }

    void value() { return; }
    const E& error() const& { return err_; }
    E& error() & { return err_; }
    const E&& error() const&& { return std::move(err_); }
    E&& error() && { return std::move(err_); }
    
    ~result() {
        if (!ok_) {
            err_.~E();
        }
    }
    
private:
    bool ok_;
    union {
        E err_;
    }; 
};

/*
 * An unbuffered file. All reads and writes are sent directly to the OS via system calls.
 */
class raw {
public:
    static result<raw, error> open(const std::string& path, mode mode);

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
    result<size_t, error> read(void* buffer, size_t count) const;
    // Writes count bytes to the file from buffer
    // Returns the number of bytes written
    result<size_t, error> write(const void* buffer, size_t count) const;

    // Closes the file.
    void close();
    // Returns true if the file is closed
    bool closed() const;

    // Seek to a specific byte offset in the file
    result<int64_t, error> seek(int64_t offset, seek_mode mode) const;
    // Return the current byte offset in the file 
    result<int64_t, error> tell() const;
    // If in write or append mode tell the OS to sync its caches to the underlying storage device 
    result<void, error> sync() const;

    // Returns the size of the opened file
    int64_t size() const;
    // Returns the filesystem block size of the opened file
    int64_t block_size() const;

private:
    raw(int fd, enum mode mode, int64_t size_, int64_t block_size);

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
    static result<file, error> open(const std::string& path, mode mode);

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
    result<size_t, error> read(void* buffer, size_t count);
    // Reads count bytes and returns then as a std::string. 
    // If count is less than 0 reads all remaining bytes in file.
    result<std::string, error> read(int64_t count = -1);
    // Reads count bytes and returns them as a std::vector.
    // If count is less than 0 reads all remaining bytes in file.
    result<std::vector<uint8_t>, error> read_bytes(int64_t count = -1);
    // Reads bytes until a newline ('\n') is read or end of file is reached.
    // line is filled in with all read bytes *except* the trailing newline, if any.
    // Returns true if any bytes were read.
    result<bool, error> read_line(std::string& line);
    // Reads bytes until the provided vec is at capacity or end of file is reached.
    // Returns the number of bytes read.
    result<size_t, error> read_into_capacity(std::vector<uint8_t>& vec);

    // Writes count bytes to the file from buffer
    // Returns the number of bytes written
    result<size_t, error> write(const void* buffer, size_t count);
    // Writes the provided string_view sv to the file
    // Returns the number of bytes written
    result<size_t, error> write(std::string_view sv);
    // Flushes the internal buffer to the underlying file
    result<void, error> flush();

    // Flushes the internal buffer to the underlying file, if applicable. and closes the file.
    void close();
    // Returns true if the file is closed
    bool closed() const;

    // Seek to a specific byte offset in the file
    result<int64_t, error> seek(int64_t offset, seek_mode mode);
    // Return the current byte offset in the file 
    result<int64_t, error> tell() const;
    // If in write or append mode tell the OS to sync its caches to the underlying storage device 
    result<void, error> sync() const;

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
    file(raw&& raw, char* buffer, size_t buf_cap);

    raw file_;

    char* buffer_ = nullptr;
    size_t buf_cap_ = 0; // capacity of buffer
    size_t buf_size_ = 0; // read size 
    size_t buf_i_ = 0; // current "pointer" for reading/writing
};

class lines_iterator {
public:
    using value_type = result<std::string, error>;
    using pointer = result<std::string, error>*;
    using reference = result<std::string, error>&;
    using difference_type = ptrdiff_t;
    using iterator_category = std::input_iterator_tag;

    explicit lines_iterator(file& f) : f_(f) {
        next();
    }

    bool operator==(eof_sentinel) {
        return eof_;
    }

    bool operator!=(eof_sentinel) {
        return !eof_;
    }

    reference operator*() { return line_; }
    
    lines_iterator& operator++() {
        next();
        return *this;
    }

    lines_iterator operator++(int) { 
        auto old = *this; 
        next();
        return old;
    }

private:
    void next() {
        assert(line_.is_ok());
        std::string line = std::move(line_).value();
        auto res = f_.read_line(line);
        if (res.is_ok()) {
            eof_ = !res.value();
            line_ = std::move(line);
        } else {
            eof_ = true;
            line_ = result_error(res.error());
        }
    }

    file& f_;
    result<std::string, error> line_;
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

inline error to_error(int err) {
    switch (err) {
        case EACCES:
            return error::access;
        case EBADF:
            return error::bad_file;
        case ENOENT:
            return error::does_not_exist; 
        case EEXIST:
            return error::exists;
        case EMFILE:
            return error::file_limit;
        case EINTR:
            return error::interrupted;
        case EINVAL:
            return error::invalid_argument;
        case EIO:
            return error::io;
        case ENOSPC:
            return error::no_space;
        default:
            return error::unknown;
    }
}

raw::raw(int fd, enum mode mode, int64_t size, int64_t block_size)
: fd_(fd), mode_(mode), size_(size), block_size_(block_size) {
}

result<raw, error> raw::open(const std::string& path, enum mode mode) {
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
    
    int fd = ::open(path.c_str(), flags, p_mode);
#endif
    if (fd < 0) {
        return result_error(to_error(errno));
    }

#ifdef _WIN32 
    struct _stat st;
    int ret = ::_fstat(fd, &st);
#else
    struct stat st;
    int ret = ::fstat(fd, &st);
#endif
    if (ret < 0) {
        return result_error(to_error(errno));
    }

    int64_t size = st.st_size;
#ifdef _WIN32
    int64_t block_size = 4096;
#else
    int64_t block_size = st.st_blksize;
#endif

    return result<raw, error>(raw(fd, mode, size, block_size));
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

result<size_t, error> raw::read(void* buffer, size_t count) const {
    if (!can_read()) {
        return result_error(error::bad_file);
    }
#ifdef _WIN32
    int bytes_read = ::_read(fd_, buffer, static_cast<unsigned int>(count));
#else
    ssize_t bytes_read = ::read(fd_, buffer, count);
#endif
    if (bytes_read < 0) {
        return result_error(to_error(errno)); 
    }

    return static_cast<size_t>(bytes_read);
}

result<size_t, error> raw::write(const void* buffer, size_t count) const {
    if (!can_write()) {
        return result_error(error::bad_file);
    }
#ifdef _WIN32
    int bytes_written = ::_write(fd_, buffer, static_cast<unsigned int>(count));
#else
    ssize_t bytes_written = ::write(fd_, buffer, count);
#endif
    if (bytes_written < 0) {
        return result_error(to_error(errno)); 
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

result<int64_t, error> raw::tell() const {
    return seek(0, seek_mode::cur);
}

result<int64_t, error> raw::seek(int64_t offset, seek_mode mode) const {
    if (closed()) {
        return result_error(error::bad_file);
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
        return result_error(to_error(errno)); 
    }
    return ret;
} 

result<void, error> raw::sync() const {
    if (!can_write()) {
        return result_error(error::bad_file);
    }

#ifdef _WIN32
    bool ret = FlushFileBuffers((HANDLE)_get_osfhandle(fd_));
    if (!ret) {
        return result_error(error::unknown); 
    }
#else
    int ret = ::fsync(fd_);
    if (ret < 0) {
        return result_error(to_error(errno)); 
    }
#endif
    return {};
}

int64_t raw::size() const {
    return size_;
}

int64_t raw::block_size() const {
    return block_size_;
}

result<file, error> file::open(const std::string& path, enum mode mode) {
    result<raw, error> r = raw::open(path, mode);
    if (r.is_error()) {
        return result_error(r.error());
    }
    
    int64_t block_s = r.value().block_size();
    size_t buf_cap = (block_s > 0) ? block_s : 4096;
    char* buffer = static_cast<char*>(malloc(buf_cap));
    if (!buffer) {
        return result_error(error::no_mem);
    }
    
    return result<file, error>(file(std::move(r).value(), buffer, buf_cap));
}

file::file(raw&& raw, char* buffer, size_t buf_cap) 
: file_(std::move(raw)),
  buffer_(buffer),
  buf_cap_(buf_cap) {}

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

result<size_t, error> file::read(void* buffer, size_t count) {
    if (!can_read()) {
        return result_error(error::bad_file);
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

        result<size_t, error> buf_size = file_.read(buffer_, buf_cap_);
        if (buf_size.is_error()) {
            return result_error(buf_size.error());
        }
        buf_size_ = buf_size.value();
        buf_i_ = 0;

        if (buf_size_ == 0) {
            return read;
        }
    }
}

result<std::string, error> file::read(int64_t count) {
    if (!can_read()) {
        return result_error(error::bad_file);
    }
    if (count < 0) {
        size_t available = buf_size_ - buf_i_;
        auto tell_res = tell();
        if (tell_res.is_error()) {
            return result_error(tell_res.error());
        } 
        count = size() - tell_res.value() + available;
        if (count < 0) {
            return result_error(error::io); 
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

        result<size_t, error> buf_size = file_.read(buffer_, buf_cap_);
        if (buf_size.is_error()) {
            return result_error(buf_size.error());
        }
        buf_size_ = buf_size.value();
        buf_i_ = 0;

        if (buf_size_ == 0) {
            return ret;
        }
    }
}

result<std::vector<uint8_t>, error> file::read_bytes(int64_t count) {
    if (!can_read()) {
        return result_error(error::bad_file);
    }
    if (count < 0) {
        size_t available = buf_size_ - buf_i_;
        auto tell_res = tell();
        if (tell_res.is_error()) {
            return result_error(tell_res.error());
        } 
        count = size() - tell_res.value() + available;
        if (count < 0) {
            return result_error(error::io);
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

        result<size_t, error> buf_size = file_.read(buffer_, buf_cap_);
        if (buf_size.is_error()) {
            return result_error(buf_size.error());
        }
        buf_size_ = buf_size.value();
        buf_i_ = 0;

        if (buf_size_ == 0) {
            return ret;
        }
    }
}

result<size_t, error> file::read_into_capacity(std::vector<uint8_t>& vec) {
    if (!can_read()) {
        return result_error(error::bad_file);
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

        result<size_t, error> buf_size = file_.read(buffer_, buf_cap_);
        if (buf_size.is_error()) {
            return result_error(buf_size.error());
        }
        buf_size_ = buf_size.value();
        buf_i_ = 0;

        if (buf_size_ == 0) {
            return read;
        }
    }
}

result<bool, error> file::read_line(std::string& line) {
    if (!can_read()) {
        return result_error(error::bad_file);
    }
    size_t start = buf_i_;
    line.clear();

    while(true) {
        if (buf_i_ == buf_size_) {
            line.append(buffer_ + start, buf_i_ - start);
            result<size_t, error> buf_size = file_.read(buffer_, buf_cap_);
            if (buf_size.is_error()) {
                return result_error(buf_size.error());
            }
            buf_size_ = buf_size.value();

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

result<size_t, error> file::write(const void* buffer, size_t count) {
    if (!can_write()) {
        return result_error(error::bad_file);
    }
    size_t written = 0;
    while (written != count) {
        size_t cap_remaining = buf_cap_ - buf_i_;
        size_t copy_size = (std::min)(count - written, cap_remaining);
        memcpy(buffer_ + buf_i_, static_cast<const char*>(buffer) + written, copy_size);
        buf_i_ += copy_size;
        written += copy_size;

        if (buf_i_ == buf_cap_) {
            auto res = flush();
            if (res.is_error()) {
                return result_error(res.error());
            }
        }
    }
    return written;
}

result<size_t, error> file::write(std::string_view sv) {
    return write(sv.data(), sv.size());
}

result<void, error> file::flush() {
    auto res = file_.write(buffer_, buf_i_);
    if (res.is_error()) {
        return result_error(res.error());
    }
    buf_i_ = 0; 
    return {};
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

result<int64_t, error> file::seek(int64_t offset, seek_mode mode) {
    auto res = file_.seek(offset, mode);
    if (res.is_error()) {
        return result_error(res.error());
    }
    buf_i_ = 0;
    buf_size_ = 0;
    return res.value();
}

result<int64_t, error> file::tell() const {
    return file_.tell();
}

result<void, error> file::sync() const {
    return file_.sync();
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

