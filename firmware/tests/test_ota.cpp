#include "vaiwu/VersionCompare.hpp"

#include <cstdio>

namespace {

int failures = 0;

void check(bool cond, const char* name)
{
    if (!cond) {
        std::printf("FAIL: %s\n", name);
        ++failures;
    }
}

} // namespace

int main()
{
    using vaiwu::isNewerVersion;

    check(!isNewerVersion("4.9", "4.9"), "equal is not newer");
    check(!isNewerVersion("4.9", "v4.9"), "equal with v prefix");
    check(isNewerVersion("4.9", "v5.0"), "major bump");
    check(isNewerVersion("4.9", "4.10"), "4.10 > 4.9 (not string compare)");
    check(!isNewerVersion("4.10", "4.9"), "reverse is false");
    check(isNewerVersion("v4.9", "4.9.1"), "patch bump");
    check(!isNewerVersion("4.9.1", "4.9"), "fewer parts is not newer");
    check(isNewerVersion("4.9", "4.9.0.1"), "fourth part");
    check(!isNewerVersion("4.9", "4.9.0.0"), "zero fourth part equal");
    check(!isNewerVersion(nullptr, "5.0"), "null current");
    check(!isNewerVersion("4.9", nullptr), "null latest");
    check(!isNewerVersion("4.9", "garbage"), "garbage latest");
    check(isNewerVersion("4.9", "5.0-rc1"), "suffix after version is cut");

    if (failures == 0) {
        std::printf("test_ota: OK\n");
        return 0;
    }
    return 1;
}
