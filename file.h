
#pragma once

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string>
#include <string_view>
#include <exception>
#include <iostream>

#define DBG(x) std::cout << #x ": " << (x) << "\n";

namespace file {
class raw;
class text;

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

template <class T = text>
T open(const std::string& path, mode mode = mode::read) {
    return T(path, mode);
}

class raw {
public:
    explicit raw(int fd);
    raw(const std::string& path, mode mode);
    raw(const raw& other) = delete;
    raw& operator=(const raw& other) = delete;
    raw(raw&& other) = default;
    raw& operator=(raw&& other) = default;
    ~raw();

    bool can_read() const;
    bool can_write() const;

    size_t read(void* buffer, size_t count) const;
    size_t write(const void* buffer, size_t count) const;

    void close();
    bool closed() const;

    int64_t seek(int64_t offset, seek_mode mode) const;
    int64_t tell() const;
    void sync() const;

    int64_t size() const;
    int64_t block_size() const;

private:
    void run_stat();

    int fd_;
    mode mode_;
    int64_t size_;
    int64_t block_size_;
};

class text : public raw {
public:
    explicit text(int fd);
    text(const std::string& path, mode mode);
    ~text();

    std::string read(int64_t count = -1);
    bool read_line(std::string& line);
    void write(std::string_view sv);
    void flush();

    class lines_iterator {
    public:
        explicit lines_iterator(text& f) : f_(f) {
            eof_ = !f_.read_line(line_);
        }

        using value_type = const std::string;
        using pointer = const std::string*;
        using reference = const std::string&;
        using difference_type = ptrdiff_t;
        using iterator_category = std::input_iterator_tag;

        bool operator==(bool eof) {
            return eof_ == eof;
        }

        bool operator!=(bool eof) {
            return eof_ != eof;
        }

        reference operator*() { return line_; }
        
        lines_iterator& operator++() { eof_ = !f_.read_line(line_); return *this; }
        lines_iterator operator++(int) { 
            auto old = *this; 
            eof_ = !f_.read_line(line_);
            return old;
        }
    
    private:
        text& f_;
        std::string line_;
        bool eof_ = false;
    };

    lines_iterator begin() {
        return lines_iterator(*this);
    }

    bool end() {
        return true;
    }

private:
    char* buffer_ = nullptr;
    size_t buf_cap_ = 0;
    size_t buf_size_ = 0; // only used for reading
    size_t buf_i_ = 0;
};

raw::raw(int fd) : fd_(fd) {
    run_stat(); 
}

raw::raw(const std::string& path, mode mode) : mode_(mode) {
    int flags = 0;
    mode_t posix_mode = 0;
    switch (mode) {
        case mode::read:
            flags |= O_RDONLY;
            break;
        case mode::write:
            flags |= O_WRONLY | O_TRUNC | O_CREAT;
            posix_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
            break;
        case mode::append:
            flags |= O_WRONLY | O_APPEND | O_CREAT;
            posix_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
            break;
    }
    
    fd_ = ::open(path.c_str(), flags, posix_mode);
    run_stat();
}

raw::~raw() {
    ::close(fd_);
}

bool raw::can_read() const {
    return mode_ == mode::read;
}

bool raw::can_write() const {
    return mode_ == mode::write || mode_ == mode::append;
}

size_t raw::read(void* buffer, size_t count) const {
    ssize_t bytes_read = ::read(fd_, buffer, count);
    if (bytes_read < 0) {
        switch(errno) {
            default:
            throw std::runtime_error("Couldn't read file");
        }
    }

    return static_cast<size_t>(bytes_read);
}

size_t raw::write(const void* buffer, size_t count) const {
    ssize_t bytes_written = ::write(fd_, buffer, count);
    if (bytes_written < 0) {
        switch(errno) {
            default:
            throw std::runtime_error("Couldn't write file");
        }
    }

    return static_cast<size_t>(bytes_written);
}

void raw::close() {
    ::close(fd_);
    fd_ = -1; 
}

bool raw::closed() const {
    return fd_ == -1;
}

int64_t raw::tell() const {
    return seek(0, seek_mode::cur);
}

int64_t raw::seek(int64_t offset, seek_mode mode) const {
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
    return ::lseek(fd_, offset, whence);
} 

void raw::sync() const {
    int ret = ::fsync(fd_);
    if (ret < 0) {
        switch(errno) {
            default:
            throw std::runtime_error("Couldn't sync file");
        }
    }
}

int64_t raw::size() const {
    return size_;
}

int64_t raw::block_size() const {
    return block_size_;
}

void raw::run_stat() {
    if (fd_ < 0) {
        switch (errno) {
            default:
            throw std::runtime_error("Couldn't open file");
        }
    }

    struct stat st;
    int ret = ::fstat(fd_, &st);
    if (ret < 0) {
        switch (errno) {
            default:
            throw std::runtime_error("Couldn't stat file");
        }
    }

    size_ = st.st_size;
    block_size_ = st.st_blksize;
}

text::text(int fd) : raw(fd) {}

text::text(const std::string& path, mode mode) : raw(path, mode) {
    int64_t block_s = block_size();
    buf_cap_ = (block_s > 0) ? block_s : 4096;
    buffer_ = static_cast<char*>(malloc(buf_cap_));
}

text::~text() {
    if (can_write()) {
        flush();
    }
    free(buffer_);
}

std::string text::read(int64_t count) {
    if (count < 0) {
        count = size() - tell();
        if (count < 0) {
            throw std::runtime_error("File offset beyond end of text");
        }
    }

    std::string ret;
    ret.reserve(static_cast<size_t>(count));

    size_t start = buf_i_;
    buf_i_ = buf_size_;
    while (true) {
        ret.append(buffer_ + start, buf_i_ - start);
        buf_size_ = raw::read(buffer_, buf_cap_);
        buf_i_ = buf_size_;
        start = 0;

        if (buf_size_ == 0) {
            return ret;
        }
    }
}

bool text::read_line(std::string& line) {
    size_t start = buf_i_;
    line.clear();

    while(true) {
        if (buf_i_ == buf_size_) {
            line.append(buffer_ + start, buf_i_ - start);
            buf_size_ = raw::read(buffer_, buf_cap_);

            buf_i_ = 0;
            start = 0;

            if (buf_size_ == 0) {
                return !line.empty();
            }
        }

        if (buffer_[buf_i_] == '\n') {
            line.append(buffer_ + start, buf_i_ - start);
            buf_i_++;
            break;
        }

        buf_i_++;
    }

    return true;
}

void text::write(std::string_view sv) {
    auto it = sv.begin();
    auto end = sv.end();
    while(it != end) {
        size_t cap_remaining = buf_cap_ - buf_i_;
        size_t count = (cap_remaining < end - it) ? cap_remaining : end - it;
        memcpy(buffer_ + buf_i_, it, count); 
        buf_i_ += count;
        it += count;

        if (buf_i_ == buf_cap_) {
            flush();
        }
    }
}

void text::flush() {
    raw::write(buffer_, buf_i_);
    buf_i_ = 0; 
}

} // namespace file

