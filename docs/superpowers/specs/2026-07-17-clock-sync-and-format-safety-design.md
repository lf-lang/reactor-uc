# Clock Synchronization and Format Safety Design

## Goal

Fix the confirmed runtime defects behind mixed POSIX/Zephyr federation startup failures and unsafe tag logging, while preserving the existing physical-clock semantics and public federation configuration.

## Scope

The change has three bounded parts:

1. Correct every `PRINTF_TAG` call in `src/` that passes a `tag_t` object instead of separate `time` and `microstep` arguments. Add compiler format checking to the logging API so equivalent mistakes fail the build.
2. Prevent a clock-synchronized non-grandmaster federate from entering start-time negotiation until its first synchronization round has completed. A grandmaster is ready immediately because it defines the clock scale.
3. Make scheduler handling of large initial clock steps safe by updating queued system events in place and using overflow-safe time arithmetic.

Changing POSIX from `CLOCK_REALTIME`, normalizing all platform epochs, or redesigning the clock synchronization protocol is outside scope.

## Architecture

### Format-safe logging

`log_message` and `log_printf` will carry the compiler's printf-format annotation on GCC- and Clang-compatible toolchains. A portable macro will hide the compiler-specific attribute.

Every runtime call site using `PRINTF_TAG` will pass exactly two scalar arguments:

```c
tag.time, tag.microstep
```

Pointer conversions will receive the pointer that corresponds to each `%p`. The audit covers all production sources under `src/`, not only `FederatedInputConnection_prepare`. Tests will compile production sources with the existing `-Wall -Wextra -Werror` settings, making a future mismatch a build failure.

### Initial synchronization gate

`ClockSynchronization` will explicitly initialize its readiness state. Grandmasters start ready; non-grandmasters become ready only after `ClockSynchronization_correct_clock` successfully completes its first correction or step.

The startup coordinator will separate "handshake completed" from "start-time proposal may begin." When clock synchronization is disabled or ready, it schedules the existing proposal event. Otherwise it remains in the negotiating state without scheduling that event. Completion of the first synchronization round notifies the startup coordinator to schedule negotiation exactly once.

This ordering ensures timer and startup events are not created on a clock scale that is about to change by decades. Existing behavior for clock-sync-disabled federations and transient joining remains unchanged.

### Large clock-step handling

`Scheduler_step_clock` will mutate each queued system event rather than a stack copy. It will use the runtime's saturating time-add helper so large positive or negative steps cannot invoke signed-overflow undefined behavior. Negative results continue to clamp to zero, matching current intent.

The initial clock correction will only mark synchronization complete after the clock update succeeds. Errors will remain diagnosable rather than silently releasing the startup gate.

The Zephyr fatal handler will not be redesigned in this change. Without hardware, there is insufficient evidence to choose an appropriate reboot, halt, or default-handler policy. The tag-format undefined behavior and the scheduler's failure to update queued events are confirmed defects that plausibly explain parts of the observed failure, but their causal connection to the Zephyr panic is not established. Hardware regression testing is the deciding validation. The startup gate independently reduces the risk surface: a non-grandmaster's large initial step occurs before application startup and timer events are queued, so `step_clock` only has to adjust runtime system events at that point.

## Data Flow

1. Federates connect and complete the existing startup handshake.
2. A grandmaster may begin start-time negotiation immediately.
3. A non-grandmaster continues processing system/network events but does not schedule application startup or timers.
4. Its first clock-sync exchange computes and applies the initial correction.
5. Clock synchronization marks itself ready and signals the startup coordinator.
6. The coordinator schedules the existing start-time proposal flow using the corrected clock scale.
7. Normal periodic synchronization proceeds unchanged.

Clock synchronization can make progress while start-time negotiation is gated. Its constructor schedules the first request as a system event at time zero, and the scheduler continues handling system and network events while the application start time is `NEVER`. The clock-sync exchange therefore does not depend on application startup or start-time proposals.

Under the normal startup sequence, `FederatedEnvironment_assemble` blocks until every network channel reports connected before the scheduler can process the first clock-sync request. A channel may still disconnect or reject a send afterward. The request handler already schedules the next periodic request regardless of the send result, so this path can recover, but its current failure branch incorrectly uses the self sentinel (`-1`) as a neighbor index. The implementation will invalidate the actual master neighbor safely and preserve the periodic retry.

## Error Handling

- A failed initial `set_time` does not set the synchronization-ready state and does not start negotiation.
- Duplicate readiness notifications are idempotent and cannot schedule duplicate proposal events.
- A failed clock-sync request cannot index neighbor state with the self sentinel; the next periodic request remains scheduled so a restored channel can complete readiness.
- Overflow while shifting system-event times saturates through the existing time arithmetic helper; values below zero clamp to zero.
- Format mismatches are rejected during supported compiler builds.

## Testing

Host-side tests will cover:

- explicit initial readiness for grandmaster and non-grandmaster clock synchronizers;
- withholding start-time negotiation before initial sync and releasing it exactly once afterward;
- the core gate invariant that no application startup or timer event exists in the event queue before a non-grandmaster becomes ready;
- a clock-sync request send failure followed by periodic retry and eventual readiness after the channel recovers, including protection against the current `-1` neighbor-index access;
- a simulated offset comparable to the POSIX epoch versus MCU uptime difference;
- in-place shifting of queued system events for large positive and negative steps;
- successful compilation of every production `PRINTF_TAG` and `PRINTF_TIME` call with format checking enabled in separate POSIX and Zephyr builds;
- the existing POSIX unit and integration suites.

A Zephyr build will verify platform compilation if the local SDK/toolchain supports the upstream target. Its final `.config` must select `CONFIG_CBPRINTF_COMPLETE=y` and `CONFIG_CBPRINTF_FULL_INTEGRAL=y`, because reactor-uc time values are 64-bit and reduced-integral `cbprintf` configurations may print the conversion specification or truncate values on a 32-bit target. The Zephyr test/example configuration used for validation will select these options explicitly rather than relying only on defaults. Hardware flashing and the mixed native/ESP32-C6 runtime test are deferred until a board is available and are required before opening the upstream pull request.

## Delivery

Implementation will be organized into reviewable commits for format safety, startup synchronization ordering, and large-step scheduler hardening/tests. No files under `/opt/reactor-uc` or `/workspaces` will be modified by the implementation.
