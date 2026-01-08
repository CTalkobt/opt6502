/**
 * @file ast.h
 * @brief Abstract Syntax Tree node management
 *
 * Provides functions for creating, initializing, and freeing AST nodes.
 * Each node represents a line or element of assembly code.
 */

#ifndef AST_H
#define AST_H

#include "../types.h"

/**
 * @brief Create a new AST node
 *
 * Allocates and initializes a new AST node with default values.
 * All pointers are set to NULL, booleans to false, and register
 * state is initialized to unknown.
 *
 * @param type The type of node to create (label, opcode, etc.)
 * @param line_num Line number in original source file
 * @return Pointer to newly allocated node, or NULL on allocation failure
 */
AstNode* create_ast_node(NodeType type, int line_num);

/**
 * @brief Free an AST node and all its children
 *
 * Recursively frees an AST node, its children, siblings, and all
 * allocated strings (label, opcode, operand, comment).
 *
 * @param node Node to free (may be NULL)
 */
void free_ast_node(AstNode *node);

#endif // AST_H
