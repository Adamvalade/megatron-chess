// Runner for tests in tests.h (move generation, perft, etc.)
#include "tests.h"
#include <iostream>

int main() {
    run_tests();
    std::cout << "All tests completed.\n";
    return 0;
}
