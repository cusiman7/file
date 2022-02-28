# file::

[![C/C++ CI](https://github.com/cusiman7/file/actions/workflows/ci.yml/badge.svg)](https://github.com/cusiman7/file/actions/workflows/ci.yml)

file:: is a simple C++ file read/write library. For when you just want to read a file. Project goals include:

* Simple
* Faster than iostream

## Read a File

```c++
file::file f = file::open("hello.txt");

std::string contents = f.read();
```

## Write a File

```c++
file::file f = file::open("hello.txt", file::mode::write);

f.write("hello file::\n");
```

## Read a File Line by Line

```c++
file::file f = file::open("hello.txt");

for (auto& line : f.lines()) {
  ...
}
```

## Read a File as Bytes

```c++
file::file f = file::open("hello.bin");

std::vector<uint8_t> = f.read_bytes();
```
