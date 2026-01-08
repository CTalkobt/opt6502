/**
 * @file output.h
 * @brief Optimized assembly output generation
 *
 * Handles writing the optimized AST back to assembly source format,
 * including optimization statistics and optional trace comments.
 */

#ifndef OUTPUT_H
#define OUTPUT_H

#include "../types.h"

/**
 * @brief Write optimized program to assembly file
 *
 * Writes the optimized AST to an assembly file, reconstructing
 * the assembly source from the AST nodes. Includes:
 * - File header with optimization statistics
 * - Assembler and CPU target information
 * - Properly formatted labels, opcodes, operands, and comments
 * - Optional optimization trace comments (if trace_level > 0)
 *
 * Dead code nodes (is_dead=true) are either:
 * - Omitted from output (default)
 * - Included as comments if trace_level > 0
 *
 * Output format respects assembler syntax:
 * - Colon after labels (if supports_colon_labels)
 * - Correct comment character (';' or '//')
 * - Proper indentation
 *
 * @param prog Program to write (contains AST and configuration)
 * @param filename Output filename to write to
 */
void write_output_ast(Program *prog, const char *filename);

#endif // OUTPUT_H
