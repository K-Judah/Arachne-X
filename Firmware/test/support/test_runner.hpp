#pragma once

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace tests {
struct Case { const char* name; void (*run)(); };
inline std::vector<Case>& cases() { static std::vector<Case> value; return value; }
struct Register {
    Register(const char* name, void (*run)()) { cases().push_back({name, run}); }
};
inline void check(bool result, const char* expression, const char* file, int line) {
    if (!result) throw std::runtime_error(std::string(file) + ":" + std::to_string(line) + ": " + expression);
}
inline int run() {
    std::size_t failures = 0;
    for (const auto& test : cases()) {
        try { test.run(); std::cout << "PASS " << test.name << '\n'; }
        catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
        }
    }
    std::cout << cases().size() - failures << '/' << cases().size() << " cases passed\n";
    return failures == 0 ? 0 : 1;
}
} // namespace tests

#define TEST(name) static void name(); static tests::Register reg_##name(#name, name); static void name()
#define CHECK(expr) tests::check(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define NEAR(a, b) CHECK(std::abs((a) - (b)) < 1e-9)
