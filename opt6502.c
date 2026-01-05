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
    
    bool accumulator_is_zero = false;
    AstNode *node = prog->root;
    
    while (node) {
        if (node->is_dead || node->no_optimize) {
            accumulator_is_zero = false; 
            node = node->next;
            continue;
        }
        
        // Handle instructions that set the accumulator to zero
        if (node->opcode && strcmp(node->opcode, "LDA") == 0 &&
            node->operand && (strcmp(node->operand, "#$00") == 0 ||
                              strcmp(node->operand, "#0") == 0)) {
            accumulator_is_zero = true;
            if (prog->trace_level > 1) {
                printf("DEBUG 65c02: LDA #0 at line %d, Accumulator is now zero.\n", node->line_num);
            }
        } 
        // Handle instructions that store a zero from the accumulator
        else if (accumulator_is_zero && node->opcode && strcmp(node->opcode, "STA") == 0 &&
                 !node->is_branch_target) {
            if (prog->trace_level > 1) {
                printf("DEBUG 65c02: Found STZ opportunity at line %d (STA after LDA #0), opts before: %d\n", node->line_num, prog->optimizations);
            }
            // Convert STA to STZ
            node->opcode = realloc(node->opcode, 4);
            if (node->opcode) {
                strcpy(node->opcode, "STZ");
            }
            prog->optimizations++;
            if (prog->trace_level > 1) {
                printf("DEBUG 65c02: Applied STZ optimization, opts after: %d\n", prog->optimizations);
            }
        }
        // Handle instructions that change the accumulator, invalidating accumulator_is_zero state
        else if (node->opcode && (strcmp(node->opcode, "LDA") == 0 ||
                                  strcmp(node->opcode, "ADC") == 0 ||
                                  strcmp(node->opcode, "SBC") == 0 ||
                                  strcmp(node->opcode, "AND") == 0 ||
                                  strcmp(node->opcode, "ORA") == 0 ||
                                  strcmp(node->opcode, "EOR") == 0 ||
                                  strcmp(node->opcode, "PLA") == 0 ||
                                  strcmp(node->opcode, "TXA") == 0 || // X to A
                                  strcmp(node->opcode, "TYA") == 0 || // Y to A
                                  strcmp(node->opcode, "ASL") == 0 ||
                                  strcmp(node->opcode, "LSR") == 0 ||
                                  strcmp(node->opcode, "ROL") == 0 ||
                                  strcmp(node->opcode, "ROR") == 0 ||
                                  strcmp(node->opcode, "INC") == 0 || // Affects A if operand is A
                                  strcmp(node->opcode, "DEC") == 0))   // Affects A if operand is A
        {
            if (node->operand && (node->operand[0] == 'A' || node->operand[0] == '\0' || 
                (strcmp(node->opcode, "LDA") == 0 && node->operand[0] != '#'))) // Non-immediate LDA
            {
                accumulator_is_zero = false;
                if (prog->trace_level > 1) {
                    printf("DEBUG 65c02: Accumulator modified at line %d, state reset.\n", node->line_num);
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
        // Pattern: LDZ #val, STA addr1, STA addr2 -> LDZ #val, STZ addr1, STZ addr2
        if (node->opcode && strcmp(node->opcode, "LDZ") == 0 && node->operand && node->operand[0] == '#') {
            // Look ahead for STA instructions that could become STZ
            AstNode *current = node->next;
            while (current && current->next && current->next != node->next) {
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
                } else if (current->opcode && (strcmp(current->opcode, "LDA") == 0 ||
                                              strcmp(current->opcode, "LDZ") == 0 ||
                                              strcmp(current->opcode, "TAX") == 0 ||
                                              strcmp(current->opcode, "TAY") == 0)) {
                    // Something that would change what we're storing
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
            if (node->label) {
                fprintf(fp, "%s", node->label);
                if (prog->config.supports_colon_labels && !node->is_local_label) {
                    fprintf(fp, ":");
                }
                fprintf(fp, "\t");
            }
            
            if (node->opcode) {
                fprintf(fp, "%s", node->opcode);
                if (node->operand) {
                    fprintf(fp, " %s", node->operand);
                }
                fprintf(fp, "\n");
            } else if (node->label) {
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
