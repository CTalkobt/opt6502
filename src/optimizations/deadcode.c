/**
 * @file deadcode.c
 * @brief Dead code elimination
 *
 * Removes unreachable code that follows unconditional control flow
 * transfers (JMP, RTS, RTI).
 */

#include "optimizer.h"
#include <string.h>

/**
 * @brief Dead code elimination - remove unreachable instructions
 *
 * Identifies and marks as dead any instructions that follow
 * unconditional control flow transfers and are not branch targets:
 * - After JMP (unconditional jump)
 * - After RTS (return from subroutine)
 * - After RTI (return from interrupt)
 *
 * Stops marking dead code when encountering:
 * - A branch target label
 * - An explicit label
 * - An instruction with no_optimize flag
 *
 * @param prog Program to optimize
 */
void optimize_dead_code_ast(Program *prog) {
    AstNode *node = prog->root;
    while (node) {
        if (node->is_dead || node->no_optimize) {
            node = node->next;
            continue;
        }

        // Unconditional jump followed by unreachable code
        if ((node->opcode && (strcmp(node->opcode, "JMP") == 0 ||
                              strcmp(node->opcode, "RTS") == 0 ||
                              strcmp(node->opcode, "RTI") == 0)) &&
            node->next && !node->next->is_branch_target && !node->next->label) {

            AstNode *current = node->next;
            while (current && !current->is_branch_target &&
                   !current->label && !current->no_optimize &&
                   current->opcode) {
                current->is_dead = true;
                prog->optimizations++;
                current = current->next;
            }
        }

        node = node->next;
    }
}
