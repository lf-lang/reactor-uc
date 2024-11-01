#ifndef REACTOR_UC_REACTOR_H
#define REACTOR_UC_REACTOR_H

#define REACTOR_NAME_MAX_LEN 128
#include "reactor-uc/error.h"
#include <stddef.h>

typedef struct Reactor Reactor;
typedef struct Environment Environment;
typedef struct BuiltinTrigger BuiltinTrigger;
typedef struct Reaction Reaction;
typedef struct Trigger Trigger;

struct Reactor {
  Environment *env;
  void (*register_startup)(Reactor *self, BuiltinTrigger *startup);
  void (*register_shutdown)(Reactor *self, BuiltinTrigger *shutdown);
  lf_ret_t (*calculate_levels)(Reactor *self);
  Reactor *parent;
  Reactor **children;
  size_t children_size;
  Reaction **reactions;
  size_t reactions_size;
  Trigger **triggers;
  size_t triggers_size;
  char name[REACTOR_NAME_MAX_LEN];
};

void Reactor_ctor(Reactor *self, const char *name, Environment *env, Reactor *parent, Reactor **children,
                  size_t children_size, Reaction **reactions, size_t reactions_size, Trigger **triggers,
                  size_t triggers_size);

void Reactor_verify(Reactor *self);
#endif
