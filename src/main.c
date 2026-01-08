/**
 * @file main.c
 * @brief Main entry point for the 6502 assembly optimizer
 *
 * Command-line tool for optimizing 6502/65C02/45GS02 assembly code.
 * Reads assembly source, applies optimization passes, and writes
 * optimized output.
 *
 * Features:
 * - Multiple CPU targets (6502, 65C02, 65816, 45GS02)
 * - Multiple assembler syntaxes
 * - Speed vs size optimization modes
 * - Optimization tracing/debugging
 * - Optimizer control directives (#NOOPT, #OPT)
 *
 * Usage:
 *   opt6502 [-speed|-size] [-cpu <type>] [-asm <type>] [-trace <level>] input.asm [output.asm]
 */

#include "types.h"
#include "program/program.h"
#include "optimizations/optimizer.h"
#include "output/output.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/**
 * @brief Main entry point
 *
 * Processes command-line arguments, reads input assembly, performs
 * optimization passes, and writes optimized output.
 *
 * Command-line options:
 * - -speed: Optimize for execution speed (default)
 * - -size: Optimize for code size
 * - -cpu <type>: Target CPU (6502, 65c02, 65816, 45gs02)
 * - -asm <type>: Assembler syntax (ca65, kick, acme, dasm, etc.)
 * - -trace <level>: Optimization trace level (1=basic, 2=verbose)
 *
 * Special CPU notes:
 * - 65C02: Enables STZ instruction (stores zero)
 * - 45GS02: MEGA65 CPU - STZ stores Z register, not zero!
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, 1 on error
 */
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
