/**
 * @file ast.c
 * @brief Abstract Syntax Tree node implementation
 *
 * Implements AST node creation, initialization, and memory management.
 */

#include "ast.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Create a new AST node
 *
 * Allocates a new AST node and initializes all fields to safe defaults:
 * - Pointers: NULL
 * - Booleans: false
 * - Register state: all unknown/unmodified
 * - Processor flags: all unknown
 *
 * @param type The type of node to create
 * @param line_num Line number in original source
 * @return Pointer to newly allocated node
 */
AstNode* create_ast_node(NodeType type, int line_num) {
    AstNode *node = malloc(sizeof(AstNode));
    node->type = type;
    node->line_num = line_num;
    node->label = NULL;
    node->opcode = NULL;
    node->operand = NULL;
    node->comment = NULL;
    node->next = NULL;
    node->child = NULL;
    node->sibling = NULL;
    node->is_dead = false;
    node->no_optimize = false;
    node->is_local_label = false;
    node->is_branch_target = false;
    node->optimization_count = 0;

    // Initialize register state
    node->reg_state.a_known = false;
    node->reg_state.x_known = false;
    node->reg_state.y_known = false;
    node->reg_state.z_known = false;
    node->reg_state.a_zero = false;
    node->reg_state.x_zero = false;
    node->reg_state.y_zero = false;
    node->reg_state.z_zero = false;
    node->reg_state.a_modified = false;
    node->reg_state.x_modified = false;
    node->reg_state.y_modified = false;
    node->reg_state.z_modified = false;
    node->reg_state.a_value[0] = '\0';
    node->reg_state.x_value[0] = '\0';
    node->reg_state.y_value[0] = '\0';
    node->reg_state.z_value[0] = '\0';

    // Initialize flag state
    node->reg_state.c_known = false;
    node->reg_state.n_known = false;
    node->reg_state.z_flag_known = false;
    node->reg_state.v_known = false;
    node->reg_state.c_set = false;
    node->reg_state.n_set = false;
    node->reg_state.z_flag_set = false;
    node->reg_state.v_set = false;

    return node;
}

/**
 * @brief Free an AST node and all its children recursively
 *
 * Frees all dynamically allocated memory associated with a node,
 * including strings (label, opcode, operand, comment) and
 * recursively frees child and sibling nodes.
 *
 * @param node Node to free (NULL-safe)
 */
void free_ast_node(AstNode *node) {
    if (!node) return;

    if (node->label) free(node->label);
    if (node->opcode) free(node->opcode);
    if (node->operand) free(node->operand);
    if (node->comment) free(node->comment);

    // Free children recursively
    free_ast_node(node->child);
    free_ast_node(node->sibling);

    free(node);
}
