/**
 * @file parser.h
 * @brief Assembly language parsing functions
 *
 * Provides functions for parsing assembly source lines into AST nodes,
 * extracting labels, opcodes, operands, and comments according to
 * assembler-specific syntax rules.
 */

#ifndef PARSER_H
#define PARSER_H

#include "../types.h"

/**
 * @brief Parse an assembly line into an AST node
 *
 * Parses a line of assembly code and populates the AST node with
 * extracted components (label, opcode, operand, comment). Handles
 * assembler-specific syntax like colon-terminated labels and
 * different comment styles.
 *
 * @param node Node to populate with parsed data
 * @param line Assembly source line to parse
 * @param line_num Line number (currently unused, for future diagnostics)
 * @param config Assembler syntax configuration
 */
void parse_line_ast(AstNode *node, const char *line, int line_num, AsmConfig *config);

/**
 * @brief Build complete AST from program lines
 *
 * Placeholder for additional AST processing after initial parsing.
 * Currently, AST is built incrementally during line-by-line parsing.
 *
 * @param prog Program structure containing parsed lines
 */
void build_ast(Program *prog);

#endif // PARSER_H
