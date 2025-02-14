//
// Created by tanneberger on 11/16/24.
//

#include "reactor-uc/scheduler.h"
#include "reactor-uc/schedulers/static/instructions.h"
#include "reactor-uc/schedulers/static/scheduler.h"

StaticScheduler scheduler;
extern const inst_t **static_schedule;

void *interpret(StaticScheduler *scheduler, int worker_number) {

  const inst_t *current_schedule = scheduler->static_schedule;
  Reaction *returned_reaction = NULL;
  bool exit_loop = false;
  size_t *pc = &scheduler->state.pc;

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

  return returned_reaction;
}

void StaticScheduler_run(Scheduler *untyped_self) {
  StaticScheduler *self = (StaticScheduler *)untyped_self;
  (void)self;
  LF_INFO(SCHED, "Scheduler starting at " PRId64, self->state.start_time);
  interpret(self, 1);
}

void Scheduler_do_shutdown(Scheduler *untyped_self, tag_t shutdown_tag) {
  StaticScheduler *self = (StaticScheduler *)untyped_self;
  LF_INFO(SCHED, "Scheduler terminating at tag %" PRId64 ":%" PRIu32, shutdown_tag.time, shutdown_tag.microstep);
  Environment *env = self->env;
  (void)self;
  (void)env;
}

lf_ret_t Scheduler_schedule_at_locked(Scheduler *untyped_self, Event *event) {
  StaticScheduler *self = (StaticScheduler *)untyped_self;
  (void)self;
  (void)event;
  LF_INFO(SCHED, "schedule_at_locked called");
  return LF_OK;
}

lf_ret_t Scheduler_schedule_at(Scheduler *self, Event *event) {
  Environment *env = ((StaticScheduler *)self)->env;
  (void)env;
  (void)event;
  LF_INFO(SCHED, "schedule_at called");

  return LF_OK;
}

void StaticScheduler_acquire_and_schedule_start_tag(Scheduler *untyped_self) {
  StaticScheduler *self = (StaticScheduler *)untyped_self;
  (void)self;
  LF_INFO(SCHED, "acquire_and_schedule_start_tag called");
}

void Scheduler_register_for_cleanup(Scheduler *untyped_self, Trigger *trigger) {
  StaticScheduler *self = (StaticScheduler *)untyped_self;
  (void)self;
  LF_DEBUG(SCHED, "Registering trigger %p for cleanup", trigger);
}

void Scheduler_request_shutdown(Scheduler *untyped_self) {
  StaticScheduler *self = (StaticScheduler *)untyped_self;
  Environment *env = self->env;
  (void)self;
  (void)env;
  LF_INFO(SCHED, "Shutdown requested");
}

void StaticScheduler_ctor(StaticScheduler *self, Environment *env) {
  self->env = env;

  self->state.pc = 0LL;
  self->state.start_time = 0LL;
  self->state.timeout = FOREVER;
  self->state.num_counters = 1;
  self->state.time_offset = 0LL;
  self->state.offset_inc = 0LL;
  self->state.zero = 0LL;
  self->state.one = 1ULL;
  self->state.counter = 0LL;
  self->state.return_addr = 0LL;
  self->state.binary_sema = 0LL;
  self->state.temp0 = 0LL;
  self->state.temp1 = 0LL;

  self->super.run = StaticScheduler_run;
  self->super.do_shutdown = Scheduler_do_shutdown;
  self->super.schedule_at = Scheduler_schedule_at; // FIXME: Expect runtime exception.
  self->super.schedule_at_locked = Scheduler_schedule_at_locked;
  self->super.register_for_cleanup = Scheduler_register_for_cleanup;
  self->super.request_shutdown = Scheduler_request_shutdown;
  self->super.acquire_and_schedule_start_tag = StaticScheduler_acquire_and_schedule_start_tag;
}

Scheduler *Scheduler_new(Environment *env) {
  printf("scheduler pointer: %p", &scheduler);
  StaticScheduler_ctor(&scheduler, env);
  return (Scheduler *)&scheduler;
}
