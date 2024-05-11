#ifndef REACTOR_UC_REACTOR_H
#define REACTOR_UC_REACTOR_H

typedef struct Startup Startup;
typedef struct Reactor Reactor;
typedef struct Environment Environment;
typedef struct Startup Startup;

struct Reactor {
  Environment *env;
  void (*assemble)(Reactor *self);
  void (*register_startup)(Reactor *self, Startup *startup);
};

void Reactor_ctor(Reactor *self, Environment *env);
#endif
