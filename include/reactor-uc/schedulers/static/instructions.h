#ifndef SCHEDULER_STATIC_FUNCTION_H
#define SCHEDULER_STATIC_FUNCTION_H

#include "scheduler_instructions.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/platform.h"

/**
 * @brief Function type with a void* argument. To make this type represent a
 * generic function, one can write a wrapper function around the target function
 * and use the first argument as a pointer to a struct of input arguments
 * and return values.
 */

#ifndef LF_PRINT_DEBUG
#define LF_PRINT_DEBUG(A, ...) {};
#endif

/**
 * @brief Wrapper function for peeking a priority queue.
 */
void push_pop_peek_pqueue(void *self);

void execute_inst_ADD(Platform* platform, size_t worker_number, operand_t op1, operand_t op2,
                      operand_t op3, bool debug, size_t *pc,
                      Reaction **returned_reaction, bool *exit_loop);
void execute_inst_ADDI(Platform* platform, size_t worker_number, operand_t op1, operand_t op2,
                       operand_t op3, bool debug, size_t *pc,
                       Reaction **returned_reaction, bool *exit_loop);
void execute_inst_BEQ(Platform* platform, size_t worker_number, operand_t op1, operand_t op2,
                      operand_t op3, bool debug, size_t *pc,
                      Reaction **returned_reaction, bool *exit_loop);
void execute_inst_BGE(Platform* platform, size_t worker_number, operand_t op1, operand_t op2,
                      operand_t op3, bool debug, size_t *pc,
                      Reaction **returned_reaction, bool *exit_loop);
void execute_inst_BLT(Platform* platform, size_t worker_number, operand_t op1, operand_t op2,
                      operand_t op3, bool debug, size_t *pc,
                      Reaction **returned_reaction, bool *exit_loop);
void execute_inst_BNE(Platform* platform, size_t worker_number, operand_t op1, operand_t op2,
                      operand_t op3, bool debug, size_t *pc,
                      Reaction **returned_reaction, bool *exit_loop);
void execute_inst_DU(Platform* platform, size_t worker_number, operand_t op1, operand_t op2,
                     operand_t op3, bool debug, size_t *pc,
                     Reaction **returned_reaction, bool *exit_loop);
void execute_inst_EXE(Platform* platform, size_t worker_number, operand_t op1, operand_t op2,
                      operand_t op3, bool debug, size_t *pc,
                      Reaction **returned_reaction, bool *exit_loop);
void execute_inst_WLT(Platform* platform, size_t worker_number, operand_t op1, operand_t op2,
                      operand_t op3, bool debug, size_t *pc,
                      Reaction **returned_reaction, bool *exit_loop);
void execute_inst_WU(Platform* platform, size_t worker_number, operand_t op1, operand_t op2,
                     operand_t op3, bool debug, size_t *pc,
                     Reaction **returned_reaction, bool *exit_loop);
void execute_inst_JAL(Platform* platform, size_t worker_number, operand_t op1, operand_t op2,
                      operand_t op3, bool debug, size_t *pc,
                      Reaction **returned_reaction, bool *exit_loop);
void execute_inst_JALR(Platform* platform, size_t worker_number, operand_t op1, operand_t op2,
                       operand_t op3, bool debug, size_t *pc,
                       Reaction **returned_reaction, bool *exit_loop);
void execute_inst_STP(Platform* platform, size_t worker_number, operand_t op1, operand_t op2,
                      operand_t op3, bool debug, size_t *pc,
                      Reaction **returned_reaction, bool *exit_loop);

#endif
