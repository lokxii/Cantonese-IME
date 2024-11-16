#include <cassert>
#include <iostream>

#include "ime.hpp"

int main(int argc, char** argv) {
    auto ime = IME("data");
    for (auto word : ime.split_words(argv[1])) {
        std::cout << word << std::endl;
    }
    // auto candidates = ime.candidates(argv[1]);
    // for (auto candidate : candidates) {
    //     std::cout << candidate << std::endl;
    // }
    return 0;
}
