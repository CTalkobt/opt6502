/**
 * @file program.h
 * @brief Program state management
 *
 * Manages the overall program structure including AST construction,
 * line-by-line parsing, and memory management.
 */

#ifndef PROGRAM_H
#define PROGRAM_H

#include "../types.h"

/**
 * @brief Create a new program structure
 *
 * Allocates and initializes a program structure with default settings.
 * Sets up assembler configuration and initializes optimization state.
 *
 * @param mode Optimization mode (speed or size)
 * @param asm_type Assembler syntax type to use
 * @return Pointer to newly allocated program structure
 */
Program* create_program(OptMode mode, AsmType asm_type);

/**
 * @brief Add a line of assembly code to the program
 *
 * Parses a line of assembly code and adds it to the program's AST.
 * Handles optimizer directives (#NOOPT, #OPT) embedded in comments.
 * Creates a new AST node and links it to the program.
 *
 * @param prog Program to add line to
 * @param line Assembly source line
 * @param line_num Line number in source file
 */
void add_line_ast(Program *prog, const char *line, int line_num);

/**
 * @brief Free program and all associated memory
 *
 * Frees the program structure, including the entire AST and all
 * dynamically allocated memory.
 *
 * @param prog Program to free (NULL-safe)
 */
void free_program_ast(Program *prog);

#endif // PROGRAM_H
