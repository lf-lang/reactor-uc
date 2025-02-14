#ifndef SCHEDULER_STATIC_FUNCTION_H
#define SCHEDULER_STATIC_FUNCTION_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/platform.h"

typedef enum {
  ADD,
  ADDI,
  ADV,
  ADVI,
  BEQ,
  BGE,
  BLT,
  BNE,
  DU,
  EXE,
  JAL,
  JALR,
  STP,
  WLT,
  WU,
} opcode_t;

/**
 * @brief Convenient typedefs for the data types used by the C implementation of
 * PRET VM. A register is 64bits and an immediate is 64bits. This avoids any
 * issue with time and overflow. Arguably it is worth it even for smaller
 * platforms.
 *
 */
typedef volatile uint64_t reg_t;
typedef uint64_t imm_t;

/**
 * @brief An union representing a single operand for the PRET VM. A union
 * means that we have one piece of memory, which is big enough to fit either
 * one of the two members of the union.
 *
 */
typedef union {
  reg_t *reg;
  imm_t imm;
} operand_t;

/**
 * @brief Virtual instruction function pointer
 */
typedef void (*function_virtual_instruction_t)(Platform *platform, size_t worker_number, operand_t op1, operand_t op2,
                                               operand_t op3, bool debug, size_t *program_counter,
                                               Reaction **returned_reaction, bool *exit_loop);

/**
 * @brief This struct represents a PRET VM instruction for C platforms.
 * There is an opcode and three operands. The operands are unions so they
 * can be either a pointer or an immediate
 *
 */
typedef struct inst_t {
  function_virtual_instruction_t func;
  opcode_t opcode;
  operand_t op1;
  operand_t op2;
  operand_t op3;
  bool debug;
} inst_t;

/**
 * @brief Wrapper function for peeking a priority queue.
 */
void push_pop_peek_pqueue(void *self);

void execute_inst_ADD(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                      size_t *program_counter, Reaction **returned_reaction, bool *exit_loop);
void execute_inst_ADDI(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3,
                       bool debug, size_t *program_counter, Reaction **returned_reaction, bool *exit_loop);
void execute_inst_BEQ(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                      size_t *program_counter, Reaction **returned_reaction, bool *exit_loop);
void execute_inst_BGE(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                      size_t *program_counter, Reaction **returned_reaction, bool *exit_loop);
void execute_inst_BLT(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                      size_t *program_counter, Reaction **returned_reaction, bool *exit_loop);
void execute_inst_BNE(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                      size_t *program_counter, Reaction **returned_reaction, bool *exit_loop);
void execute_inst_DU(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                     size_t *program_counter, Reaction **returned_reaction, bool *exit_loop);
void execute_inst_EXE(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                      size_t *program_counter, Reaction **returned_reaction, bool *exit_loop);
void execute_inst_WLT(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                      size_t *program_counter, Reaction **returned_reaction, bool *exit_loop);
void execute_inst_WU(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                     size_t *program_counter, Reaction **returned_reaction, bool *exit_loop);
void execute_inst_JAL(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                      size_t *program_counter, Reaction **returned_reaction, bool *exit_loop);
void execute_inst_JALR(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3,
                       bool debug, size_t *program_counter, Reaction **returned_reaction, bool *exit_loop);
void execute_inst_STP(Platform *platform, size_t worker_number, operand_t op1, operand_t op2, operand_t op3, bool debug,
                      size_t *program_counter, Reaction **returned_reaction, bool *exit_loop);

#endif
