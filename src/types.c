/**
 * @file types.c
 * @brief Implementation of type utilities and assembler configuration
 *
 * Provides functions for working with assembler configurations, including
 * syntax rules, comment styles, and label formats for various assemblers.
 */

#include "types.h"
#include <string.h>
#include <strings.h>  // For strcasecmp on POSIX systems
#include <ctype.h>

/**
 * @brief Get assembler configuration for a given type
 *
 * Returns a static configuration structure containing syntax rules for
 * the specified assembler type. This includes comment character, label
 * format, and other assembler-specific syntax rules.
 *
 * @param type The assembler type to get configuration for
 * @return AsmConfig structure with syntax rules for the assembler
 *         Returns generic configuration if type is unknown
 */
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

/**
 * @brief Parse assembler type from string name
 *
 * Converts a string name (e.g., "ca65", "kick") into the corresponding
 * AsmType enum value. Case-insensitive comparison.
 *
 * @param type_str String name of assembler
 * @return Corresponding AsmType enum value, or ASM_GENERIC if unknown
 */
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

/**
 * @brief Check if string position starts a comment
 *
 * Checks if the current position in a string marks the start of a comment
 * based on the assembler's comment syntax (e.g., ';' or '//').
 *
 * @param p Pointer to string position to check
 * @param config Assembler configuration with comment syntax
 * @return true if position starts a comment, false otherwise
 */
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

/**
 * @brief Check if a label is a local label (scoped)
 *
 * Determines if a label is local (scoped) based on assembler syntax rules.
 * Local labels typically start with a special character (@, !, ., :) or
 * are purely numeric (e.g., "1", "2", "3" in DASM).
 *
 * @param label Label string to check
 * @param config Assembler configuration with local label rules
 * @return true if label is local, false if global
 */
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
