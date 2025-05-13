/**
 * @file
 * @author Edward A. Lee
 * @author Soroush Bateni
 * @author Hou Seng (Steven) Wong
 * @copyright (c) 2020-2023, The University of California at Berkeley.
 * License: <a href="https://github.com/lf-lang/reactor-c/blob/main/LICENSE.md">BSD 2-clause</a>
 * @brief Time and tag definitions and functions for Lingua Franca
 */

#ifndef REACTOR_UC_TAG_H
#define REACTOR_UC_TAG_H

#define NSEC(t) ((interval_t)(t * 1LL))
#define NSECS(t) ((interval_t)(t * 1LL))
#define USEC(t) ((interval_t)(t * 1000LL))
#define USECS(t) ((interval_t)(t * 1000LL))
#define MSEC(t) ((interval_t)(t * 1000000LL))
#define MSECS(t) ((interval_t)(t * 1000000LL))
#define SEC(t) ((interval_t)(t * 1000000000LL))
#define SECS(t) ((interval_t)(t * 1000000000LL))
#define SECOND(t) ((interval_t)(t * 1000000000LL))
#define SECONDS(t) ((interval_t)(t * 1000000000LL))
#define MINS(t) ((interval_t)(t * 60000000000LL))
#define MINUTE(t) ((interval_t)(t * 60000000000LL))
#define MINUTES(t) ((interval_t)(t * 60000000000LL))
#define HOUR(t) ((interval_t)(t * 3600000000000LL))
#define HOURS(t) ((interval_t)(t * 3600000000000LL))
#define DAY(t) ((interval_t)(t * 86400000000000LL))
#define DAYS(t) ((interval_t)(t * 86400000000000LL))
#define WEEK(t) ((interval_t)(t * 604800000000000LL))
#define WEEKS(t) ((interval_t)(t * 604800000000000LL))

#define NEVER ((interval_t)LLONG_MIN)
#define NEVER_MICROSTEP 0u
#define FOREVER ((interval_t)LLONG_MAX)
#define FOREVER_MICROSTEP UINT_MAX
#define NEVER_TAG                                                                                                      \
  (tag_t) {                                                                                                            \
    .time = NEVER, .microstep = NEVER_MICROSTEP                                                                        \
  }
// Need a separate initializer expression to comply with some C compilers
#define NEVER_TAG_INITIALIZER {NEVER, NEVER_MICROSTEP}
#define FOREVER_TAG                                                                                                    \
  (tag_t) {                                                                                                            \
    .time = FOREVER, .microstep = FOREVER_MICROSTEP                                                                    \
  }
// Need a separate initializer expression to comply with some C compilers
#define FOREVER_TAG_INITIALIZER {FOREVER, FOREVER_MICROSTEP}
#define ZERO_TAG (tag_t){.time = 0LL, .microstep = 0u}

// Returns true if timeout has elapsed.
#define CHECK_TIMEOUT(start, duration) (lf_time_physical() > ((start) + (duration)))

// Convenience for converting times
#define BILLION ((instant_t)1000000000LL)

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>

#ifdef PRId64
#define PRINTF_TIME "%" PRId64
#else
#define PRINTF_TIME "%" PRIu32
#endif

#define PRINTF_MICROSTEP "%" PRIu32
#define PRINTF_TAG "(" PRINTF_TIME "," PRINTF_MICROSTEP ")"

////////////////  Type definitions

/**
 * Time instant. Both physical and logical times are represented
 * using this typedef.
 */
typedef int64_t instant_t;

/**
 * Interval of time.
 */
typedef int64_t interval_t;

/**
 * Microstep instant.
 */
typedef uint32_t microstep_t;

/**
 * A tag is a time, microstep pair.
 */
typedef struct {
  instant_t time;
  microstep_t microstep;
} tag_t;

////////////////  Functions

/**
 * Return the current tag, a logical time, microstep pair.
 * @param env A pointer to the environment from which we want the current tag.
 */
tag_t lf_tag(void *env);

/**
 * Add two tags.  If either tag has has NEVER or FOREVER in its time field, then
 * return NEVER_TAG or FOREVER_TAG, respectively. Also return NEVER_TAG or FOREVER_TAG
 * if the result underflows or overflows when adding the times.
 * If the microstep overflows, also return FOREVER_TAG.
 * If the time field of the second tag is greater than 0, then the microstep of the first tag
 * is reset to 0 before adding. This models the delay semantics in LF and makes this
 * addition operation non-commutative.
 * @param tag1 The first tag.
 * @param tag2 The second tag.
 */
tag_t lf_tag_add(tag_t tag1, tag_t tag2);

/**
 * Compare two tags. Return -1 if the first is less than
 * the second, 0 if they are equal, and +1 if the first is
 * greater than the second. A tag is greater than another if
 * its time is greater or if its time is equal and its microstep
 * is greater.
 * @param tag1
 * @param tag2
 * @return -1, 0, or 1 depending on the relation.
 */
int lf_tag_compare(tag_t tag1, tag_t tag2);

/**
 * Delay a tag by the specified time interval to realize the "after" keyword.
 * Any interval less than 0 (including NEVER) is interpreted as "no delay",
 * whereas an interval equal to 0 is interpreted as one microstep delay.
 * If the time field of the tag is NEVER or the interval is negative,
 * return the unmodified tag. If the time interval is 0LL, add one to
 * the microstep, leave the time field alone, and return the result.
 * Otherwise, add the interval to the time field of the tag and reset
 * the microstep to 0. If the sum overflows, saturate the time value at
 * FOREVER. For example:
 * - if tag = (t, 0) and interval = 10, return (t + 10, 0)
 * - if tag = (t, 0) and interval = 0, return (t, 1)
 * - if tag = (t, 0) and interval = NEVER, return (t, 0)
 * - if tag = (FOREVER, 0) and interval = 10, return (FOREVER, 0)
 *
 * @param tag The tag to increment.
 * @param interval The time interval.
 */
tag_t lf_delay_tag(tag_t tag, interval_t interval);

/**
 * Return the latest tag strictly less than the specified tag plus the
 * interval, unless tag is NEVER or interval is negative (including NEVER),
 * in which case return the tag unmodified.  Any interval less than 0
 * (including NEVER) is interpreted as "no delay", whereas an interval
 * equal to 0 is interpreted as one microstep delay. If the time sum
 * overflows, saturate the time value at FOREVER.  For example:
 * - if tag = (t, 0) and interval = 10, return (t + 10 - 1, UINT_MAX)
 * - if tag = (t, 0) and interval = 0, return (t, 0)
 * - if tag = (t, 0) and interval = NEVER, return (t, 0)
 * - if tag = (FOREVER, 0) and interval = 10, return (FOREVER, 0)
 *
 * @param tag The tag to increment.
 * @param interval The time interval.
 */
tag_t lf_delay_strict(tag_t tag, interval_t interval);

instant_t lf_time_add(instant_t time, interval_t interval);

#endif // REACTOR_UC_TAG_H
