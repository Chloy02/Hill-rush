#include "TestFramework.hpp"

#include "core/Log.hpp"

#include <cstring>

int main(int argc, char** argv)
{
    hr::log::setLevel(hr::log::Level::Warn);
    const char* filter = argc > 1 ? argv[1] : nullptr;

    int run = 0;
    int failedTests = 0;
    for (const auto& test : testfw::registry()) {
        if (filter && !std::strstr(test.name, filter)) continue;
        const int before = testfw::failures();
        std::fprintf(stderr, "[ RUN  ] %s\n", test.name);
        test.fn();
        run++;
        if (testfw::failures() != before) {
            failedTests++;
            std::fprintf(stderr, "[ FAIL ] %s\n", test.name);
        } else {
            std::fprintf(stderr, "[  OK  ] %s\n", test.name);
        }
    }
    std::fprintf(stderr, "\n%d tests, %d failed\n", run, failedTests);
    return failedTests == 0 && run > 0 ? 0 : 1;
}
