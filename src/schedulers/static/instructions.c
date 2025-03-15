
/**
 * A static scheduler for the threaded runtime of the C target of Lingua Franca.
 *
 * @author{Shaokai Lin <shaokai@berkeley.edu>}
 */

#define REACTOR_LOCAL_TIME

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "reactor-uc/logging.h"
#include "reactor-uc/schedulers/static/instructions.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/schedulers/static/scheduler.h"

#ifndef TRACE_ALL_INSTRUCTIONS
#define TRACE_ALL_INSTRUCTIONS false
#endif
#define SPIN_WAIT_THRESHOLD SEC(1)

#define PRETVM_ERR(fmt, ...)                                                                                   \
  LF_ERR(SCHED, "PretVM: [%d] " fmt, ((StaticScheduler*)_lf_environment->scheduler)->state.pc, ##__VA_ARGS__)
#define PRETVM_WARN(fmt, ...)                                                                                   \
  LF_WARN(SCHED, "PretVM: [%d] " fmt, ((StaticScheduler*)_lf_environment->scheduler)->state.pc, ##__VA_ARGS__)
#define PRETVM_INFO(fmt, ...)                                                                                   \
  LF_INFO(SCHED, "PretVM: [%d] " fmt, ((StaticScheduler*)_lf_environment->scheduler)->state.pc, ##__VA_ARGS__)
#define PRETVM_DEBUG(fmt, ...)                                                                                   \
  LF_DEBUG(SCHED, "PretVM: [%d] " fmt, ((StaticScheduler*)_lf_environment->scheduler)->state.pc, ##__VA_ARGS__)

extern Environment *_lf_environment; // NOLINT

/**
 * @brief The implementation of the ADD instruction
 */
void execute_inst_ADD(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, // NOLINT
                      operand_t op3, bool debug, size_t *program_counter, Reaction **returned_reaction,
                      bool *exit_loop) {
  (void)worker_number;
  (void)debug;
  (void)returned_reaction;
  (void)exit_loop;
  (void)platform;
  PRETVM_DEBUG("Scheduler executing ADD");
  reg_t *dst = op1.reg;
  reg_t *src = op2.reg;
  reg_t *src2 = op3.reg;
  *dst = *src + *src2;
  *program_counter += 1; // Increment pc.
}

/**
 * @brief The implementation of the ADDI instruction
 */
void execute_inst_ADDI(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3,
                       bool debug, size_t *program_counter, Reaction **returned_reaction, bool *exit_loop) {
  (void)worker_number;
  (void)debug;
  (void)returned_reaction;
  (void)exit_loop;
  (void)platform;
  PRETVM_DEBUG("Scheduler executing ADDI");
  reg_t *dst = op1.reg;
  reg_t *src = op2.reg;
  // FIXME: Will there be problems if instant_t adds reg_t?
  *dst = *src + op3.imm;
  *program_counter += 1; // Increment pc.
}

/**
 * @brief The implementation of the BEQ instruction
 */
void execute_inst_BEQ(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                      size_t *program_counter, Reaction **returned_reaction, bool *exit_loop) {
  (void)worker_number;
  (void)debug;
  (void)returned_reaction;
  (void)exit_loop;
  (void)platform;
  PRETVM_DEBUG("Scheduler executing BEQ");
  reg_t *_op1 = op1.reg;
  reg_t *_op2 = op2.reg;
  // These NULL checks allow _op1 and _op2 to be uninitialized in the static
  // schedule, which can save a few lines in the schedule. But it is debatable
  // whether this is good practice.
  PRETVM_DEBUG("_op1 = %p", _op1);
  PRETVM_DEBUG("_op2 = %p", _op2);
  if (_op1 != NULL) PRETVM_DEBUG("*_op1 = %lld", *_op1);
  if (_op2 != NULL) PRETVM_DEBUG("*_op2 = %lld", *_op2);
  if (_op1 != NULL && _op2 != NULL && *_op1 == *_op2)
    *program_counter = op3.imm;
  else
    *program_counter += 1;
}

/**
 * @brief The implementation of the BGE instruction
 */
void execute_inst_BGE(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                      size_t *program_counter, Reaction **returned_reaction, bool *exit_loop) {
  (void)worker_number;
  (void)debug;
  (void)returned_reaction;
  (void)exit_loop;
  (void)platform;
  PRETVM_DEBUG("Scheduler executing BGE");
  reg_t *_op1 = op1.reg;
  reg_t *_op2 = op2.reg;
  if (_op1 != NULL && _op2 != NULL && *_op1 >= *_op2)
    *program_counter = op3.imm;
  else
    *program_counter += 1;
}

/**
 * @brief The implementation of the BLT instruction
 */
void execute_inst_BLT(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                      size_t *program_counter, Reaction **returned_reaction, bool *exit_loop) {
  (void)worker_number;
  (void)debug;
  (void)returned_reaction;
  (void)exit_loop;
  (void)platform;
  PRETVM_DEBUG("Scheduler executing BLT");
  reg_t *_op1 = op1.reg;
  reg_t *_op2 = op2.reg;
  if (_op1 != NULL && _op2 != NULL && *_op1 < *_op2)
    *program_counter = op3.imm;
  else
    *program_counter += 1;
}

/**
 * @brief The implementation of the BNE instruction
 */
void execute_inst_BNE(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                      size_t *program_counter, Reaction **returned_reaction, bool *exit_loop) {
  (void)worker_number;
  (void)debug;
  (void)returned_reaction;
  (void)exit_loop;
  (void)platform;
  PRETVM_DEBUG("Scheduler executing BNE");
  reg_t *_op1 = op1.reg;
  reg_t *_op2 = op2.reg;
  if (_op1 != NULL && _op2 != NULL && *_op1 != *_op2)
    *program_counter = op3.imm;
  else
    *program_counter += 1;
}

/**
 * @brief The implementation of the DU instruction
 */
void execute_inst_DU(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                     size_t *program_counter, Reaction **returned_reaction, bool *exit_loop) {
  (void)worker_number;
  (void)op3;
  (void)debug;
  (void)returned_reaction;
  (void)exit_loop;
  (void)platform;
  PRETVM_DEBUG("Scheduler executing DU");
  // FIXME: There seems to be an overflow problem.
  // When wakeup_time overflows but lf_time_physical() doesn't,
  // _lf_interruptable_sleep_until_locked() terminates immediately.
  reg_t *src = op1.reg;
  instant_t current_time = platform->get_physical_time(platform);
  instant_t wakeup_time = *src + op2.imm;
  instant_t wait_interval = wakeup_time - current_time;
  PRETVM_DEBUG("wakeup_time = %lld, current_time = %lld, wait_interval = %lld", wakeup_time, current_time, wait_interval);

  if (wait_interval > 0) {
    // Approach 1: Only spin when the wait interval is less than
    // SPIN_WAIT_THRESHOLD.
    /*
    if (wait_interval < SPIN_WAIT_THRESHOLD) {
      // Spin wait if the wait interval is less than 1 ms.
      while (platform->get_physical_time(platform) < wakeup_time)
        ;
    } else {
      // Otherwise sleep.
      // TODO: _lf_interruptable_sleep_until_locked(scheduler->env,
      // wakeup_time);
    }
    */
    // Approach 2: Spin wait.
    while (_lf_environment->get_physical_time(_lf_environment) < wakeup_time);
  }
  *program_counter += 1; // Increment pc.
}

/**
 * @brief The implementation of the EXE instruction
 */
void execute_inst_EXE(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                      size_t *program_counter, Reaction **returned_reaction, bool *exit_loop) {
  (void)worker_number;
  (void)op3;
  (void)debug;
  (void)returned_reaction;
  (void)exit_loop;
  (void)platform;
  void (*function)(void *);
  PRETVM_DEBUG("Scheduler executing EXE");
  function = (void (*)(void *))(uintptr_t)op1.reg;

  void *args = (void *)op2.reg;
  // Execute the function directly.
  function(args);
  *program_counter += 1; // Increment pc.
}

/**
 * @brief The implementation of the WLT instruction
 */
void execute_inst_WLT(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                      size_t *program_counter, Reaction **returned_reaction, bool *exit_loop) {
  (void)worker_number;
  (void)op3;
  (void)debug;
  (void)returned_reaction;
  (void)exit_loop;
  (void)platform;
  PRETVM_DEBUG("Scheduler executing WLT");
  reg_t *var = op1.reg;
  while (*var >= op2.imm)
    ;
  *program_counter += 1; // Increment pc.
}

/**
 * @brief The implementation of the WU instruction
 */
void execute_inst_WU(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                     size_t *program_counter, Reaction **returned_reaction, bool *exit_loop) {
  (void)worker_number;
  (void)op3;
  (void)debug;
  (void)returned_reaction;
  (void)exit_loop;
  (void)platform;
  PRETVM_DEBUG("Scheduler executing WU");
  reg_t *var = op1.reg;
  while (*var < op2.imm)
    ;
  *program_counter += 1; // Increment pc.
}

/**
 * @brief The implementation of the JAL instruction
 */
void execute_inst_JAL(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                      size_t *program_counter, Reaction **returned_reaction, bool *exit_loop) {
  (void)worker_number;
  (void)debug;
  (void)returned_reaction;
  (void)exit_loop;
  (void)platform;
  PRETVM_DEBUG("Scheduler executing JAL");
  // Use the destination register as the return address and, if the
  // destination register is not the zero register, store program_counter+1 in it.
  reg_t *destReg = op1.reg;
  if (destReg != &((StaticScheduler*)_lf_environment->scheduler)->state.zero) {
    *destReg = *program_counter + 1;
  }
  *program_counter = op2.imm + op3.imm; // New pc = label + offset
}

/**
 * @brief The implementation of the JALR instruction
 */
void execute_inst_JALR(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3,
                       bool debug, size_t *program_counter, Reaction **returned_reaction, bool *exit_loop) {
  (void)worker_number;
  (void)debug;
  (void)returned_reaction;
  (void)exit_loop;
  (void)platform;
  PRETVM_DEBUG("Scheduler executing JALR");
  // Use the destination register as the return address and, if the
  // destination register is not the zero register, store program_counter+1 in it.
  reg_t *destReg = op1.reg;
  if (destReg != &((StaticScheduler*)_lf_environment->scheduler)->state.zero)
    *destReg = *program_counter + 1;
  // Set program_counter to base addr + immediate.
  reg_t *baseAddr = op2.reg;
  *program_counter = *baseAddr + op3.imm;
}

/**
 * @brief The implementation of the STP instruction
 */
void execute_inst_STP(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                      size_t *program_counter, Reaction **returned_reaction, bool *exit_loop) {
  (void)worker_number;
  (void)debug;
  (void)returned_reaction;
  (void)exit_loop;
  (void)program_counter;
  (void)op1;
  (void)op2;
  (void)op3;
  (void)platform;
  PRETVM_DEBUG("Scheduler executing STP");
  *exit_loop = true;
}
