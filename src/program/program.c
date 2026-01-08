/**
 * @file program.c
 * @brief Program state management implementation
 *
 * Implements program creation, line parsing, and memory management.
 * Handles optimizer directive processing and AST construction.
 */

#include "program.h"
#include "../ast/ast.h"
#include "../ast/parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/**
 * @brief Create a new program structure
 *
 * Initializes all program state including:
 * - Assembler configuration
 * - Optimization settings
 * - CPU type and features
 * - AST pointers
 *
 * @param mode Optimization mode (speed or size)
 * @param asm_type Assembler type for syntax rules
 * @return Pointer to initialized program structure
 */
Program* create_program(OptMode mode, AsmType asm_type) {
    Program *prog = malloc(sizeof(Program));
    prog->root = NULL;
    prog->current_node = NULL;
    prog->count = 0;
    prog->mode = mode;
    prog->optimizations = 0;
    prog->opt_enabled = true;
    prog->config = get_asm_config(asm_type);
    prog->cpu_type = CPU_6502;
    prog->allow_65c02 = false;
    prog->allow_undocumented = false;
    prog->is_45gs02 = false;
    prog->trace_level = 0;
    return prog;
}

/**
 * @brief Add a line of assembly code to the program AST
 *
 * Processes a line of assembly code:
 * 1. Checks for optimizer directives (#NOOPT, #OPT)
 * 2. Creates a new AST node
 * 3. Parses the line into the node
 * 4. Links the node into the program's AST
 *
 * Optimizer directives in comments control whether optimization
 * is enabled for subsequent lines.
 *
 * @param prog Program to add line to
 * @param line Assembly source line
 * @param line_num Line number in source file
 */
void add_line_ast(Program *prog, const char *line, int line_num) {
    AstNode *node = create_ast_node(NODE_ASM_LINE, line_num);

    // Check for optimizer directives in comments
    const char *trimmed = line;
    while (*trimmed && isspace(*trimmed)) trimmed++;

    // Check for directive after comment character
    if (is_comment_start(trimmed, &prog->config)) {
        const char *comment_content = trimmed;
        // Skip comment character(s)
        if (strcmp(prog->config.comment_char, "//") == 0) {
            comment_content += 2;
        } else {
            comment_content += 1;
        }

        // Skip whitespace after comment
        while (*comment_content && isspace(*comment_content)) comment_content++;

        if (strncmp(comment_content, "#NOOPT", 6) == 0) {
            prog->opt_enabled = false;
            printf("Optimization disabled at line %d\n", line_num);
        } else if (strncmp(comment_content, "#OPT", 4) == 0) {
            prog->opt_enabled = true;
            printf("Optimization enabled at line %d\n", line_num);
        }
    }

    parse_line_ast(node, line, line_num, &prog->config);
    node->no_optimize = !prog->opt_enabled;

    // Add to AST
    if (prog->root == NULL) {
        prog->root = node;
        prog->current_node = node;
    } else {
        prog->current_node->next = node;
        prog->current_node = node;
    }

    prog->count++;
}

/**
 * @brief Free program and all associated memory
 *
 * Frees the program structure and recursively frees the entire AST
 * including all nodes and their strings.
 *
 * @param prog Program to free (NULL-safe)
 */
void free_program_ast(Program *prog) {
    if (!prog) return;

    free_ast_node(prog->root);
    free(prog);
}
