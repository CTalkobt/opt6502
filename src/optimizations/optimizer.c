/**
 * @file optimizer.c
 * @brief Main optimization coordinator implementation
 *
 * Implements the main optimization loop that coordinates all optimization
 * passes. Runs passes iteratively until convergence (no more optimizations)
 * or a maximum iteration count is reached.
 */

#include "optimizer.h"
#include "../analysis/analysis.h"
#include "../analysis/registers.h"
#include <stdio.h>

/**
 * @brief Main optimization routine
 *
 * Coordinates all optimization passes in a multi-pass loop:
 * 1. Performs subroutine inlining once (before main loop)
 * 2. Runs optimization passes iteratively until convergence:
 *    - Analysis passes (call flow, branch targets)
 *    - Basic optimizations (peephole, load/store, registers)
 *    - Constant propagation
 *    - CPU-specific optimizations (65C02, 45GS02)
 *    - Control flow (jump optimization)
 *    - Dead code elimination (must be last)
 * 3. Validates register tracking (if trace_level >= 2)
 *
 * Terminates when:
 * - No optimizations found in a pass (convergence)
 * - Maximum of 10 passes reached
 *
 * @param prog Program to optimize (modified in place)
 */
void optimize_program_ast(Program *prog) {
    int prev_opts;
    int pass = 0;

    // First perform inlining (only once, at the beginning)
    printf("Performing subroutine inlining...\n");
    analyze_call_flow_ast(prog);
    optimize_inline_subroutines_ast(prog);

    // Multiple passes until no more optimizations found
    do {
        prev_opts = prog->optimizations;

        analyze_call_flow_ast(prog);

        // Basic optimizations
        optimize_peephole_ast(prog);
        optimize_load_store_ast(prog);
        optimize_register_usage_ast(prog);
        optimize_constant_propagation_ast(prog);

        // Arithmetic and logic
        optimize_65c02_instructions_ast(prog);
        optimize_45gs02_instructions_ast(prog);

        // Control flow
        optimize_jumps_ast(prog);

        // Must be last
        optimize_dead_code_ast(prog);

        pass++;
    } while (prog->optimizations > prev_opts && pass < 10);

    printf("Optimization completed in %d passes\n", pass);

    // Validate register and flag tracking
    validate_register_and_flag_tracking(prog);
}
