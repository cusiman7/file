
#include "catch.hpp"

#include <fstream>
#include <sstream>
#include "../file.h"

static const char* test_txt = "this is a line\nthis is line 2\nend\n";
static const char* test_txt_crlf = "this is a line\r\nthis is line 2\r\nend\r\n";

TEST_CASE("Read a file", "[unit]") {

    auto f = file::open("test.txt");

    SECTION("Can read into a raw buffer") {
        char buf[100];
        size_t size = f.read(buf, 100);
        REQUIRE(size == 34);
        REQUIRE(size == static_cast<size_t>(f.size()));
    } 

    SECTION("Can read whole file as a string") {
        std::string s = f.read();
        REQUIRE(s.size() == 34);
        REQUIRE(s.size() == static_cast<size_t>(f.size()));    
        REQUIRE(s == test_txt);
    }

    SECTION("Can read a few bytes as a string") {
        std::string s = f.read(5);
        REQUIRE(s.size() == 5);
        REQUIRE(s == "this ");

        s = f.read(2);
        REQUIRE(s.size() == 2);
        REQUIRE(s == "is");
    }

    SECTION("Can read line by line") {
        std::string line;
        REQUIRE(f.read_line(line));
        REQUIRE(line == "this is a line");
        REQUIRE(f.read_line(line));
        REQUIRE(line == "this is line 2");
        REQUIRE(f.read_line(line));
        REQUIRE(line == "end");
        REQUIRE(!f.read_line(line));
    }

    SECTION("Can read lines as iterator") {
        size_t count = 0;
        for(auto& line : f.lines()) {
            (void)line;
            count++; 
        }
        REQUIRE(count == 3);
    }

    SECTION("Can read bytes as a vector") {
        std::vector<uint8_t> v = f.read_bytes(5);
        REQUIRE(v.size() == 5);
        REQUIRE(v[0] == 't');

        v = f.read_bytes(2);
        REQUIRE(v.size() == 2);
        REQUIRE(v[0] == 'i');
    }

    SECTION("Can read bytes into an existing vector's capacity") {
        std::vector<uint8_t> v;
        v.reserve(5);

        REQUIRE(f.read_into_capacity(v) == 5);
        REQUIRE(v[0] == 't');
        
        REQUIRE(f.read_into_capacity(v) == 0);
        v.clear();

        REQUIRE(v.capacity() == 5);
        REQUIRE(f.read_into_capacity(v) == 5);
        REQUIRE(v[0] == 'i');
    }

    SECTION("Can't write to a file opened for reading") {
        REQUIRE_THROWS(f.write("try this"));
    }
    
    SECTION("Can't flush to a file opened for reading") {
        REQUIRE_THROWS(f.flush());
    }

    SECTION("Can close") {
        f.close();
        REQUIRE(f.closed());
        REQUIRE_THROWS(f.read());
    }

    SECTION("Can seek") {
        REQUIRE(f.seek(5, file::seek_mode::set) == 5);
        REQUIRE(f.tell() == 5);
        REQUIRE(f.read(2) == "is");
    }

    SECTION("Move construct") {
        file::file f2(std::move(f));

        REQUIRE(f.closed());
        REQUIRE(!f2.closed());
        REQUIRE_THROWS(f.read());
        REQUIRE(!f2.read().empty());
    }
    
    SECTION("Move assign") {
        file::file f2 = std::move(f);

        REQUIRE(f.closed());
        REQUIRE(!f2.closed());
        REQUIRE_THROWS(f.read());
        REQUIRE(!f2.read().empty());
    }
}

TEST_CASE("Read a file (CRLF)", "[unit]") {

    auto f = file::open("test_crlf.txt");

    SECTION("Can read into a raw buffer") {
        char buf[100];
        size_t size = f.read(buf, 100);
        REQUIRE(size == 37);
        REQUIRE(size == static_cast<size_t>(f.size()));
    } 

    SECTION("Can read whole file as a string") {
        std::string s = f.read();
        REQUIRE(s.size() == 37);
        REQUIRE(s.size() == static_cast<size_t>(f.size()));    
        REQUIRE(s == test_txt_crlf);
    }

    SECTION("Can read a few bytes as a string") {
        std::string s = f.read(5);
        REQUIRE(s.size() == 5);
        REQUIRE(s == "this ");

        s = f.read(2);
        REQUIRE(s.size() == 2);
        REQUIRE(s == "is");
    }

    SECTION("Can read line by line") {
        std::string line;
        REQUIRE(f.read_line(line));
        REQUIRE(line == "this is a line");
        REQUIRE(f.read_line(line));
        REQUIRE(line == "this is line 2");
        REQUIRE(f.read_line(line));
        REQUIRE(line == "end");
        REQUIRE(!f.read_line(line));
    }

    SECTION("Can read lines as iterator") {
        size_t count = 0;
        for(auto& line : f.lines()) {
            (void)line;
            count++; 
        }
        REQUIRE(count == 3);
    }

    SECTION("Can read bytes as a vector") {
        std::vector<uint8_t> v = f.read_bytes(5);
        REQUIRE(v.size() == 5);
        REQUIRE(v[0] == 't');

        v = f.read_bytes(2);
        REQUIRE(v.size() == 2);
        REQUIRE(v[0] == 'i');
    }

    SECTION("Can read bytes into an existing vector's capacity") {
        std::vector<uint8_t> v;
        v.reserve(5);

        REQUIRE(f.read_into_capacity(v) == 5);
        REQUIRE(v[0] == 't');
        
        REQUIRE(f.read_into_capacity(v) == 0);
        v.clear();

        REQUIRE(v.capacity() == 5);
        REQUIRE(f.read_into_capacity(v) == 5);
        REQUIRE(v[0] == 'i');
    }

    SECTION("Can't write to a file opened for reading") {
        REQUIRE_THROWS(f.write("try this"));
    }
    
    SECTION("Can't flush to a file opened for reading") {
        REQUIRE_THROWS(f.flush());
    }

    SECTION("Can close") {
        f.close();
        REQUIRE(f.closed());
        REQUIRE_THROWS(f.read());
    }

    SECTION("Can seek") {
        REQUIRE(f.seek(5, file::seek_mode::set) == 5);
        REQUIRE(f.tell() == 5);
        REQUIRE(f.read(2) == "is");
    }

    SECTION("Move construct") {
        file::file f2(std::move(f));

        REQUIRE(f.closed());
        REQUIRE(!f2.closed());
        REQUIRE_THROWS(f.read());
        REQUIRE(!f2.read().empty());
    }
    
    SECTION("Move assign") {
        file::file f2 = std::move(f);

        REQUIRE(f.closed());
        REQUIRE(!f2.closed());
        REQUIRE_THROWS(f.read());
        REQUIRE(!f2.read().empty());
    }
}

TEST_CASE("Benchmark words", "[bench]") {
    BENCHMARK("file wc") {
        auto f = file::open("words");
        uint64_t count = 0;
        for (auto& line : f.lines()) {
            (void)line;
            count++;
        }
        return count;
    };
    
    BENCHMARK("iostream wc") {
        std::ifstream f("words"); 
        uint64_t count = 0;
        for (std::string line; std::getline(f, line);) {
            (void)line;
            count++;
        }
        return count;
    };
    
    BENCHMARK("file read as string") {
        auto f = file::open("words");
        return f.read(); 
    };

    BENCHMARK("iostream read as string (stringstream)") {
        std::ifstream t("words");
        std::stringstream buffer;
        buffer << t.rdbuf(); 
        return buffer.str();
    };

    BENCHMARK("iostream read as string (low level)") {
        std::ifstream t("words");
        t.seekg(0, std::ios::end);
        size_t size = t.tellg();
        std::string buffer(size, ' ');
        t.seekg(0);
        t.read(&buffer[0], size);
        return buffer;
    };
}



