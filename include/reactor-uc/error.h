#ifndef REACTOR_UC_ERROR_H
#define REACTOR_UC_ERROR_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief An enumeration of possible return values from functions in reactor-uc.
 * This is comparable to errno in C. Feel free to add more error codes as needed.
 */
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
  LF_NO_MEM,
  LF_COULD_NOT_CONNECT,
  LF_NETWORK_SETUP_FAILED
} lf_ret_t;

/**
 * @brief We use assert for "zero-cost" checking of assumptions. Zero-cost because it is removed in release builds.
 * We use the following validate macros for runtime checking of assumptions. They are not removed in release builds.
 */
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

#define throw(msg)                                                                                                     \
  do {                                                                                                                 \
    printf("Exception `%s` at %s:%d\n", msg, __FILE__, __LINE__);                                                      \
    exit(1);                                                                                                           \
  } while (0)

#endif
