/**
 * @file
 * @author Edward A. Lee
 * @author Soroush Bateni
 * @author Hou Seng (Steven) Wong
 * @copyright (c) 2020-2023, The University of California at Berkeley
 * License in [BSD 2-clause](https://github.com/lf-lang/reactor-c/blob/main/LICENSE.md)
 * @brief Implementation of time and tag functions for Lingua Franca programs.
 */

#include "reactor-uc/environment.h"
#include "reactor-uc/tag.h"

/**
 * An enum for specifying the desired tag when calling "lf_time"
 */
typedef enum { LF_LOGICAL, LF_PHYSICAL, LF_ELAPSED_LOGICAL, LF_ELAPSED_PHYSICAL, LF_START } lf_time_type;

////////////////  Functions declared in tag.h

tag_t lf_tag(void *env) {
  (void)env;
  return ((Environment *)env)->current_tag;
}

instant_t lf_time_add(instant_t time, interval_t interval) {
  if (time == NEVER || interval == NEVER) {
    return NEVER;
  }
  if (time == FOREVER || interval == FOREVER) {
    return FOREVER;
  }
  return time + interval; // TODO: Here we assume that we never wrap...
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

// instant_t lf_time_logical(void *env) { return ((Environment *)env)->current_tag.time; }

// interval_t lf_time_logical_elapsed(void *env) { return lf_time_logical(env) - start_time; }

// instant_t lf_time_physical(void) {
//   instant_t now = MSEC(0);
//   // Get the current clock value
//   return now;
// }

// instant_t lf_time_physical_elapsed(void) { return lf_time_physical() - start_time; }

// instant_t lf_time_start(void) { return start_time; }

// size_t lf_readable_time(char *buffer, instant_t time) {
//   if (time <= (instant_t)0) {
//     snprintf(buffer, 2, "0");
//     return 1;
//   }
//   char *original_buffer = buffer;
//   bool lead = false; // Set to true when first clause has been printed.
//   if (time > WEEKS(1)) {
//     lead = true;
//     size_t printed = lf_comma_separated_time(buffer, time / WEEKS(1));
//     time = time % WEEKS(1);
//     buffer += printed;
//     snprintf(buffer, 7, " weeks");
//     buffer += 6;
//   }
//   if (time > DAYS(1)) {
//     if (lead == true) {
//       snprintf(buffer, 3, ", ");
//       buffer += 2;
//     }
//     lead = true;
//     size_t printed = lf_comma_separated_time(buffer, time / DAYS(1));
//     time = time % DAYS(1);
//     buffer += printed;
//     snprintf(buffer, 3, " d");
//     buffer += 2;
//   }
//   if (time > HOURS(1)) {
//     if (lead == true) {
//       snprintf(buffer, 3, ", ");
//       buffer += 2;
//     }
//     lead = true;
//     size_t printed = lf_comma_separated_time(buffer, time / HOURS(1));
//     time = time % HOURS(1);
//     buffer += printed;
//     snprintf(buffer, 3, " h");
//     buffer += 2;
//   }
//   if (time > MINUTES(1)) {
//     if (lead == true) {
//       snprintf(buffer, 3, ", ");
//       buffer += 2;
//     }
//     lead = true;
//     size_t printed = lf_comma_separated_time(buffer, time / MINUTES(1));
//     time = time % MINUTES(1);
//     buffer += printed;
//     snprintf(buffer, 5, " min");
//     buffer += 4;
//   }
//   if (time > SECONDS(1)) {
//     if (lead == true) {
//       snprintf(buffer, 3, ", ");
//       buffer += 2;
//     }
//     lead = true;
//     size_t printed = lf_comma_separated_time(buffer, time / SECONDS(1));
//     time = time % SECONDS(1);
//     buffer += printed;
//     snprintf(buffer, 3, " s");
//     buffer += 2;
//   }
//   if (time > (instant_t)0) {
//     if (lead == true) {
//       snprintf(buffer, 3, ", ");
//       buffer += 2;
//     }
//     const char *units = "ns";
//     if (time % MSEC(1) == (instant_t)0) {
//       units = "ms";
//       time = time / MSEC(1);
//     } else if (time % USEC(1) == (instant_t)0) {
//       units = "us";
//       time = time / USEC(1);
//     }
//     size_t printed = lf_comma_separated_time(buffer, time);
//     buffer += printed;
//     snprintf(buffer, 4, " %s", units);
//     buffer += strlen(units) + 1;
//   }
//   return (buffer - original_buffer);
// }

// size_t lf_comma_separated_time(char *buffer, instant_t time) {
//   size_t result = 0; // The number of characters printed.
//   // If the number is zero, print it and return.
//   if (time == (instant_t)0) {
//     snprintf(buffer, 2, "0");
//     return 1;
//   }
//   // If the number is negative, print a minus sign.
//   if (time < (instant_t)0) {
//     snprintf(buffer, 2, "-");
//     buffer++;
//     result++;
//   }
//   int count = 0;
//   // Assume the time value is no larger than 64 bits.
//   instant_t clauses[7];
//   while (time > (instant_t)0) {
//     clauses[count++] = time;
//     time = time / 1000;
//   }
//   // Highest order clause should not be filled with zeros.
//   instant_t to_print = clauses[--count] % 1000;
//   snprintf(buffer, 5, "%lld", (long long)to_print);
//   if (to_print >= 100LL) {
//     buffer += 3;
//     result += 3;
//   } else if (to_print >= 10LL) {
//     buffer += 2;
//     result += 2;
//   } else {
//     buffer += 1;
//     result += 1;
//   }
//   while (count-- > 0) {
//     to_print = clauses[count] % 1000LL;
//     snprintf(buffer, 8, ",%03lld", (long long)to_print);
//     buffer += 4;
//     result += 4;
//   }
//   return result;
// }
