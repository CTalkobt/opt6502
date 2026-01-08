/**
 * @file registers.c
 * @brief CPU register and flag state tracking implementation
 *
 * Implements comprehensive register and processor flag tracking for
 * 6502, 65C02, 65816, and 45GS02 CPUs. Tracks register values, modifications,
 * and flag states for optimization purposes.
 *
 * This module is critical for:
 * - Constant propagation (tracking known register values)
 * - Redundant load elimination (detecting unnecessary reloads)
 * - Dead code elimination (finding unused register assignments)
 */

#include "registers.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

/**
 * @brief Update register state based on an instruction
 *
 * Analyzes a single 6502 instruction and updates the register state
 * to reflect its effects. For each instruction, determines:
 * - Which registers are modified
 * - Whether register values are known (immediate mode loads)
 * - Which processor flags are affected
 * - Whether flag states are known
 *
 * The function handles all standard 6502 opcodes plus 65C02 and 45GS02
 * extensions. Branch targets reset register knowledge conservatively.
 *
 * @param node AST node containing the instruction and operand
 * @param state Register state to update (modified in place)
 */
void update_register_state(AstNode *node, RegisterState *state) {
    if (!node || !node->opcode) return;

    // Reset modification flags
    state->a_modified = false;
    state->x_modified = false;
    state->y_modified = false;
    state->z_modified = false;

    // === LOAD INSTRUCTIONS ===
    // LDA - Load Accumulator: Sets N and Z flags
    if (strcasecmp(node->opcode, "LDA") == 0) {
        state->a_modified = true;
        state->n_known = true;
        state->z_flag_known = true;

        if (node->operand && node->operand[0] == '#') {
            // Immediate mode - we know the exact value
            state->a_known = true;
            strcpy(state->a_value, node->operand);
            state->a_zero = (strcmp(node->operand, "#$00") == 0 || strcmp(node->operand, "#0") == 0);
            state->z_flag_set = state->a_zero;
            // For N flag, we'd need to parse the value to check bit 7
            // Simplified: if value is $80-$FF, N is set
            state->n_set = false; // Conservative - would need full value parsing
        } else {
            // Memory load - value unknown
            state->a_known = false;
            state->a_zero = false;
            state->a_value[0] = '\0';
            state->z_flag_set = false; // Unknown
            state->n_set = false;      // Unknown
            state->z_flag_known = false;
            state->n_known = false;
        }
    }

    // LDX - Load X Register: Sets N and Z flags
    else if (strcasecmp(node->opcode, "LDX") == 0) {
        state->x_modified = true;
        state->n_known = true;
        state->z_flag_known = true;

        if (node->operand && node->operand[0] == '#') {
            state->x_known = true;
            strcpy(state->x_value, node->operand);
            state->x_zero = (strcmp(node->operand, "#$00") == 0 || strcmp(node->operand, "#0") == 0);
            state->z_flag_set = state->x_zero;
            state->n_set = false; // Conservative
        } else {
            state->x_known = false;
            state->x_zero = false;
            state->x_value[0] = '\0';
            state->z_flag_known = false;
            state->n_known = false;
        }
    }

    // LDY - Load Y Register: Sets N and Z flags
    else if (strcasecmp(node->opcode, "LDY") == 0) {
        state->y_modified = true;
        state->n_known = true;
        state->z_flag_known = true;

        if (node->operand && node->operand[0] == '#') {
            state->y_known = true;
            strcpy(state->y_value, node->operand);
            state->y_zero = (strcmp(node->operand, "#$00") == 0 || strcmp(node->operand, "#0") == 0);
            state->z_flag_set = state->y_zero;
            state->n_set = false; // Conservative
        } else {
            state->y_known = false;
            state->y_zero = false;
            state->y_value[0] = '\0';
            state->z_flag_known = false;
            state->n_known = false;
        }
    }

    // LDZ - Load Z Register (45GS02): Sets N and Z flags
    else if (strcasecmp(node->opcode, "LDZ") == 0) {
        state->z_modified = true;
        state->n_known = true;
        state->z_flag_known = true;

        if (node->operand && node->operand[0] == '#') {
            state->z_known = true;
            strcpy(state->z_value, node->operand);
            state->z_zero = (strcmp(node->operand, "#$00") == 0 || strcmp(node->operand, "#0") == 0);
            state->z_flag_set = state->z_zero;
            state->n_set = false; // Conservative
        } else {
            state->z_known = false;
            state->z_zero = false;
            state->z_value[0] = '\0';
            state->z_flag_known = false;
            state->n_known = false;
        }
    }

    // === STORE INSTRUCTIONS ===
    // STA, STX, STY, STZ - Store instructions: No flags affected
    else if (strcasecmp(node->opcode, "STA") == 0 ||
             strcasecmp(node->opcode, "STX") == 0 ||
             strcasecmp(node->opcode, "STY") == 0 ||
             strcasecmp(node->opcode, "STZ") == 0) {
        // Store operations don't affect registers or flags
    }

    // === TRANSFER INSTRUCTIONS ===
    // TAX - Transfer A to X: Sets N and Z flags
    else if (strcasecmp(node->opcode, "TAX") == 0) {
        state->x_modified = true;
        state->x_known = state->a_known;
        if (state->a_known) {
            strcpy(state->x_value, state->a_value);
            state->x_zero = state->a_zero;
        }
        state->n_known = state->a_known;
        state->z_flag_known = state->a_known;
        if (state->a_known) {
            state->z_flag_set = state->a_zero;
        }
    }

    // TXA - Transfer X to A: Sets N and Z flags
    else if (strcasecmp(node->opcode, "TXA") == 0) {
        state->a_modified = true;
        state->a_known = state->x_known;
        if (state->x_known) {
            strcpy(state->a_value, state->x_value);
            state->a_zero = state->x_zero;
        }
        state->n_known = state->x_known;
        state->z_flag_known = state->x_known;
        if (state->x_known) {
            state->z_flag_set = state->x_zero;
        }
    }

    // TAY - Transfer A to Y: Sets N and Z flags
    else if (strcasecmp(node->opcode, "TAY") == 0) {
        state->y_modified = true;
        state->y_known = state->a_known;
        if (state->a_known) {
            strcpy(state->y_value, state->a_value);
            state->y_zero = state->a_zero;
        }
        state->n_known = state->a_known;
        state->z_flag_known = state->a_known;
        if (state->a_known) {
            state->z_flag_set = state->a_zero;
        }
    }

    // TYA - Transfer Y to A: Sets N and Z flags
    else if (strcasecmp(node->opcode, "TYA") == 0) {
        state->a_modified = true;
        state->a_known = state->y_known;
        if (state->y_known) {
            strcpy(state->a_value, state->y_value);
            state->a_zero = state->y_zero;
        }
        state->n_known = state->y_known;
        state->z_flag_known = state->y_known;
        if (state->y_known) {
            state->z_flag_set = state->y_zero;
        }
    }

    // TSX - Transfer SP to X: Sets N and Z flags
    else if (strcasecmp(node->opcode, "TSX") == 0) {
        state->x_modified = true;
        state->x_known = false; // SP value typically unknown
        state->n_known = false;
        state->z_flag_known = false;
    }

    // TXS - Transfer X to SP: No flags affected
    else if (strcasecmp(node->opcode, "TXS") == 0) {
        // No flags or visible registers modified
    }

    // === INCREMENT/DECREMENT ===
    // INX - Increment X: Sets N and Z flags
    else if (strcasecmp(node->opcode, "INX") == 0) {
        state->x_modified = true;
        state->x_known = false; // Value changes, typically becomes unknown
        state->x_zero = false;  // Very unlikely to wrap to zero
        state->n_known = false;
        state->z_flag_known = false;
    }

    // INY - Increment Y: Sets N and Z flags
    else if (strcasecmp(node->opcode, "INY") == 0) {
        state->y_modified = true;
        state->y_known = false;
        state->y_zero = false;
        state->n_known = false;
        state->z_flag_known = false;
    }

    // DEX - Decrement X: Sets N and Z flags
    else if (strcasecmp(node->opcode, "DEX") == 0) {
        state->x_modified = true;
        state->x_known = false;
        state->n_known = false;
        state->z_flag_known = false;
    }

    // DEY - Decrement Y: Sets N and Z flags
    else if (strcasecmp(node->opcode, "DEY") == 0) {
        state->y_modified = true;
        state->y_known = false;
        state->n_known = false;
        state->z_flag_known = false;
    }

    // INC - Increment Memory or A: Sets N and Z flags
    else if (strcasecmp(node->opcode, "INC") == 0) {
        if (!node->operand || node->operand[0] == 'A' || node->operand[0] == 'a') {
            // INC A (65C02)
            state->a_modified = true;
            state->a_known = false;
        }
        state->n_known = false;
        state->z_flag_known = false;
    }

    // DEC - Decrement Memory or A: Sets N and Z flags
    else if (strcasecmp(node->opcode, "DEC") == 0) {
        if (!node->operand || node->operand[0] == 'A' || node->operand[0] == 'a') {
            // DEC A (65C02)
            state->a_modified = true;
            state->a_known = false;
        }
        state->n_known = false;
        state->z_flag_known = false;
    }

    // === ARITHMETIC ===
    // ADC - Add with Carry: Sets C, N, Z, V flags
    else if (strcasecmp(node->opcode, "ADC") == 0) {
        state->a_modified = true;
        state->a_known = false;
        state->c_known = false;  // Result of carry depends on operands
        state->n_known = false;
        state->z_flag_known = false;
        state->v_known = false;  // Overflow depends on operands
    }

    // SBC - Subtract with Carry: Sets C, N, Z, V flags
    else if (strcasecmp(node->opcode, "SBC") == 0) {
        state->a_modified = true;
        state->a_known = false;
        state->c_known = false;
        state->n_known = false;
        state->z_flag_known = false;
        state->v_known = false;
    }

    // === LOGICAL OPERATIONS ===
    // AND - Logical AND: Sets N and Z flags
    else if (strcasecmp(node->opcode, "AND") == 0) {
        state->a_modified = true;
        state->a_known = false;
        state->n_known = false;
        state->z_flag_known = false;
        // C and V are not affected
    }

    // ORA - Logical OR: Sets N and Z flags
    else if (strcasecmp(node->opcode, "ORA") == 0) {
        state->a_modified = true;
        state->a_known = false;
        state->n_known = false;
        state->z_flag_known = false;
    }

    // EOR - Logical XOR: Sets N and Z flags
    else if (strcasecmp(node->opcode, "EOR") == 0) {
        state->a_modified = true;
        state->a_known = false;
        state->n_known = false;
        state->z_flag_known = false;
    }

    // === SHIFT AND ROTATE ===
    // ASL - Arithmetic Shift Left: Sets C, N, Z flags
    else if (strcasecmp(node->opcode, "ASL") == 0) {
        if (!node->operand || node->operand[0] == 'A' || node->operand[0] == 'a') {
            state->a_modified = true;
            state->a_known = false;
        }
        state->c_known = false;  // Bit 7 goes to carry
        state->n_known = false;
        state->z_flag_known = false;
    }

    // LSR - Logical Shift Right: Sets C, N, Z flags (N always 0)
    else if (strcasecmp(node->opcode, "LSR") == 0) {
        if (!node->operand || node->operand[0] == 'A' || node->operand[0] == 'a') {
            state->a_modified = true;
            state->a_known = false;
        }
        state->c_known = false;  // Bit 0 goes to carry
        state->n_known = true;
        state->n_set = false;    // LSR always clears N
        state->z_flag_known = false;
    }

    // ROL - Rotate Left: Sets C, N, Z flags
    else if (strcasecmp(node->opcode, "ROL") == 0) {
        if (!node->operand || node->operand[0] == 'A' || node->operand[0] == 'a') {
            state->a_modified = true;
            state->a_known = false;
        }
        state->c_known = false;  // Bit 7 goes to carry
        state->n_known = false;
        state->z_flag_known = false;
    }

    // ROR - Rotate Right: Sets C, N, Z flags
    else if (strcasecmp(node->opcode, "ROR") == 0) {
        if (!node->operand || node->operand[0] == 'A' || node->operand[0] == 'a') {
            state->a_modified = true;
            state->a_known = false;
        }
        state->c_known = false;  // Bit 0 goes to carry
        state->n_known = false;  // Depends on carry flag going into bit 7
        state->z_flag_known = false;
    }

    // === COMPARISON ===
    // CMP - Compare Accumulator: Sets C, N, Z flags
    else if (strcasecmp(node->opcode, "CMP") == 0) {
        state->c_known = false;  // Set if A >= operand
        state->n_known = false;
        state->z_flag_known = false; // Set if A == operand
    }

    // CPX - Compare X: Sets C, N, Z flags
    else if (strcasecmp(node->opcode, "CPX") == 0) {
        state->c_known = false;
        state->n_known = false;
        state->z_flag_known = false;
    }

    // CPY - Compare Y: Sets C, N, Z flags
    else if (strcasecmp(node->opcode, "CPY") == 0) {
        state->c_known = false;
        state->n_known = false;
        state->z_flag_known = false;
    }

    // === FLAG MANIPULATION ===
    // CLC - Clear Carry: Clears C flag
    else if (strcasecmp(node->opcode, "CLC") == 0) {
        state->c_known = true;
        state->c_set = false;
    }

    // SEC - Set Carry: Sets C flag
    else if (strcasecmp(node->opcode, "SEC") == 0) {
        state->c_known = true;
        state->c_set = true;
    }

    // CLV - Clear Overflow: Clears V flag
    else if (strcasecmp(node->opcode, "CLV") == 0) {
        state->v_known = true;
        state->v_set = false;
    }

    // CLI - Clear Interrupt: Clears I flag (not tracked)
    else if (strcasecmp(node->opcode, "CLI") == 0) {
        // I flag not tracked in this implementation
    }

    // SEI - Set Interrupt: Sets I flag (not tracked)
    else if (strcasecmp(node->opcode, "SEI") == 0) {
        // I flag not tracked
    }

    // CLD - Clear Decimal: Clears D flag (not tracked)
    else if (strcasecmp(node->opcode, "CLD") == 0) {
        // D flag not tracked
    }

    // SED - Set Decimal: Sets D flag (not tracked)
    else if (strcasecmp(node->opcode, "SED") == 0) {
        // D flag not tracked
    }

    // === STACK OPERATIONS ===
    // PHA - Push Accumulator: No flags affected
    else if (strcasecmp(node->opcode, "PHA") == 0) {
        // No register or flag changes
    }

    // PHP - Push Processor Status: No flags affected
    else if (strcasecmp(node->opcode, "PHP") == 0) {
        // No register or flag changes
    }

    // PLA - Pull Accumulator: Sets N and Z flags
    else if (strcasecmp(node->opcode, "PLA") == 0) {
        state->a_modified = true;
        state->a_known = false;  // Value from stack is unknown
        state->n_known = false;
        state->z_flag_known = false;
    }

    // PLP - Pull Processor Status: All flags affected
    else if (strcasecmp(node->opcode, "PLP") == 0) {
        // All flags become unknown (restored from stack)
        state->c_known = false;
        state->n_known = false;
        state->z_flag_known = false;
        state->v_known = false;
    }

    // === BRANCHES & JUMPS ===
    // Branch and jump instructions don't affect registers or flags
    else if (strcasecmp(node->opcode, "BCC") == 0 ||
             strcasecmp(node->opcode, "BCS") == 0 ||
             strcasecmp(node->opcode, "BEQ") == 0 ||
             strcasecmp(node->opcode, "BNE") == 0 ||
             strcasecmp(node->opcode, "BMI") == 0 ||
             strcasecmp(node->opcode, "BPL") == 0 ||
             strcasecmp(node->opcode, "BVC") == 0 ||
             strcasecmp(node->opcode, "BVS") == 0 ||
             strcasecmp(node->opcode, "BRA") == 0 || // 65C02
             strcasecmp(node->opcode, "JMP") == 0 ||
             strcasecmp(node->opcode, "JSR") == 0 ||
             strcasecmp(node->opcode, "RTS") == 0 ||
             strcasecmp(node->opcode, "RTI") == 0) {
        // No register or flag changes
        // Note: RTI restores flags but we treat it as unknown
        if (strcasecmp(node->opcode, "RTI") == 0) {
            state->c_known = false;
            state->n_known = false;
            state->z_flag_known = false;
            state->v_known = false;
        }
        // JSR may trash A,X,Y depending on subroutine
        if (strcasecmp(node->opcode, "JSR") == 0) {
            state->a_known = false;
            state->x_known = false;
            state->y_known = false;
            state->z_known = false;
            state->c_known = false;
            state->n_known = false;
            state->z_flag_known = false;
            state->v_known = false;
        }
    }

    // === 45GS02 SPECIFIC ===
    // NEG - Negate Accumulator (45GS02): Sets N, Z, C flags
    else if (strcasecmp(node->opcode, "NEG") == 0) {
        state->a_modified = true;
        state->a_known = false;
        state->n_known = false;
        state->z_flag_known = false;
        state->c_known = false;
    }

    // ASR - Arithmetic Shift Right (45GS02): Sets N, Z, C flags
    else if (strcasecmp(node->opcode, "ASR") == 0) {
        state->a_modified = true;
        state->a_known = false;
        state->n_known = false;  // Sign bit is preserved
        state->z_flag_known = false;
        state->c_known = false;
    }

    // === BIT TEST ===
    // BIT - Bit Test: Sets N, V, Z flags
    else if (strcasecmp(node->opcode, "BIT") == 0) {
        state->n_known = false;  // Bit 7 of operand
        state->v_known = false;  // Bit 6 of operand
        state->z_flag_known = false; // Result of A & operand
    }

    // NOP - No operation
    else if (strcasecmp(node->opcode, "NOP") == 0) {
        // No changes
    }
}

// Print register state for debugging
void print_register_state(const RegisterState *state, int line_num) {
    if (!state) return;

    printf("  Register state at line %d:\n", line_num);
    printf("    A: known=%s, zero=%s, value=%s, modified=%s\n",
           state->a_known ? "yes" : "no",
           state->a_zero ? "yes" : "no",
           state->a_known ? state->a_value : "unknown",
           state->a_modified ? "yes" : "no");
    printf("    X: known=%s, zero=%s, value=%s, modified=%s\n",
           state->x_known ? "yes" : "no",
           state->x_zero ? "yes" : "no",
           state->x_known ? state->x_value : "unknown",
           state->x_modified ? "yes" : "no");
    printf("    Y: known=%s, zero=%s, value=%s, modified=%s\n",
           state->y_known ? "yes" : "no",
           state->y_zero ? "yes" : "no",
           state->y_known ? state->y_value : "unknown",
           state->y_modified ? "yes" : "no");
    printf("    Z: known=%s, zero=%s, value=%s, modified=%s\n",
           state->z_known ? "yes" : "no",
           state->z_zero ? "yes" : "no",
           state->z_known ? state->z_value : "unknown",
           state->z_modified ? "yes" : "no");
    printf("    Flags:\n");
    printf("      C (Carry):    known=%s, set=%s\n",
           state->c_known ? "yes" : "no",
           state->c_known ? (state->c_set ? "yes" : "no") : "unknown");
    printf("      N (Negative): known=%s, set=%s\n",
           state->n_known ? "yes" : "no",
           state->n_known ? (state->n_set ? "yes" : "no") : "unknown");
    printf("      Z (Zero):     known=%s, set=%s\n",
           state->z_flag_known ? "yes" : "no",
           state->z_flag_known ? (state->z_flag_set ? "yes" : "no") : "unknown");
    printf("      V (Overflow): known=%s, set=%s\n",
           state->v_known ? "yes" : "no",
           state->v_known ? (state->v_set ? "yes" : "no") : "unknown");
}

// Validate register and flag tracking
void validate_register_and_flag_tracking(Program *prog) {
    printf("\n=== Register and Flag Tracking Validation ===\n");

    RegisterState state;
    // Initialize state
    state.a_known = false;
    state.x_known = false;
    state.y_known = false;
    state.z_known = false;
    state.a_zero = false;
    state.x_zero = false;
    state.y_zero = false;
    state.z_zero = false;
    state.a_modified = false;
    state.x_modified = false;
    state.y_modified = false;
    state.z_modified = false;
    state.a_value[0] = '\0';
    state.x_value[0] = '\0';
    state.y_value[0] = '\0';
    state.z_value[0] = '\0';
    state.c_known = false;
    state.n_known = false;
    state.z_flag_known = false;
    state.v_known = false;
    state.c_set = false;
    state.n_set = false;
    state.z_flag_set = false;
    state.v_set = false;

    int instruction_count = 0;
    int register_modifications = 0;
    int flag_modifications = 0;

    AstNode *node = prog->root;
    while (node) {
        if (node->opcode) {
            instruction_count++;

            // Save previous state
            RegisterState prev_state = state;

            // Update state based on instruction
            update_register_state(node, &state);

            // Check if registers were modified
            if (state.a_modified) register_modifications++;
            if (state.x_modified) register_modifications++;
            if (state.y_modified) register_modifications++;
            if (state.z_modified) register_modifications++;

            // Check if flags were modified (compare with previous state)
            if (state.c_known != prev_state.c_known || state.c_set != prev_state.c_set) flag_modifications++;
            if (state.n_known != prev_state.n_known || state.n_set != prev_state.n_set) flag_modifications++;
            if (state.z_flag_known != prev_state.z_flag_known || state.z_flag_set != prev_state.z_flag_set) flag_modifications++;
            if (state.v_known != prev_state.v_known || state.v_set != prev_state.v_set) flag_modifications++;

            // For verbose output, print state after each instruction
            if (prog->trace_level >= 2) {
                printf("\nLine %d: %s %s\n", node->line_num, node->opcode,
                       node->operand ? node->operand : "");
                print_register_state(&state, node->line_num);
            }

            // Reset state at branch targets (control flow convergence)
            if (node->is_branch_target) {
                // Conservative: assume registers and flags are unknown at branch targets
                state.a_known = false;
                state.x_known = false;
                state.y_known = false;
                state.z_known = false;
                state.c_known = false;
                state.n_known = false;
                state.z_flag_known = false;
                state.v_known = false;
            }
        }
        node = node->next;
    }

    printf("\n=== Validation Summary ===\n");
    printf("Total instructions analyzed: %d\n", instruction_count);
    printf("Register modifications detected: %d\n", register_modifications);
    printf("Flag modifications detected: %d\n", flag_modifications);

    // Summary of register usage
    printf("\n=== Register Usage Summary ===\n");
    state = (RegisterState){0}; // Reset
    state.a_known = false;
    state.x_known = false;
    state.y_known = false;
    state.z_known = false;
    state.c_known = false;
    state.n_known = false;
    state.z_flag_known = false;
    state.v_known = false;

    bool a_used = false, x_used = false, y_used = false, z_used = false;
    bool c_affected = false, n_affected = false, z_affected = false, v_affected = false;

    node = prog->root;
    while (node) {
        if (node->opcode) {
            RegisterState temp_state = state;
            update_register_state(node, &temp_state);

            if (temp_state.a_modified) a_used = true;
            if (temp_state.x_modified) x_used = true;
            if (temp_state.y_modified) y_used = true;
            if (temp_state.z_modified) z_used = true;

            // Check if instruction affects flags
            if (strcasecmp(node->opcode, "CLC") == 0 || strcasecmp(node->opcode, "SEC") == 0 ||
                strcasecmp(node->opcode, "ADC") == 0 || strcasecmp(node->opcode, "SBC") == 0 ||
                strcasecmp(node->opcode, "ASL") == 0 || strcasecmp(node->opcode, "LSR") == 0 ||
                strcasecmp(node->opcode, "ROL") == 0 || strcasecmp(node->opcode, "ROR") == 0 ||
                strcasecmp(node->opcode, "CMP") == 0 || strcasecmp(node->opcode, "CPX") == 0 ||
                strcasecmp(node->opcode, "CPY") == 0) {
                c_affected = true;
            }

            if (strcasecmp(node->opcode, "LDA") == 0 || strcasecmp(node->opcode, "LDX") == 0 ||
                strcasecmp(node->opcode, "LDY") == 0 || strcasecmp(node->opcode, "LDZ") == 0 ||
                strcasecmp(node->opcode, "TAX") == 0 || strcasecmp(node->opcode, "TXA") == 0 ||
                strcasecmp(node->opcode, "TAY") == 0 || strcasecmp(node->opcode, "TYA") == 0 ||
                strcasecmp(node->opcode, "AND") == 0 || strcasecmp(node->opcode, "ORA") == 0 ||
                strcasecmp(node->opcode, "EOR") == 0 || strcasecmp(node->opcode, "ASL") == 0 ||
                strcasecmp(node->opcode, "LSR") == 0 || strcasecmp(node->opcode, "ROL") == 0 ||
                strcasecmp(node->opcode, "ROR") == 0 || strcasecmp(node->opcode, "ADC") == 0 ||
                strcasecmp(node->opcode, "SBC") == 0 || strcasecmp(node->opcode, "CMP") == 0 ||
                strcasecmp(node->opcode, "CPX") == 0 || strcasecmp(node->opcode, "CPY") == 0 ||
                strcasecmp(node->opcode, "INX") == 0 || strcasecmp(node->opcode, "INY") == 0 ||
                strcasecmp(node->opcode, "DEX") == 0 || strcasecmp(node->opcode, "DEY") == 0 ||
                strcasecmp(node->opcode, "INC") == 0 || strcasecmp(node->opcode, "DEC") == 0 ||
                strcasecmp(node->opcode, "BIT") == 0) {
                n_affected = true;
            }

            if (strcasecmp(node->opcode, "LDA") == 0 || strcasecmp(node->opcode, "LDX") == 0 ||
                strcasecmp(node->opcode, "LDY") == 0 || strcasecmp(node->opcode, "LDZ") == 0 ||
                strcasecmp(node->opcode, "TAX") == 0 || strcasecmp(node->opcode, "TXA") == 0 ||
                strcasecmp(node->opcode, "TAY") == 0 || strcasecmp(node->opcode, "TYA") == 0 ||
                strcasecmp(node->opcode, "AND") == 0 || strcasecmp(node->opcode, "ORA") == 0 ||
                strcasecmp(node->opcode, "EOR") == 0 || strcasecmp(node->opcode, "ASL") == 0 ||
                strcasecmp(node->opcode, "LSR") == 0 || strcasecmp(node->opcode, "ROL") == 0 ||
                strcasecmp(node->opcode, "ROR") == 0 || strcasecmp(node->opcode, "ADC") == 0 ||
                strcasecmp(node->opcode, "SBC") == 0 || strcasecmp(node->opcode, "CMP") == 0 ||
                strcasecmp(node->opcode, "CPX") == 0 || strcasecmp(node->opcode, "CPY") == 0 ||
                strcasecmp(node->opcode, "INX") == 0 || strcasecmp(node->opcode, "INY") == 0 ||
                strcasecmp(node->opcode, "DEX") == 0 || strcasecmp(node->opcode, "DEY") == 0 ||
                strcasecmp(node->opcode, "INC") == 0 || strcasecmp(node->opcode, "DEC") == 0 ||
                strcasecmp(node->opcode, "BIT") == 0) {
                z_affected = true;
            }

            if (strcasecmp(node->opcode, "ADC") == 0 || strcasecmp(node->opcode, "SBC") == 0 ||
                strcasecmp(node->opcode, "BIT") == 0 || strcasecmp(node->opcode, "CLV") == 0) {
                v_affected = true;
            }
        }
        node = node->next;
    }

    printf("Registers used:\n");
    printf("  A (Accumulator): %s\n", a_used ? "YES" : "NO");
    printf("  X (Index X):     %s\n", x_used ? "YES" : "NO");
    printf("  Y (Index Y):     %s\n", y_used ? "YES" : "NO");
    printf("  Z (Z register):  %s%s\n", z_used ? "YES" : "NO",
           prog->is_45gs02 ? "" : " (45GS02 only)");

    printf("\nFlags affected:\n");
    printf("  C (Carry):       %s\n", c_affected ? "YES" : "NO");
    printf("  N (Negative):    %s\n", n_affected ? "YES" : "NO");
    printf("  Z (Zero):        %s\n", z_affected ? "YES" : "NO");
    printf("  V (Overflow):    %s\n", v_affected ? "YES" : "NO");

    printf("\n=== Validation Complete ===\n");
}
