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
    char line[MAX_LINE];
    char opcode[16];
    char operand[64];
    char label[64];
    bool is_label;
    bool is_local_label;
    bool is_branch_target;
    bool is_dead;
    bool no_optimize;
    int line_num;
    char parent_label[64];  // For local labels, track their parent
} AsmLine;

typedef struct {
    char name[64];
    int line_num;
    int ref_count;
    int ref_lines[MAX_REFS];
    bool is_subroutine;
    bool is_local;
    char parent_label[64];  // For local labels
    int sub_start;
    int sub_end;
} Label;

typedef struct {
    AsmLine *lines;
    int count;
    int capacity;
    Label labels[MAX_LABELS];
    int label_count;
    OptMode mode;
    int optimizations;
    bool opt_enabled;
    AsmConfig config;
    char current_parent_label[64];  // Track current scope for local labels
    CpuType cpu_type;               // Target CPU type
    bool allow_65c02;               // Allow 65C02 instructions
    bool allow_undocumented;        // Allow undocumented opcodes
    bool is_45gs02;                 // Special flag for 45GS02 (STZ behavior different)
    int trace_level;                // Level of optimization tracing (0=off, 1=basic, 2=expanded)
} Program;

// Forward declarations
void parse_line(AsmLine *asmline, const char *line, int line_num, AsmConfig *config, const char *parent_label);
void build_label_table(Program *prog);
void mark_branch_targets(Program *prog);
void analyze_call_flow(Program *prog);
void optimize_program(Program *prog);
void write_output(Program *prog, const char *filename);
AsmConfig get_asm_config(AsmType type);
AsmType parse_asm_type(const char *type_str);
bool is_comment_start(const char *p, AsmConfig *config);
bool is_local_label(const char *label, AsmConfig *config);
void reconstruct_line(AsmLine *asmline, AsmConfig *config);

// Optimization passes
void optimize_peephole(Program *prog);
void optimize_dead_code(Program *prog);
void optimize_jumps(Program *prog);
void optimize_load_store(Program *prog);
void optimize_register_usage(Program *prog);
void optimize_branches(Program *prog);
void optimize_constant_propagation(Program *prog);
void optimize_inline_subroutines(Program *prog);
void optimize_strength_reduction(Program *prog);
void optimize_common_subexpressions(Program *prog);
void optimize_addressing_modes(Program *prog);
void optimize_zero_page(Program *prog);
void optimize_branch_chaining(Program *prog);
void optimize_flag_usage(Program *prog);
void optimize_loop_invariants(Program *prog);
void optimize_bit_operations(Program *prog);
void optimize_arithmetic(Program *prog);
void optimize_tail_calls(Program *prog);
void optimize_loop_unrolling(Program *prog);
void optimize_stack_operations(Program *prog);
void optimize_constant_folding(Program *prog);
void optimize_boolean_logic(Program *prog);
void optimize_65c02_instructions(Program *prog);
void optimize_45gs02_instructions(Program *prog);

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

// Initialize program structure
Program* create_program(OptMode mode, AsmType asm_type) {
    Program *prog = malloc(sizeof(Program));
    prog->capacity = 1000;
    prog->lines = malloc(prog->capacity * sizeof(AsmLine));
    prog->count = 0;
    prog->label_count = 0;
    prog->mode = mode;
    prog->optimizations = 0;
    prog->opt_enabled = true;
    prog->config = get_asm_config(asm_type);
    prog->current_parent_label[0] = ' ';
    prog->cpu_type = CPU_6502;
    prog->allow_65c02 = false;
    prog->allow_undocumented = false;
    prog->is_45gs02 = false;
    prog->trace_level = 0;
    return prog;
}

void free_program(Program *prog) {
    free(prog->lines);
    free(prog);
}

// Add line to program
void add_line(Program *prog, const char *line, int line_num) {
    if (prog->count >= prog->capacity) {
        prog->capacity *= 2;
        prog->lines = realloc(prog->lines, prog->capacity * sizeof(AsmLine));
    }
    
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
    
    parse_line(&prog->lines[prog->count], line, line_num, &prog->config, prog->current_parent_label);
    prog->lines[prog->count].no_optimize = !prog->opt_enabled;
    
    // Update parent label tracking
    if (prog->lines[prog->count].is_label && !prog->lines[prog->count].is_local_label) {
        snprintf(prog->current_parent_label, sizeof(prog->current_parent_label), "%s", prog->lines[prog->count].label);
    }
    
    prog->count++;
}

// Parse assembly line into components
void parse_line(AsmLine *asmline, const char *line, int line_num, AsmConfig *config, const char *parent_label) {
    strncpy(asmline->line, line, MAX_LINE - 1);
    asmline->line[MAX_LINE - 1] = ' ';
    asmline->is_dead = false;
    asmline->is_branch_target = false;
    asmline->no_optimize = false;
    asmline->is_local_label = false;
    asmline->line_num = line_num;
    asmline->label[0] = ' ';
    asmline->opcode[0] = ' ';
    asmline->operand[0] = ' ';
    asmline->parent_label[0] = ' ';
    
    if (parent_label) {
        strncpy(asmline->parent_label, parent_label, 63);
    }
    
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
            asmline->label[i++] = *p++;
        }
        asmline->label[i] = ' ';
        
        // Check if this is a local label
        if (is_local_label(asmline->label, config)) {
            asmline->is_local_label = true;
        }
        
        // Check if this is actually a label
        if (config->supports_colon_labels && *p == ':') {
            // Definitely a label
            asmline->is_label = true;
            p++; // Skip colon
        } else if (i > 0 && asmline->label[0]) {
            // Might be a label (for Merlin style without colons)
            // We'll assume it is if followed by whitespace or end of line
            asmline->is_label = true;
        }
        
        while (*p && isspace(*p)) p++;
    } else {
        asmline->is_label = false;
    }
    
    // Skip comments
    if (is_comment_start(p, config) || *p == ' ') return;
    
    // Parse opcode
    int i = 0;
    while (*p && !isspace(*p) && !is_comment_start(p, config) && i < 15) {
        asmline->opcode[i++] = config->case_sensitive ? *p : toupper(*p);
        p++;
    }
    asmline->opcode[i] = ' ';
    
    while (*p && isspace(*p)) p++;
    
    // Parse operand
    i = 0;
    while (*p && !is_comment_start(p, config) && i < 63) {
        asmline->operand[i++] = *p++;
    }
    asmline->operand[i] = ' ';
    // Trim trailing whitespace
    while (i > 0 && isspace(asmline->operand[i-1])) {
        asmline->operand[--i] = ' ';
    }
}

// Reconstruct the full line string from parsed components
void reconstruct_line(AsmLine *asmline, AsmConfig *config) {
    char buffer[MAX_LINE];
    int offset = 0;

    // Add label if present
    if (asmline->label[0]) {
        offset += snprintf(buffer + offset, MAX_LINE - offset, "%s", asmline->label);
        if (config->supports_colon_labels && !asmline->is_local_label) {
            offset += snprintf(buffer + offset, MAX_LINE - offset, ":");
        }
        // Add a tab or space after label
        offset += snprintf(buffer + offset, MAX_LINE - offset, "\t");
    }

    // Add opcode
    if (asmline->opcode[0]) {
        // Ensure at least one tab/space if no label but opcode exists
        if (offset == 0) {
            offset += snprintf(buffer + offset, MAX_LINE - offset, "\t");
        }
        offset += snprintf(buffer + offset, MAX_LINE - offset, "%s", asmline->opcode);
        // Add space after opcode if operand exists
        if (asmline->operand[0]) {
            offset += snprintf(buffer + offset, MAX_LINE - offset, " ");
        }
    }

    // Add operand
    if (asmline->operand[0]) {
        offset += snprintf(buffer + offset, MAX_LINE - offset, "%s", asmline->operand);
    }
    
    // Copy reconstructed line back, ensuring it's not empty for dead lines to allow trace comments.
    // If a line becomes empty due to optimization, it will be marked dead and handled by write_output.
    if (offset == 0 && asmline->line[0] != ' ') {
        // If the line was originally not empty but now is, keep it as empty string.
        // This handles cases where comments might have been on the same line as code,
        // and the code was removed. We don't want to lose the original comment if tracing is off.
        asmline->line[0] = ' ';
    } else {
        strncpy(asmline->line, buffer, MAX_LINE - 1);
        asmline->line[MAX_LINE - 1] = ' ';
    }
}



// Build label reference table
void build_label_table(Program *prog) {
    prog->label_count = 0;
    
    // First pass: collect all labels
    for (int i = 0; i < prog->count; i++) {
        if (prog->lines[i].is_label && prog->lines[i].label[0]) {
            Label *lbl = &prog->labels[prog->label_count];
            snprintf(lbl->name, sizeof(lbl->name), "%s", prog->lines[i].label);
            lbl->line_num = i;
            lbl->ref_count = 0;
            lbl->is_subroutine = false;
            lbl->is_local = prog->lines[i].is_local_label;
            lbl->sub_start = i;
            lbl->sub_end = -1;
            
            // Store parent label for local labels
            if (lbl->is_local) {
                snprintf(lbl->parent_label, sizeof(lbl->parent_label), "%s", prog->lines[i].parent_label);
            } else {
                lbl->parent_label[0] = ' ';
            }
            
            prog->label_count++;
        }
    }
    
    // Second pass: find references and identify subroutines
    for (int i = 0; i < prog->count; i++) {
        if (prog->lines[i].operand[0]) {
            for (int j = 0; j < prog->label_count; j++) {
                // For local labels, only match if we're in the same scope
                bool match = false;
                
                if (prog->labels[j].is_local) {
                    // Local label - must match parent scope
                    if (strcmp(prog->lines[i].parent_label, prog->labels[j].parent_label) == 0 &&
                        strstr(prog->lines[i].operand, prog->labels[j].name)) {
                        match = true;
                    }
                } else {
                    // Global label - simple match
                    if (strstr(prog->lines[i].operand, prog->labels[j].name)) {
                        match = true;
                    }
                }
                
                if (match) {
                    if (prog->labels[j].ref_count < MAX_REFS) {
                        prog->labels[j].ref_lines[prog->labels[j].ref_count++] = i;
                    }
                    // If referenced by JSR, it's a subroutine
                    if (strcmp(prog->lines[i].opcode, "JSR") == 0) {
                        prog->labels[j].is_subroutine = true;
                    }
                }
            }
        }
    }
    
    // Third pass: determine subroutine boundaries
    for (int i = 0; i < prog->label_count; i++) {
        if (prog->labels[i].is_subroutine) {
            int start = prog->labels[i].line_num;
            // Find the RTS that ends this subroutine
            for (int j = start + 1; j < prog->count; j++) {
                if (strcmp(prog->lines[j].opcode, "RTS") == 0) {
                    prog->labels[i].sub_end = j;
                    break;
                }
                // Stop if we hit another global label (start of another routine)
                if (prog->lines[j].is_label && !prog->lines[j].is_local_label && j != start) {
                    break;
                }
            }
        }
    }
}

// Mark lines that are branch targets
void mark_branch_targets(Program *prog) {
    for (int i = 0; i < prog->label_count; i++) {
        int line = prog->labels[i].line_num;
        if (line >= 0 && line < prog->count) {
            prog->lines[line].is_branch_target = true;
        }
    }
}

// Call flow analysis
void analyze_call_flow(Program *prog) {
    build_label_table(prog);
    mark_branch_targets(prog);
}

// Peephole optimization patterns
void optimize_peephole(Program *prog) {
    for (int i = 0; i < prog->count - 1; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].no_optimize) continue;
        
        // LDA #value followed by STA then LDA #same_value
        if (strcmp(prog->lines[i].opcode, "LDA") == 0 && i + 2 < prog->count) {
            if (!prog->lines[i+1].no_optimize && !prog->lines[i+2].no_optimize &&
                strcmp(prog->lines[i+1].opcode, "STA") == 0 &&
                strcmp(prog->lines[i+2].opcode, "LDA") == 0 &&
                strcmp(prog->lines[i].operand, prog->lines[i+2].operand) == 0) {
                prog->lines[i+2].is_dead = true;
                prog->optimizations++;
            }
        }
        
        // STA followed immediately by LDA same location
        if (!prog->lines[i+1].no_optimize &&
            strcmp(prog->lines[i].opcode, "STA") == 0 &&
            strcmp(prog->lines[i+1].opcode, "LDA") == 0 &&
            strcmp(prog->lines[i].operand, prog->lines[i+1].operand) == 0 &&
            !prog->lines[i+1].is_branch_target) {
            prog->lines[i+1].is_dead = true;
            prog->optimizations++;
        }
        
        // LDA followed by PHA then PLA (can remove PHA/PLA if A not used between)
        if (strcmp(prog->lines[i].opcode, "LDA") == 0 && i + 2 < prog->count) {
            if (!prog->lines[i+1].no_optimize && !prog->lines[i+2].no_optimize &&
                strcmp(prog->lines[i+1].opcode, "PHA") == 0 &&
                strcmp(prog->lines[i+2].opcode, "PLA") == 0 &&
                !prog->lines[i+2].is_branch_target) {
                prog->lines[i+1].is_dead = true;
                prog->lines[i+2].is_dead = true;
                prog->optimizations++;
            }
        }
        
        // CLC followed by ADC #0 (just remove both)
        if (!prog->lines[i+1].no_optimize &&
            strcmp(prog->lines[i].opcode, "CLC") == 0 &&
            strcmp(prog->lines[i+1].opcode, "ADC") == 0 &&
            strcmp(prog->lines[i+1].operand, "#0") == 0 &&
            !prog->lines[i+1].is_branch_target) {
            prog->lines[i].is_dead = true;
            prog->lines[i+1].is_dead = true;
            prog->optimizations++;
        }
        
        // SEC followed by SBC #0 (just remove both)
        if (!prog->lines[i+1].no_optimize &&
            strcmp(prog->lines[i].opcode, "SEC") == 0 &&
            strcmp(prog->lines[i+1].opcode, "SBC") == 0 &&
            strcmp(prog->lines[i+1].operand, "#0") == 0 &&
            !prog->lines[i+1].is_branch_target) {
            prog->lines[i].is_dead = true;
            prog->lines[i+1].is_dead = true;
            prog->optimizations++;
        }
        
        // AND #$FF (no operation)
        if (strcmp(prog->lines[i].opcode, "AND") == 0 &&
            (strcmp(prog->lines[i].operand, "#$FF") == 0 ||
             strcmp(prog->lines[i].operand, "#255") == 0)) {
            prog->lines[i].is_dead = true;
            prog->optimizations++;
        }
        
        // ORA #0 (no operation)
        if (strcmp(prog->lines[i].opcode, "ORA") == 0 &&
            (strcmp(prog->lines[i].operand, "#0") == 0 ||
             strcmp(prog->lines[i].operand, "#$00") == 0)) {
            prog->lines[i].is_dead = true;
            prog->optimizations++;
        }
        
        // EOR #0 (no operation)
        if (strcmp(prog->lines[i].opcode, "EOR") == 0 &&
            (strcmp(prog->lines[i].operand, "#0") == 0 ||
             strcmp(prog->lines[i].operand, "#$00") == 0)) {
            prog->lines[i].is_dead = true;
            prog->optimizations++;
        }
        
        // LDA followed by TAX/TAY then TXA/TYA (can remove transfer pair)
        if (!prog->lines[i+1].no_optimize && i + 2 < prog->count && !prog->lines[i+2].no_optimize) {
            if (strcmp(prog->lines[i].opcode, "LDA") == 0 &&
                strcmp(prog->lines[i+1].opcode, "TAX") == 0 &&
                strcmp(prog->lines[i+2].opcode, "TXA") == 0 &&
                !prog->lines[i+2].is_branch_target) {
                prog->lines[i+1].is_dead = true;
                prog->lines[i+2].is_dead = true;
                prog->optimizations++;
            }
            if (strcmp(prog->lines[i].opcode, "LDA") == 0 &&
                strcmp(prog->lines[i+1].opcode, "TAY") == 0 &&
                strcmp(prog->lines[i+2].opcode, "TYA") == 0 &&
                !prog->lines[i+2].is_branch_target) {
                prog->lines[i+1].is_dead = true;
                prog->lines[i+2].is_dead = true;
                prog->optimizations++;
            }
        }
        
        // CLC/SEC pairs that cancel out
        if (!prog->lines[i+1].no_optimize &&
            ((strcmp(prog->lines[i].opcode, "CLC") == 0 && strcmp(prog->lines[i+1].opcode, "SEC") == 0) ||
             (strcmp(prog->lines[i].opcode, "SEC") == 0 && strcmp(prog->lines[i+1].opcode, "CLC") == 0)) &&
            !prog->lines[i+1].is_branch_target) {
            prog->lines[i].is_dead = true;
            prog->optimizations++;
        }
    }
}

// Dead code elimination
void optimize_dead_code(Program *prog) {
    for (int i = 0; i < prog->count - 1; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].no_optimize) continue;
        
        // Unconditional jump followed by unreachable code
        if ((strcmp(prog->lines[i].opcode, "JMP") == 0 ||
             strcmp(prog->lines[i].opcode, "RTS") == 0 ||
             strcmp(prog->lines[i].opcode, "RTI") == 0 ||
             (prog->is_45gs02 && strcmp(prog->lines[i].opcode, "BRA") == 0) ) &&
            !prog->lines[i+1].is_branch_target &&
            !prog->lines[i+1].is_label) {
            
            int j = i + 1;
            while (j < prog->count && !prog->lines[j].is_branch_target && 
                   !prog->lines[j].is_label && !prog->lines[j].no_optimize && 
                   prog->lines[j].opcode[0]) {
                prog->lines[j].is_dead = true;
                prog->optimizations++;
                j++;
            }
        }
    }
}

// Jump optimization
void optimize_jumps(Program *prog) {
    for (int i = 0; i < prog->count - 1; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].no_optimize) continue;
        
        // JMP to next line (remove)
        if (strcmp(prog->lines[i].opcode, "JMP") == 0 && i + 1 < prog->count) {
            for (int j = 0; j < prog->label_count; j++) {
                if (strstr(prog->lines[i].operand, prog->labels[j].name) &&
                    prog->labels[j].line_num == i + 1) {
                    prog->lines[i].is_dead = true;
                    prog->optimizations++;
                    break;
                }
            }
        }
        
        // Branch to next instruction (remove)
        if ((strstr(prog->lines[i].opcode, "BEQ") || 
             strstr(prog->lines[i].opcode, "BNE") ||
             ( prog->is_45gs02 && 
               strstr(prog->lines[i].opcode, "BRA") != NULL
             ) || 
             strstr(prog->lines[i].opcode, "BNE") ||
             strstr(prog->lines[i].opcode, "BCC") ||
             strstr(prog->lines[i].opcode, "BCS")) && i + 1 < prog->count) {
            for (int j = 0; j < prog->label_count; j++) {
                if (strstr(prog->lines[i].operand, prog->labels[j].name) &&
                    prog->labels[j].line_num == i + 1) {
                    prog->lines[i].is_dead = true;
                    prog->optimizations++;
                    break;
                }
            }
        }
    }
}

// Load/Store optimization
void optimize_load_store(Program *prog) {
    for (int i = 0; i < prog->count - 2; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].no_optimize) continue;
        
        // LDA addr, STA addr2, LDA addr (remove third LDA)
        if (!prog->lines[i+1].no_optimize && !prog->lines[i+2].no_optimize &&
            strcmp(prog->lines[i].opcode, "LDA") == 0 &&
            strcmp(prog->lines[i+1].opcode, "STA") == 0 &&
            strcmp(prog->lines[i+2].opcode, "LDA") == 0 &&
            strcmp(prog->lines[i].operand, prog->lines[i+2].operand) == 0 &&
            !prog->lines[i+2].is_branch_target) {
            prog->lines[i+2].is_dead = true;
            prog->optimizations++;
        }
        
        // Double store optimization: STA addr, STA addr
        if (!prog->lines[i+1].no_optimize &&
            strcmp(prog->lines[i].opcode, "STA") == 0 &&
            strcmp(prog->lines[i+1].opcode, "STA") == 0 &&
            strcmp(prog->lines[i].operand, prog->lines[i+1].operand) == 0 &&
            !prog->lines[i+1].is_branch_target) {
            prog->lines[i].is_dead = true;
            prog->optimizations++;
        }
    }
}

// Register usage optimization
void optimize_register_usage(Program *prog) {
    for (int i = 0; i < prog->count - 1; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].no_optimize) continue;
        
        // TAX followed by TXA (no operation if no X usage between)
        if (!prog->lines[i+1].no_optimize &&
            strcmp(prog->lines[i].opcode, "TAX") == 0 &&
            strcmp(prog->lines[i+1].opcode, "TXA") == 0 &&
            !prog->lines[i+1].is_branch_target) {
            prog->lines[i].is_dead = true;
            prog->lines[i+1].is_dead = true;
            prog->optimizations++;
        }
        
        // TAY followed by TYA
        if (!prog->lines[i+1].no_optimize &&
            strcmp(prog->lines[i].opcode, "TAY") == 0 &&
            strcmp(prog->lines[i+1].opcode, "TYA") == 0 &&
            !prog->lines[i+1].is_branch_target) {
            prog->lines[i].is_dead = true;
            prog->lines[i+1].is_dead = true;
            prog->optimizations++;
        }
        
        // TXA followed by TAX
        if (!prog->lines[i+1].no_optimize &&
            strcmp(prog->lines[i].opcode, "TXA") == 0 &&
            strcmp(prog->lines[i+1].opcode, "TAX") == 0 &&
            !prog->lines[i+1].is_branch_target) {
            prog->lines[i].is_dead = true;
            prog->lines[i+1].is_dead = true;
            prog->optimizations++;
        }
    }
}

// Branch optimization
void optimize_branches(Program *prog) {
    // Convert long jumps to short branches when possible (for size optimization)
    if (prog->mode == OPT_SIZE) {
        for (int i = 0; i < prog->count; i++) {
            if (prog->lines[i].is_dead || prog->lines[i].no_optimize) continue;
            
            // Could analyze distance and convert JMP to branch instructions
            // This requires more complex distance calculation
        }
    }
}

// Constant propagation
void optimize_constant_propagation(Program *prog) {
    char last_a_value[64] = "";
    bool a_known = false;
    
    for (int i = 0; i < prog->count; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].is_branch_target || prog->lines[i].no_optimize) {
            a_known = false;
            continue;
        }
        
        // Track LDA immediate values
        if (strcmp(prog->lines[i].opcode, "LDA") == 0 &&
            prog->lines[i].operand[0] == '#') {
            strncpy(last_a_value, prog->lines[i].operand, 63);
            a_known = true;
        }
        // If we see another LDA with same value, remove it
        else if (a_known && strcmp(prog->lines[i].opcode, "LDA") == 0 &&
                 strcmp(prog->lines[i].operand, last_a_value) == 0) {
            prog->lines[i].is_dead = true;
            prog->optimizations++;
        }
        // Operations that modify A
        else if (strcmp(prog->lines[i].opcode, "ADC") == 0 ||
                 strcmp(prog->lines[i].opcode, "SBC") == 0 ||
                 strcmp(prog->lines[i].opcode, "AND") == 0 ||
                 strcmp(prog->lines[i].opcode, "ORA") == 0 ||
                 strcmp(prog->lines[i].opcode, "EOR") == 0 ||
                 strcmp(prog->lines[i].opcode, "LDA") == 0 ||
                 strcmp(prog->lines[i].opcode, "PLA") == 0 ||
                 strcmp(prog->lines[i].opcode, "TXA") == 0 ||
                 strcmp(prog->lines[i].opcode, "TYA") == 0 ||
                 strcmp(prog->lines[i].opcode, "ASL") == 0 ||
                 strcmp(prog->lines[i].opcode, "LSR") == 0 ||
                 strcmp(prog->lines[i].opcode, "ROL") == 0 ||
                 strcmp(prog->lines[i].opcode, "ROR") == 0) {
            a_known = false;
        }
    }
}

// Strength reduction - replace expensive operations with cheaper equivalents
void optimize_strength_reduction(Program *prog) {
    for (int i = 0; i < prog->count; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].no_optimize) continue;
        
        // Multiplication by 2 can be replaced with ASL (if no carry needed)
        if (strcmp(prog->lines[i].opcode, "ASL") == 0 && 
            prog->lines[i].operand[0] == 'A' && prog->lines[i].operand[1] == ' ') {
            // This is already optimal (ASL A is multiply by 2)
        }
        
        // CLC + ADC same_reg = multiply by 2
        if (i + 1 < prog->count && !prog->lines[i+1].no_optimize &&
            strcmp(prog->lines[i].opcode, "CLC") == 0 &&
            strcmp(prog->lines[i+1].opcode, "ADC") == 0 &&
            !prog->lines[i+1].is_branch_target) {
            
            // Check if adding a register to itself from memory
            char operand[64];
            strncpy(operand, prog->lines[i+1].operand, 63);
            
            // Look back to see if we just loaded this value
            if (i > 0 && strcmp(prog->lines[i-1].opcode, "LDA") == 0 &&
                !prog->lines[i-1].no_optimize &&
                strcmp(prog->lines[i-1].operand, operand) == 0) {
                // Could potentially be optimized to ASL
                // This is a complex optimization that requires data flow analysis
            }
        }
        
        // Division/Multiplication by powers of 2 using shifts
        // LDA #value, where value is power of 2, followed by ADC/SBC
        // This is advanced and would need constant evaluation
    }
}

// Common subexpression elimination
void optimize_common_subexpressions(Program *prog) {
    for (int i = 0; i < prog->count - 5; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].no_optimize) continue;
        
        // Look for repeated sequences of 2-3 instructions
        for (int j = i + 3; j < prog->count - 2 && j < i + 50; j++) {
            if (prog->lines[j].is_dead || prog->lines[j].no_optimize) continue;
            if (prog->lines[j].is_branch_target || prog->lines[j].is_label) break;
            
            // Check if next 2 instructions match
            bool match = true;
            for (int k = 0; k < 2 && i+k < prog->count && j+k < prog->count; k++) {
                if (strcmp(prog->lines[i+k].opcode, prog->lines[j+k].opcode) != 0 ||
                    strcmp(prog->lines[i+k].operand, prog->lines[j+k].operand) != 0) {
                    match = false;
                    break;
                }
                if (prog->lines[j+k].is_branch_target || prog->lines[j+k].is_label) {
                    match = false;
                    break;
                }
            }
            
            if (match) {
                // If we found a match, we could potentially extract to subroutine
                // This is complex and would need careful analysis
                // For now, just detect it (framework for future implementation)
            }
        }
    }
}

// Addressing mode optimization - use faster/smaller addressing modes
void optimize_addressing_modes(Program *prog) {
    for (int i = 0; i < prog->count; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].no_optimize) continue;
        
        // Absolute to Zero Page conversion
        // LDA $00nn can become LDA $nn (3 bytes -> 2 bytes, 4 cycles -> 3 cycles)
        if ((strcmp(prog->lines[i].opcode, "LDA") == 0 ||
             strcmp(prog->lines[i].opcode, "STA") == 0 ||
             strcmp(prog->lines[i].opcode, "LDX") == 0 ||
             strcmp(prog->lines[i].opcode, "STX") == 0 ||
             strcmp(prog->lines[i].opcode, "LDY") == 0 ||
             strcmp(prog->lines[i].opcode, "STY") == 0 ||
             strcmp(prog->lines[i].opcode, "ADC") == 0 ||
             strcmp(prog->lines[i].opcode, "SBC") == 0 ||
             strcmp(prog->lines[i].opcode, "AND") == 0 ||
             strcmp(prog->lines[i].opcode, "ORA") == 0 ||
             strcmp(prog->lines[i].opcode, "EOR") == 0 ||
             strcmp(prog->lines[i].opcode, "CMP") == 0 ||
             strcmp(prog->lines[i].opcode, "BIT") == 0) &&
            prog->lines[i].operand[0] == '$') {
            
            // Check if address is in zero page range ($0000-$00FF)
            int addr = 0;
            if (sscanf(prog->lines[i].operand, "$%x", &addr) == 1) {
                if (addr >= 0 && addr <= 0xFF) {
                    // Already zero page - good!
                } else if (addr >= 0x0000 && addr <= 0x00FF) {
                    // Could be converted to zero page
                    // Note: This is informational - actual conversion needs care
                }
            }
        }
        
        // Absolute,X to Zero Page,X conversion
        if (strstr(prog->lines[i].operand, ",X") || strstr(prog->lines[i].operand, ",x")) {
            // Similar logic for indexed modes
        }
    }
}

// Zero page optimization - identify frequently used variables for ZP allocation
void optimize_zero_page(Program *prog) {
    // Track memory location usage frequency
    typedef struct {
        char address[32];
        int read_count;
        int write_count;
        bool is_absolute;
    } MemUsage;
    
    MemUsage usage[256];
    memset(usage, 0, sizeof(usage));
    int usage_count = 0;
    
    // Scan for memory access patterns
    for (int i = 0; i < prog->count; i++) {
        if (prog->lines[i].is_dead) continue;
        
        // Count reads and writes
        bool is_load = (strcmp(prog->lines[i].opcode, "LDA") == 0 ||
                       strcmp(prog->lines[i].opcode, "LDX") == 0 ||
                       strcmp(prog->lines[i].opcode, "LDY") == 0);
        
        bool is_store = (strcmp(prog->lines[i].opcode, "STA") == 0 ||
                        strcmp(prog->lines[i].opcode, "STX") == 0 ||
                        strcmp(prog->lines[i].opcode, "STY") == 0);
        
        if ((is_load || is_store) && prog->lines[i].operand[0] == '$') {
            // Track this address
            bool found = false;
            for (int j = 0; j < usage_count; j++) {
                if (strcmp(usage[j].address, prog->lines[i].operand) == 0) {
                    if (is_load) usage[j].read_count++;
                    if (is_store) usage[j].write_count++;
                    found = true;
                    break;
                }
            }
            
            if (!found && usage_count < 256) {
                strncpy(usage[usage_count].address, prog->lines[i].operand, 31);
                usage[usage_count].read_count = is_load ? 1 : 0;
                usage[usage_count].write_count = is_store ? 1 : 0;
                usage_count++;
            }
        }
    }
    
    // Report heavily used absolute addresses that could benefit from ZP
    for (int i = 0; i < usage_count; i++) {
        int total = usage[i].read_count + usage[i].write_count;
        if (total > 5) {
            int addr;
            if (sscanf(usage[i].address, "$%x", &addr) == 1) {
                if (addr > 0xFF) {
                    // This could benefit from ZP allocation
                    // Actual implementation would track this for reporting
                }
            }
        }
    }
}

// Branch chaining optimization - eliminate unnecessary branch intermediaries
void optimize_branch_chaining(Program *prog) {
    for (int i = 0; i < prog->count; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].no_optimize) continue;
        
        // Look for: BNE label1 where label1: JMP label2
        // Can be optimized to: BNE label2 (if in range)
        if (strstr(prog->lines[i].opcode, "B") == prog->lines[i].opcode && 
            strlen(prog->lines[i].opcode) == 3) {
            
            // Find the target label
            for (int j = 0; j < prog->label_count; j++) {
                if (strstr(prog->lines[i].operand, prog->labels[j].name)) {
                    int target_line = prog->labels[j].line_num;
                    
                    // Check if target is just a JMP
                    if (target_line + 1 < prog->count &&
                        strcmp(prog->lines[target_line + 1].opcode, "JMP") == 0 &&
                        !prog->lines[target_line + 1].no_optimize) {
                        
                        // Could potentially redirect branch to JMP target
                        // Need to verify branch distance is valid (-128 to +127)
                        prog->optimizations++; // Placeholder
                    }
                    break;
                }
            }
        }
    }
}

// Flag usage optimization - eliminate redundant flag-setting operations
void optimize_flag_usage(Program *prog) {
    bool carry_known = false;
    bool carry_state = false;
    
    for (int i = 0; i < prog->count; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].is_branch_target || 
            prog->lines[i].no_optimize) {
            carry_known = false;
            continue;
        }
        
        // Track CLC/SEC
        if (strcmp(prog->lines[i].opcode, "CLC") == 0) {
            if (carry_known && !carry_state) {
                // Carry already clear
                prog->lines[i].is_dead = true;
                prog->optimizations++;
            }
            carry_known = true;
            carry_state = false;
        } else if (strcmp(prog->lines[i].opcode, "SEC") == 0) {
            if (carry_known && carry_state) {
                // Carry already set
                prog->lines[i].is_dead = true;
                prog->optimizations++;
            }
            carry_known = true;
            carry_state = true;
        }
        
        // Operations that use/modify carry
        else if (strcmp(prog->lines[i].opcode, "ADC") == 0 ||
                 strcmp(prog->lines[i].opcode, "SBC") == 0 ||
                 strcmp(prog->lines[i].opcode, "ROL") == 0 ||
                 strcmp(prog->lines[i].opcode, "ROR") == 0) {
            carry_known = false; // Carry modified
        }
        
        // Branches affect flag knowledge
        else if (strcmp(prog->lines[i].opcode, "BCC") == 0 ||
                 strcmp(prog->lines[i].opcode, "BCS") == 0) {
            carry_known = false;
        }
    }
}

// Loop invariant code motion - move loop-invariant calculations outside loops
void optimize_loop_invariants(Program *prog) {
    // Identify loops (backward branches)
    for (int i = 0; i < prog->count; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].no_optimize) continue;
        
        // Look for backward branches (likely loops)
        if (strstr(prog->lines[i].opcode, "B") == prog->lines[i].opcode) {
            for (int j = 0; j < prog->label_count; j++) {
                if (strstr(prog->lines[i].operand, prog->labels[j].name)) {
                    int target = prog->labels[j].line_num;
                    
                    if (target < i) {
                        // Backward branch - likely a loop from target to i
                        // Analyze loop body for invariant code
                        
                        for (int k = target; k < i; k++) {
                            if (prog->lines[k].is_dead) continue;
                            
                            // Look for immediate loads that don't change
                            if (strcmp(prog->lines[k].opcode, "LDA") == 0 &&
                                prog->lines[k].operand[0] == '#') {
                                
                                // Check if this value is used but never modified
                                for (int m = k + 1; m < i; m++) {
                                    if (strcmp(prog->lines[m].opcode, "LDA") == 0) {
                                        break;
                                    }
                                }
                                
                                // If not modified, could be moved before loop
                                // This is a candidate for optimization
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
}

// Bit operation optimizations
void optimize_bit_operations(Program *prog) {
    for (int i = 0; i < prog->count - 2; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].no_optimize) continue;
        
        // AND followed by CMP to test bits -> use BIT instruction
        if (strcmp(prog->lines[i].opcode, "LDA") == 0 && i + 2 < prog->count &&
            !prog->lines[i+1].no_optimize && !prog->lines[i+2].no_optimize) {
            
            if (strcmp(prog->lines[i+1].opcode, "AND") == 0 &&
                strcmp(prog->lines[i+2].opcode, "CMP") == 0) {
                
                // Check if testing bit 7 or 6
                if ((strcmp(prog->lines[i+1].operand, "#$80") == 0 ||
                     strcmp(prog->lines[i+1].operand, "#$40") == 0) &&
                    strcmp(prog->lines[i+2].operand, "#$00") == 0) {
                    
                    // Can use BIT instruction which sets N and V flags
                    // This is a candidate for BIT optimization
                    prog->optimizations++;
                }
            }
        }
        
        // Combine multiple AND operations
        if (strcmp(prog->lines[i].opcode, "AND") == 0 && i + 1 < prog->count &&
            !prog->lines[i+1].no_optimize &&
            strcmp(prog->lines[i+1].opcode, "AND") == 0 &&
            !prog->lines[i+1].is_branch_target) {
            
            // Check if both are immediate values
            if (prog->lines[i].operand[0] == '#' && prog->lines[i+1].operand[0] == '#') {
                int val1, val2;
                if (sscanf(prog->lines[i].operand, "#$%x", &val1) == 1 &&
                    sscanf(prog->lines[i+1].operand, "#$%x", &val2) == 1) {
                    
                    // Can combine: AND #$FE, AND #$FD -> AND #$FC
                    int combined = val1 & val2;
                    char new_operand[16];
                    snprintf(new_operand, 16, "#$%02X", combined);
                    strncpy(prog->lines[i].operand, new_operand, 63);
                    prog->lines[i+1].is_dead = true;
                    prog->optimizations++;
                }
            }
        }
    }
}

// Advanced arithmetic optimizations
void optimize_arithmetic(Program *prog) {
    for (int i = 0; i < prog->count - 3; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].no_optimize) continue;
        
        // Multiply by 2: replace with ASL
        // Pattern: CLC, ADC same_value (A = A + A = A * 2)
        if (strcmp(prog->lines[i].opcode, "STA") == 0 && i + 2 < prog->count &&
            !prog->lines[i+1].no_optimize && !prog->lines[i+2].no_optimize) {
            
            if (strcmp(prog->lines[i+1].opcode, "CLC") == 0 &&
                strcmp(prog->lines[i+2].opcode, "ADC") == 0 &&
                strcmp(prog->lines[i].operand, prog->lines[i+2].operand) == 0 &&
                !prog->lines[i+2].is_branch_target) {
                
                // This is A = A * 2, can use ASL
                strcpy(prog->lines[i+1].opcode, "ASL");
                strcpy(prog->lines[i+1].operand, prog->lines[i].operand);
                prog->lines[i+2].is_dead = true;
                prog->optimizations++;
            }
        }
        
        // Multiply by 3: STA temp, ASL, ADC temp
        // This is already optimal, just detect it
        
        // Negate optimization: EOR #$FF, CLC, ADC #$01 -> EOR #$FF, SEC, ADC #$00
        if (strcmp(prog->lines[i].opcode, "EOR") == 0 && i + 2 < prog->count &&
            strcmp(prog->lines[i].operand, "#$FF") == 0 &&
            !prog->lines[i+1].no_optimize && !prog->lines[i+2].no_optimize) {
            
            if (strcmp(prog->lines[i+1].opcode, "CLC") == 0 &&
                strcmp(prog->lines[i+2].opcode, "ADC") == 0 &&
                strcmp(prog->lines[i+2].operand, "#$01") == 0 &&
                !prog->lines[i+2].is_branch_target) {
                
                // Replace CLC with SEC and ADC #$01 with ADC #$00
                strcpy(prog->lines[i+1].opcode, "SEC");
                strcpy(prog->lines[i+2].operand, "#$00");
                prog->optimizations++;
            }
        }
    }
}

// Tail call optimization
void optimize_tail_calls(Program *prog) {
    for (int i = 0; i < prog->count - 1; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].no_optimize) continue;
        
        // JSR followed immediately by RTS -> convert to JMP
        if (strcmp(prog->lines[i].opcode, "JSR") == 0 && i + 1 < prog->count &&
            !prog->lines[i+1].no_optimize &&
            strcmp(prog->lines[i+1].opcode, "RTS") == 0 &&
            !prog->lines[i+1].is_branch_target) {
            
            // Convert JSR to JMP
            strcpy(prog->lines[i].opcode, "JMP");
            prog->lines[i+1].is_dead = true;
            prog->optimizations++;
        }
    }
}

// Loop unrolling for small fixed loops
void optimize_loop_unrolling(Program *prog) {
    if (prog->mode != OPT_SPEED) return; // Only for speed optimization
    
    for (int i = 0; i < prog->count - 3; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].no_optimize) continue;
        
        // Look for simple counting loops: LDX #n, loop body, DEX, BNE
        if (strcmp(prog->lines[i].opcode, "LDX") == 0 && 
            prog->lines[i].operand[0] == '#') {
            
            int count = 0;
            if (sscanf(prog->lines[i].operand, "#$%x", &count) == 1 ||
                sscanf(prog->lines[i].operand, "#%d", &count) == 1) {
                
                // Only unroll very small loops (2-4 iterations)
                if (count >= 2 && count <= 4) {
                    // Find the loop end (DEX, BNE pattern)
                    for (int j = i + 1; j < prog->count - 1 && j < i + 20; j++) {
                        if (strcmp(prog->lines[j].opcode, "DEX") == 0 &&
                            j + 1 < prog->count &&
                            strcmp(prog->lines[j+1].opcode, "BNE") == 0) {
                            
                            // Found loop end - candidate for unrolling
                            // Mark for potential unrolling (complex to implement fully)
                            prog->optimizations++;
                            break;
                        }
                    }
                }
            }
        }
    }
}

// Stack operation optimizations
void optimize_stack_operations(Program *prog) {
    for (int i = 0; i < prog->count - 1; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].no_optimize) continue;
        
        // PHA followed by immediate PLA with no intervening code
        if (strcmp(prog->lines[i].opcode, "PHA") == 0 && i + 1 < prog->count &&
            !prog->lines[i+1].no_optimize &&
            strcmp(prog->lines[i+1].opcode, "PLA") == 0 &&
            !prog->lines[i+1].is_branch_target) {
            
            // Remove both - they cancel out
            prog->lines[i].is_dead = true;
            prog->lines[i+1].is_dead = true;
            prog->optimizations++;
        }
        
        // PHA, TXA, PHA, work, PLA, TAX, PLA -> may be optimizable
        // If work doesn't modify X, can remove TXA/TAX pair
    }
}



// Boolean logic optimizations
void optimize_boolean_logic(Program *prog) {
    for (int i = 0; i < prog->count - 3; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].no_optimize) continue;
        
        // Test for zero: CMP #$00 -> can often be eliminated
        if (strcmp(prog->lines[i].opcode, "CMP") == 0 &&
            (strcmp(prog->lines[i].operand, "#$00") == 0 ||
             strcmp(prog->lines[i].operand, "#0") == 0)) {
            
            // If previous instruction set flags, CMP #0 is redundant
            if (i > 0 && (strcmp(prog->lines[i-1].opcode, "LDA") == 0 ||
                         strcmp(prog->lines[i-1].opcode, "LDX") == 0 ||
                         strcmp(prog->lines[i-1].opcode, "LDY") == 0 ||
                         strcmp(prog->lines[i-1].opcode, "AND") == 0 ||
                         strcmp(prog->lines[i-1].opcode, "ORA") == 0)) {
                prog->lines[i].is_dead = true;
                prog->optimizations++;
            }
        }
        
        // Double negative: EOR #$FF, EOR #$FF -> remove both
        if (strcmp(prog->lines[i].opcode, "EOR") == 0 && i + 1 < prog->count &&
            strcmp(prog->lines[i].operand, "#$FF") == 0 &&
            !prog->lines[i+1].no_optimize &&
            strcmp(prog->lines[i+1].opcode, "EOR") == 0 &&
            strcmp(prog->lines[i+1].operand, "#$FF") == 0 &&
            !prog->lines[i+1].is_branch_target) {
            
            prog->lines[i].is_dead = true;
            prog->lines[i+1].is_dead = true;
            prog->optimizations++;
        }
    }
}



// 65C02-specific optimizations
void optimize_65c02_instructions(Program *prog) {
    if (!prog->allow_65c02 || prog->is_45gs02) return;  // Don't apply to 45GS02!
    
    bool accumulator_is_zero = false;

    for (int i = 0; i < prog->count; i++) { // Loop through all lines
        if (prog->lines[i].is_dead || prog->lines[i].no_optimize) {
            // Reset accumulator_is_zero if we skip a line, as its state becomes unknown
            accumulator_is_zero = false; 
            continue;
        }
        
        // Handle instructions that set the accumulator to zero
        if (strcmp(prog->lines[i].opcode, "LDA") == 0 &&
            (strcmp(prog->lines[i].operand, "#$00") == 0 ||
             strcmp(prog->lines[i].operand, "#0") == 0)) {
            accumulator_is_zero = true;
            if (prog->trace_level > 1) {
                printf("DEBUG 65c02: LDA #0 at line %d, Accumulator is now zero.\n", prog->lines[i].line_num);
            }
        } 
        // Handle instructions that store a zero from the accumulator
        else if (accumulator_is_zero && strcmp(prog->lines[i].opcode, "STA") == 0 &&
                 !prog->lines[i].is_branch_target) {
            if (prog->trace_level > 1) {
                printf("DEBUG 65c02: Found STZ opportunity at line %d (STA after LDA #0), opts before: %d\n", prog->lines[i].line_num, prog->optimizations);
            }
            strcpy(prog->lines[i].opcode, "STZ");
            reconstruct_line(&prog->lines[i], &prog->config);
            prog->optimizations++;
            if (prog->trace_level > 1) {
                printf("DEBUG 65c02: Applied STZ optimization, opts after: %d\n", prog->optimizations);
            }
        }
        // Handle instructions that change the accumulator, invalidating accumulator_is_zero state
        else if (strcmp(prog->lines[i].opcode, "LDA") == 0 ||
                 strcmp(prog->lines[i].opcode, "ADC") == 0 ||
                 strcmp(prog->lines[i].opcode, "SBC") == 0 ||
                 strcmp(prog->lines[i].opcode, "AND") == 0 ||
                 strcmp(prog->lines[i].opcode, "ORA") == 0 ||
                 strcmp(prog->lines[i].opcode, "EOR") == 0 ||
                 strcmp(prog->lines[i].opcode, "PLA") == 0 ||
                 strcmp(prog->lines[i].opcode, "TXA") == 0 || // X to A
                 strcmp(prog->lines[i].opcode, "TYA") == 0 || // Y to A
                 strcmp(prog->lines[i].opcode, "ASL") == 0 ||
                 strcmp(prog->lines[i].opcode, "LSR") == 0 ||
                 strcmp(prog->lines[i].opcode, "ROL") == 0 ||
                 strcmp(prog->lines[i].opcode, "ROR") == 0 ||
                 strcmp(prog->lines[i].opcode, "INC") == 0 || // Affects A if operand is A
                 strcmp(prog->lines[i].opcode, "DEC") == 0)   // Affects A if operand is A
                 {
            if (prog->lines[i].operand[0] == 'A' || prog->lines[i].operand[0] == '\0' || 
                (strcmp(prog->lines[i].opcode, "LDA") == 0 && prog->lines[i].operand[0] != '#')) // Non-immediate LDA
            {
                accumulator_is_zero = false;
                if (prog->trace_level > 1) {
                    printf("DEBUG 65c02: Accumulator modified at line %d, state reset.\n", prog->lines[i].line_num);
                }
            }
        }
    }
}

// 45GS02-specific optimizations (MEGA65)
void optimize_45gs02_instructions(Program *prog) {
    if (!prog->is_45gs02) return;
    
    for (int i = 0; i < prog->count; i++) {
        if (prog->lines[i].is_dead || prog->lines[i].no_optimize) continue;
        
        // ===== Z Register Optimizations =====
        
        // Pattern: Multiple stores of SAME VALUE using LDA #val, STA -> LDZ #val, then STZ
        // Generalized for any immediate value, not just zero
        // Look for: LDA #val, STA addr1, LDA #val, STA addr2
        if (i + 3 < prog->count &&
            strcmp(prog->lines[i].opcode, "LDA") == 0 && prog->lines[i].operand[0] == '#' &&
            strcmp(prog->lines[i+1].opcode, "STA") == 0 &&
            strcmp(prog->lines[i+2].opcode, "LDA") == 0 && prog->lines[i+2].operand[0] == '#' &&
            strcmp(prog->lines[i+3].opcode, "STA") == 0 &&
            !prog->lines[i+1].no_optimize && !prog->lines[i+2].no_optimize && !prog->lines[i+3].no_optimize &&
            strcmp(prog->lines[i].operand, prog->lines[i+2].operand) == 0) {  // Same value!
            
                                    // Convert to: LDZ #val, STZ addr1, STZ addr2
            
                                    strcpy(prog->lines[i].opcode, "LDZ");
            
                                    reconstruct_line(&prog->lines[i], &prog->config);
            
                                    strcpy(prog->lines[i+1].opcode, "STZ");
            
                                    reconstruct_line(&prog->lines[i+1], &prog->config);
            
                                    prog->lines[i+2].is_dead = true;  // Remove second LDA
            
                                    strcpy(prog->lines[i+3].opcode, "STZ");
            
                                    reconstruct_line(&prog->lines[i+3], &prog->config);
            
                                    prog->optimizations++;
            
                        
            
                                    // Look ahead for more stores of the same value
            
                                    int j = i + 4;
            
                                    while (j + 1 < prog->count && j < i + 20) {
            
                                        if (prog->lines[j].is_dead || prog->lines[j].no_optimize) {
            
                                            j++;
            
                                            continue;
            
                                        }
            
                        
            
                                        // Check for another LDA #same_value, STA pattern
            
                                        if (strcmp(prog->lines[j].opcode, "LDA") == 0 &&
            
                                            strcmp(prog->lines[j].operand, prog->lines[i].operand) == 0 &&
            
                                            j + 1 < prog->count &&
            
                                            strcmp(prog->lines[j+1].opcode, "STA") == 0 &&
            
                                            !prog->lines[j+1].no_optimize) {
            
                        
            
                                            // Convert this STA to STZ and remove the LDA
            
                                            prog->lines[j].is_dead = true;
            
                                            strcpy(prog->lines[j+1].opcode, "STZ");
            
                                            reconstruct_line(&prog->lines[j+1], &prog->config);
            
                                            prog->optimizations++;
            
                                            j += 2;
            
                                        } else if (strcmp(prog->lines[j].opcode, "LDA") == 0 ||
            
                                                  strcmp(prog->lines[j].opcode, "LDZ") == 0) {
            
                                            // Different load - stop looking
            
                                            break;
            
                                        } else {
            
                                            j++;
            
                                        }
            
                                    }        }
        
        // Also handle cases where we already have LDZ followed by stores
        // Pattern: LDZ #val, STA addr1, STA addr2 -> LDZ #val, STZ addr1, STZ addr2
        if (strcmp(prog->lines[i].opcode, "LDZ") == 0 && prog->lines[i].operand[0] == '#') {
            // Look ahead for STA instructions that could become STZ
            int j = i + 1;
            while (j < prog->count && j < i + 20) {
                if (prog->lines[j].is_dead || prog->lines[j].no_optimize) {
                    j++;
                    continue;
                }
                
                if (strcmp(prog->lines[j].opcode, "STA") == 0 &&
                    !prog->lines[j].is_branch_target) {
                    // Convert STA to STZ (stores Z register value)
                    strcpy(prog->lines[j].opcode, "STZ");
                    prog->optimizations++;
                    j++;
                } else if (strcmp(prog->lines[j].opcode, "LDA") == 0 ||
                          strcmp(prog->lines[j].opcode, "LDZ") == 0 ||
                          strcmp(prog->lines[j].opcode, "TAX") == 0 ||
                          strcmp(prog->lines[j].opcode, "TAY") == 0) {
                    // Something that would change what we're storing
                    break;
                } else {
                    j++;
                }
            }
        }
        
        // ===== 32-bit Q Register Optimizations =====
        // IMPORTANT: Q register is composite of A (low), X, Y, Z (high)
        // Q = [Z:Y:X:A] as a 32-bit value
        // Modifying Q affects A, X, Y, Z and vice versa!
        
        // Pattern: Four consecutive loads that form 32-bit value
        // LDA #low, LDX #mid1, LDY #mid2, LDZ #high
        // Could become: LDQ #value32
        if (i + 3 < prog->count &&
            strcmp(prog->lines[i].opcode, "LDA") == 0 && prog->lines[i].operand[0] == '#' &&
            strcmp(prog->lines[i+1].opcode, "LDX") == 0 && prog->lines[i+1].operand[0] == '#' &&
            strcmp(prog->lines[i+2].opcode, "LDY") == 0 && prog->lines[i+2].operand[0] == '#' &&
            strcmp(prog->lines[i+3].opcode, "LDZ") == 0 && prog->lines[i+3].operand[0] == '#' &&
            !prog->lines[i+1].no_optimize && !prog->lines[i+2].no_optimize && !prog->lines[i+3].no_optimize) {
            
            // Parse the four byte values
            int a_val, x_val, y_val, z_val;
            if ((sscanf(prog->lines[i].operand, "#$%x", &a_val) == 1 || sscanf(prog->lines[i].operand, "#%d", &a_val) == 1) &&
                (sscanf(prog->lines[i+1].operand, "#$%x", &x_val) == 1 || sscanf(prog->lines[i+1].operand, "#%d", &x_val) == 1) &&
                (sscanf(prog->lines[i+2].operand, "#$%x", &y_val) == 1 || sscanf(prog->lines[i+2].operand, "#%d", &y_val) == 1) &&
                (sscanf(prog->lines[i+3].operand, "#$%x", &z_val) == 1 || sscanf(prog->lines[i+3].operand, "#%d", &z_val) == 1)) {
                
                // Combine into 32-bit value: Q = [Z:Y:X:A]
                unsigned long q_val = ((z_val & 0xFF) << 24) | ((y_val & 0xFF) << 16) | 
                                     ((x_val & 0xFF) << 8) | (a_val & 0xFF);
                
                // Replace with LDQ
                strcpy(prog->lines[i].opcode, "LDQ");
                char q_operand[32];
                snprintf(q_operand, 32, "#$%08lX", q_val);
                strcpy(prog->lines[i].operand, q_operand);
                
                prog->lines[i+1].is_dead = true;
                prog->lines[i+2].is_dead = true;
                prog->lines[i+3].is_dead = true;
                prog->optimizations++;
            }
        }
        
        // Pattern: 32-bit store sequence
        // STA addr, STX addr+1, STY addr+2, STZ addr+3 -> STQ addr
        // But must verify addresses are consecutive
        
        // ===== Base Page Register (B) =====
        // IMPORTANT: B register controls base page mapping (where ZP is mapped)
        // NOT a second accumulator! Do not use for general purpose storage!
        // B register remaps zero page to different memory location
        
        // We should NOT optimize stack operations to use B register
        // B is for memory mapping only
        
        // ===== NOP -> NEG Optimization =====
        
        // 45GS02 has NEG instruction (negate accumulator)
        // Pattern: EOR #$FF, SEC, ADC #$00 -> NEG
        if (i + 2 < prog->count &&
            strcmp(prog->lines[i].opcode, "EOR") == 0 &&
            strcmp(prog->lines[i].operand, "#$FF") == 0 &&
            strcmp(prog->lines[i+1].opcode, "SEC") == 0 &&
            strcmp(prog->lines[i+2].opcode, "ADC") == 0 &&
            strcmp(prog->lines[i+2].operand, "#$00") == 0 &&
            !prog->lines[i+1].no_optimize && !prog->lines[i+2].no_optimize &&
            !prog->lines[i+2].is_branch_target) {
            
            // Replace with NEG
            strcpy(prog->lines[i].opcode, "NEG");
            strcpy(prog->lines[i].operand, "");
            prog->lines[i+1].is_dead = true;
            prog->lines[i+2].is_dead = true;
            prog->optimizations++;
        }
        
        // ===== ASR (Arithmetic Shift Right) =====
        
        // 45GS02 has ASR instruction (arithmetic shift right, preserves sign)
        // Pattern: CMP #$80, ROR -> ASR
        if (i + 1 < prog->count &&
            strcmp(prog->lines[i].opcode, "CMP") == 0 &&
            strcmp(prog->lines[i].operand, "#$80") == 0 &&
            strcmp(prog->lines[i+1].opcode, "ROR") == 0 &&
            !prog->lines[i+1].no_optimize &&
            !prog->lines[i+1].is_branch_target) {
            
            // Can use ASR for signed right shift
            strcpy(prog->lines[i].opcode, "ASR");
            strcpy(prog->lines[i].operand, "");
            prog->lines[i+1].is_dead = true;
            prog->optimizations++;
        }
        
        // ===== ASRQ (32-bit Arithmetic Shift Right) =====
        // Since Q = [Z:Y:X:A], shifting Q affects all four registers
        
        // ===== 32-bit Arithmetic with Q =====
        // Pattern: Multi-byte addition/subtraction sequences
        // Can be replaced with ADCQ/SBCQ but must be careful about register side effects
        
        // Example: 16-bit add that doesn't use Y or Z could benefit
        if (i + 5 < prog->count &&
            strcmp(prog->lines[i].opcode, "CLC") == 0 &&
            strcmp(prog->lines[i+1].opcode, "LDA") == 0 &&
            strcmp(prog->lines[i+2].opcode, "ADC") == 0 &&
            strcmp(prog->lines[i+3].opcode, "STA") == 0 &&
            strcmp(prog->lines[i+4].opcode, "LDA") == 0 &&
            strcmp(prog->lines[i+5].opcode, "ADC") == 0) {
            
            // This is 16-bit addition pattern
            // Could potentially use Q register operations
            // But must verify Y and Z are not in use
            // Complex analysis needed - mark as candidate
        }
        
        // ===== INC16/DEC16 (16-bit increment/decrement) =====
        
        // Pattern: INC addr, BNE skip, INC addr+1, skip:
        // Can become: INW addr (increment word) if available
        
        // ===== PHW/PLW (Push/Pull Word) =====
        
        // Pattern: LDA low, PHA, LDA high, PHA
        // Can become: PHW value (push 16-bit word)
        if (i + 3 < prog->count &&
            strcmp(prog->lines[i].opcode, "LDA") == 0 &&
            strcmp(prog->lines[i+1].opcode, "PHA") == 0 &&
            strcmp(prog->lines[i+2].opcode, "LDA") == 0 &&
            strcmp(prog->lines[i+3].opcode, "PHA") == 0 &&
            !prog->lines[i+1].no_optimize && !prog->lines[i+2].no_optimize &&
            !prog->lines[i+3].no_optimize) {
            
            // This could be PHW if we can determine the 16-bit value
            // Complex pattern - needs address analysis
        }
        
        // ===== CPQ (Compare Q register) =====
        // 32-bit comparisons
        // Remember: Changes to A, X, Y, Z affect Q!
        
        // ===== MAP Instruction Optimizations =====
        // MAP instruction allows memory banking
        // Pattern: Multiple bank switches could be optimized
        

    }
    
    // Note: Q register optimization requires careful register tracking
    // Since Q = [Z:Y:X:A], any operation on Q affects all four registers
    // Similarly, operations on A, X, Y, or Z affect Q
    // This requires sophisticated data flow analysis
}

// Inline subroutines that are only called once
void optimize_inline_subroutines(Program *prog) {
    for (int i = 0; i < prog->label_count; i++) {
        Label *lbl = &prog->labels[i];
        
        // Only inline if:
        // 1. It's a subroutine (called via JSR)
        // 2. Referenced exactly once
        // 3. We found the RTS that ends it
        // 4. The subroutine is reasonably small
        if (!lbl->is_subroutine || lbl->ref_count != 1 || lbl->sub_end < 0) {
            continue;
        }
        
        int sub_size = lbl->sub_end - lbl->sub_start;
        
        // Don't inline very large subroutines (arbitrary limit)
        if (sub_size > 30) {
            continue;
        }
        
        // Check if subroutine is marked as no_optimize
        bool sub_no_opt = false;
        for (int j = lbl->sub_start; j <= lbl->sub_end; j++) {
            if (prog->lines[j].no_optimize) {
                sub_no_opt = true;
                break;
            }
        }
        if (sub_no_opt) {
            printf("  Skipping inlining of '%s' (marked no_optimize)\n", lbl->name);
            continue;
        }
        
        // Don't inline if subroutine contains JSR (nested calls - complex)
        bool has_jsr = false;
        for (int j = lbl->sub_start + 1; j <= lbl->sub_end; j++) {
            if (strcmp(prog->lines[j].opcode, "JSR") == 0) {
                has_jsr = true;
                break;
            }
        }
        if (has_jsr) continue;
        
        // Find the JSR call site
        int call_site = lbl->ref_lines[0];
        if (call_site < 0 || call_site >= prog->count) continue;
        if (strcmp(prog->lines[call_site].opcode, "JSR") != 0) continue;
        
        // Check if call site is marked as no_optimize
        if (prog->lines[call_site].no_optimize) {
            printf("  Skipping inlining of '%s' (call site marked no_optimize)\n", lbl->name);
            continue;
        }
        
        printf("  Inlining subroutine '%s' (size: %d) at line %d\n", 
               lbl->name, sub_size, call_site);
        
        // Mark the JSR instruction as dead
        prog->lines[call_site].is_dead = true;
        
        // Mark the subroutine label as dead (but preserve code)
        prog->lines[lbl->sub_start].is_dead = true;
        
        // Mark the RTS as dead
        prog->lines[lbl->sub_end].is_dead = true;
        
        // Copy subroutine body to call site
        // We need to insert lines, so we'll need to expand the array
        int insert_count = 0;
        for (int j = lbl->sub_start + 1; j < lbl->sub_end; j++) {
            if (!prog->lines[j].is_dead && prog->lines[j].opcode[0]) {
                insert_count++;
            }
        }
        
        if (insert_count > 0) {
            // Make room for inlined code
            if (prog->count + insert_count >= prog->capacity) {
                prog->capacity = prog->count + insert_count + 100;
                prog->lines = realloc(prog->lines, prog->capacity * sizeof(AsmLine));
            }
            
            // Shift existing code down
            for (int j = prog->count - 1; j > call_site; j--) {
                prog->lines[j + insert_count] = prog->lines[j];
            }
            
            // Copy inlined code
            int dest = call_site + 1;
            for (int j = lbl->sub_start + 1; j < lbl->sub_end; j++) {
                if (!prog->lines[j].is_dead && prog->lines[j].opcode[0]) {
                    prog->lines[dest] = prog->lines[j];
                    // Add comment showing this was inlined
                    char comment[MAX_LINE];
                    int needed = snprintf(comment, MAX_LINE, "%s ; inlined from %s", 
                            prog->lines[dest].line, lbl->name);
                    if (needed < MAX_LINE) {
                        snprintf(prog->lines[dest].line, MAX_LINE, "%s", comment);
                    }
                    prog->lines[dest].is_branch_target = false;
                    dest++;
                }
            }
            
            prog->count += insert_count;
            prog->optimizations++;
        }
        
        // Mark all original subroutine lines as dead
        for (int j = lbl->sub_start; j <= lbl->sub_end; j++) {
            prog->lines[j].is_dead = true;
        }
    }
}

// Main optimization routine
void optimize_program(Program *prog) {
    int prev_opts;
    int pass = 0;
    
    // First perform inlining (only once, at the beginning)
    printf("Performing subroutine inlining...\n");
    analyze_call_flow(prog);
    optimize_inline_subroutines(prog);
    
    // Multiple passes until no more optimizations found
    do {
        prev_opts = prog->optimizations;
        
        analyze_call_flow(prog);
        
                        // Basic optimizations
        
                        optimize_peephole(prog);
        
                        optimize_load_store(prog);
        
                        optimize_register_usage(prog);
        
                        optimize_constant_propagation(prog);
        
                
        
                        
        
                        // Arithmetic and logic
        
                        optimize_strength_reduction(prog);
        
                        optimize_arithmetic(prog);
        
                        optimize_bit_operations(prog);
        
                        optimize_boolean_logic(prog);
        
                        
        
                        // Control flow
        
                        optimize_flag_usage(prog);
        
                        optimize_tail_calls(prog);
        
                        optimize_branch_chaining(prog);
        
                        optimize_jumps(prog);
        
                        optimize_branches(prog);
        
                        
        
                        // Stack and addressing
        
                        optimize_stack_operations(prog);
        
                        optimize_addressing_modes(prog);
        
                        
        
                        // Advanced
        
                        optimize_common_subexpressions(prog);
        
                        
        
                        // CPU-specific
                        if (prog->trace_level > 1) {
                            printf("DEBUG optimize_program: Checking CPU-specific optimizations. allow_65c02=%d, is_45gs02=%d\n", prog->allow_65c02, prog->is_45gs02);
                        }
                        if (prog->allow_65c02 && !prog->is_45gs02) {
                            if (prog->trace_level > 1) {
                                printf("DEBUG optimize_program: Entering optimize_65c02_instructions.\n");
                            }
                            optimize_65c02_instructions(prog);
                        }
        
                        
        
                        if (prog->is_45gs02) {
        
                            optimize_45gs02_instructions(prog);
        
                        }
        
                        
        
                        // Must be last
        
                        optimize_dead_code(prog);
        
        pass++;
    } while (prog->optimizations > prev_opts && pass < 10);
    
    // Analysis-only passes (run once at end)
    if (prog->mode == OPT_SPEED) {
        optimize_loop_unrolling(prog);
    }
    optimize_zero_page(prog);
    optimize_loop_invariants(prog);
    
    printf("Optimization completed in %d passes\n", pass);
}

// Write optimized output
void write_output(Program *prog, const char *filename) {
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
    
    for (int i = 0; i < prog->count; i++) {
        if (!prog->lines[i].is_dead) {
            fprintf(fp, "%s\n", prog->lines[i].line);
        } else if (prog->trace_level > 0) {
            fprintf(fp, "%s OPT: Removed - %s\n", cmt, prog->lines[i].line);
        }
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
        add_line(prog, line, current_line_num++);
    }
    fclose(fp);
    
    // Count local vs global labels
    int local_count = 0;
    int global_count = 0;
    for (int i = 0; i < prog->count; i++) {
        if (prog->lines[i].is_label) {
            if (prog->lines[i].is_local_label) {
                local_count++;
            } else {
                global_count++;
            }
        }
    }
    printf("Read %d lines from %s\n", prog->count, input_file);
    printf("Optimizing for %s...\n", mode == OPT_SPEED ? "speed" : "size");
    
    if (prog->is_45gs02) {
        printf("\n** 45GS02 Mode: LDA #0, STA will NOT be converted to STZ **\n");
        printf("** Use LDZ #0, STZ if you want to store zero **\n");
    }
    printf("Found %d global labels and %d local labels\n", global_count, local_count);
    
    printf("\nOptimizer directives recognized:\n");
    printf("  %s#NOOPT  - Disable optimizations from this point\n", prog->config.comment_char);
    printf("  %s#OPT    - Re-enable optimizations from this point\n\n", prog->config.comment_char);
    
    // Perform optimizations
    optimize_program(prog);
    
    printf("\n=== Optimization Summary ===\n");
    printf("Applied %d optimizations\n", prog->optimizations);
    
    // Write output
    write_output(prog, output_file);
    printf("Wrote optimized code to %s\n", output_file);
    
    if (prog->trace_level > 0) { // Check trace_level for general trace messages
        printf("Optimization trace comments included in output (Level %d)\n", prog->trace_level);
    }
    
    // Statistics
    int lines_removed = 0;
    for (int i = 0; i < prog->count; i++) {
        if (prog->lines[i].is_dead) lines_removed++;
    }
    printf("Removed %d dead code lines\n", lines_removed);
    printf("Final line count: %d (%.1f%% reduction)\n", 
           prog->count - lines_removed,
           lines_removed > 0 ? (100.0 * lines_removed / prog->count) : 0.0);
    
    if (prog->is_45gs02) {
        printf("\n** Remember: On 45GS02, STZ stores the Z register! **\n");
    }
    
    free_program(prog);
    return 0;
}
