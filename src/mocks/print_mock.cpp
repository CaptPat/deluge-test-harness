#include <cstdio>
#include <iostream>

// Stub debug output
namespace Debug {
void println(const char* msg) { std::cout << msg << std::endl; }
void print(const char* msg) { std::cout << msg; }
} // namespace Debug

extern "C" {
void putchar_(char c) { putchar(c); }
}
