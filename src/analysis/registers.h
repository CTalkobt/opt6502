/**
 * @file registers.h
 * @brief CPU register and flag state tracking
 *
 * Provides functions for tracking register values and processor flags
 * throughout program execution. Used for constant propagation and
 * identifying redundant operations.
 */

#ifndef REGISTERS_H
#define REGISTERS_H

#include "../types.h"

/**
 * @brief Update register state based on an instruction
 *
 * Updates the register state structure based on the effects of a
 * single instruction. Tracks which registers are modified, their
 * known values (if immediate mode), and processor flag changes.
 *
 * Handles all 6502/65C02/45GS02 instructions including:
 * - Load/store operations (LDA, STA, etc.)
 * - Transfer operations (TAX, TXA, etc.)
 * - Arithmetic (ADC, SBC, INC, DEC)
 * - Logical operations (AND, ORA, EOR)
 * - Shifts and rotates (ASL, LSR, ROL, ROR)
 * - Comparisons (CMP, CPX, CPY)
 * - Flag manipulation (CLC, SEC, CLV)
 * - Stack operations (PHA, PLA, PHP, PLP)
 * - Control flow (branches, jumps, JSR, RTS, RTI)
 *
 * @param node AST node containing the instruction
 * @param state Register state to update (modified in place)
 */
void update_register_state(AstNode *node, RegisterState *state);

/**
 * @brief Print register state for debugging
 *
 * Outputs the current register state including register values,
 * modification flags, and processor flag states. Used for
 * debugging and verbose optimization tracing.
 *
 * @param state Register state to print
 * @param line_num Line number associated with this state
 */
void print_register_state(const RegisterState *state, int line_num);

/**
 * @brief Validate register and flag tracking throughout program
 *
 * Walks through the entire program and validates register/flag tracking.
 * Generates a summary report of:
 * - Total instructions analyzed
 * - Register modifications detected
 * - Flag modifications detected
 * - Register usage summary
 * - Flag usage summary
 *
 * Used for validation and debugging the optimizer. Outputs detailed
 * trace when trace_level >= 2.
 *
 * @param prog Program to validate
 */
void validate_register_and_flag_tracking(Program *prog);

#endif // REGISTERS_H
