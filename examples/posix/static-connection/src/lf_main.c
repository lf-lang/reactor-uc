#include "reactor-uc/macros.h"
#include "SimpleConnection/SimpleConnection.h"
#include "reactor-uc/schedulers/static/scheduler.h"
#include "reactor-uc/schedulers/static/instructions.h"
#include "reactor-uc/schedulers/static/circular_buffer.h"

#define WORKER_0_SYNC_BLOCK 0LL
#define WORKER_0_INIT 4LL
#define WORKER_0_CLEANUP_0 7LL
#define WORKER_0_END 14LL


Reactor_SimpleConnection main_reactor;
Environment env;
Environment *_lf_environment = &env;
void lf_exit(void) { Environment_free(&env); }
void lf_start() {
    Environment_ctor(&env, (Reactor *)&main_reactor);
    Reactor_SimpleConnection_ctor(&main_reactor, NULL, &env);
    env.scheduler->duration = NEVER;
    env.scheduler->keep_alive = false;
    env.fast_mode = false;
    env.assemble(&env);
}
void lf_execute() {
    env.start(&env);
    lf_exit();
}

void conn_0_pop_event_and_prepare_port(TriggerBufferPair *trigger_buffer) {
  CircularBuffer buffer = trigger_buffer->buffer;
  Event e;
  cb_pop_front(&buffer, &e);
  main_reactor.conn_source_out_0[0][0].super.super.super.prepare(&main_reactor.conn_source_out_0[0][0], &e);
}

int main() {
    lf_start();
    StaticSchedulerState* state = &((StaticScheduler*)_lf_environment->scheduler)->state;
    const size_t reactor_tags_size = 3;
    ReactorTagPair reactor_tags[reactor_tags_size] = {{&main_reactor, 0}, {&main_reactor.source, 0}, {&main_reactor.sink, 0}};
    size_t trigger_buffers_size = 1;
    TriggerBufferPair trigger_buffers[trigger_buffers_size];
    cb_init(&trigger_buffers[0].buffer, 10, sizeof(Event));
    trigger_buffers[0].trigger = &main_reactor.conn_source_out_0[0][0];
    inst_t schedule_0[] = {
      // WORKER_0_PREAMBLE:
      // Line 0: Increment OFFSET by adding START_TIME and 0LL
      {.func=execute_inst_ADDI, .opcode=ADDI, .op1.reg=(reg_t*)&(state->time_offset), .op2.reg=(reg_t*)&(state->start_time), .op3.imm=0LL},
      // Line 1: Increment TIMEOUT by adding START_TIME and 10000000000LL
      {.func=execute_inst_ADDI, .opcode=ADDI, .op1.reg=(reg_t*)&(state->timeout), .op2.reg=(reg_t*)&(state->start_time), .op3.imm=10000000000LL},
      // Line 2: Increment OFFSET_INC by adding ZERO and 0LL
      {.func=execute_inst_ADDI, .opcode=ADDI, .op1.reg=(reg_t*)&(state->offset_inc), .op2.reg=(reg_t*)&(state->zero), .op3.imm=0LL},
      // Line 3: JAL: store return address in ZERO and jump to INIT
      {.func=execute_inst_JAL, .opcode=JAL, .op1.reg=(reg_t*)&(state->zero), .op2.imm=WORKER_0_INIT, .op3.imm=0},
      // WORKER_0_EXECUTE_SimpleConnection_source_reaction_1_6570d54d:
      // WORKER_0_INIT:
      // Line 4: Execute function envs[simpleconnection_main].reaction_array[0]->function with argument envs[simpleconnection_main].reactor_self_array[1]
      // IMPORTANT NOTE: To let reaction body and DelayConnection_cleanup both use scheduler->current_tag, which needs to access the reactor's pointer, pass the reactor pointer from op3, which can be accessed from current_tag to identify the reactor.
      {.func=execute_inst_EXE, .opcode=EXE, .op1.reg=(reg_t*)(main_reactor.source->reaction0.super.body), .op2.reg=(reg_t*)&(main_reactor.source->reaction0.super), .op3.reg=(reg_t*)&(main_reactor.source)},
      // Line 5: Increment Worker 0's COUNTER by adding Worker 0's COUNTER and 1LL
      {.func=execute_inst_ADDI, .opcode=ADDI, .op1.reg=(reg_t*)&(state->counter), .op2.reg=(reg_t*)&(state->counter), .op3.imm=1LL},
      
      // Line 6: Check if the port is being set by the reaction. If so, invoke the cleanup function to push the payload into the buffer. Otherwise, skip the cleanup, which is not supposed to be called when the out port is not set.
      {.func=execute_inst_BEQ, .opcode=BEQ, .op1.reg=(reg_t*)&(main_reactor.source->out->super.super.is_present), .op2.reg=(reg_t*)&(state->zero), .op3.imm=WORKER_0_CLEANUP_0 + 1},

      // Line 7: Call DelayConnection_cleanup to get to the schedule_at in StaticScheduler.
      {.func=execute_inst_EXE, .opcode=EXE, .op1.reg=(reg_t*)(main_reactor.conn_source_out_0[0][0].super.super.super.cleanup), .op2.reg=(reg_t*)&(main_reactor.conn_source_out_0[0][0]), .op3.reg=(reg_t*)&(main_reactor.source)},

      // Line 8: Advance time for sink = reactor_tags[2].tag.time
      {.func=execute_inst_ADDI, .opcode=ADDI, .op1.reg=(reg_t*)&(reactor_tags[2].tag.time), .op2.reg=(reg_t*)&(reactor_tags[2].tag.time), .op3.imm=2000000000LL},

      // Line 9: Check if the current count of the connection buffer is 0.
      {.func=execute_inst_BEQ, .opcode=BEQ, .op1.reg=(reg_t*)&(trigger_buffers[0].buffer.count), .op2.reg=(reg_t*)&(state->zero), .op3.imm=WORKER_0_END},

      // Line 10: If not null, check if the time field of the current head matches the time of sink. If they don't, skip the reaction.
      {.func=execute_inst_BNE, .opcode=BEQ, .op1.reg=(reg_t*)&(((Event*)trigger_buffers[0].buffer.head)->tag.time), .op2.reg=(reg_t*)&(reactor_tags[2].tag.time), .op3.imm=WORKER_0_END},

      // Line 11: Pop the event from the connection buffer and prepare the downstream port.
      {.func=execute_inst_EXE, .opcode=EXE, .op1.reg=(reg_t*)conn_0_pop_event_and_prepare_port, .op2.reg=(reg_t*)&trigger_buffers[0]},

      // WORKER_0_EXECUTE_SimpleConnection_sink_reaction_1_7ed46f7f:
      // Line 12: Execute function envs[simpleconnection_main].reaction_array[2]->function with argument envs[simpleconnection_main].reactor_self_array[2]
      {.func=execute_inst_EXE, .opcode=EXE, .op1.reg=(reg_t*)(main_reactor.sink->reaction0.super.body), .op2.reg=(reg_t*)&((main_reactor.sink->reaction0.super)), .op3.reg=(reg_t*)&(main_reactor.sink)},
      
      // WORKER_0_EXECUTE_SimpleConnection_sink_reaction_2_f52ba644:
      // Line 13: Execute function envs[simpleconnection_main].reaction_array[3]->function with argument envs[simpleconnection_main].reactor_self_array[2]
      {.func=execute_inst_EXE, .opcode=EXE, .op1.reg=(reg_t*)(main_reactor.sink->reaction1.super.body), .op2.reg=(reg_t*)&((main_reactor.sink->reaction1.super)), .op3.reg=(reg_t*)&(main_reactor.sink)},
      
      // Line 14
      {.func=execute_inst_STP, .opcode=STP},
    };
    ((StaticScheduler*)_lf_environment->scheduler)->static_schedule = schedule_0;
    ((StaticScheduler*)_lf_environment->scheduler)->reactor_tags = reactor_tags;
    ((StaticScheduler*)_lf_environment->scheduler)->reactor_tags_size = reactor_tags_size;
    ((StaticScheduler*)_lf_environment->scheduler)->trigger_buffers = trigger_buffers;
    ((StaticScheduler*)_lf_environment->scheduler)->trigger_buffers_size = trigger_buffers_size;
    lf_execute();
}
