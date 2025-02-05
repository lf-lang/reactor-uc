//
// Created by tanneberger on 11/16/24.
//

#include "reactor-uc/scheduler.h"
#include "reactor-uc/schedulers/static/instructions.h"
#include "reactor-uc/schedulers/static/scheduler.h"

static StaticScheduler scheduler;
extern const inst_t **static_schedule;

void *interpret(StaticScheduler *scheduler, int worker_number) {
  LF_PRINT_DEBUG("Worker %d inside lf_sched_get_ready_reaction", worker_number);

  const inst_t *current_schedule = scheduler->static_schedule[worker_number];
  Reaction *returned_reaction = NULL;
  bool exit_loop = false;
  size_t *pc = &scheduler->pc[worker_number];

  function_virtual_instruction_t func;
  operand_t op1;
  operand_t op2;
  operand_t op3;
  bool debug;

  while (!exit_loop) {
    func = current_schedule[*pc].func;
    op1 = current_schedule[*pc].op1;
    op2 = current_schedule[*pc].op2;
    op3 = current_schedule[*pc].op3;
    debug = current_schedule[*pc].debug;

    // Execute the current instruction
    func(scheduler->env->platform, worker_number, op1, op2, op3, debug, pc, &returned_reaction, &exit_loop);
  }

  LF_PRINT_DEBUG("Worker %d leaves lf_sched_get_ready_reaction", worker_number);
  return returned_reaction;
}

void StaticScheduler_run(Scheduler *untyped_self) {
  StaticScheduler *self = (StaticScheduler *)untyped_self;
  (void)self;
  // Environment *env = self->env;
  // lf_ret_t res;
  printf("Hello from the static scheduler\n");
}

void StaticScheduler_acquire_and_schedule_start_tag(Scheduler *untyped_self) {
  StaticScheduler *self = (StaticScheduler *)untyped_self;
  (void)self;
  // Environment *env = self->env;
  // lf_ret_t res;
}

void StaticScheduler_ctor(StaticScheduler *self, Environment *env, const inst_t **static_schedule) {
  self->env = env;
  self->static_schedule = static_schedule;

  self->super.run = StaticScheduler_run;
  self->super.do_shutdown = NULL;
  self->super.schedule_at = NULL; // FIXME: Expect runtime exception.
  self->super.schedule_at_locked = NULL;
  self->super.register_for_cleanup = NULL;
  self->super.request_shutdown = NULL;
  self->super.acquire_and_schedule_start_tag = StaticScheduler_acquire_and_schedule_start_tag;
}

Scheduler *Scheduler_new(Environment *env) {
  StaticScheduler_ctor(&scheduler, env, NULL); // FIXME: Supply scheduler pointer.
  return (Scheduler *)&scheduler;
}
