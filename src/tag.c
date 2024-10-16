/**
 * @file
 * @author Edward A. Lee
 * @author Soroush Bateni
 * @author Hou Seng (Steven) Wong
 * @copyright (c) 2020-2023, The University of California at Berkeley
 * License in [BSD 2-clause](https://github.com/lf-lang/reactor-c/blob/main/LICENSE.md)
 * @brief Implementation of time and tag functions for Lingua Franca programs.
 */

#include "reactor-uc/tag.h"
#include "reactor-uc/environment.h"

instant_t lf_time_add(instant_t time, interval_t interval) {
  if (time == NEVER || interval == NEVER) {
    return NEVER;
  }
  if (time == FOREVER || interval == FOREVER) {
    return FOREVER;
  }
  return time + interval;
}

tag_t lf_tag_add(tag_t tag1, tag_t tag2) {
  if (tag1.time == NEVER || tag2.time == NEVER) {
    return NEVER_TAG;
  }
  if (tag1.time == FOREVER || tag2.time == FOREVER) {
    return FOREVER_TAG;
  }
  if (tag2.time > 0) {
    tag1.microstep = 0; // Ignore microstep of first arg if time of second is > 0.
  }
  tag_t result = {.time = tag1.time + tag2.time, .microstep = tag1.microstep + tag2.microstep};
  if (result.microstep < tag1.microstep) {
    return FOREVER_TAG;
  }
  if (result.time < tag1.time && tag2.time > 0) {
    return FOREVER_TAG;
  }
  if (result.time > tag1.time && tag2.time < 0) {
    return NEVER_TAG;
  }
  return result;
}

int lf_tag_compare(tag_t tag1, tag_t tag2) {
  if (tag1.time < tag2.time) {
    return -1;
  }
  if (tag1.time > tag2.time) {
    return 1;
  }
  if (tag1.microstep < tag2.microstep) {
    return -1;
  }
  if (tag1.microstep > tag2.microstep) {
    return 1;
  }
  return 0;
}

tag_t lf_delay_tag(tag_t tag, interval_t interval) {
  if (tag.time == NEVER || interval < 0LL) {
    return tag;
  }
  // Note that overflow in C is undefined for signed variables.
  if (tag.time >= FOREVER - interval) {
    return FOREVER_TAG; // Overflow.
  }
  tag_t result = tag;
  if (interval == 0LL) {
    // Note that unsigned variables will wrap on overflow.
    // This is probably the only reasonable thing to do with overflowing
    // microsteps.
    result.microstep++;
  } else {
    result.time += interval;
    result.microstep = 0;
  }
  return result;
}

tag_t lf_delay_strict(tag_t tag, interval_t interval) {
  tag_t result = lf_delay_tag(tag, interval);
  if (interval != 0 && interval != NEVER && interval != FOREVER && result.time != NEVER && result.time != FOREVER) {
    result.time -= 1;
    result.microstep = UINT_MAX;
  }
  return result;
}