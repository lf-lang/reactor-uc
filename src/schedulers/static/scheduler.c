//
// Created by tanneberger on 11/16/24.
//

#include "reactor-uc/scheduler.h"
#include "reactor-uc/schedulers/static/instructions.h"
#include "reactor-uc/schedulers/static/scheduler.h"

#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"

static const char *opcode_to_string(opcode_t opcode) {
  switch (opcode) {
  case ADD:  return "ADD";
  case ADDI: return "ADDI";
  case ADV:  return "ADV";
  case ADVI: return "ADVI";
  case BEQ:  return "BEQ";
  case BGE:  return "BGE";
  case BLT:  return "BLT";
  case BNE:  return "BNE";
  case DU:   return "DU";
  case EXE:  return "EXE";
  case JAL:  return "JAL";
  case JALR: return "JALR";
  case STP:  return "STP";
  case WLT:  return "WLT";
  case WU:   return "WU";
  default:   return "UNKNOWN";
  }
}

StaticScheduler scheduler;
extern const inst_t **static_schedule;

void *interpret(StaticScheduler *scheduler, int worker_number) {

  const inst_t *current_schedule = scheduler->static_schedule;
  printf("schedule: %p", current_schedule);
  Reaction *returned_reaction = NULL;
  bool exit_loop = false;
  size_t *pc = &scheduler->state.pc;

  function_virtual_instruction_t func;
  operand_t op1;
  operand_t op2;
  operand_t op3;
  bool debug;

  printf("curren pc: %ld", *pc);

  while (!exit_loop) {
    const inst_t *inst = &current_schedule[*pc];
    func = inst->func;
    op1 = inst->op1;
    op2 = inst->op2;
    op3 = inst->op3;
    debug = inst->debug;

    LF_DEBUG(SCHED, "[PC=%zu] %s: op1=0x%llx op2=0x%llx op3=0x%llx debug=%d func=%p",
             *pc,
             opcode_to_string(inst->opcode),
             (unsigned long long)op1.imm,
             (unsigned long long)op2.imm,
             (unsigned long long)op3.imm,
             debug,
             (void *)func);

    if (func == NULL) {
      LF_ERR(SCHED, "[PC=%zu] NULL function pointer for instruction %s! Aborting.",
             *pc, opcode_to_string(inst->opcode));
      break;
    }

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

lf_ret_t Scheduler_schedule_at(Scheduler *untyped_self, Event *event) {
  Environment *env = ((StaticScheduler *)untyped_self)->env;
  StaticScheduler *self = (StaticScheduler *)untyped_self;
  (void)env;
  (void)event;
  LF_INFO(SCHED, "schedule_at called");
  for (size_t i = 0; i < self->trigger_buffers_size; i++) {
    if (self->trigger_buffers[i].trigger == event->trigger) {
      cb_push_back(&(self->trigger_buffers[i].buffer), event);
      LF_DEBUG(SCHED, "Insert event into buffer %d (%p) with payload %p @ %lld. Buffered events: %d. TriggerBuffer* %p", i, &(self->trigger_buffers[i].buffer), event->super.payload, event->super.tag.time, self->trigger_buffers[i].buffer.count, &(self->trigger_buffers[i]));
    }
  }
  return LF_OK;
}

void StaticScheduler_acquire_and_schedule_start_tag(Scheduler *untyped_self, instant_t start_time) {
  StaticScheduler *self = (StaticScheduler *)untyped_self;
  (void)self;
  (void)start_time;
  LF_INFO(SCHED, "acquire_and_schedule_start_tag called");
}

void Scheduler_register_for_cleanup(Scheduler *untyped_self, Trigger *trigger) {
  StaticScheduler *self = (StaticScheduler *)untyped_self;
  (void)self;
  trigger->is_registered_for_cleanup = true;
  LF_INFO(SCHED, "Registering trigger %p for cleanup", trigger);
}

void Scheduler_request_shutdown(Scheduler *untyped_self) {
  StaticScheduler *self = (StaticScheduler *)untyped_self;
  Environment *env = self->env;
  (void)self;
  (void)env;
  LF_INFO(SCHED, "Shutdown requested");
}

tag_t Scheduler_current_tag(Scheduler *untyped_self) {
  StaticScheduler *self = (StaticScheduler *)untyped_self;
  LF_INFO(SCHED, "current_tag requested %li", self->state.pc);
  Reaction *reaction_executing = *((Reaction**)&(self->static_schedule[self->state.pc].op2));
  Reactor* reactor_executing = reaction_executing->parent;
  LF_INFO(SCHED, "current reactor: %s", reactor_executing->name);
  for (size_t i = 0; i < self->reactor_tags_size; i++) {
    if (self->reactor_tags[i].reactor == reactor_executing)
      return self->reactor_tags[i].tag;
  }
  throw("No reactor tag found.");
}

lf_ret_t Scheduler_add_to_reaction_queue(Scheduler *untyped_self, Reaction *reaction) {
  StaticScheduler *self = (StaticScheduler *)untyped_self;
  (void)self;
  (void)reaction;
  LF_INFO(SCHED, "add_to_reaction_queue requested");
  return LF_OK;
}

void StaticScheduler_prepare_timestep(Scheduler* self, tag_t tag) {
  (void)self;
  (void)tag;
}


void StaticScheduler_ctor(StaticScheduler *self, Environment *env, const inst_t* schedule, size_t schedule_size, ReactorTagPair* reactor_tags, size_t reactor_tags_size) {
  self->env = env;

  function_virtual_instruction_t func;
  operand_t op1;
  operand_t op2;
  operand_t op3;
  bool debug;
  const inst_t *inst = &schedule[0];
  func = inst->func;
  op1 = inst->op1;
  op2 = inst->op2;
  op3 = inst->op3;
  debug = inst->debug;
  LF_DEBUG(SCHED, "[PC=%zu] %s: op1=0x%llx op2=0x%llx op3=0x%llx debug=%d func=%p",
           0,
           opcode_to_string(inst->opcode),
           (unsigned long long)op1.imm,
           (unsigned long long)op2.imm,
           (unsigned long long)op3.imm,
           debug,
           (void *)func);

  self->reactor_tags = reactor_tags;
  self->reactor_tags_size = reactor_tags_size;
  self->static_schedule = schedule;
  self->static_schedule_size = schedule_size;
  self->state.pc = 0LL;
  self->state.start_time = 0LL;
  self->state.timeout = FOREVER;
  self->state.num_progress_index = 1;
  self->state.time_offset = 0LL;
  self->state.offset_inc = 0LL;
  self->state.zero = 0ULL;
  self->state.one = 1ULL;
  self->state.progress_index = 0LL;
  self->state.return_addr = 0LL;
  self->state.binary_sema = 0LL;
  self->state.temp0 = 0LL;
  self->state.temp1 = 0LL;

  self->super.run = StaticScheduler_run;
  self->super.prepare_timestep = StaticScheduler_prepare_timestep;
  self->super.do_shutdown = Scheduler_do_shutdown;
  self->super.schedule_at = Scheduler_schedule_at; // FIXME: Expect runtime exception.
  self->super.schedule_at = Scheduler_schedule_at_locked;
  self->super.register_for_cleanup = Scheduler_register_for_cleanup;
  self->super.request_shutdown = Scheduler_request_shutdown;
  self->super.set_and_schedule_start_tag = StaticScheduler_acquire_and_schedule_start_tag;
  self->super.current_tag = Scheduler_current_tag;
  self->super.add_to_reaction_queue = Scheduler_add_to_reaction_queue;
}

/*
Scheduler* Scheduler_new(Environment* env, interval_t duration, bool keep_alive) {
  (void)duration;
  (void)keep_alive;
  printf("scheduler pointer: %p", &scheduler);
  StaticScheduler_ctor(&scheduler, env);
  return (Scheduler *)&scheduler;
}
*/
