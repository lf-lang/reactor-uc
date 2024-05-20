#ifndef REACTOR_UC_REACTOR_H
#define REACTOR_UC_REACTOR_H

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
  Reactor **children;
  size_t children_size;
  Reaction **reactions;
  size_t reactions_size;
  Trigger **triggers;
  size_t triggers_size;
};

void Reactor_ctor(Reactor *self, Environment *env, Reactor **children, size_t children_size, Reaction **reactions,
                  size_t reactions_size, Trigger **triggers, size_t triggers_size);
#endif
