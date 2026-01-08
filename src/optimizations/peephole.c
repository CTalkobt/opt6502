/**
 * @file peephole.c
 * @brief Peephole optimization patterns
 *
 * Implements small-window pattern matching optimizations that detect
 * and eliminate redundant instruction sequences.
 */

#include "optimizer.h"
#include <string.h>

/**
 * @brief Peephole optimization - redundant load elimination
 *
 * Detects and removes redundant loads in the pattern:
 *   LDA #value
 *   STA address
 *   LDA #value    <- redundant, remove this
 *
 * The accumulator still contains the value after STA, so the
 * second LDA is unnecessary.
 *
 * @param prog Program to optimize
 */
void optimize_peephole_ast(Program *prog) {
    AstNode *node = prog->root;
    while (node) {
        if (node->is_dead || node->no_optimize) {
            node = node->next;
            continue;
        }

        // LDA #value followed by STA then LDA #same_value
        if (node->opcode && strcmp(node->opcode, "LDA") == 0 && node->next) {
            AstNode *next1 = node->next;
            if (next1->opcode && strcmp(next1->opcode, "STA") == 0 && next1->next) {
                AstNode *next2 = next1->next;
                if (next2->opcode && strcmp(next2->opcode, "LDA") == 0) {
                    if (node->operand && next2->operand && strcmp(node->operand, next2->operand) == 0) {
                        next2->is_dead = true;
                        prog->optimizations++;
                    }
                }
            }
        }

        node = node->next;
    }
}
