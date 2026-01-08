/**
 * @file optimizer.h
 * @brief Main optimization coordinator
 *
 * Coordinates multiple optimization passes over the program AST.
 * Runs passes iteratively until no more optimizations are found.
 */

#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "../types.h"

/**
 * @brief Main optimization routine - coordinates all optimization passes
 *
 * Runs multiple optimization passes over the program in an iterative manner
 * until no more optimizations are found (or a maximum of 10 passes).
 *
 * Pass order:
 * 1. Subroutine inlining (once, before iteration)
 * 2. Iterative optimization loop:
 *    - Call flow analysis
 *    - Peephole optimization
 *    - Load/store optimization
 *    - Register usage optimization
 *    - Constant propagation
 *    - CPU-specific optimizations (65C02, 45GS02)
 *    - Jump optimization
 *    - Dead code elimination (must be last)
 *
 * After optimization, validates register tracking if trace level >= 2.
 *
 * @param prog Program to optimize
 */
void optimize_program_ast(Program *prog);

/* Individual optimization passes */

/**
 * @brief Peephole optimization patterns
 * Detects and optimizes small instruction sequences (2-3 instructions)
 * @param prog Program to optimize
 */
void optimize_peephole_ast(Program *prog);

/**
 * @brief Dead code elimination
 * Removes unreachable code after unconditional jumps/returns
 * @param prog Program to optimize
 */
void optimize_dead_code_ast(Program *prog);

/**
 * @brief Jump optimization
 * Removes redundant jumps (e.g., JMP to next instruction)
 * @param prog Program to optimize
 */
void optimize_jumps_ast(Program *prog);

/**
 * @brief Load/store optimization
 * Eliminates redundant loads of the same value
 * @param prog Program to optimize
 */
void optimize_load_store_ast(Program *prog);

/**
 * @brief Register usage optimization
 * Removes useless register transfer sequences (TAX/TXA pairs)
 * @param prog Program to optimize
 */
void optimize_register_usage_ast(Program *prog);

/**
 * @brief Constant propagation
 * Tracks and eliminates redundant immediate loads
 * @param prog Program to optimize
 */
void optimize_constant_propagation_ast(Program *prog);

/**
 * @brief 65C02-specific optimizations
 * Converts LDA #$00 / STA sequences to STZ (65C02 instruction)
 * @param prog Program to optimize
 */
void optimize_65c02_instructions_ast(Program *prog);

/**
 * @brief 45GS02-specific optimizations (MEGA65)
 * Uses Z register for repeated stores, converts to NEG/ASR instructions
 * @param prog Program to optimize
 */
void optimize_45gs02_instructions_ast(Program *prog);

/**
 * @brief Inline small subroutines
 * Placeholder for future subroutine inlining optimization
 * @param prog Program to optimize
 */
void optimize_inline_subroutines_ast(Program *prog);

#endif // OPTIMIZER_H
