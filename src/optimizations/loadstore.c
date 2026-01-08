/**
 * @file loadstore.c
 * @brief Load/store optimization
 *
 * Eliminates redundant load instructions when the register already
 * contains the desired value.
 */

#include "optimizer.h"
#include <string.h>

/**
 * @brief Load/store optimization - eliminate redundant loads
 *
 * Detects and removes redundant loads in the pattern:
 *   LDA address1
 *   STA address2
 *   LDA address1  <- redundant, accumulator still has this value
 *
 * The accumulator is not modified by STA, so reloading the same
 * value is unnecessary.
 *
 * @param prog Program to optimize
 */
void optimize_load_store_ast(Program *prog) {
    AstNode *node = prog->root;
    while (node) {
        if (node->is_dead || node->no_optimize) {
            node = node->next;
            continue;
        }

        // LDA addr, STA addr2, LDA addr (remove third LDA)
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
