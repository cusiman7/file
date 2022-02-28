# file::

[![C/C++ CI](https://github.com/cusiman7/file/actions/workflows/ci.yml/badge.svg)](https://github.com/cusiman7/file/actions/workflows/ci.yml)

file:: is a simple C++ file read/write library. For when you just want to read a file. Project goals include:

* Simple
* Faster than iostream

## Read a File

```c++
#include "file.h"

file::file f = file::open("hello.txt");

std::string contents = f.read();
```

## Write a File

```c++
#include "file.h"

auto f = file::open("hello.txt", file::mode::write);

f.write("hello file::\n");
```

## Read a File Line by Line

```c++
#include "file.h"

auto f = file::open("hello.txt");

for (const std::string& line : f.lines()) {
  ...
}
```

## Read a File as Bytes

```c++
#include "file.h"

auto f = file::open("hello.bin");

std::vector<uint8_t> = f.read_bytes();
```

# Benchmarks

Benchmarks are run every build as a Github Action. A snapshot is provided here:

```
...............................................................................

benchmark name                       samples       iterations    estimated
                                     mean          low mean      high mean
                                     std dev       low std dev   high std dev
-------------------------------------------------------------------------------
file wc                                        100             1    534.896 ms
                                        5.33554 ms    5.29389 ms    5.40911 ms
                                        275.894 us    164.333 us    438.725 us

iostream wc                                    100             1      1.5509 s
                                        16.5573 ms    16.2947 ms    16.8311 ms
                                        1.37111 ms    1.26738 ms    1.56254 ms

file read as string                            100             1    63.6997 ms
                                        637.751 us    630.194 us     650.61 us
                                        49.2005 us    31.2956 us    77.6773 us

iostream read as string
(stringstream)                                 100             1    983.546 ms
                                        10.0051 ms    9.93925 ms    10.1528 ms
                                        476.759 us    262.504 us    928.352 us

iostream read as string (low level)            100             1    81.3872 ms
                                        805.159 us    794.023 us     828.26 us
                                        78.5828 us    41.9946 us    130.531 us


===============================================================================
```

# TODO

* More extensive testing
* Code coverage
