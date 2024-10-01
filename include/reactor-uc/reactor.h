#ifndef REACTOR_UC_REACTOR_H
#define REACTOR_UC_REACTOR_H

#define REACTOR_NAME_MAX_LEN 128
#include <stdlib.h>

typedef struct Startup Startup;
typedef struct Shutdown Shutdown;
typedef struct Reactor Reactor;
typedef struct Environment Environment;
typedef struct Startup Startup;
typedef struct Reaction Reaction;
typedef struct Trigger Trigger;

struct Reactor {
  Environment *env;
  void (*assemble)(Reactor *self);
  void (*register_startup)(Reactor *self, Startup *startup);
  void (*register_shutdown)(Reactor *self, Shutdown *shutdown);
  void (*calculate_levels)(Reactor *self);
  Reactor *parent;
  Reactor **children;
  size_t children_size;
  Reaction **reactions;
  size_t reactions_size;
  ReactorElement **triggers;
  size_t triggers_size;
  char name[REACTOR_NAME_MAX_LEN];
};

void Reactor_ctor(Reactor *self, const char *name, Environment *env, Reactor *parent, Reactor **children,
                  size_t children_size, Reaction **reactions, size_t reactions_size, Trigger **triggers,
                  size_t triggers_size);
#endif
