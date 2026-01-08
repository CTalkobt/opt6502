/**
 * @file jumps.c
 * @brief Jump optimization
 *
 * Optimizes control flow by removing unnecessary jumps.
 */

#include "optimizer.h"
#include <string.h>

/**
 * @brief Jump optimization - remove jumps to next instruction
 *
 * Detects and removes unnecessary JMP instructions that jump to
 * the immediately following instruction (a branch target).
 *
 * Pattern:
 *   JMP label
 * label:        <- next instruction is the target
 *
 * The JMP can be removed as execution will naturally fall through.
 *
 * @param prog Program to optimize
 */
void optimize_jumps_ast(Program *prog) {
    AstNode *node = prog->root;
    while (node) {
        if (node->is_dead || node->no_optimize) {
            node = node->next;
            continue;
        }

        // JMP to next line (remove)
        if (node->opcode && strcmp(node->opcode, "JMP") == 0 && node->next) {
            if (node->next->is_branch_target) {
                node->is_dead = true;
                prog->optimizations++;
            }
        }

        node = node->next;
    }
}
