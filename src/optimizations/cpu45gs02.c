/**
 * @file cpu45gs02.c
 * @brief 45GS02-specific optimizations (MEGA65 CPU)
 *
 * Implements optimizations for the 45GS02 CPU found in the MEGA65.
 * This CPU has special features including:
 * - Z register (base page offset register)
 * - STZ instruction that stores Z register (NOT zero like 65C02!)
 * - NEG instruction (two's complement negation)
 * - ASR instruction (arithmetic shift right, preserves sign)
 *
 * CRITICAL: The 45GS02's STZ stores the Z REGISTER, not zero!
 * This is completely different from the 65C02's STZ instruction.
 */

#include "optimizer.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/**
 * @brief 45GS02-specific optimizations
 *
 * Implements several 45GS02-specific optimization patterns:
 *
 * 1. Z Register usage for repeated stores:
 *    LDA #val / STA addr1 / LDA #val / STA addr2
 *    Becomes:
 *    LDZ #val / STZ addr1 / STZ addr2 (removes second LDA)
 *
 * 2. NEG instruction (two's complement):
 *    EOR #$FF / SEC / ADC #$00
 *    Becomes:
 *    NEG
 *
 * 3. ASR instruction (arithmetic shift right):
 *    CMP #$80 / ROR
 *    Becomes:
 *    ASR
 *
 * IMPORTANT: Does NOT convert LDA #0 / STA to STZ, because on 45GS02,
 * STZ stores the Z register, not zero! Use LDZ #0 / STZ explicitly if
 * you want to store zero on 45GS02.
 *
 * @param prog Program to optimize (only runs if is_45gs02=true)
 */
void optimize_45gs02_instructions_ast(Program *prog) {
    if (!prog->is_45gs02) return;

    AstNode *node = prog->root;
    while (node) {
        if (node->is_dead || node->no_optimize) {
            node = node->next;
            continue;
        }

        // ===== Z Register Optimizations =====

        // Pattern: Multiple stores of SAME VALUE using LDA #val, STA -> LDZ #val, then STZ
        // Generalized for any immediate value, not just zero
        // Look for: LDA #val, STA addr1, LDA #val, STA addr2
        if (node->opcode && strcmp(node->opcode, "LDA") == 0 && node->operand && node->operand[0] == '#' &&
            node->next && node->next->opcode && strcmp(node->next->opcode, "STA") == 0 &&
            node->next->next && node->next->next->opcode && strcmp(node->next->next->opcode, "LDA") == 0 &&
            node->next->next->operand && node->next->next->operand[0] == '#' &&
            node->next->next->next && node->next->next->next->opcode && strcmp(node->next->next->next->opcode, "STA") == 0 &&
            !node->next->no_optimize && !node->next->next->no_optimize && !node->next->next->next->no_optimize &&
            strcmp(node->operand, node->next->next->operand) == 0) {  // Same value!

            // Convert to: LDZ #val, STZ addr1, STZ addr2
            node->opcode = realloc(node->opcode, 4);
            if (node->opcode) {
                strcpy(node->opcode, "LDZ");
            }

            node->next->opcode = realloc(node->next->opcode, 4);
            if (node->next->opcode) {
                strcpy(node->next->opcode, "STZ");
            }

            node->next->next->is_dead = true;  // Remove second LDA

            node->next->next->next->opcode = realloc(node->next->next->next->opcode, 4);
            if (node->next->next->next->opcode) {
                strcpy(node->next->next->next->opcode, "STZ");
            }

            prog->optimizations++;
        }

        // Also handle cases where we already have LDZ followed by stores
        // Pattern: LDZ #val, STA addr1, STA addr2, ... -> LDZ #val, STZ addr1, STZ addr2, ...
        // Also handles: LDZ #val, ..., LDA #val, STA addr -> LDZ #val, ..., STZ addr (with LDA marked dead)
        if (node->opcode && strcmp(node->opcode, "LDZ") == 0 && node->operand && node->operand[0] == '#') {
            // Look ahead for STA instructions that could become STZ
            AstNode *current = node->next;
            while (current) {
                if (current->is_dead || current->no_optimize) {
                    current = current->next;
                    continue;
                }

                if (current->opcode && strcmp(current->opcode, "STA") == 0 &&
                    !current->is_branch_target) {
                    // Convert STA to STZ (stores Z register value)
                    current->opcode = realloc(current->opcode, 4);
                    if (current->opcode) {
                        strcpy(current->opcode, "STZ");
                    }
                    prog->optimizations++;
                    current = current->next;
                } else if (current->opcode && strcmp(current->opcode, "LDA") == 0 &&
                           current->operand && node->operand &&
                           strcmp(current->operand, node->operand) == 0 &&
                           current->next && current->next->opcode &&
                           strcmp(current->next->opcode, "STA") == 0) {
                    // LDA with same value followed by STA - mark LDA dead and convert STA to STZ
                    current->is_dead = true;
                    AstNode *sta_node = current->next;
                    sta_node->opcode = realloc(sta_node->opcode, 4);
                    if (sta_node->opcode) {
                        strcpy(sta_node->opcode, "STZ");
                    }
                    prog->optimizations++;
                    current = sta_node->next;
                } else if (current->opcode && (strcmp(current->opcode, "LDA") == 0 ||
                                              strcmp(current->opcode, "LDZ") == 0 ||
                                              strcmp(current->opcode, "TAX") == 0 ||
                                              strcmp(current->opcode, "TAY") == 0)) {
                    // Something that would change what we're storing (with different value)
                    break;
                } else {
                    current = current->next;
                }
            }
        }

        // 45GS02 has NEG instruction (negate accumulator)
        // Pattern: EOR #$FF, SEC, ADC #$00 -> NEG
        if (node->opcode && strcmp(node->opcode, "EOR") == 0 &&
            node->operand && strcmp(node->operand, "#$FF") == 0 &&
            node->next && node->next->opcode && strcmp(node->next->opcode, "SEC") == 0 &&
            node->next->next && node->next->next->opcode && strcmp(node->next->next->opcode, "ADC") == 0 &&
            node->next->next->operand && strcmp(node->next->next->operand, "#$00") == 0 &&
            !node->next->no_optimize && !node->next->next->no_optimize &&
            !node->next->next->is_branch_target) {

            // Replace with NEG
            node->opcode = realloc(node->opcode, 4);
            if (node->opcode) {
                strcpy(node->opcode, "NEG");
            }
            node->operand = NULL;
            node->next->is_dead = true;
            node->next->next->is_dead = true;
            prog->optimizations++;
        }

        // ===== ASR (Arithmetic Shift Right) =====

        // 45GS02 has ASR instruction (arithmetic shift right, preserves sign)
        // Pattern: CMP #$80, ROR -> ASR
        if (node->opcode && strcmp(node->opcode, "CMP") == 0 &&
            node->operand && strcmp(node->operand, "#$80") == 0 &&
            node->next && node->next->opcode && strcmp(node->next->opcode, "ROR") == 0 &&
            !node->next->no_optimize &&
            !node->next->is_branch_target) {

            // Can use ASR for signed right shift
            node->opcode = realloc(node->opcode, 4);
            if (node->opcode) {
                strcpy(node->opcode, "ASR");
            }
            node->operand = NULL;
            node->next->is_dead = true;
            prog->optimizations++;
        }

        node = node->next;
    }
}
