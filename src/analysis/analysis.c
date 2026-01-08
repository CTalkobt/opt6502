/**
 * @file analysis.c
 * @brief Code flow and branch target analysis implementation
 *
 * Implements control flow analysis functions for identifying branch
 * targets and analyzing program structure.
 */

#include "analysis.h"

/**
 * @brief Mark all branch target labels in the AST
 *
 * Iterates through the AST and marks all label nodes as branch targets.
 * Labels are potential targets for jumps, branches, and subroutine calls,
 * so they must be preserved during optimization.
 *
 * @param prog Program to analyze
 */
void mark_branch_targets_ast(Program *prog) {
    AstNode *node = prog->root;
    while (node) {
        if (node->type == NODE_LABEL) {
            node->is_branch_target = true;
        }
        node = node->next;
    }
}

/**
 * @brief Analyze call flow and control flow patterns
 *
 * Wrapper function that performs all necessary control flow analysis.
 * Currently calls mark_branch_targets_ast() to identify all labels
 * that can be branched to.
 *
 * Future enhancements could include:
 * - Dead code detection based on reachability
 * - Loop detection
 * - Subroutine call graph construction
 *
 * @param prog Program to analyze
 */
void analyze_call_flow_ast(Program *prog) {
    mark_branch_targets_ast(prog);
}
