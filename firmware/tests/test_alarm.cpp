// Host-side tests for AlarmService (no ESP-IDF needed).
#include "AlarmService.hpp"

#include <cstdio>

namespace {
int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
}

int main()
{
    using vaiwu::AlarmService;
    using vaiwu::ClockReading;

    AlarmService svc;
    CHECK(svc.count() == 0);

    // Relative reminder fires after its deadline.
    CHECK(svc.addAfter(10, 0));            // due at t = 600000 ms
    ClockReading unsynced{};
    CHECK(!svc.due(unsynced, 599000));     // not yet
    CHECK(svc.due(unsynced, 600001));      // now
    CHECK(svc.count() == 0);               // consumed

    // Absolute alarm fires once when the clock reaches the minute.
    CHECK(svc.addAt(7, 30));
    ClockReading before{}; before.synced = true; before.hour = 7; before.minute = 29;
    CHECK(!svc.due(before, 1000));
    ClockReading at{}; at.synced = true; at.hour = 7; at.minute = 30;
    CHECK(svc.due(at, 2000));
    CHECK(svc.count() == 0);

    // Absolute alarm does NOT fire while unsynced.
    CHECK(svc.addAt(8, 0));
    CHECK(!svc.due(unsynced, 3000));
    CHECK(svc.count() == 1);
    svc.cancelAll();
    CHECK(svc.count() == 0);

    // Capacity limit.
    for (int i = 0; i < AlarmService::kMaxAlarms; ++i) {
        CHECK(svc.addAt(i, 0));
    }
    CHECK(!svc.addAt(23, 0)); // full
    CHECK(svc.count() == AlarmService::kMaxAlarms);

    // Reject invalid times.
    AlarmService s2;
    CHECK(!s2.addAt(24, 0));
    CHECK(!s2.addAt(0, 60));
    CHECK(!s2.addAfter(0, 0));

    if (failures == 0) { std::printf("ALL ALARM TESTS PASSED\n"); return 0; }
    std::printf("%d FAILURES\n", failures);
    return 1;
}
