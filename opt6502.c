#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <strings.h>  // For strcasecmp on POSIX systems

#define MAX_LINE 256
#define MAX_LINES 10000
#define MAX_LABELS 1000
#define MAX_REFS 100

typedef enum { OPT_SPEED, OPT_SIZE } OptMode;

typedef enum {
    CPU_6502,         // Original NMOS 6502
    CPU_65C02,        // CMOS 65C02 with additional instructions
    CPU_65816,        // 65816 (16-bit extension)
    CPU_45GS02        // 45GS02 (MEGA65) - backwards compatible but STZ stores Z register
} CpuType;

typedef enum {
    ASM_GENERIC,      // Generic - supports both ; and //
    ASM_CA65,         // ca65 - uses ;
    ASM_KICK,         // Kick Assembler - uses //
    ASM_ACME,         // ACME - uses ;
    ASM_DASM,         // DASM - uses ;
    ASM_TASS,         // Turbo Assembler - uses ;
    ASM_64TASS,       // 64tass - uses ;
    ASM_BUDDY,        // Buddy Assembler - uses //
    ASM_MERLIN,       // Merlin - uses ;
    ASM_LISA          // LISA - uses ;
} AsmType;

typedef enum {
    NODE_LABEL,
    NODE_OPCODE,
    NODE_BRANCH,
    NODE_JUMP,
    NODE_LOAD,
    NODE_STORE,
    NODE_CONSTANT,
    NODE_REGISTER,
    NODE_EXPRESSION,
    NODE_BLOCK,
    NODE_FUNCTION,
    NODE_ASM_LINE
} NodeType;

// Register tracking structure
typedef struct {
    bool a_known;      // Whether accumulator value is known
    bool x_known;      // Whether X register value is known
    bool y_known;      // Whether Y register value is known
    bool z_known;      // Whether Z register value is known (45GS02)
    bool a_zero;       // Whether accumulator is zero
    bool x_zero;       // Whether X register is zero
    bool y_zero;       // Whether Y register is zero
    bool z_zero;       // Whether Z register is zero (45GS02)
    char a_value[32];  // Known accumulator value (if any)
    char x_value[32];  // Known X register value (if any)
    char y_value[32];  // Known Y register value (if any)
    char z_value[32];  // Known Z register value (if any) (45GS02)
    bool a_modified;   // Whether accumulator was modified in current scope
    bool x_modified;   // Whether X register was modified in current scope
    bool y_modified;   // Whether Y register was modified in current scope
    bool z_modified;   // Whether Z register was modified in current scope (45GS02)

    // Processor flags
    bool c_known;      // Whether carry flag state is known
    bool n_known;      // Whether negative flag state is known
    bool z_flag_known; // Whether zero flag state is known
    bool v_known;      // Whether overflow flag state is known
    bool c_set;        // Carry flag value (if known)
    bool n_set;        // Negative flag value (if known)
    bool z_flag_set;   // Zero flag value (if known)
    bool v_set;        // Overflow flag value (if known)
} RegisterState;

// AST Node structure
typedef struct AstNode {
    NodeType type;
    int line_num;
    char* label;
    char* opcode;
    char* operand;
    char* comment;
    struct AstNode* next;
    struct AstNode* child;
    struct AstNode* sibling;
    bool is_dead;
    bool no_optimize;
    bool is_local_label;
    bool is_branch_target;
    int optimization_count;
    RegisterState reg_state;  // Register state at this node
} AstNode;

typedef struct {
    AsmType type;
    const char *name;
    const char *comment_char;
    bool supports_colon_labels;
    bool case_sensitive;
    const char *local_label_prefix;  // Prefix for local labels (@ ! . :)
    bool local_labels_numeric;        // Supports numeric local labels
} AsmConfig;

typedef struct {
    AstNode *root;
    AstNode *current_node;
    int count;
    OptMode mode;
    int optimizations;
    bool opt_enabled;
    AsmConfig config;
    CpuType cpu_type;               // Target CPU type
    bool allow_65c02;               // Allow 65C02 instructions
    bool allow_undocumented;        // Allow undocumented opcodes
    bool is_45gs02;                 // Special flag for 45GS02 (STZ behavior different)
    int trace_level;                // Level of optimization tracing (0=off, 1=basic, 2=expanded)
} Program;

// Forward declarations
AstNode* create_ast_node(NodeType type, int line_num);
void parse_line_ast(AstNode *node, const char *line, int line_num, AsmConfig *config);
void build_ast(Program *prog);
void mark_branch_targets_ast(Program *prog);
void analyze_call_flow_ast(Program *prog);
void optimize_program_ast(Program *prog);
void write_output_ast(Program *prog, const char *filename);
AsmConfig get_asm_config(AsmType type);
AsmType parse_asm_type(const char *type_str);
bool is_comment_start(const char *p, AsmConfig *config);
bool is_local_label(const char *label, AsmConfig *config);
void free_ast_node(AstNode *node);
void free_program_ast(Program *prog);
void update_register_state(AstNode *node, RegisterState *state);
void print_register_state(const RegisterState *state, int line_num);
void validate_register_and_flag_tracking(Program *prog);

// Get assembler configuration
AsmConfig get_asm_config(AsmType type) {
    AsmConfig configs[] = {
        {ASM_GENERIC, "Generic", ";", true, false, "@", false},
        {ASM_CA65, "ca65", ";", true, false, "@", false},
        {ASM_KICK, "Kick Assembler", "//", true, true, "!", true},
        {ASM_ACME, "ACME", ";", true, false, ".", false},
        {ASM_DASM, "DASM", ";", true, false, ".", true},
        {ASM_TASS, "Turbo Assembler", ";", true, false, "@", false},
        {ASM_64TASS, "64tass", ";", true, true, "", false},
        {ASM_BUDDY, "Buddy Assembler", "//", true, false, "@", false},
        {ASM_MERLIN, "Merlin", ";", false, false, ":", false},
        {ASM_LISA, "LISA", ";", true, false, ".", false}
    };
    
    size_t config_count = sizeof(configs) / sizeof(AsmConfig);
    for (size_t i = 0; i < config_count; i++) {
        if (configs[i].type == type) {
            return configs[i];
        }
    }
    
    return configs[0]; // Default to generic
}

// Parse assembler type from string
AsmType parse_asm_type(const char *type_str) {
    if (strcasecmp(type_str, "ca65") == 0) return ASM_CA65;
    if (strcasecmp(type_str, "kick") == 0) return ASM_KICK;
    if (strcasecmp(type_str, "kickass") == 0) return ASM_KICK;
    if (strcasecmp(type_str, "acme") == 0) return ASM_ACME;
    if (strcasecmp(type_str, "dasm") == 0) return ASM_DASM;
    if (strcasecmp(type_str, "tass") == 0) return ASM_TASS;
    if (strcasecmp(type_str, "64tass") == 0) return ASM_64TASS;
    if (strcasecmp(type_str, "buddy") == 0) return ASM_BUDDY;
    if (strcasecmp(type_str, "merlin") == 0) return ASM_MERLIN;
    if (strcasecmp(type_str, "lisa") == 0) return ASM_LISA;
    return ASM_GENERIC;
}

// Check if position is start of a comment
bool is_comment_start(const char *p, AsmConfig *config) {
    if (strcmp(config->comment_char, ";") == 0) {
        // Also support // for generic mode
        if (config->type == ASM_GENERIC && p[0] == '/' && p[1] == '/') {
            return true;
        }
        return *p == ';';
    } else if (strcmp(config->comment_char, "//") == 0) {
        return p[0] == '/' && p[1] == '/';
    }
    return false;
}

// Check if a label is a local label
bool is_local_label(const char *label, AsmConfig *config) {
    if (!label || !label[0]) return false;
    
    // Check for prefix-based local labels
    if (config->local_label_prefix && label[0] == config->local_label_prefix[0]) {
        return true;
    }
    
    // Check for numeric local labels (like 1, 2, 3 in DASM/Kick)
    if (config->local_labels_numeric) {
        bool all_digits = true;
        for (int i = 0; label[i]; i++) {
            if (!isdigit(label[i])) {
                all_digits = false;
                break;
            }
        }
        if (all_digits) return true;
    }
    
    return false;
}

// Create AST node
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

// Free AST node
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

// Free program AST
void free_program_ast(Program *prog) {
    if (!prog) return;
    
    free_ast_node(prog->root);
    free(prog);
}

// Initialize program structure
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

// Add line to AST
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

// Parse assembly line into AST components
void parse_line_ast(AstNode *node, const char *line, int line_num, AsmConfig *config) {
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

// Build AST from program lines
void build_ast(Program *prog) {
    // AST is built during parsing in add_line_ast
    // This function can be used for additional AST processing if needed
}

// Mark lines that are branch targets
void mark_branch_targets_ast(Program *prog) {
    AstNode *node = prog->root;
    while (node) {
        if (node->type == NODE_LABEL) {
            node->is_branch_target = true;
        }
        node = node->next;
    }
}

// Call flow analysis
void analyze_call_flow_ast(Program *prog) {
    mark_branch_targets_ast(prog);
}

// Update register state based on instruction
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

// Peephole optimization patterns
void optimize_peephole_ast(Program *prog) {
    AstNode *node = prog->root;
    while (node) {
        if (node->is_dead || node->no_optimize) {
            node = node->next;
            continue;
        }
        
        // LDA #value followed by STA then LDA #same_value
        if (node->opcode && strcmp(node->opcode, "LDA") == 0 && node->next) {
            AstNode *next1 = node->next;
            if (next1->opcode && strcmp(next1->opcode, "STA") == 0 && next1->next) {
                AstNode *next2 = next1->next;
                if (next2->opcode && strcmp(next2->opcode, "LDA") == 0) {
                    if (node->operand && next2->operand && strcmp(node->operand, next2->operand) == 0) {
                        next2->is_dead = true;
                        prog->optimizations++;
                    }
                }
            }
        }
        
        node = node->next;
    }
}

// Dead code elimination
void optimize_dead_code_ast(Program *prog) {
    AstNode *node = prog->root;
    while (node) {
        if (node->is_dead || node->no_optimize) {
            node = node->next;
            continue;
        }
        
        // Unconditional jump followed by unreachable code
        if ((node->opcode && (strcmp(node->opcode, "JMP") == 0 ||
                              strcmp(node->opcode, "RTS") == 0 ||
                              strcmp(node->opcode, "RTI") == 0)) &&
            node->next && !node->next->is_branch_target && !node->next->label) {
            
            AstNode *current = node->next;
            while (current && !current->is_branch_target && 
                   !current->label && !current->no_optimize && 
                   current->opcode) {
                current->is_dead = true;
                prog->optimizations++;
                current = current->next;
            }
        }
        
        node = node->next;
    }
}

// Jump optimization
void optimize_jumps_ast(Program *prog) {
    AstNode *node = prog->root;
    while (node) {
        if (node->is_dead || node->no_optimize) {
            node = node->next;
            continue;
        }
        
        // JMP to next line (remove)
        if (node->opcode && strcmp(node->opcode, "JMP") == 0 && node->next) {
            if (node->next->is_branch_target) {
                node->is_dead = true;
                prog->optimizations++;
            }
        }
        
        node = node->next;
    }
}

// Load/Store optimization
void optimize_load_store_ast(Program *prog) {
    AstNode *node = prog->root;
    while (node) {
        if (node->is_dead || node->no_optimize) {
            node = node->next;
            continue;
        }
        
        // LDA addr, STA addr2, LDA addr (remove third LDA)
        if (node->opcode && strcmp(node->opcode, "LDA") == 0 && node->next) {
            AstNode *next1 = node->next;
            if (next1->opcode && strcmp(next1->opcode, "STA") == 0 && next1->next) {
                AstNode *next2 = next1->next;
                if (next2->opcode && strcmp(next2->opcode, "LDA") == 0) {
                    if (node->operand && next2->operand && strcmp(node->operand, next2->operand) == 0) {
                        next2->is_dead = true;
                        prog->optimizations++;
                    }
                }
            }
        }
        
        node = node->next;
    }
}

// Register usage optimization
void optimize_register_usage_ast(Program *prog) {
    AstNode *node = prog->root;
    while (node) {
        if (node->is_dead || node->no_optimize) {
            node = node->next;
            continue;
        }
        
        // TAX followed by TXA (no operation if no X usage between)
        if (node->opcode && strcmp(node->opcode, "TAX") == 0 && node->next) {
            AstNode *next = node->next;
            if (next->opcode && strcmp(next->opcode, "TXA") == 0) {
                node->is_dead = true;
                next->is_dead = true;
                prog->optimizations++;
            }
        }
        
        node = node->next;
    }
}

// Constant propagation
void optimize_constant_propagation_ast(Program *prog) {
    char last_a_value[64] = "";
    bool a_known = false;
    
    AstNode *node = prog->root;
    while (node) {
        if (node->is_dead || node->is_branch_target || node->no_optimize) {
            a_known = false;
            node = node->next;
            continue;
        }
        
        // Track LDA immediate values
        if (node->opcode && strcmp(node->opcode, "LDA") == 0 &&
            node->operand && node->operand[0] == '#') {
            strncpy(last_a_value, node->operand, 63);
            a_known = true;
        }
        // If we see another LDA with same value, remove it
        else if (a_known && node->opcode && strcmp(node->opcode, "LDA") == 0 &&
                 node->operand && strcmp(node->operand, last_a_value) == 0) {
            node->is_dead = true;
            prog->optimizations++;
        }
        // Operations that modify A
        else if (node->opcode && (strcmp(node->opcode, "ADC") == 0 ||
                                  strcmp(node->opcode, "SBC") == 0 ||
                                  strcmp(node->opcode, "AND") == 0 ||
                                  strcmp(node->opcode, "ORA") == 0 ||
                                  strcmp(node->opcode, "EOR") == 0 ||
                                  strcmp(node->opcode, "LDA") == 0 ||
                                  strcmp(node->opcode, "PLA") == 0 ||
                                  strcmp(node->opcode, "TXA") == 0 ||
                                  strcmp(node->opcode, "TYA") == 0 ||
                                  strcmp(node->opcode, "ASL") == 0 ||
                                  strcmp(node->opcode, "LSR") == 0 ||
                                  strcmp(node->opcode, "ROL") == 0 ||
                                  strcmp(node->opcode, "ROR") == 0)) {
            a_known = false;
        }
        
        node = node->next;
    }
}

// 65C02-specific optimizations
void optimize_65c02_instructions_ast(Program *prog) {
    if (!prog->allow_65c02 || prog->is_45gs02) return;  // Don't apply to 45GS02!

    AstNode *node = prog->root;

    while (node) {
        if (node->is_dead || node->no_optimize) {
            node = node->next;
            continue;
        }

        // Pattern: LDA #$00 followed by one or more STA -> convert all STA to STZ
        // If A is not used after the STAs, also mark LDA #$00 as dead
        if (node->opcode && strcmp(node->opcode, "LDA") == 0 &&
            node->operand && (strcmp(node->operand, "#$00") == 0 || strcmp(node->operand, "#0") == 0)) {

            // First pass: scan forward to check if A is used and find STAs
            AstNode *current = node->next;
            bool found_sta = false;
            bool a_value_used = false;  // Track if accumulator value is used (not just modified)

            while (current && !current->is_branch_target) {
                if (current->is_dead || current->no_optimize) {
                    current = current->next;
                    continue;
                }

                if (current->opcode && strcmp(current->opcode, "STA") == 0) {
                    found_sta = true;
                    current = current->next;
                } else if (current->opcode && (
                    strcmp(current->opcode, "ADC") == 0 ||
                    strcmp(current->opcode, "SBC") == 0 ||
                    strcmp(current->opcode, "AND") == 0 ||
                    strcmp(current->opcode, "ORA") == 0 ||
                    strcmp(current->opcode, "EOR") == 0 ||
                    strcmp(current->opcode, "CMP") == 0 ||
                    strcmp(current->opcode, "BIT") == 0 ||
                    strcmp(current->opcode, "PHA") == 0 ||
                    strcmp(current->opcode, "TAX") == 0 ||
                    strcmp(current->opcode, "TAY") == 0)) {
                    // Instruction uses A value - can't remove LDA but can still convert STA to STZ
                    a_value_used = true;
                    break;
                } else if (current->opcode && (
                    strcmp(current->opcode, "LDA") == 0 ||
                    strcmp(current->opcode, "PLA") == 0 ||
                    strcmp(current->opcode, "TXA") == 0 ||
                    strcmp(current->opcode, "TYA") == 0)) {
                    // A is reloaded/modified - safe to remove original LDA
                    break;
                } else {
                    // Other instructions that don't use A - safe to continue
                    current = current->next;
                }
            }

            // Second pass: convert STAs to STZ if we found any
            if (found_sta) {
                current = node->next;
                while (current && !current->is_branch_target) {
                    if (current->is_dead || current->no_optimize) {
                        current = current->next;
                        continue;
                    }

                    if (current->opcode && strcmp(current->opcode, "STA") == 0) {
                        // Convert STA to STZ
                        current->opcode = realloc(current->opcode, 4);
                        if (current->opcode) {
                            strcpy(current->opcode, "STZ");
                        }
                        prog->optimizations++;
                        current = current->next;
                    } else if (current->opcode && (
                        strcmp(current->opcode, "LDA") == 0 ||
                        strcmp(current->opcode, "PLA") == 0 ||
                        strcmp(current->opcode, "TXA") == 0 ||
                        strcmp(current->opcode, "TYA") == 0 ||
                        strcmp(current->opcode, "ADC") == 0 ||
                        strcmp(current->opcode, "SBC") == 0 ||
                        strcmp(current->opcode, "AND") == 0 ||
                        strcmp(current->opcode, "ORA") == 0 ||
                        strcmp(current->opcode, "EOR") == 0 ||
                        strcmp(current->opcode, "CMP") == 0 ||
                        strcmp(current->opcode, "BIT") == 0 ||
                        strcmp(current->opcode, "PHA") == 0 ||
                        strcmp(current->opcode, "TAX") == 0 ||
                        strcmp(current->opcode, "TAY") == 0)) {
                        // A is modified or used - stop scanning
                        break;
                    } else {
                        current = current->next;
                    }
                }

                // Only mark LDA #$00 as dead if A value is not used later
                if (!a_value_used) {
                    node->is_dead = true;
                    if (prog->trace_level > 1) {
                        printf("DEBUG 65c02: Marked LDA #0 at line %d as dead, converted STAs to STZ\n", node->line_num);
                    }
                } else {
                    if (prog->trace_level > 1) {
                        printf("DEBUG 65c02: Kept LDA #0 at line %d (A used later), but converted STAs to STZ\n", node->line_num);
                    }
                }
            }
        }

        node = node->next;
    }
}

// 45GS02-specific optimizations (MEGA65)
void optimize_45gs02_instructions_ast(Program *prog) {
    if (!prog->is_45gs02) return;
    
    AstNode *node = prog->root;
    while (node) {
        if (node->is_dead || node->no_optimize) {
            node = node->next;
            continue;
        }
        
        // ===== Z Register Optimizations =====
        
        // Pattern: Multiple stores of SAME VALUE using LDA #val, STA -> LDZ #val, then STZ
        // Generalized for any immediate value, not just zero
        // Look for: LDA #val, STA addr1, LDA #val, STA addr2
        if (node->opcode && strcmp(node->opcode, "LDA") == 0 && node->operand && node->operand[0] == '#' &&
            node->next && node->next->opcode && strcmp(node->next->opcode, "STA") == 0 &&
            node->next->next && node->next->next->opcode && strcmp(node->next->next->opcode, "LDA") == 0 &&
            node->next->next->operand && node->next->next->operand[0] == '#' &&
            node->next->next->next && node->next->next->next->opcode && strcmp(node->next->next->next->opcode, "STA") == 0 &&
            !node->next->no_optimize && !node->next->next->no_optimize && !node->next->next->next->no_optimize &&
            strcmp(node->operand, node->next->next->operand) == 0) {  // Same value!
            
            // Convert to: LDZ #val, STZ addr1, STZ addr2
            node->opcode = realloc(node->opcode, 4);
            if (node->opcode) {
                strcpy(node->opcode, "LDZ");
            }
            
            node->next->opcode = realloc(node->next->opcode, 4);
            if (node->next->opcode) {
                strcpy(node->next->opcode, "STZ");
            }
            
            node->next->next->is_dead = true;  // Remove second LDA
            
            node->next->next->next->opcode = realloc(node->next->next->next->opcode, 4);
            if (node->next->next->next->opcode) {
                strcpy(node->next->next->next->opcode, "STZ");
            }
            
            prog->optimizations++;
        }
        
        // Also handle cases where we already have LDZ followed by stores
        // Pattern: LDZ #val, STA addr1, STA addr2, ... -> LDZ #val, STZ addr1, STZ addr2, ...
        // Also handles: LDZ #val, ..., LDA #val, STA addr -> LDZ #val, ..., STZ addr (with LDA marked dead)
        if (node->opcode && strcmp(node->opcode, "LDZ") == 0 && node->operand && node->operand[0] == '#') {
            // Look ahead for STA instructions that could become STZ
            AstNode *current = node->next;
            while (current) {
                if (current->is_dead || current->no_optimize) {
                    current = current->next;
                    continue;
                }

                if (current->opcode && strcmp(current->opcode, "STA") == 0 &&
                    !current->is_branch_target) {
                    // Convert STA to STZ (stores Z register value)
                    current->opcode = realloc(current->opcode, 4);
                    if (current->opcode) {
                        strcpy(current->opcode, "STZ");
                    }
                    prog->optimizations++;
                    current = current->next;
                } else if (current->opcode && strcmp(current->opcode, "LDA") == 0 &&
                           current->operand && node->operand &&
                           strcmp(current->operand, node->operand) == 0 &&
                           current->next && current->next->opcode &&
                           strcmp(current->next->opcode, "STA") == 0) {
                    // LDA with same value followed by STA - mark LDA dead and convert STA to STZ
                    current->is_dead = true;
                    AstNode *sta_node = current->next;
                    sta_node->opcode = realloc(sta_node->opcode, 4);
                    if (sta_node->opcode) {
                        strcpy(sta_node->opcode, "STZ");
                    }
                    prog->optimizations++;
                    current = sta_node->next;
                } else if (current->opcode && (strcmp(current->opcode, "LDA") == 0 ||
                                              strcmp(current->opcode, "LDZ") == 0 ||
                                              strcmp(current->opcode, "TAX") == 0 ||
                                              strcmp(current->opcode, "TAY") == 0)) {
                    // Something that would change what we're storing (with different value)
                    break;
                } else {
                    current = current->next;
                }
            }
        }
        
        // 45GS02 has NEG instruction (negate accumulator)
        // Pattern: EOR #$FF, SEC, ADC #$00 -> NEG
        if (node->opcode && strcmp(node->opcode, "EOR") == 0 &&
            node->operand && strcmp(node->operand, "#$FF") == 0 &&
            node->next && node->next->opcode && strcmp(node->next->opcode, "SEC") == 0 &&
            node->next->next && node->next->next->opcode && strcmp(node->next->next->opcode, "ADC") == 0 &&
            node->next->next->operand && strcmp(node->next->next->operand, "#$00") == 0 &&
            !node->next->no_optimize && !node->next->next->no_optimize &&
            !node->next->next->is_branch_target) {
            
            // Replace with NEG
            node->opcode = realloc(node->opcode, 4);
            if (node->opcode) {
                strcpy(node->opcode, "NEG");
            }
            node->operand = NULL;
            node->next->is_dead = true;
            node->next->next->is_dead = true;
            prog->optimizations++;
        }
        
        // ===== ASR (Arithmetic Shift Right) =====
        
        // 45GS02 has ASR instruction (arithmetic shift right, preserves sign)
        // Pattern: CMP #$80, ROR -> ASR
        if (node->opcode && strcmp(node->opcode, "CMP") == 0 &&
            node->operand && strcmp(node->operand, "#$80") == 0 &&
            node->next && node->next->opcode && strcmp(node->next->opcode, "ROR") == 0 &&
            !node->next->no_optimize &&
            !node->next->is_branch_target) {
            
            // Can use ASR for signed right shift
            node->opcode = realloc(node->opcode, 4);
            if (node->opcode) {
                strcpy(node->opcode, "ASR");
            }
            node->operand = NULL;
            node->next->is_dead = true;
            prog->optimizations++;
        }
        
        node = node->next;
    }
}

// Inline subroutines that are only called once
void optimize_inline_subroutines_ast(Program *prog) {
    // This would require more complex AST traversal to identify subroutines
    // For now, we'll leave this as a placeholder
}

// Main optimization routine
void optimize_program_ast(Program *prog) {
    int prev_opts;
    int pass = 0;
    
    // First perform inlining (only once, at the beginning)
    printf("Performing subroutine inlining...\n");
    analyze_call_flow_ast(prog);
    optimize_inline_subroutines_ast(prog);
    
    // Multiple passes until no more optimizations found
    do {
        prev_opts = prog->optimizations;
        
        analyze_call_flow_ast(prog);
        
        // Basic optimizations
        optimize_peephole_ast(prog);
        optimize_load_store_ast(prog);
        optimize_register_usage_ast(prog);
        optimize_constant_propagation_ast(prog);
        
        // Arithmetic and logic
        optimize_65c02_instructions_ast(prog);
        optimize_45gs02_instructions_ast(prog);
        
        // Control flow
        optimize_jumps_ast(prog);
        
        // Must be last
        optimize_dead_code_ast(prog);
        
        pass++;
    } while (prog->optimizations > prev_opts && pass < 10);

    printf("Optimization completed in %d passes\n", pass);

    // Validate register and flag tracking
    validate_register_and_flag_tracking(prog);
}

// Write optimized output from AST
void write_output_ast(Program *prog, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot write to %s\n", filename);
        return;
    }
    
    // Use the appropriate comment style for the assembler
    const char *cmt = prog->config.comment_char;
    
    fprintf(fp, "%s Optimized for %s\n", cmt,
            prog->mode == OPT_SPEED ? "speed" : "size");
    fprintf(fp, "%s Assembler: %s\n", cmt, prog->config.name);
    fprintf(fp, "%s Target CPU: %s\n", cmt, 
            prog->cpu_type == CPU_6502 ? "6502" : 
            prog->cpu_type == CPU_65C02 ? "65C02" :
            prog->cpu_type == CPU_65816 ? "65816" : "45GS02");
    fprintf(fp, "%s Total optimizations: %d\n\n", cmt, prog->optimizations);
    
    if (prog->trace_level > 0) {
        fprintf(fp, "%s Optimization trace enabled (Level %d)\n", cmt, prog->trace_level);
        fprintf(fp, "%s Lines marked with %s OPT: show applied optimizations\n\n", 
                cmt, cmt);
    }
    
    AstNode *node = prog->root;
    while (node) {
        if (!node->is_dead) {
            // Reconstruct line from AST node
            bool has_opcode = node->opcode && node->opcode[0] != '\0';

            if (node->label) {
                fprintf(fp, "%s", node->label);
                if (prog->config.supports_colon_labels) {
                    fprintf(fp, ":");
                }
                if (has_opcode) {
                    // Label with opcode on same line
                    fprintf(fp, "\t");
                } else {
                    // Label only, newline
                    fprintf(fp, "\n");
                }
            } else if (has_opcode) {
                // No label, add indentation for opcode
                fprintf(fp, "    ");
            }

            if (has_opcode) {
                fprintf(fp, "%s", node->opcode);
                if (node->operand && node->operand[0] != '\0') {
                    fprintf(fp, " %s", node->operand);
                }
                // Add comment if present
                if (node->comment && node->comment[0] != '\0') {
                    fprintf(fp, "\t%s", node->comment);
                }
                fprintf(fp, "\n");
            }
        } else if (prog->trace_level > 0) {
            fprintf(fp, "%s OPT: Removed - %s\n", cmt, node->label ? node->label : "unknown");
        }
        node = node->next;
    }
    
    fclose(fp);
}

// Main program
int main(int argc, char *argv[]) {
    OptMode mode = OPT_SPEED;
    AsmType asm_type = ASM_GENERIC;
    CpuType cpu_type = CPU_6502;
    const char *input_file = NULL;
    const char *output_file = "output.asm";
    int trace_level_arg = 0;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-speed") == 0) {
            mode = OPT_SPEED;
        } else if (strcmp(argv[i], "-size") == 0) {
            mode = OPT_SIZE;
        } else if (strcmp(argv[i], "-trace") == 0) {
            if (i + 1 < argc && isdigit(argv[i+1][0])) {
                trace_level_arg = atoi(argv[++i]);
            } else {
                trace_level_arg = 1; // Default to level 1 if no level specified
            }
        } else if (strcmp(argv[i], "-asm") == 0 && i + 1 < argc) {
            asm_type = parse_asm_type(argv[++i]);
        } else if (strcmp(argv[i], "-cpu") == 0 && i + 1 < argc) {
            i++;
            if (strcasecmp(argv[i], "6502") == 0) {
                cpu_type = CPU_6502;
            } else if (strcasecmp(argv[i], "65c02") == 0) {
                cpu_type = CPU_65C02;
            } else if (strcasecmp(argv[i], "65816") == 0) {
                cpu_type = CPU_65816;
            } else if (strcasecmp(argv[i], "45gs02") == 0) {
                cpu_type = CPU_45GS02;
            }
        } else if (!input_file) {
            input_file = argv[i];
        } else {
            output_file = argv[i];
        }
    }
    
    if (!input_file || argc < 3) {
        printf("Usage: %s [-speed|-size] [-asm <type>] [-cpu <type>] [-trace <level>] input.asm [output.asm]\n", argv[0]);
        printf("  -speed: Optimize for execution speed\n");
        printf("  -size:  Optimize for code size\n");
        printf("  -asm:   Assembler type (default: generic)\n");
        printf("  -cpu:   Target CPU (6502, 65c02, 65816, 45gs02)\n");
        printf("  -trace: Generate optimization trace comments in output (level 1 = basic, level 2 = expanded)\n");
        printf("\nSupported assemblers:\n");
        printf("  generic   - Generic (supports both ; and // comments)\n");
        printf("  ca65      - ca65 (cc65 assembler)\n");
        printf("  kick      - Kick Assembler\n");
        printf("  acme      - ACME Crossassembler\n");
        printf("  dasm      - DASM\n");
        printf("  tass      - Turbo Assembler\n");
        printf("  64tass    - 64tass\n");
        printf("  buddy     - Buddy Assembler\n");
        printf("  merlin    - Merlin\n");
        printf("  lisa      - LISA\n");
        printf("\nCPU types:\n");
        printf("  6502      - Original NMOS 6502\n");
        printf("  65c02     - CMOS 65C02 (enables STZ, BRA, etc.)\n");
        printf("  65816     - 65816 (16-bit extensions)\n");
        printf("  45gs02    - 45GS02 (MEGA65 CPU - NOTE: STZ stores Z register!)\n");
        printf("\nOptimizer directives (place in assembly as comments):\n");
        printf("  <comment>#NOOPT - Disable optimizations from this point\n");
        printf("  <comment>#OPT   - Re-enable optimizations from this point\n");
        printf("  (where <comment> is ; or // depending on assembler)\n");
        printf("\nIMPORTANT: 45GS02 WARNING\n");
        printf("  The 45GS02 (MEGA65) STZ instruction stores the Z REGISTER, not zero!\n");
        printf("  The optimizer will NOT convert LDA #0, STA to STZ on 45GS02.\n");
        printf("  Use LDZ #0, STZ if you want to store zero on 45GS02.\n");
        return 1;
    }
    
    // Read input file
    FILE *fp = fopen(input_file, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open %s\n", input_file);
        return 1;
    }
    
    Program *prog = create_program(mode, asm_type);
    prog->cpu_type = cpu_type;
    prog->allow_65c02 = (cpu_type == CPU_65C02 || cpu_type == CPU_65816);
    prog->is_45gs02 = (cpu_type == CPU_45GS02);
    prog->trace_level = trace_level_arg;
    
    // 45GS02 is backwards compatible with 65C02 but has different STZ behavior
    if (prog->is_45gs02) {
        prog->allow_65c02 = true;  // Can use most 65C02 instructions
    }
    
    char line[MAX_LINE];
    int current_line_num = 0; 
    
    printf("Assembler: %s (comments: %s)\n", prog->config.name, prog->config.comment_char);
    printf("Target CPU: %s", cpu_type == CPU_6502 ? "6502" : 
                             cpu_type == CPU_65C02 ? "65C02" :
                             cpu_type == CPU_65816 ? "65816" : "45GS02 (MEGA65)");
    
    if (prog->is_45gs02) {
        printf(" ** WARNING: STZ stores Z register, not zero! **");
    }
    printf("\n");
    
    if (prog->trace_level > 0) {
        printf("Optimization tracing: ENABLED (Level %d)\n", prog->trace_level);
    }
    
    if (prog->config.local_label_prefix && prog->config.local_label_prefix[0]) {
        printf("Local labels: %s prefix", prog->config.local_label_prefix);
        if (prog->config.local_labels_numeric) {
            printf(" (also numeric)\n");
        } else {
            printf("\n");
        }
    }
    
    while (fgets(line, MAX_LINE, fp)) {
        // Remove newline
        line[strcspn(line, "\r\n")] = '\0';
        add_line_ast(prog, line, current_line_num++);
    }
    fclose(fp);
    
    printf("Read %d lines from %s\n", prog->count, input_file);
    printf("Optimizing for %s...\n", mode == OPT_SPEED ? "speed" : "size");
    
    if (prog->is_45gs02) {
        printf("\n** 45GS02 Mode: LDA #0, STA will NOT be converted to STZ **\n");
        printf("** Use LDZ #0, STZ if you want to store zero **\n");
    }
    
    printf("\nOptimizer directives recognized:\n");
    printf("  %s#NOOPT  - Disable optimizations from this point\n", prog->config.comment_char);
    printf("  %s#OPT    - Re-enable optimizations from this point\n\n", prog->config.comment_char);
    
    // Perform optimizations
    optimize_program_ast(prog);
    
    printf("\n=== Optimization Summary ===\n");
    printf("Applied %d optimizations\n", prog->optimizations);
    
    // Write output
    write_output_ast(prog, output_file);
    printf("Wrote optimized code to %s\n", output_file);
    
    if (prog->trace_level > 0) { // Check trace_level for general trace messages
        printf("Optimization trace comments included in output (Level %d)\n", prog->trace_level);
    }
    
    // Statistics
    int lines_removed = 0;
    AstNode *node = prog->root;
    while (node) {
        if (node->is_dead) lines_removed++;
        node = node->next;
    }
    printf("Removed %d dead code lines\n", lines_removed);
    printf("Final line count: %d (%.1f%% reduction)\n", 
           prog->count - lines_removed,
           lines_removed > 0 ? (100.0 * lines_removed / prog->count) : 0.0);
    
    if (prog->is_45gs02) {
        printf("\n** Remember: On 45GS02, STZ stores the Z register! **\n");
    }
    
    free_program_ast(prog);
    return 0;
}
