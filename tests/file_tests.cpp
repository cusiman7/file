
#include <catch.hpp>

#include <fstream>
#include <sstream>
#include "../file.h"

#ifdef _WIN32
static const char* test_txt = "this is a line\r\nthis is line 2\r\nend\r\n";
#else
static const char* test_txt = "this is a line\nthis is line 2\nend\n";
#endif

TEST_CASE("Read a file", "[unit]") {

    auto f_res = file::open("test.txt");
    REQUIRE(f_res.is_ok());
    auto f = std::move(f_res).value();

    SECTION("Can read into a raw buffer") {
        char buf[100];
        auto size = f.read(buf, 100);
        REQUIRE(size.is_ok());
        REQUIRE(size.value() == static_cast<size_t>(f.size()));
    } 

    SECTION("Can read whole file as a string") {
        auto s = f.read();
        REQUIRE(s.is_ok());
        REQUIRE(s.value().size() == static_cast<size_t>(f.size()));    
        REQUIRE(s.value() == test_txt);
    }

    SECTION("Can read a few bytes as a string") {
        auto s = f.read(5);
        REQUIRE(s.is_ok());
        REQUIRE(s.value().size() == 5);
        REQUIRE(s.value() == "this ");

        s = f.read(2);
        REQUIRE(s.value().size() == 2);
        REQUIRE(s.value() == "is");
    }

    SECTION("Can read line by line") {
        std::string line;
        auto res = f.read_line(line);
        REQUIRE(res.is_ok());
        REQUIRE(res.value());
        REQUIRE(line == "this is a line");
        res = f.read_line(line);
        REQUIRE(res.is_ok());
        REQUIRE(res.value());
        REQUIRE(line == "this is line 2");
        res = f.read_line(line);
        REQUIRE(res.is_ok());
        REQUIRE(res.value());
        REQUIRE(line == "end");
        res = f.read_line(line);
        REQUIRE(res.is_ok());
        REQUIRE(!res.value());
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
        auto v = f.read_bytes(5);
        REQUIRE(v.is_ok());
        REQUIRE(v.value().size() == 5);
        REQUIRE(v.value()[0] == 't');

        v = f.read_bytes(2);
        REQUIRE(v.is_ok());
        REQUIRE(v.value().size() == 2);
        REQUIRE(v.value()[0] == 'i');
    }

    SECTION("Can read bytes into an existing vector's capacity") {
        std::vector<uint8_t> v;
        v.reserve(5);

        auto res = f.read_into_capacity(v);
        REQUIRE(res.is_ok());
        REQUIRE(res.value() == 5);
        REQUIRE(v[0] == 't');
        
        res = f.read_into_capacity(v);
        REQUIRE(res.is_ok());
        REQUIRE(res.value() == 0);
        v.clear();

        res = f.read_into_capacity(v);
        REQUIRE(res.is_ok());
        REQUIRE(res.value() == 5);
        REQUIRE(v[0] == 'i');
    }

    SECTION("Can't write to a file opened for reading") {
        REQUIRE(f.write("try this").is_error());
    }
    
    SECTION("Can't flush to a file opened for reading") {
        REQUIRE(f.flush().is_error());
    }

    SECTION("Can close") {
        f.close();
        REQUIRE(f.closed());
        REQUIRE(f.read().is_error());
    }

    SECTION("Can seek") {
        auto res = f.seek(5, file::seek_mode::set);
        REQUIRE(res.is_ok());
        REQUIRE(res.value() == 5);
        res = f.tell();
        REQUIRE(res.is_ok());
        REQUIRE(res.value() == 5);
        auto s = f.read(2);
        REQUIRE(s.is_ok());
        REQUIRE(s.value() == "is");
    }

    SECTION("Move construct") {
        file::file f2(std::move(f));

        REQUIRE(f.closed());
        REQUIRE(!f2.closed());
        REQUIRE(f.read().is_error());
        REQUIRE(f2.read().is_ok());
    }
    
    SECTION("Move assign") {
        file::file f2 = std::move(f);

        REQUIRE(f.closed());
        REQUIRE(!f2.closed());
        REQUIRE(f.read().is_error());
        REQUIRE(f2.read().is_ok());
    }
}

TEST_CASE("Benchmark words", "[bench]") {
    BENCHMARK("file wc") {
        uint64_t count = 0;
        if (auto f = file::open("words"); f.is_ok()) {
            auto& file = f.value();
            for (auto& line : file.lines()) {
                (void)line;
                count++;
            }
            return count;
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
        return f.value().read().value();
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



