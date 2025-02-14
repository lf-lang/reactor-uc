#include "reactor-uc/macros.h"
#include "Connections.h"
#include "reactor-uc/schedulers/static/scheduler.h"
#include "reactor-uc/schedulers/static/instructions.h"


Reactor_Connections main_reactor;
Environment env;
Environment *_lf_environment = &env;
void lf_exit(void) { Environment_free(&env); }
void lf_start() {
    Environment_ctor(&env, (Reactor *)&main_reactor);
    Reactor_Connections_ctor(&main_reactor, NULL, &env);
    env.scheduler->duration = NEVER;
    env.scheduler->keep_alive = false;
    env.fast_mode = false;
    env.assemble(&env);
}
void lf_execute() {
    env.start(&env);
    lf_exit();
}

int main() {
    lf_start();
    StaticSchedulerState* state = &((StaticScheduler*)_lf_environment->scheduler)->state;
    inst_t schedule_0[] = {
        {.func=execute_inst_ADDI, .opcode=ADDI, .op1.reg=(reg_t*)&(state->timeout), .op2.reg=(reg_t*)&(state->start_time), .op3.imm=0LL},
        {.func=execute_inst_STP, .opcode=STP},
    };
    ((StaticScheduler*)_lf_environment->scheduler)->static_schedule = schedule_0;
    lf_execute();
}