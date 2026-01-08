/**
 * @file regusage.c
 * @brief Register usage optimization
 *
 * Optimizes register transfer patterns to eliminate useless
 * transfer sequences.
 */

#include "optimizer.h"
#include <string.h>

/**
 * @brief Register usage optimization - remove useless transfers
 *
 * Detects and removes useless register transfer pairs:
 *   TAX    <- transfer A to X
 *   TXA    <- transfer X back to A
 *
 * If there are no instructions between these transfers that use X,
 * both instructions are useless and can be removed (A ends up
 * unchanged).
 *
 * Note: Currently this is a simple pattern match without checking
 * for intervening X usage. A more sophisticated version would scan
 * for X-modifying instructions between the transfers.
 *
 * @param prog Program to optimize
 */
void optimize_register_usage_ast(Program *prog) {
    AstNode *node = prog->root;
    while (node) {
        if (node->is_dead || node->no_optimize) {
            node = node->next;
            continue;
        }

        // TAX followed by TXA (no operation if no X usage between)
        if (node->opcode && strcmp(node->opcode, "TAX") == 0 && node->next) {
            AstNode *next = node->next;
            if (next->opcode && strcmp(next->opcode, "TXA") == 0) {
                node->is_dead = true;
                next->is_dead = true;
                prog->optimizations++;
            }
        }

        node = node->next;
    }
}
