#include "mediaxx.h"
#include "simdjson.h"
#include <iostream>
#include <map>

using namespace std;

void test() {
    mediaxx_get_libav_version();
}

int main(int argn, char** argv) {
    std::cout << "======= Test Start =======" << std::endl;
    test();

    std::map<std::string, std::string> data{
        {"name",     "simdjson"},
        {"type",     "parser"  },
        {"language", "C++23"   },
        {"version",  "4.0.7"   }
    };

    simdjson::builder::string_builder sb;
    sb.append(data);
    std::string_view p{sb};

    std::cout << "Generated JSON: " << p << std::endl;

    mediaxx_get_media_info("C:/0Acoolight/creation/下一站翻身.mov");
    // get_media_info("C:/0Acoolight/Music/VSinger/Great Voyage_洛天依.mp3");
    std::cout << "======= Test Done =======" << std::endl;
    std::cout << ">>>";
    int num = 0;
    cin >> num;
    return 0;
}