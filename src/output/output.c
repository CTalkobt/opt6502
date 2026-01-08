/**
 * @file output.c
 * @brief Optimized assembly output generation implementation
 *
 * Implements writing of optimized AST back to assembly source format.
 * Handles proper formatting for different assembler syntaxes and
 * includes optional optimization trace information.
 */

#include "output.h"
#include <stdio.h>
#include <stdbool.h>

/**
 * @brief Write optimized program to assembly file
 *
 * Reconstructs assembly source from the optimized AST and writes it
 * to a file. The output includes:
 *
 * Header:
 * - Optimization mode (speed/size)
 * - Assembler type
 * - Target CPU
 * - Total optimization count
 * - Trace information (if enabled)
 *
 * Body:
 * - Labels (with or without colons based on assembler)
 * - Opcodes with operands
 * - Original comments
 * - Optional optimization trace comments
 *
 * Format rules:
 * - Labels at start of line, followed by colon if supported
 * - Opcodes indented (4 spaces if no label on line)
 * - Operands separated by space
 * - Comments preserved or added for trace
 * - Dead code omitted (or commented if trace_level > 0)
 *
 * @param prog Program containing optimized AST and configuration
 * @param filename Output file path
 */
void write_output_ast(Program *prog, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot write to %s\n", filename);
        return;
    }

    // Use the appropriate comment style for the assembler
    const char *cmt = prog->config.comment_char;

    fprintf(fp, "%s Optimized for %s\n", cmt,
            prog->mode == OPT_SPEED ? "speed" : "size");
    fprintf(fp, "%s Assembler: %s\n", cmt, prog->config.name);
    fprintf(fp, "%s Target CPU: %s\n", cmt,
            prog->cpu_type == CPU_6502 ? "6502" :
            prog->cpu_type == CPU_65C02 ? "65C02" :
            prog->cpu_type == CPU_65816 ? "65816" : "45GS02");
    fprintf(fp, "%s Total optimizations: %d\n\n", cmt, prog->optimizations);

    if (prog->trace_level > 0) {
        fprintf(fp, "%s Optimization trace enabled (Level %d)\n", cmt, prog->trace_level);
        fprintf(fp, "%s Lines marked with %s OPT: show applied optimizations\n\n",
                cmt, cmt);
    }

    AstNode *node = prog->root;
    while (node) {
        if (!node->is_dead) {
            // Reconstruct line from AST node
            bool has_opcode = node->opcode && node->opcode[0] != '\0';

            if (node->label) {
                fprintf(fp, "%s", node->label);
                if (prog->config.supports_colon_labels) {
                    fprintf(fp, ":");
                }
                if (has_opcode) {
                    // Label with opcode on same line
                    fprintf(fp, "\t");
                } else {
                    // Label only, newline
                    fprintf(fp, "\n");
                }
            } else if (has_opcode) {
                // No label, add indentation for opcode
                fprintf(fp, "    ");
            }

            if (has_opcode) {
                fprintf(fp, "%s", node->opcode);
                if (node->operand && node->operand[0] != '\0') {
                    fprintf(fp, " %s", node->operand);
                }
                // Add comment if present
                if (node->comment && node->comment[0] != '\0') {
                    fprintf(fp, "\t%s", node->comment);
                }
                fprintf(fp, "\n");
            }
        } else if (prog->trace_level > 0) {
            fprintf(fp, "%s OPT: Removed - %s\n", cmt, node->label ? node->label : "unknown");
        }
        node = node->next;
    }

    fclose(fp);
}
