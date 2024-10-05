#ifndef REACTOR_UC_ERROR_H
#define REACTOR_UC_ERROR_H
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
  LF_OK = 0,
  LF_ERR,
  LF_SLEEP_INTERRUPTED,
  LF_INVALID_TAG,
  LF_AFTER_STOP_TAG,
  LF_PAST_TAG,
  LF_EMPTY,
  LF_INVALID_VALUE,
  LF_OUT_OF_BOUNDS,
} lf_ret_t;

// Runtime validation. Crashes the program if expr is not true
#define validate(expr)                                                                                                 \
  do {                                                                                                                 \
    if ((expr) == 0) {                                                                                                 \
      printf("Assertion failed at  %s:%d\n", __FILE__, __LINE__);                                                      \
      exit(1);                                                                                                         \
    }                                                                                                                  \
  } while (0)

// Runtime validation that expr==0
#define validaten(expr)                                                                                                \
  do {                                                                                                                 \
    if ((expr) != 0) {                                                                                                 \
      printf("Assertion failed at  %s:%d\n", __FILE__, __LINE__);                                                      \
      exit(1);                                                                                                         \
    }                                                                                                                  \
  } while (0)

#endif
