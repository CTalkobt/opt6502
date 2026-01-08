/**
 * @file analysis.h
 * @brief Code flow and branch target analysis
 *
 * Provides analysis functions for identifying branch targets and
 * analyzing control flow in the program AST.
 */

#ifndef ANALYSIS_H
#define ANALYSIS_H

#include "../types.h"

/**
 * @brief Mark all branch target labels in the AST
 *
 * Scans the AST and marks all label nodes that can be jumped or
 * branched to. This information is used by optimizers to preserve
 * labels that are referenced by control flow instructions.
 *
 * @param prog Program to analyze
 */
void mark_branch_targets_ast(Program *prog);

/**
 * @brief Analyze call flow and control flow patterns
 *
 * Performs control flow analysis including branch target identification.
 * This is typically called before optimization passes to ensure that
 * control flow is properly understood.
 *
 * @param prog Program to analyze
 */
void analyze_call_flow_ast(Program *prog);

#endif // ANALYSIS_H
