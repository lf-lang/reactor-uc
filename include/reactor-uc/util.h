#ifndef REACTOR_UC_UTIL_H
#define REACTOR_UC_UTIL_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define CAT(a, b) a##b

#define PASTE(a, b) CAT(a, b)

#define JOIN(prefix, name) PASTE(prefix, PASTE(_, name))

#define SWAP(TYPE, a, b)                                                                                               \
  {                                                                                                                    \
    TYPE temp = *(a);                                                                                                  \
    *(a) = *(b);                                                                                                       \
    *(b) = temp;                                                                                                       \
  }

#define foreach(a, b, c) for (JOIN(a, it) c = JOIN(JOIN(a, it), each)(b); !c.done; c.step(&c))

#define len(a) (sizeof(a) / sizeof(*(a)))

#endif
