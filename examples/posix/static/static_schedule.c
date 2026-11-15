#include "reactor-uc/macros_internal.h"
#include "Connections.h"
#include "reactor-uc/schedulers/static/scheduler.h"
#include "reactor-uc/schedulers/static/instructions.h"


Reactor_Connections main_reactor;
Environment env;
StaticScheduler static_scheduler;
Environment *_lf_environment = &env;


void lf_exit(void) { Environment_free(&env); }
void lf_start() {
    Environment_ctor(&env, (Reactor *)&main_reactor, &static_scheduler.super, false);
    Reactor_Connections_ctor(&main_reactor, NULL, &env);

    env.assemble(&env);
    StaticSchedulerState* state = &static_scheduler.state;
    state->timeout = SEC(10);
    state->start_time = SEC(10);
    interval_t current_time;
    inst_t schedule_0[] = {
      {.func=execute_inst_ADDI, .opcode=ADDI, .op1.reg=(reg_t*)&(state->timeout), .op2.reg=(reg_t*)&(state->start_time), .op3.imm=0LL},
      {.func=execute_inst_EXE, .opcode=EXE, .op1.reg=(reg_t*)(main_reactor.r1->reaction0.super.body), .op2.reg=(reg_t*)&(main_reactor.r1->reaction0.super), .op3.imm=0LL},
      {.func=execute_inst_EXE, .opcode=EXE, .op1.reg=(reg_t*)(main_reactor.r1->reaction1.super.body), .op2.reg=(reg_t*)&(main_reactor.r1->reaction1.super), .op3.imm=0LL},
      {.func=execute_inst_ADDI, .opcode=ADDI, .op1.reg=(reg_t*)&current_time, .op2.reg=(reg_t*)&(state->zero), .op3.imm=(env.platform->get_physical_time(env.platform))},
      {.func=execute_inst_DU, .opcode=DU, .op1.reg=(reg_t*)&current_time, .op2.imm = SEC(5), .op3.imm=0LL},
      {.func=execute_inst_BEQ, .opcode = BEQ, .op1.reg = (reg_t*)main_reactor.r1->out->super.super.is_present, .op3.reg = (reg_t*)&state->zero, .op2.imm = 2},
      {.func=execute_inst_EXE, .opcode=EXE, .op1.reg=(reg_t*)(main_reactor.r1->reaction1.super.body), .op2.reg=(reg_t*)&(main_reactor.r1->reaction1.super), .op3.imm=0LL},
      {.func=execute_inst_STP, .opcode=STP},
    };

    printf("schedule: %p", schedule_0);

    ReactorTagPair reactor_tags[] = {
      {&main_reactor.super, {.time = 0, .microstep = 0}},
      {&main_reactor.r1->super, {.time = 0, .microstep = 0}},
      {&main_reactor.r2->super, {.time = 0, .microstep = 0}}
    };
    StaticScheduler_ctor(&static_scheduler, &env, schedule_0, 4, reactor_tags, 2);
    env.start(&env);
    lf_exit();
}
int main() {
    lf_start();
}
