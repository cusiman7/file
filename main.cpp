
#include "file.h"

#include <vector>

int main() {
    std::cout << "write raw file\n";
    {
        auto f = file::open<file::raw>("test_write.txt", file::mode::write);

        std::string s("jello\n");

        f.write(s.data(), s.size());
        f.sync();
    }

    std::cout << "read whole file\n";
    {
        auto f = file::open("test_write.txt");
        std::cout << f.read();
    }
    
    std::cout << "read line loop\n";
    {
        auto f = file::open("test.txt");
        std::string line;
        while(f.read_line(line)) {
            std::cout << line << "\n";
        }
    }

    std::cout << "read test.txt\n";
    {
        auto f = file::open("test.txt");

        for (const std::string& line : f) {
            std::cout << line << "\n";
        }
    }

    std::cout << "read day1\n";
    {
        auto f = file::open("day1.txt");
        std::string day1 = f.read();
        DBG(day1.size());
    }

    std::cout << "write text\n";
    {
        auto f = file::open("test_write2.txt", file::mode::write);
        std::string s("Hello World\n");
        f.write(s);
    }
}

