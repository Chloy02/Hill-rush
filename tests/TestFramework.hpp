#pragma once

// A deliberately tiny test framework: TEST(name) registers a function,
// CHECK(cond) records a failure without aborting the test.

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace testfw {

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failures()
{
    static int count = 0;
    return count;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

inline void fail(const char* file, int line, const std::string& what)
{
    failures()++;
    std::fprintf(stderr, "  FAILED %s:%d: %s\n", file, line, what.c_str());
}

} // namespace testfw

#define TF_CONCAT2(a, b) a##b
#define TF_CONCAT(a, b) TF_CONCAT2(a, b)

#define TEST(name)                                                                           \
    static void TF_CONCAT(test_fn_, __LINE__)();                                             \
    static ::testfw::Registrar TF_CONCAT(test_reg_, __LINE__)(name, TF_CONCAT(test_fn_, __LINE__)); \
    static void TF_CONCAT(test_fn_, __LINE__)()

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) ::testfw::fail(__FILE__, __LINE__, #cond);         \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                                  \
    do {                                                                                       \
        const double tf_a = (a), tf_b = (b);                                                   \
        if (!(std::fabs(tf_a - tf_b) <= (eps)))                                                \
            ::testfw::fail(__FILE__, __LINE__,                                                 \
                           std::string(#a " ~= " #b " (") + std::to_string(tf_a) + " vs " +    \
                               std::to_string(tf_b) + ")");                                    \
    } while (0)
