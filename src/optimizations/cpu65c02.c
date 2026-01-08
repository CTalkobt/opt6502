/**
 * @file cpu65c02.c
 * @brief 65C02-specific optimizations
 *
 * Implements optimizations that utilize 65C02-specific instructions,
 * primarily the STZ (Store Zero) instruction which the original 6502
 * does not have.
 *
 * IMPORTANT: These optimizations are NOT applied to 45GS02 targets,
 * as the 45GS02's STZ instruction stores the Z register, not zero!
 */

#include "optimizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/**
 * @brief 65C02-specific optimizations - LDA #$00 / STA to STZ conversion
 *
 * Converts sequences of loading zero and storing it to use the 65C02's
 * STZ instruction, which is more efficient:
 *
 * Pattern 1 - Simple case:
 *   LDA #$00
 *   STA address1
 *   STA address2
 * Becomes:
 *   STZ address1    (LDA removed)
 *   STZ address2
 *
 * Pattern 2 - A value used after:
 *   LDA #$00
 *   STA address1
 *   ADC something   <- uses A
 * Becomes:
 *   LDA #$00        (kept, A value needed)
 *   STZ address1
 *   ADC something
 *
 * The optimization:
 * 1. Finds LDA #$00 instructions
 * 2. Scans forward to find following STA instructions
 * 3. Converts all STAs to STZ
 * 4. Only removes LDA #$00 if A value is not used afterward
 *
 * CRITICAL: Disabled for 45GS02 where STZ has different semantics!
 *
 * @param prog Program to optimize (must have allow_65c02=true, is_45gs02=false)
 */
void optimize_65c02_instructions_ast(Program *prog) {
    if (!prog->allow_65c02 || prog->is_45gs02) return;  // Don't apply to 45GS02!

    AstNode *node = prog->root;

    while (node) {
        if (node->is_dead || node->no_optimize) {
            node = node->next;
            continue;
        }

        // Pattern: LDA #$00 followed by one or more STA -> convert all STA to STZ
        // If A is not used after the STAs, also mark LDA #$00 as dead
        if (node->opcode && strcmp(node->opcode, "LDA") == 0 &&
            node->operand && (strcmp(node->operand, "#$00") == 0 || strcmp(node->operand, "#0") == 0)) {

            // First pass: scan forward to check if A is used and find STAs
            AstNode *current = node->next;
            bool found_sta = false;
            bool a_value_used = false;  // Track if accumulator value is used (not just modified)

            while (current && !current->is_branch_target) {
                if (current->is_dead || current->no_optimize) {
                    current = current->next;
                    continue;
                }

                if (current->opcode && strcmp(current->opcode, "STA") == 0) {
                    found_sta = true;
                    current = current->next;
                } else if (current->opcode && (
                    strcmp(current->opcode, "ADC") == 0 ||
                    strcmp(current->opcode, "SBC") == 0 ||
                    strcmp(current->opcode, "AND") == 0 ||
                    strcmp(current->opcode, "ORA") == 0 ||
                    strcmp(current->opcode, "EOR") == 0 ||
                    strcmp(current->opcode, "CMP") == 0 ||
                    strcmp(current->opcode, "BIT") == 0 ||
                    strcmp(current->opcode, "PHA") == 0 ||
                    strcmp(current->opcode, "TAX") == 0 ||
                    strcmp(current->opcode, "TAY") == 0)) {
                    // Instruction uses A value - can't remove LDA but can still convert STA to STZ
                    a_value_used = true;
                    break;
                } else if (current->opcode && (
                    strcmp(current->opcode, "LDA") == 0 ||
                    strcmp(current->opcode, "PLA") == 0 ||
                    strcmp(current->opcode, "TXA") == 0 ||
                    strcmp(current->opcode, "TYA") == 0)) {
                    // A is reloaded/modified - safe to remove original LDA
                    break;
                } else {
                    // Other instructions that don't use A - safe to continue
                    current = current->next;
                }
            }

            // Second pass: convert STAs to STZ if we found any
            if (found_sta) {
                current = node->next;
                while (current && !current->is_branch_target) {
                    if (current->is_dead || current->no_optimize) {
                        current = current->next;
                        continue;
                    }

                    if (current->opcode && strcmp(current->opcode, "STA") == 0) {
                        // Convert STA to STZ
                        current->opcode = realloc(current->opcode, 4);
                        if (current->opcode) {
                            strcpy(current->opcode, "STZ");
                        }
                        prog->optimizations++;
                        current = current->next;
                    } else if (current->opcode && (
                        strcmp(current->opcode, "LDA") == 0 ||
                        strcmp(current->opcode, "PLA") == 0 ||
                        strcmp(current->opcode, "TXA") == 0 ||
                        strcmp(current->opcode, "TYA") == 0 ||
                        strcmp(current->opcode, "ADC") == 0 ||
                        strcmp(current->opcode, "SBC") == 0 ||
                        strcmp(current->opcode, "AND") == 0 ||
                        strcmp(current->opcode, "ORA") == 0 ||
                        strcmp(current->opcode, "EOR") == 0 ||
                        strcmp(current->opcode, "CMP") == 0 ||
                        strcmp(current->opcode, "BIT") == 0 ||
                        strcmp(current->opcode, "PHA") == 0 ||
                        strcmp(current->opcode, "TAX") == 0 ||
                        strcmp(current->opcode, "TAY") == 0)) {
                        // A is modified or used - stop scanning
                        break;
                    } else {
                        current = current->next;
                    }
                }

                // Only mark LDA #$00 as dead if A value is not used later
                if (!a_value_used) {
                    node->is_dead = true;
                    if (prog->trace_level > 1) {
                        printf("DEBUG 65c02: Marked LDA #0 at line %d as dead, converted STAs to STZ\n", node->line_num);
                    }
                } else {
                    if (prog->trace_level > 1) {
                        printf("DEBUG 65c02: Kept LDA #0 at line %d (A used later), but converted STAs to STZ\n", node->line_num);
                    }
                }
            }
        }

        node = node->next;
    }
}
