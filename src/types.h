/**
 * @file types.h
 * @brief Core type definitions for the 6502 assembly optimizer
 *
 * This file contains all fundamental data structures used throughout the optimizer,
 * including AST nodes, program state, register tracking, and assembler configurations.
 */

#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>

/* Configuration constants */
#define MAX_LINE 256      /**< Maximum length of an assembly line */
#define MAX_LINES 10000   /**< Maximum number of lines in a program */
#define MAX_LABELS 1000   /**< Maximum number of labels */
#define MAX_REFS 100      /**< Maximum number of references */

/**
 * @brief Optimization mode selection
 */
typedef enum {
    OPT_SPEED,  /**< Optimize for execution speed */
    OPT_SIZE    /**< Optimize for code size */
} OptMode;

/**
 * @brief Target CPU type
 */
typedef enum {
    CPU_6502,    /**< Original NMOS 6502 */
    CPU_65C02,   /**< CMOS 65C02 with additional instructions */
    CPU_65816,   /**< 65816 with 16-bit extensions */
    CPU_45GS02   /**< 45GS02 (MEGA65) - STZ stores Z register, not zero! */
} CpuType;

/**
 * @brief Assembler syntax type
 */
typedef enum {
    ASM_GENERIC,  /**< Generic - supports both ; and // comments */
    ASM_CA65,     /**< ca65 assembler */
    ASM_KICK,     /**< Kick Assembler */
    ASM_ACME,     /**< ACME Crossassembler */
    ASM_DASM,     /**< DASM */
    ASM_TASS,     /**< Turbo Assembler */
    ASM_64TASS,   /**< 64tass */
    ASM_BUDDY,    /**< Buddy Assembler */
    ASM_MERLIN,   /**< Merlin */
    ASM_LISA      /**< LISA */
} AsmType;

/**
 * @brief AST node type classification
 */
typedef enum {
    NODE_LABEL,      /**< Label definition */
    NODE_OPCODE,     /**< Opcode instruction */
    NODE_BRANCH,     /**< Branch instruction */
    NODE_JUMP,       /**< Jump instruction */
    NODE_LOAD,       /**< Load instruction */
    NODE_STORE,      /**< Store instruction */
    NODE_CONSTANT,   /**< Constant value */
    NODE_REGISTER,   /**< Register reference */
    NODE_EXPRESSION, /**< Expression */
    NODE_BLOCK,      /**< Code block */
    NODE_FUNCTION,   /**< Function definition */
    NODE_ASM_LINE    /**< Generic assembly line */
} NodeType;

/**
 * @brief Register and processor flag state tracking
 *
 * Tracks the known state of CPU registers and flags during optimization.
 * Used for constant propagation and detecting redundant operations.
 */
typedef struct {
    /* Register value tracking */
    bool a_known;       /**< Whether accumulator value is known */
    bool x_known;       /**< Whether X register value is known */
    bool y_known;       /**< Whether Y register value is known */
    bool z_known;       /**< Whether Z register value is known (45GS02 only) */

    bool a_zero;        /**< Whether accumulator is zero */
    bool x_zero;        /**< Whether X register is zero */
    bool y_zero;        /**< Whether Y register is zero */
    bool z_zero;        /**< Whether Z register is zero (45GS02 only) */

    char a_value[32];   /**< Known accumulator value string (e.g., "#$FF") */
    char x_value[32];   /**< Known X register value string */
    char y_value[32];   /**< Known Y register value string */
    char z_value[32];   /**< Known Z register value string (45GS02 only) */

    /* Modification tracking */
    bool a_modified;    /**< Whether accumulator was modified in current scope */
    bool x_modified;    /**< Whether X register was modified in current scope */
    bool y_modified;    /**< Whether Y register was modified in current scope */
    bool z_modified;    /**< Whether Z register was modified in current scope (45GS02 only) */

    /* Processor flags */
    bool c_known;       /**< Whether carry flag state is known */
    bool n_known;       /**< Whether negative flag state is known */
    bool z_flag_known;  /**< Whether zero flag state is known */
    bool v_known;       /**< Whether overflow flag state is known */

    bool c_set;         /**< Carry flag value (if known) */
    bool n_set;         /**< Negative flag value (if known) */
    bool z_flag_set;    /**< Zero flag value (if known) */
    bool v_set;         /**< Overflow flag value (if known) */
} RegisterState;

/**
 * @brief Abstract Syntax Tree node for assembly code
 *
 * Represents a single line or element of assembly code. Nodes are linked
 * together to form the complete program AST.
 */
typedef struct AstNode {
    NodeType type;              /**< Type of AST node */
    int line_num;               /**< Original line number in source */
    char* label;                /**< Label text (if present) */
    char* opcode;               /**< Instruction opcode */
    char* operand;              /**< Instruction operand */
    char* comment;              /**< Line comment (if present) */
    struct AstNode* next;       /**< Next node in sequence */
    struct AstNode* child;      /**< First child node */
    struct AstNode* sibling;    /**< Next sibling node */
    bool is_dead;               /**< Marked for removal (dead code) */
    bool no_optimize;           /**< Optimization disabled for this line */
    bool is_local_label;        /**< Label is local scope */
    bool is_branch_target;      /**< Label can be jumped/branched to */
    int optimization_count;     /**< Number of optimizations applied */
    RegisterState reg_state;    /**< Register state at this node */
} AstNode;

/**
 * @brief Assembler syntax configuration
 *
 * Defines syntax rules for a specific assembler (comment style, label format, etc.)
 */
typedef struct {
    AsmType type;                   /**< Assembler type identifier */
    const char *name;               /**< Human-readable assembler name */
    const char *comment_char;       /**< Comment character(s) (e.g., ";" or "//") */
    bool supports_colon_labels;     /**< Whether labels can end with ':' */
    bool case_sensitive;            /**< Whether opcodes are case-sensitive */
    const char *local_label_prefix; /**< Prefix for local labels (@ ! . :) */
    bool local_labels_numeric;      /**< Supports numeric local labels (1, 2, 3...) */
} AsmConfig;

/**
 * @brief Complete program state and configuration
 *
 * Contains the AST, optimization settings, and all state needed for
 * parsing, optimizing, and outputting assembly code.
 */
typedef struct {
    AstNode *root;              /**< Root of the AST */
    AstNode *current_node;      /**< Current node during parsing */
    int count;                  /**< Total line count */
    OptMode mode;               /**< Optimization mode (speed/size) */
    int optimizations;          /**< Number of optimizations applied */
    bool opt_enabled;           /**< Whether optimizations are currently enabled */
    AsmConfig config;           /**< Assembler syntax configuration */
    CpuType cpu_type;           /**< Target CPU type */
    bool allow_65c02;           /**< Allow 65C02 instructions */
    bool allow_undocumented;    /**< Allow undocumented opcodes */
    bool is_45gs02;             /**< Special 45GS02 mode (STZ stores Z register) */
    int trace_level;            /**< Optimization trace level (0=off, 1=basic, 2=verbose) */
} Program;

/* Assembler configuration functions */

/**
 * @brief Get assembler configuration for a given type
 * @param type The assembler type to get configuration for
 * @return AsmConfig structure with syntax rules for the assembler
 */
AsmConfig get_asm_config(AsmType type);

/**
 * @brief Parse assembler type from string name
 * @param type_str String name of assembler (e.g., "ca65", "kick")
 * @return Corresponding AsmType enum value, or ASM_GENERIC if unknown
 */
AsmType parse_asm_type(const char *type_str);

/**
 * @brief Check if string position starts a comment
 * @param p Pointer to string position to check
 * @param config Assembler configuration with comment syntax
 * @return true if position starts a comment, false otherwise
 */
bool is_comment_start(const char *p, AsmConfig *config);

/**
 * @brief Check if a label is a local label (scoped)
 * @param label Label string to check
 * @param config Assembler configuration with local label rules
 * @return true if label is local, false if global
 */
bool is_local_label(const char *label, AsmConfig *config);

#endif // TYPES_H
