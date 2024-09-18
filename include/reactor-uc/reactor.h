#ifndef REACTOR_UC_REACTOR_H
#define REACTOR_UC_REACTOR_H

#define REACTOR_NAME_MAX_LEN 128
#include <stdlib.h>

typedef struct Startup Startup;
typedef struct Reactor Reactor;
typedef struct Environment Environment;
typedef struct Startup Startup;
typedef struct Reaction Reaction;
typedef struct Trigger Trigger;

struct Reactor {
  Environment *env;
  void (*assemble)(Reactor *self);
  void (*register_startup)(Reactor *self, Startup *startup);
  void (*calculate_levels)(Reactor *self);
  Reactor **children;
  size_t children_size;
  Reaction **reactions;
  size_t reactions_size;
  Trigger **triggers;
  size_t triggers_size;
  char name[REACTOR_NAME_MAX_LEN];
};

void Reactor_ctor(Reactor *self, const char *name, Environment *env, Reactor **children, size_t children_size,
                  Reaction **reactions, size_t reactions_size, Trigger **triggers, size_t triggers_size);
#endif
