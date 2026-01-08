/**
 * @file constant.c
 * @brief Constant propagation optimization
 *
 * Tracks known register values through the program and eliminates
 * redundant immediate loads.
 */

#include "optimizer.h"
#include <string.h>
#include <stdbool.h>

/**
 * @brief Constant propagation - track and eliminate redundant loads
 *
 * Tracks the accumulator value through the program flow and removes
 * redundant immediate loads:
 *
 *   LDA #$42       <- A = $42
 *   STA address
 *   LDA #$42       <- redundant, A is already $42
 *
 * The optimization:
 * - Tracks LDA immediate values
 * - Removes subsequent LDA with the same value
 * - Invalidates tracking when A is modified by other instructions
 * - Resets tracking at branch targets (control flow convergence)
 *
 * @param prog Program to optimize
 */
void optimize_constant_propagation_ast(Program *prog) {
    char last_a_value[64] = "";
    bool a_known = false;

    AstNode *node = prog->root;
    while (node) {
        if (node->is_dead || node->is_branch_target || node->no_optimize) {
            a_known = false;
            node = node->next;
            continue;
        }

        // Track LDA immediate values
        if (node->opcode && strcmp(node->opcode, "LDA") == 0 &&
            node->operand && node->operand[0] == '#') {
            strncpy(last_a_value, node->operand, 63);
            a_known = true;
        }
        // If we see another LDA with same value, remove it
        else if (a_known && node->opcode && strcmp(node->opcode, "LDA") == 0 &&
                 node->operand && strcmp(node->operand, last_a_value) == 0) {
            node->is_dead = true;
            prog->optimizations++;
        }
        // Operations that modify A
        else if (node->opcode && (strcmp(node->opcode, "ADC") == 0 ||
                                  strcmp(node->opcode, "SBC") == 0 ||
                                  strcmp(node->opcode, "AND") == 0 ||
                                  strcmp(node->opcode, "ORA") == 0 ||
                                  strcmp(node->opcode, "EOR") == 0 ||
                                  strcmp(node->opcode, "LDA") == 0 ||
                                  strcmp(node->opcode, "PLA") == 0 ||
                                  strcmp(node->opcode, "TXA") == 0 ||
                                  strcmp(node->opcode, "TYA") == 0 ||
                                  strcmp(node->opcode, "ASL") == 0 ||
                                  strcmp(node->opcode, "LSR") == 0 ||
                                  strcmp(node->opcode, "ROL") == 0 ||
                                  strcmp(node->opcode, "ROR") == 0)) {
            a_known = false;
        }

        node = node->next;
    }
}
