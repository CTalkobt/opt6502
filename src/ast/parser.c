/**
 * @file parser.c
 * @brief Assembly language parser implementation
 *
 * Implements parsing of assembly source lines into AST nodes.
 * Handles various assembler syntaxes, label formats, and comment styles.
 */

#include "parser.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/**
 * @brief Parse an assembly line into an AST node
 *
 * Parses a line of assembly code according to assembler syntax rules:
 * 1. Labels: May start at column 0 or end with ':'
 * 2. Opcodes: Follow labels or start after whitespace
 * 3. Operands: Follow opcodes
 * 4. Comments: Start with assembler-specific character(s)
 *
 * The function handles:
 * - Local vs global label detection
 * - Colon-terminated labels (ca65, ACME, etc.)
 * - No-colon labels (Merlin)
 * - Comment character detection (';' or '//')
 *
 * @param node Node to populate with parsed components
 * @param line Source line to parse
 * @param line_num Line number (unused, kept for future diagnostics)
 * @param config Assembler syntax configuration
 */
void parse_line_ast(AstNode *node, const char *line, int line_num, AsmConfig *config) {
    (void)line_num;  // Suppress unused parameter warning
    const char *p = line;
    while (*p && isspace(*p)) p++;

    // Check for label (starts at column 0 or ends with :)
    bool potential_label = false;
    if (line[0] != ' ' && line[0] != '\t' && line[0] != ' ' && !is_comment_start(line, config)) {
        potential_label = true;
    }

    if (potential_label) {
        int i = 0;

        // Parse label - stops at whitespace or colon
        while (*p && !isspace(*p) && *p != ':' && !is_comment_start(p, config) && i < 63) {
            i++;
            p++;
        }

        // Allocate and copy label
        node->label = malloc(i + 1);
        if (node->label) {
            strncpy(node->label, line, i);
            node->label[i] = '\0';
        }

        // Check if this is a local label
        if (is_local_label(node->label, config)) {
            node->is_local_label = true;
        }

        // Check if this is actually a label
        if (config->supports_colon_labels && *p == ':') {
            // Definitely a label
            node->type = NODE_LABEL;
            p++; // Skip colon
        } else if (i > 0 && node->label[0]) {
            // Might be a label (for Merlin style without colons)
            // We'll assume it is if followed by whitespace or end of line
            node->type = NODE_LABEL;
        }

        while (*p && isspace(*p)) p++;
    } else {
        node->type = NODE_ASM_LINE;
    }

    // Skip comments
    if (is_comment_start(p, config) || *p == ' ') return;

    // Parse opcode
    int i = 0;
    while (*p && !isspace(*p) && !is_comment_start(p, config) && i < 15) {
        i++;
        p++;
    }

    // Allocate and copy opcode
    node->opcode = malloc(i + 1);
    if (node->opcode) {
        strncpy(node->opcode, line + (p - line - i), i);
        node->opcode[i] = '\0';
    }

    while (*p && isspace(*p)) p++;

    // Parse operand
    i = 0;
    while (*p && !is_comment_start(p, config) && i < 63) {
        i++;
        p++;
    }

    // Allocate and copy operand
    node->operand = malloc(i + 1);
    if (node->operand) {
        strncpy(node->operand, line + (p - line - i), i);
        node->operand[i] = '\0';
    }

    // Trim trailing whitespace
    while (i > 0 && isspace(node->operand[i-1])) {
        node->operand[--i] = '\0';
    }

    // Parse comment if present
    if (is_comment_start(p, config)) {
        // Skip past comment start character(s)
        const char *comment_start = p;
        if (strcmp(config->comment_char, "//") == 0) {
            p += 2;
        } else {
            p += 1;
        }

        // Copy the rest of the line as comment (including comment character)
        size_t comment_len = strlen(comment_start);
        if (comment_len > 0) {
            node->comment = malloc(comment_len + 1);
            if (node->comment) {
                strcpy(node->comment, comment_start);
            }
        }
    }
}

/**
 * @brief Build complete AST from program lines
 *
 * Placeholder for additional AST post-processing. Currently, the AST
 * is built incrementally during line-by-line parsing via add_line_ast().
 * This function is reserved for future enhancements like AST validation
 * or structural analysis.
 *
 * @param prog Program containing parsed AST
 */
void build_ast(Program *prog) {
    (void)prog;  // Suppress unused parameter warning
    // AST is built during parsing in add_line_ast
    // This function can be used for additional AST processing if needed
}
