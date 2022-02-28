
#include "catch.hpp"

#include "../file.h"

static const char* test_txt = 
R"(this is a line
this is line 2
end
)";


TEST_CASE("Read a file") {

    auto f = file::open("test.txt");

    SECTION("Can read into a raw buffer") {
        char buf[100];
        size_t size = f.read(buf, 100);
        REQUIRE(size == 34);
        REQUIRE(size == f.size());
    } 

    SECTION("Can read whole file as a string") {
        std::string s = f.read();
        REQUIRE(s.size() == 34);
        REQUIRE(s.size() == f.size());    
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
}
