# Clock Synchronization and Format Safety Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate runtime tag-format undefined behavior, gate federated startup on initial clock synchronization, and safely handle large clock steps.

**Architecture:** Compile-time printf checking protects every runtime log call. A readiness handoff from `ClockSynchronization` to `StartupCoordinator` delays only initial start-time proposals, while system/network events continue running. Saturating time arithmetic and in-place system-event queue updates make large initial steps deterministic.

**Tech Stack:** C11, CMake/CTest, Unity, POSIX pthread platform, Zephyr 4.1, GCC/Clang printf attributes.

## Global Constraints

- Preserve the existing POSIX `CLOCK_REALTIME` and Zephyr uptime clock semantics.
- Do not redesign the clock synchronization protocol or Zephyr fatal handler.
- Non-grandmasters must not queue application startup or timer events before initial synchronization readiness.
- Grandmasters and clock-sync-disabled federates retain their existing startup behavior.
- Zephyr validation must use `CONFIG_CBPRINTF_COMPLETE=y` and `CONFIG_CBPRINTF_FULL_INTEGRAL=y`.
- Hardware regression is required before the upstream PR, but is deferred until an ESP32-C6 board is available.
- Modify only `/home/hao/reactor-uc`; do not modify `/opt/reactor-uc` or `/workspaces`.

---

## File Map

- `include/reactor-uc/logging.h`: portable printf-format attribute and annotated public logger.
- `src/logging.c`: annotated internal `log_printf`.
- `src/federated.c`, `src/environment.c`, `src/environments/federated_environment.c`, `src/queues.c`, `src/schedulers/dynamic/scheduler.c`: corrected production tag-format arguments.
- `test/compile/logging_format_mismatch.c`, `test/compile/logging_format_check.cmake`, `test/CMakeLists.txt`: negative compile test proving the attribute is active.
- `examples/zephyr/basic_federated/*/prj.conf`, `examples/zephyr/hello/prj.conf`: explicit 64-bit Zephyr formatting configuration.
- `src/tag.c`, `test/unit/tag_test.c`: overflow-safe `lf_time_add`.
- `src/schedulers/dynamic/scheduler.c`, `test/unit/scheduler_clock_step_test.c`: in-place queued system-event shifting.
- `include/reactor-uc/startup_coordinator.h`, `src/startup_coordinator.c`: idempotent initial-sync readiness gate.
- `include/reactor-uc/clock_synchronization.h`, `src/clock_synchronization.c`: explicit readiness initialization, successful-correction notification, and safe send-failure handling.
- `test/unit/clock_sync_startup_test.c`: readiness, queue invariant, retry, and large-offset integration tests.

### Task 1: Enforce Format-Safe Runtime Logging

**Files:**
- Create: `test/compile/logging_format_mismatch.c`
- Create: `test/compile/logging_format_check.cmake`
- Modify: `test/CMakeLists.txt`
- Modify: `include/reactor-uc/logging.h`
- Modify: `src/logging.c`
- Modify: `src/federated.c`
- Modify: `src/environment.c`
- Modify: `src/environments/federated_environment.c`
- Modify: `src/queues.c`
- Modify: `src/schedulers/dynamic/scheduler.c`
- Modify: `examples/zephyr/basic_federated/federated_sender/prj.conf`
- Modify: `examples/zephyr/basic_federated/federated_receiver1/prj.conf`
- Modify: `examples/zephyr/basic_federated/federated_receiver2/prj.conf`
- Modify: `examples/zephyr/hello/prj.conf`

**Interfaces:**
- Produces: `LF_PRINTF_FORMAT(format_index, first_argument_index)`.
- Produces: `log_message(int, const char*, const char*, ...) LF_PRINTF_FORMAT(3, 4)`.
- Preserves: `PRINTF_TAG` as a two-conversion format requiring `tag.time, tag.microstep`.

- [ ] **Step 1: Add a negative compile test**

Create `test/compile/logging_format_mismatch.c`:

```c
#include "reactor-uc/logging.h"

void logging_format_mismatch_probe(void) {
  log_message(LF_LOG_LEVEL_INFO, "TEST", "%d", "not an integer");
}
```

Create `test/compile/logging_format_check.cmake` to invoke `${CMAKE_C_COMPILER}` with `-Wformat -Werror`, the reactor include paths, and `-c logging_format_mismatch.c`. The CMake script must call `message(FATAL_ERROR ...)` when compilation succeeds and must pass only when the compiler rejects the mismatch.

Register it in `test/CMakeLists.txt`:

```cmake
add_test(
  NAME logging_format_check
  COMMAND ${CMAKE_COMMAND}
          -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
          -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
          -DBINARY_DIR=${CMAKE_BINARY_DIR}
          -P ${CMAKE_CURRENT_LIST_DIR}/compile/logging_format_check.cmake
)
```

- [ ] **Step 2: Run the test and verify RED**

Run:

```bash
cmake -S . -B build-posix -DBUILD_UNIT_TESTS=ON -DFEDERATED=ON
ctest --test-dir build-posix -R '^logging_format_check$' --output-on-failure
```

Expected: FAIL because the unannotated `log_message` declaration lets the intentionally invalid call compile.

- [ ] **Step 3: Add portable format annotations**

In `include/reactor-uc/logging.h`, define:

```c
#if defined(__GNUC__) || defined(__clang__)
#define LF_PRINTF_FORMAT(format_index, first_argument_index) \
  __attribute__((format(printf, format_index, first_argument_index)))
#else
#define LF_PRINTF_FORMAT(format_index, first_argument_index)
#endif
```

Declare:

```c
void log_message(int level, const char* module, const char* fmt, ...)
    LF_PRINTF_FORMAT(3, 4);
```

In `src/logging.c`, add an annotated forward declaration before the definition:

```c
static void log_printf(const char* fmt, ...) LF_PRINTF_FORMAT(1, 2);
```

Make the existing definition `static`.

- [ ] **Step 4: Verify the negative compile test is GREEN**

Run the same focused CTest command.

Expected: PASS because GCC/Clang reports the intentional `%d`/string mismatch and the CMake test recognizes that rejection.

- [ ] **Step 5: Build to expose every production mismatch**

Run:

```bash
cmake --build build-posix -j2
```

Expected: FAIL with `-Wformat` diagnostics at production `PRINTF_TAG` calls, proving the annotation reaches macro-expanded `LF_INFO`/`LF_DEBUG` calls.

- [ ] **Step 6: Correct the complete production audit**

For every `tag_t value` passed to `PRINTF_TAG`, replace the one struct argument with:

```c
value.time, value.microstep
```

For `FederatedInputConnection_prepare`, supply the missing downstream pointer:

```c
LF_INFO(FED,
        "FederatedInputConnection %p preparing downstream port %p for tag: " PRINTF_TAG,
        (void*)trigger, (void*)down, event->super.tag.time,
        event->super.tag.microstep);
```

For calls containing two tags, pass four scalar members in format order. Correct the two malformed scheduler strings:

```c
LF_DEBUG(SCHED, "sleeping until " PRINTF_TIME, next_tag.time);
LF_DEBUG(SCHED, "Acquired tag " PRINTF_TAG, next_tag.time, next_tag.microstep);
```

Run `rg -n "PRINTF_TAG" src` and inspect every remaining call; no production call may pass a struct value.

- [ ] **Step 7: Make Zephyr's 64-bit formatting requirement explicit**

Append to all four listed Zephyr `prj.conf` files:

```text
CONFIG_CBPRINTF_COMPLETE=y
CONFIG_CBPRINTF_FULL_INTEGRAL=y
```

- [ ] **Step 8: Verify POSIX build and tests**

Run:

```bash
cmake --build build-posix -j2
ctest --test-dir build-posix --output-on-failure
```

Expected: build succeeds, `logging_format_check` passes, and all existing unit tests pass.

- [ ] **Step 9: Commit**

```bash
git add include/reactor-uc/logging.h src/logging.c src/federated.c src/environment.c \
  src/environments/federated_environment.c src/queues.c \
  src/schedulers/dynamic/scheduler.c test/CMakeLists.txt test/compile \
  examples/zephyr/basic_federated examples/zephyr/hello/prj.conf
git commit -m "fix: make runtime tag logging format-safe"
```

### Task 2: Saturate Time Addition and Shift System Events In Place

**Files:**
- Create: `test/unit/tag_test.c`
- Create: `test/unit/scheduler_clock_step_test.c`
- Modify: `src/tag.c`
- Modify: `src/schedulers/dynamic/scheduler.c`

**Interfaces:**
- Preserves: `instant_t lf_time_add(instant_t time, interval_t interval)`.
- Produces: overflow saturation to `FOREVER`, underflow saturation to `NEVER`.
- Uses: `Scheduler.step_clock(Scheduler*, interval_t)`.

- [ ] **Step 1: Write failing boundary tests for `lf_time_add`**

Create `test/unit/tag_test.c` with Unity cases asserting:

```c
TEST_ASSERT_EQUAL_INT64(FOREVER, lf_time_add(FOREVER - 5, 10));
TEST_ASSERT_EQUAL_INT64(NEVER, lf_time_add(NEVER + 5, -10));
TEST_ASSERT_EQUAL_INT64(42, lf_time_add(40, 2));
TEST_ASSERT_EQUAL_INT64(FOREVER, lf_time_add(FOREVER, -1));
TEST_ASSERT_EQUAL_INT64(NEVER, lf_time_add(NEVER, 1));
```

Register the cases in `main` using `RUN_TEST`.

- [ ] **Step 2: Verify RED**

Run:

```bash
cmake --build build-posix --target tag_test_c
ctest --test-dir build-posix -R '^unit/tag_test_c$|tag_test_c' --output-on-failure
```

Expected: FAIL under UBSan or with an incorrect wrapped value because `lf_time_add` currently performs raw signed addition.

- [ ] **Step 3: Implement overflow-safe addition**

Replace the raw return in `src/tag.c` with pre-addition bounds checks:

```c
if (interval > 0 && time > FOREVER - interval) {
  return FOREVER;
}
if (interval < 0 && time < NEVER - interval) {
  return NEVER;
}
return time + interval;
```

Keep the existing sentinel checks before these comparisons.

- [ ] **Step 4: Verify tag tests GREEN**

Run the focused build and CTest command. Expected: PASS.

- [ ] **Step 5: Write failing scheduler queue tests**

Create `test/unit/scheduler_clock_step_test.c`. Construct real `EventQueue`, `ReactionQueue`, and `DynamicScheduler` objects, insert two `SystemEvent` values, call `scheduler.super.step_clock`, and assert directly on the queue:

```c
scheduler.super.step_clock(&scheduler.super, SEC(10));
TEST_ASSERT_EQUAL_INT64(SEC(11), system_event_queue.array[0].system_event.super.tag.time);
```

Add cases for:

- two events shifting forward while preserving heap order;
- a negative step clamping results below zero to zero;
- a near-`FOREVER` event saturating instead of wrapping.

- [ ] **Step 6: Verify scheduler tests RED**

Run:

```bash
cmake --build build-posix --target scheduler_clock_step_test_c
ctest --test-dir build-posix -R 'scheduler_clock_step_test_c' --output-on-failure
```

Expected: FAIL because `Scheduler_step_clock` modifies a local `ArbitraryEvent` copy.

- [ ] **Step 7: Mutate queue entries in place**

Implement:

```c
for (size_t i = 0; i < queue->size; i++) {
  instant_t new_tag =
      lf_time_add(queue->array[i].system_event.super.tag.time, step);
  if (new_tag < 0) {
    new_tag = 0;
  }
  queue->array[i].system_event.super.tag.time = new_tag;
}
```

Keep the queue mutex held for the complete loop.

- [ ] **Step 8: Verify focused and full tests GREEN**

Run both focused tests, then:

```bash
cmake --build build-posix -j2
ctest --test-dir build-posix --output-on-failure
```

Expected: all pass.

- [ ] **Step 9: Commit**

```bash
git add src/tag.c src/schedulers/dynamic/scheduler.c \
  test/unit/tag_test.c test/unit/scheduler_clock_step_test.c
git commit -m "fix: handle large clock steps without overflow"
```

### Task 3: Gate Initial Start-Time Negotiation on Clock Readiness

**Files:**
- Create: `test/unit/clock_sync_startup_test.c`
- Modify: `include/reactor-uc/startup_coordinator.h`
- Modify: `src/startup_coordinator.c`
- Modify: `src/clock_synchronization.c`

**Interfaces:**
- Produces: `void StartupCoordinator_clock_sync_ready(StartupCoordinator* self)`.
- Adds: `bool start_time_proposal_scheduled` to `StartupCoordinator`.
- Preserves: `ClockSynchronization.has_initial_sync` as the readiness state.

- [ ] **Step 1: Build a real-queue test fixture**

In `test/unit/clock_sync_startup_test.c`, construct:

- a `FederatedEnvironment` with real application and system `EventQueue` objects;
- a `DynamicScheduler`;
- a one-neighbor `StartupCoordinator` and `ClockSynchronization`;
- a fake `NetworkChannel` whose `send_blocking` result and sent-message count are controlled by the test.

Use real coordinator and clock-sync `SystemEventHandler.handle` functions rather than duplicating their logic.

- [ ] **Step 2: Write failing initialization tests**

Assert immediately after construction:

```c
TEST_ASSERT_TRUE(grandmaster.has_initial_sync);
TEST_ASSERT_FALSE(non_grandmaster.has_initial_sync);
```

Expected RED: `ClockSynchronization_ctor` does not initialize the field.

- [ ] **Step 3: Write the gate invariant test**

Drive the coordinator through completion of startup handshakes for a non-grandmaster while `has_initial_sync == false`. Assert:

```c
TEST_ASSERT_EQUAL(StartupCoordinationState_NEGOTIATING, coordinator.state);
TEST_ASSERT_FALSE(coordinator.start_time_proposal_scheduled);
TEST_ASSERT_EQUAL_UINT32(0, application_event_queue.size);
```

Also process an early external proposal and reassert that no application startup or timer event has entered the application queue.

Expected RED: current code always schedules the local proposal 50 ms after the handshake.

- [ ] **Step 4: Add the idempotent coordinator readiness API**

Add the field and declaration to the header:

```c
bool start_time_proposal_scheduled;
void StartupCoordinator_clock_sync_ready(StartupCoordinator* self);
```

In `src/startup_coordinator.c`, extract the proposal scheduling decision into a helper that requires:

```c
self->state == StartupCoordinationState_NEGOTIATING
&& !self->start_time_proposal_scheduled
&& (!env_fed->do_clock_sync || env_fed->clock_sync->has_initial_sync)
```

The helper schedules the existing proposal system event at physical time plus 50 ms and then sets the flag. Initialize the flag to false in the constructor. Replace the unconditional handshake-completion scheduling with the helper. The public readiness function calls the same helper, making duplicate notifications harmless.

- [ ] **Step 5: Initialize clock readiness explicitly**

In `ClockSynchronization_ctor`:

```c
self->has_initial_sync = self->is_grandmaster;
```

Do not notify the coordinator from the constructor; the environment may not have finished wiring its members yet.

- [ ] **Step 6: Release readiness only after successful correction**

Refactor `ClockSynchronization_correct_clock` so the initial path:

1. computes whether the step threshold is exceeded without negating `INT64_MIN`;
2. checks the return from `PhysicalClock.set_time` or `adjust_time`;
3. calls `scheduler->step_clock` only after a successful large step;
4. sets `has_initial_sync = true`;
5. calls `StartupCoordinator_clock_sync_ready(env_fed->startup_coordinator)`;
6. notifies the platform after the state and queue updates.

On failure, log the return code and leave readiness false.

- [ ] **Step 7: Verify gate and release tests GREEN**

Extend the test to deliver a valid first delay response with a simulated epoch-sized offset. Assert:

```c
TEST_ASSERT_TRUE(clock_sync.has_initial_sync);
TEST_ASSERT_TRUE(coordinator.start_time_proposal_scheduled);
TEST_ASSERT_EQUAL_UINT32(0, application_event_queue.size);
```

Call `StartupCoordinator_clock_sync_ready` again and assert the system queue size does not increase.

Run:

```bash
cmake --build build-posix --target clock_sync_startup_test_c
ctest --test-dir build-posix -R 'clock_sync_startup_test_c' --output-on-failure
```

Expected: PASS.

- [ ] **Step 8: Run the full POSIX suite**

Run the full build and CTest commands. Expected: all pass.

- [ ] **Step 9: Commit**

```bash
git add include/reactor-uc/startup_coordinator.h src/startup_coordinator.c \
  src/clock_synchronization.c test/unit/clock_sync_startup_test.c
git commit -m "fix: wait for initial clock sync before federation startup"
```

### Task 4: Recover Safely from Clock-Sync Send Failure and Verify Platforms

**Files:**
- Modify: `src/clock_synchronization.c`
- Modify: `test/unit/clock_sync_startup_test.c`

**Interfaces:**
- Preserves: periodic `ClockSyncMessage_request_sync_tag` self-event scheduling after every request attempt.
- Corrects: priority invalidation uses the saved master neighbor index, never `NEIGHBOR_INDEX_SELF`.

- [ ] **Step 1: Write the failing send/retry test**

Configure the fake channel so the first `send_blocking` returns `LF_ERR`. Process the initial request system event and assert:

- no memory before `neighbor_clock[0]` is modified (use a canary in the fixture);
- `master_neighbor_index` is reset through a valid neighbor update;
- a later request system event remains queued;
- readiness remains false.

Then simulate a restored channel and renewed master-priority message, process the periodic retry and a complete response exchange, and assert readiness becomes true.

- [ ] **Step 2: Verify RED**

Run the focused clock-sync test.

Expected: FAIL or ASan out-of-bounds report because the failure path currently calls `ClockSynchronization_handle_priority_update(self, -1, UNKNOWN_PRIORITY)`.

- [ ] **Step 3: Correct the failure index**

Before sending, save:

```c
int master_neighbor_index = self->master_neighbor_index;
```

Use that saved non-negative index for the bundle lookup, log, and failure update:

```c
ClockSynchronization_handle_priority_update(
    self, master_neighbor_index, UNKNOWN_PRIORITY);
```

Keep periodic request scheduling outside the success/failure branch.

- [ ] **Step 4: Verify focused test GREEN under sanitizers**

Configure and run:

```bash
cmake -S . -B build-asan -DBUILD_UNIT_TESTS=ON -DFEDERATED=ON -DASAN=ON
cmake --build build-asan -j2
ctest --test-dir build-asan -R 'clock_sync_startup_test_c' --output-on-failure
```

Expected: PASS with no sanitizer report.

- [ ] **Step 5: Verify the full POSIX build**

```bash
cmake --build build-posix -j2
ctest --test-dir build-posix --output-on-failure
git diff --check
```

Expected: all tests pass and no whitespace errors.

- [ ] **Step 6: Compile the Zephyr platform on a 32-bit target**

With the local Zephyr environment active:

```bash
export ZEPHYR_BASE=/workspaces/zephyrproject/zephyr
export ZEPHYR_SDK_INSTALL_DIR=/workspaces/zephyr-sdk
west build -d /home/hao/reactor-uc/build-zephyr-esp32c6 \
  -b esp32c6_devkitc /home/hao/reactor-uc/examples/zephyr/hello -p always
```

Expected: the Zephyr build succeeds, compiling the runtime with the target's 32-bit ABI and expanded `PRId64` format.

Verify:

```bash
rg '^CONFIG_CBPRINTF_(COMPLETE|FULL_INTEGRAL)=y$' \
  build-zephyr-esp32c6/zephyr/.config
```

Expected: both required settings are present.

- [ ] **Step 7: Record the hardware gate**

Do not claim the observed Zephyr panic fixed. Record in the final handoff that ESP32-C6 flash/run validation remains required before opening the PR, including both grandmaster assignments and at least 30 timer messages per direction.

- [ ] **Step 8: Commit**

```bash
git add src/clock_synchronization.c test/unit/clock_sync_startup_test.c
git commit -m "fix: retry clock sync safely after send failure"
```

### Task 5: Final Regression Review

**Files:**
- Review only: all files changed by Tasks 1-4.

- [ ] **Step 1: Inspect commit and scope**

```bash
git status --short
git log --oneline origin/main..HEAD
git diff --stat origin/main...HEAD
git diff --check origin/main...HEAD
```

Expected: only design/plan, runtime fixes, tests, and explicit Zephyr formatting configs are present.

- [ ] **Step 2: Re-run clean verification**

Delete no user data. Configure fresh build directories with new names, run the POSIX suite, ASan clock-sync test, and Zephyr compile commands from the prior tasks.

- [ ] **Step 3: Compare against the design**

Confirm every requirement in `docs/superpowers/specs/2026-07-17-clock-sync-and-format-safety-design.md` maps to a passing test or documented hardware gate. Confirm no claim states that the hardware panic is resolved.

- [ ] **Step 4: Prepare PR notes without opening a PR**

Summarize:

- confirmed defects fixed;
- test evidence and exact commands;
- the unverified causal relationship to the Zephyr panic;
- the required later ESP32-C6 matrix;
- removal criteria for downstream workarounds after hardware validation.

Do not push or open a PR until the user explicitly requests it.
