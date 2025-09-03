#include "regex.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

// Use the same compilation logic as v2, just change the execution
// I'll copy the key parts and focus on the VM execution

// Character set helpers (same as before)
static void charset_add_char(uint8_t *charset, char c) {
    int bit = (unsigned char)c;
    charset[bit / 8] |= (1 << (bit % 8));
}

static void charset_add_range(uint8_t *charset, char start, char end) {
    for (char c = start; c <= end; c++) {
        charset_add_char(charset, c);
    }
}

static int charset_contains(uint8_t *charset, char c) {
    int bit = (unsigned char)c;
    return (charset[bit / 8] & (1 << (bit % 8))) != 0;
}

// Compiler (simplified - just support basic patterns for now)
typedef struct {
    const char *pattern;
    int pos;
    int len;
    Instruction *code;
    int code_len;
    int code_capacity;
    int group_count;
    int flags;
} Compiler;

static void emit_op(Compiler *c, OpCode op) {
    if (c->code_len >= c->code_capacity) {
        c->code_capacity *= 2;
        c->code = realloc(c->code, c->code_capacity * sizeof(Instruction));
    }
    c->code[c->code_len].op = op;
    c->code_len++;
}

static void emit_char(Compiler *c, char ch) {
    emit_op(c, OP_CHAR);
    c->code[c->code_len - 1].c = ch;
}

static void emit_jump(Compiler *c, OpCode op, int addr) {
    emit_op(c, op);
    c->code[c->code_len - 1].addr = addr;
}

// Helper function to ensure bytecode buffer capacity
static void ensure_capacity(CompiledRegex *regex, int needed) {
    while (regex->code_len + needed >= regex->code_capacity) {
        regex->code_capacity *= 2;
        regex->code = realloc(regex->code, regex->code_capacity * sizeof(Instruction));
    }
}

// Helper function to safely add an instruction
static int emit_instruction(CompiledRegex *regex, OpCode op) {
    ensure_capacity(regex, 1);
    regex->code[regex->code_len].op = op;
    return regex->code_len++;
}

// Enhanced compilation to handle basic patterns
CompiledRegex* compile_regex(const char *pattern, int flags) {
    CompiledRegex *regex = malloc(sizeof(CompiledRegex));
    regex->code = malloc(64 * sizeof(Instruction));
    regex->code_len = 0;
    regex->code_capacity = 64;
    regex->group_count = 1;
    regex->flags = flags;
    
    // Handle empty pattern - matches everything
    if (!pattern || strlen(pattern) == 0) {
        int start_group_pc = emit_instruction(regex, OP_SAVE_GROUP);
        regex->code[start_group_pc].group_num = 0;
        regex->code[start_group_pc].is_end = 0;
        
        int end_group_pc = emit_instruction(regex, OP_SAVE_GROUP);
        regex->code[end_group_pc].group_num = 0;
        regex->code[end_group_pc].is_end = 1;
        
        emit_instruction(regex, OP_MATCH);
        regex->code_len = regex->code_len;
        return regex;
    }
    
    
    // General pattern compilation
    int pc = 0;
    
    // SAVE_GROUP 0 START
    int start_group_pc = emit_instruction(regex, OP_SAVE_GROUP);
    regex->code[start_group_pc].group_num = 0;
    regex->code[start_group_pc].is_end = 0;
    pc = regex->code_len;
    
    // Compile each character in the pattern
    int pattern_len = strlen(pattern);
    for (int i = 0; i < pattern_len; i++) {
        char ch = pattern[i];
        
        // Handle anchors
        if (ch == '^') {
            emit_instruction(regex, OP_ANCHOR_START);
            pc = regex->code_len;
            continue;
        }
        if (ch == '$') {
            emit_instruction(regex, OP_ANCHOR_END);
            pc = regex->code_len;
            continue;
        }
        
        // Handle escape sequences
        if (ch == '\\' && i + 1 < pattern_len) {
            char escape_char = pattern[i + 1];
            
            // Handle character class escapes
            if (escape_char == 'd' || escape_char == 'D' ||
                escape_char == 'w' || escape_char == 'W' ||
                escape_char == 's' || escape_char == 'S') {
                
                int charset_pc = emit_instruction(regex, OP_CHARSET);
                memset(regex->code[charset_pc].charset, 0, sizeof(regex->code[charset_pc].charset));
                
                if (escape_char == 'd') {
                    // \d matches [0-9]
                    regex->code[charset_pc].negate = 0;
                    for (char c = '0'; c <= '9'; c++) {
                        int bit = (unsigned char)c;
                        regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                    }
                } else if (escape_char == 'D') {
                    // \D matches [^0-9]
                    regex->code[charset_pc].negate = 1;
                    for (char c = '0'; c <= '9'; c++) {
                        int bit = (unsigned char)c;
                        regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                    }
                } else if (escape_char == 'w') {
                    // \w matches [a-zA-Z0-9_]
                    regex->code[charset_pc].negate = 0;
                    for (char c = 'a'; c <= 'z'; c++) {
                        int bit = (unsigned char)c;
                        regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                    }
                    for (char c = 'A'; c <= 'Z'; c++) {
                        int bit = (unsigned char)c;
                        regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                    }
                    for (char c = '0'; c <= '9'; c++) {
                        int bit = (unsigned char)c;
                        regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                    }
                    int bit = (unsigned char)'_';
                    regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                } else if (escape_char == 'W') {
                    // \W matches [^a-zA-Z0-9_]
                    regex->code[charset_pc].negate = 1;
                    for (char c = 'a'; c <= 'z'; c++) {
                        int bit = (unsigned char)c;
                        regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                    }
                    for (char c = 'A'; c <= 'Z'; c++) {
                        int bit = (unsigned char)c;
                        regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                    }
                    for (char c = '0'; c <= '9'; c++) {
                        int bit = (unsigned char)c;
                        regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                    }
                    int bit = (unsigned char)'_';
                    regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                } else if (escape_char == 's') {
                    // \s matches [ \t\n\r\f\v]
                    regex->code[charset_pc].negate = 0;
                    int space_chars[] = {' ', '\t', '\n', '\r', '\f', '\v'};
                    for (int j = 0; j < 6; j++) {
                        int bit = (unsigned char)space_chars[j];
                        regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                    }
                } else if (escape_char == 'S') {
                    // \S matches [^ \t\n\r\f\v]
                    regex->code[charset_pc].negate = 1;
                    int space_chars[] = {' ', '\t', '\n', '\r', '\f', '\v'};
                    for (int j = 0; j < 6; j++) {
                        int bit = (unsigned char)space_chars[j];
                        regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                    }
                }
                
                pc = regex->code_len;
                i += 2; // Skip both \ and the escape character
                continue;
            }
            
            // Handle literal escapes
            if (escape_char == 'n' || escape_char == 't' || escape_char == 'r') {
                int char_pc = emit_instruction(regex, OP_CHAR);
                if (escape_char == 'n') {
                    regex->code[char_pc].c = '\n';
                } else if (escape_char == 't') {
                    regex->code[char_pc].c = '\t';
                } else if (escape_char == 'r') {
                    regex->code[char_pc].c = '\r';
                }
                pc = regex->code_len;
                i += 2; // Skip both \ and the escape character
                continue;
            }
            
            // For other escapes, treat as literal (e.g., \. becomes .)
            int char_pc = emit_instruction(regex, OP_CHAR);
            regex->code[char_pc].c = escape_char;
            pc = regex->code_len;
            i += 2; // Skip both \ and the escaped character
            continue;
        }
        
        // Handle character classes [...]
        if (ch == '[') {
            // Parse character class
            int class_start = i + 1;
            int negate = 0;
            
            // Check for negation
            if (class_start < pattern_len && pattern[class_start] == '^') {
                negate = 1;
                class_start++;
            }
            
            int class_end = class_start;
            
            // Find the closing ]
            class_end = class_start;
            while (class_end < pattern_len && pattern[class_end] != ']') {
                class_end++;
            }
            
            if (class_end >= pattern_len) {
                // Malformed character class - treat [ as literal
                int char_pc = emit_instruction(regex, OP_CHAR);
                regex->code[char_pc].c = ch;
                pc = regex->code_len;
                continue;
            }
            
            // Build character class
            int charset_pc = emit_instruction(regex, OP_CHARSET);
            memset(regex->code[charset_pc].charset, 0, sizeof(regex->code[charset_pc].charset));
            regex->code[charset_pc].negate = negate;
            
            // Parse the character class content
            for (int j = class_start; j < class_end; j++) {
                // Handle escape sequences within character classes
                if (pattern[j] == '\\' && j + 1 < class_end) {
                    char escape_char = pattern[j + 1];
                    
                    if (escape_char == 'd') {
                        // \d matches [0-9]
                        for (char c = '0'; c <= '9'; c++) {
                            int bit = (unsigned char)c;
                            regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                        }
                    } else if (escape_char == 'w') {
                        // \w matches [a-zA-Z0-9_]
                        for (char c = 'a'; c <= 'z'; c++) {
                            int bit = (unsigned char)c;
                            regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                        }
                        for (char c = 'A'; c <= 'Z'; c++) {
                            int bit = (unsigned char)c;
                            regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                        }
                        for (char c = '0'; c <= '9'; c++) {
                            int bit = (unsigned char)c;
                            regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                        }
                        int bit = (unsigned char)'_';
                        regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                    } else if (escape_char == 's') {
                        // \s matches [ \t\n\r\f\v]
                        int space_chars[] = {' ', '\t', '\n', '\r', '\f', '\v'};
                        for (int k = 0; k < 6; k++) {
                            int bit = (unsigned char)space_chars[k];
                            regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                        }
                    } else if (escape_char == 'n') {
                        int bit = (unsigned char)'\n';
                        regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                    } else if (escape_char == 't') {
                        int bit = (unsigned char)'\t';
                        regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                    } else if (escape_char == 'r') {
                        int bit = (unsigned char)'\r';
                        regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                    } else {
                        // Treat as literal escaped character
                        int bit = (unsigned char)escape_char;
                        regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                    }
                    
                    j++; // Skip the escape character
                } else if (j + 2 < class_end && pattern[j + 1] == '-') {
                    // Character range: a-z (but make sure j+2 is not the last char)
                    char start_char = pattern[j];
                    char end_char = pattern[j + 2];
                    for (char c = start_char; c <= end_char; c++) {
                        int bit = (unsigned char)c;
                        regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                    }
                    j += 2; // Skip the range
                } else {
                    // Individual character
                    char class_char = pattern[j];
                    int bit = (unsigned char)class_char;
                    regex->code[charset_pc].charset[bit / 8] |= (1 << (bit % 8));
                }
            }
            
            pc = regex->code_len;
            i = class_end; // Skip to after the ]
            continue;
        }
        
        // Check for quantifiers - but character classes and escapes are already handled above
        if (i + 1 < pattern_len && (pattern[i + 1] == '*' || pattern[i + 1] == '+' || pattern[i + 1] == '?' || pattern[i + 1] == '{')) {
            char quantifier = pattern[i + 1];
            
            if (quantifier == '*') {
                // Zero-or-more quantifier: X*
                int choice_addr = pc;
                
                // CHOICE +N (to skip the loop)
                int choice_pc = emit_instruction(regex, OP_CHOICE);
                pc = regex->code_len;
                
                // SAVE_POINTER
                emit_instruction(regex, OP_SAVE_POINTER);
                pc = regex->code_len;
                
                // The character/pattern to repeat
                if (ch == '.') {
                    emit_instruction(regex, OP_DOT);
                } else {
                    int char_pc = emit_instruction(regex, OP_CHAR);
                    regex->code[char_pc].c = ch;
                }
                pc = regex->code_len;
                
                // ZERO_LENGTH
                emit_instruction(regex, OP_ZERO_LENGTH);
                pc = regex->code_len;
                
                // BRANCH_IF_NOT back to choice
                int branch_pc = emit_instruction(regex, OP_BRANCH_IF_NOT);
                regex->code[branch_pc].addr = choice_addr - branch_pc;
                pc = regex->code_len;
                
                // Patch the CHOICE instruction to jump here
                regex->code[choice_pc].addr = pc - choice_pc;
                
                i++; // Skip the quantifier character
            } else if (quantifier == '+') {
                // One-or-more quantifier: X+
                // First match the character at least once
                if (ch == '.') {
                    emit_instruction(regex, OP_DOT);
                } else {
                    int char_pc = emit_instruction(regex, OP_CHAR);
                    regex->code[char_pc].c = ch;
                }
                pc = regex->code_len;
                
                // Then add X* pattern for additional matches
                int choice_addr = pc;
                
                // CHOICE +N (to skip the loop)
                int choice_pc = emit_instruction(regex, OP_CHOICE);
                pc = regex->code_len;
                
                // SAVE_POINTER
                emit_instruction(regex, OP_SAVE_POINTER);
                pc = regex->code_len;
                
                // The character/pattern to repeat
                if (ch == '.') {
                    emit_instruction(regex, OP_DOT);
                } else {
                    int char_pc = emit_instruction(regex, OP_CHAR);
                    regex->code[char_pc].c = ch;
                }
                pc = regex->code_len;
                
                // ZERO_LENGTH
                emit_instruction(regex, OP_ZERO_LENGTH);
                pc = regex->code_len;
                
                // BRANCH_IF_NOT back to choice
                int branch_pc = emit_instruction(regex, OP_BRANCH_IF_NOT);
                regex->code[branch_pc].addr = choice_addr - branch_pc;
                pc = regex->code_len;
                
                // Patch the CHOICE instruction to jump here
                regex->code[choice_pc].addr = pc - choice_pc;
                
                i++; // Skip the quantifier character
            } else if (quantifier == '?') {
                // Zero-or-one quantifier: X?
                // CHOICE +N (to skip the optional part)
                int choice_pc = emit_instruction(regex, OP_CHOICE);
                pc = regex->code_len;
                
                // The optional character/pattern
                if (ch == '.') {
                    emit_instruction(regex, OP_DOT);
                } else {
                    int char_pc = emit_instruction(regex, OP_CHAR);
                    regex->code[char_pc].c = ch;
                }
                pc = regex->code_len;
                
                // Patch the CHOICE instruction to jump here
                regex->code[choice_pc].addr = pc - choice_pc;
                
                i++; // Skip the quantifier character
            } else if (quantifier == '{') {
                // Exact quantifiers: {n} or {n,m}
                int brace_start = i + 1;
                int brace_end = brace_start + 1;
                
                // Find the closing }
                while (brace_end < pattern_len && pattern[brace_end] != '}') {
                    brace_end++;
                }
                
                if (brace_end >= pattern_len) {
                    // Malformed quantifier - treat as literal
                    int char_pc = emit_instruction(regex, OP_CHAR);
                    regex->code[char_pc].c = ch;
                    pc = regex->code_len;
                    continue;
                }
                
                // Parse the quantifier content
                int min_count = 0, max_count = 0;
                int has_comma = 0;
                
                // Simple parser for {n} or {n,m}
                int j = brace_start + 1;
                while (j < brace_end && pattern[j] >= '0' && pattern[j] <= '9') {
                    min_count = min_count * 10 + (pattern[j] - '0');
                    j++;
                }
                
                if (j < brace_end && pattern[j] == ',') {
                    has_comma = 1;
                    j++;
                    while (j < brace_end && pattern[j] >= '0' && pattern[j] <= '9') {
                        max_count = max_count * 10 + (pattern[j] - '0');
                        j++;
                    }
                    // For {n,} case, max_count remains 0 - we'll handle this as unlimited later
                } else {
                    max_count = min_count; // {n} same as {n,n}
                }
                
                // Generate code for exact repetition
                // For {n,m}, generate n required matches, then (m-n) optional matches
                for (int rep = 0; rep < min_count; rep++) {
                    if (ch == '.') {
                        emit_instruction(regex, OP_DOT);
                    } else {
                        int char_pc = emit_instruction(regex, OP_CHAR);
                        regex->code[char_pc].c = ch;
                    }
                    pc = regex->code_len;
                }
                
                // Handle {n,} case (unlimited) vs {n,m} case (limited)
                if (has_comma && max_count == 0) {
                    // {n,} case - unlimited repetition (like X*)
                    int choice_addr = pc;
                    
                    // CHOICE +N (to skip the loop)
                    int choice_pc_unlim = emit_instruction(regex, OP_CHOICE);
                    pc = regex->code_len;
                    
                    // SAVE_POINTER
                    emit_instruction(regex, OP_SAVE_POINTER);
                    pc = regex->code_len;
                    
                    // The character/pattern to repeat
                    if (ch == '.') {
                        emit_instruction(regex, OP_DOT);
                    } else {
                        int char_pc = emit_instruction(regex, OP_CHAR);
                        regex->code[char_pc].c = ch;
                    }
                    pc = regex->code_len;
                    
                    // ZERO_LENGTH
                    emit_instruction(regex, OP_ZERO_LENGTH);
                    pc = regex->code_len;
                    
                    // BRANCH_IF_NOT back to choice
                    int branch_pc = emit_instruction(regex, OP_BRANCH_IF_NOT);
                    regex->code[branch_pc].addr = choice_addr - branch_pc;
                    pc = regex->code_len;
                    
                    // Patch the CHOICE instruction to jump here
                    regex->code[choice_pc_unlim].addr = pc - choice_pc_unlim;
                } else {
                    // {n,m} case - limited repetition
                    // Add optional matches for {n,m} where m > n
                    for (int rep = min_count; rep < max_count; rep++) {
                        // CHOICE +N (to skip this optional match)
                        int choice_pc_opt = emit_instruction(regex, OP_CHOICE);
                        pc = regex->code_len;
                        
                        // The optional character/pattern
                        if (ch == '.') {
                            emit_instruction(regex, OP_DOT);
                        } else {
                            int char_pc = emit_instruction(regex, OP_CHAR);
                            regex->code[char_pc].c = ch;
                        }
                        pc = regex->code_len;
                        
                        // Patch the CHOICE instruction
                        regex->code[choice_pc_opt].addr = pc - choice_pc_opt;
                    }
                }
                
                i = brace_end; // Skip to the }, main loop will i++ to next char
            } else {
                i++; // Skip the quantifier character (*, +, ?), main loop will i++ to next char
            }
            continue; // Skip regular character processing since we handled quantifier
        } else {
            // Regular character or dot (character classes handled above)
            if (ch == '.') {
                emit_instruction(regex, OP_DOT);
            } else {
                int char_pc = emit_instruction(regex, OP_CHAR);
                regex->code[char_pc].c = ch;
            }
            pc = regex->code_len;
        }
    }
    
    // SAVE_GROUP 0 END
    int save_group_pc = emit_instruction(regex, OP_SAVE_GROUP);
    regex->code[save_group_pc].group_num = 0;
    regex->code[save_group_pc].is_end = 1;
    pc = regex->code_len;
    
    // MATCH
    emit_instruction(regex, OP_MATCH);
    pc = regex->code_len;
    
    regex->code_len = pc;
    return regex;
}

// VM execution with simplified integer data stack
static void push_choice(VM *vm, int alt_pc) {
    if (vm->choice_top >= vm->choice_capacity) {
        vm->choice_capacity *= 2;
        vm->choice_stack = realloc(vm->choice_stack, 
                                 vm->choice_capacity * sizeof(struct ChoicePoint));
    }
    
    struct ChoicePoint *cp = &vm->choice_stack[vm->choice_top++];
    cp->pc = alt_pc;
    cp->pos = vm->pos;
    cp->flags = vm->flags;
    cp->last_operation_success = vm->last_operation_success;
    cp->transferred = 0;  // Not transferred yet
    
    // Take immutable snapshot of integer data stack - ALWAYS retain when storing
    cp->data_stack = vm->data_stack;
    int_stack_retain(cp->data_stack);
    
    // Copy capture groups
    cp->group_starts = malloc(vm->group_count * sizeof(int));
    cp->group_ends = malloc(vm->group_count * sizeof(int));
    memcpy(cp->group_starts, vm->group_starts, vm->group_count * sizeof(int));
    memcpy(cp->group_ends, vm->group_ends, vm->group_count * sizeof(int));
}

static int pop_choice(VM *vm) {
    if (vm->choice_top <= 0) return 0;
    
    vm->choice_count++;
    if (vm->choice_count > vm->max_choices) return 0;
    
    struct ChoicePoint *cp = &vm->choice_stack[--vm->choice_top];
    vm->pc = cp->pc;
    vm->pos = cp->pos;
    vm->flags = cp->flags;
    vm->last_operation_success = cp->last_operation_success;
    
    // Restore immutable integer data stack - transfer ownership
    int_stack_release(vm->data_stack);        // Release current VM stack
    vm->data_stack = cp->data_stack;          // Transfer ownership - don't release!
    cp->transferred = 1;                      // Mark as transferred
    
    // Restore capture groups
    memcpy(vm->group_starts, cp->group_starts, vm->group_count * sizeof(int));
    memcpy(vm->group_ends, cp->group_ends, vm->group_count * sizeof(int));
    
    free(cp->group_starts);
    free(cp->group_ends);
    
    return 1;
}

static int vm_execute(CompiledRegex *compiled, VM *vm) {
    int instruction_count = 0;
    const int max_instructions = 100000;
    
    while (vm->pc < compiled->code_len && instruction_count < max_instructions) {
        instruction_count++;
        Instruction *inst = &compiled->code[vm->pc];
        
        switch (inst->op) {
            case OP_CHAR:
                if (vm->pos >= vm->text_len || vm->text[vm->pos] != inst->c) {
                    vm->last_operation_success = 0;
                    if (!pop_choice(vm)) return 0;
                    continue;
                }
                vm->pos++;
                vm->pc++;
                vm->last_match_was_zero_length = 0;
                vm->last_operation_success = 1;
                break;
                
            case OP_DOT:
                if (vm->pos >= vm->text_len) {
                    vm->last_operation_success = 0;
                    if (!pop_choice(vm)) return 0;
                    continue;
                }
                // Check if we should match newlines (dotall flag)
                char ch = vm->text[vm->pos];
                if (ch == '\n' && !(vm->flags & 1)) {  // 's' flag not set
                    vm->last_operation_success = 0;
                    if (!pop_choice(vm)) return 0;
                    continue;
                }
                vm->pos++;
                vm->pc++;
                vm->last_match_was_zero_length = 0;
                vm->last_operation_success = 1;
                break;
                
            case OP_CHARSET:
                if (vm->pos >= vm->text_len) {
                    vm->last_operation_success = 0;
                    if (!pop_choice(vm)) return 0;
                    continue;
                }
                // Check if character matches the character class
                char test_ch = vm->text[vm->pos];
                int bit = (unsigned char)test_ch;
                int matches = (inst->charset[bit / 8] & (1 << (bit % 8))) != 0;
                
                // Apply negation if needed
                if (inst->negate) {
                    matches = !matches;
                }
                
                if (matches) {
                    vm->pos++;
                    vm->pc++;
                    vm->last_match_was_zero_length = 0;
                    vm->last_operation_success = 1;
                } else {
                    vm->last_operation_success = 0;
                    if (!pop_choice(vm)) return 0;
                }
                break;
                
            case OP_CHOICE:
                push_choice(vm, vm->pc + inst->addr);
                vm->pc++;
                break;
                
            case OP_BRANCH:
                vm->pc += inst->addr;
                break;
                
            case OP_BRANCH_IF_NOT:
                // Branch back if the last operation was successful (to continue the loop)
                if (vm->last_operation_success) {
                    vm->pc += inst->addr;
                } else {
                    vm->pc++;
                }
                break;
                
            case OP_SAVE_POINTER: {
                // Push current position to integer data stack
                IntStack *new_stack = int_stack_push(vm->data_stack, vm->pos);
                int_stack_release(vm->data_stack);
                vm->data_stack = new_stack;
                vm->pc++;
                break;
            }
            
            case OP_RESTORE_POSITION: {
                // Pop position from integer data stack
                int saved_pos;
                IntStack *new_stack = int_stack_pop(vm->data_stack, &saved_pos);
                vm->pos = saved_pos;
                int_stack_release(vm->data_stack);
                vm->data_stack = new_stack;
                vm->pc++;
                break;
            }
            
            case OP_SAVE_GROUP:
                if (inst->is_end) {
                    vm->group_ends[inst->group_num] = vm->pos;
                } else {
                    vm->group_starts[inst->group_num] = vm->pos;
                }
                vm->pc++;
                break;
                
            case OP_ZERO_LENGTH:
                // For now, just continue (need proper zero-length detection)
                vm->pc++;
                break;
                
            case OP_ANCHOR_START:
                // Match start of string, or start of line in multiline mode
                if (vm->pos == 0) {
                    // At start of string - always matches
                    vm->pc++;
                    vm->last_operation_success = 1;
                } else if ((vm->flags & 8) && vm->pos > 0 && vm->text[vm->pos - 1] == '\n') {
                    // Multiline mode and after a newline - matches start of line
                    vm->pc++;
                    vm->last_operation_success = 1;
                } else {
                    // Doesn't match
                    vm->last_operation_success = 0;
                    if (!pop_choice(vm)) return 0;
                }
                break;
                
            case OP_ANCHOR_END:
                // Match end of string, or end of line in multiline mode
                if (vm->pos == vm->text_len) {
                    // At end of string - always matches
                    vm->pc++;
                    vm->last_operation_success = 1;
                } else if ((vm->flags & 8) && vm->text[vm->pos] == '\n') {
                    // Multiline mode and before a newline - matches end of line
                    vm->pc++;
                    vm->last_operation_success = 1;
                } else {
                    // Doesn't match
                    vm->last_operation_success = 0;
                    if (!pop_choice(vm)) return 0;
                }
                break;
                
            case OP_MATCH:
                return 1;
                
            case OP_FAIL:
                if (!pop_choice(vm)) return 0;
                break;
                
            default:
                vm->pc++;
                break;
        }
    }
    
    return 0;
}

int execute_regex(CompiledRegex *compiled, const char *text, int start_pos) {
    if (!compiled || !text) return 0;
    
    int text_len = strlen(text);
    
    for (int pos = start_pos; pos <= text_len; pos++) {
        VM vm = {
            .text = text,
            .text_len = text_len,
            .pc = 0,
            .pos = pos,
            .data_stack = int_stack_new(),
            .choice_stack = malloc(64 * sizeof(struct ChoicePoint)),
            .choice_top = 0,
            .choice_capacity = 64,
            .choice_count = 0,
            .max_choices = 10000,
            .flags = compiled->flags,
            .last_match_was_zero_length = 0,
            .last_operation_success = 0,
            .group_count = compiled->group_count
        };
        
        // Initialize capture groups
        vm.group_starts = malloc(compiled->group_count * sizeof(int));
        vm.group_ends = malloc(compiled->group_count * sizeof(int));
        for (int i = 0; i < compiled->group_count; i++) {
            vm.group_starts[i] = -1;
            vm.group_ends[i] = -1;
        }
        
        int result = vm_execute(compiled, &vm);
        
        // Cleanup
        int_stack_release(vm.data_stack);
        for (int i = 0; i < vm.choice_top; i++) {
            // Only release if not transferred back to VM
            if (!vm.choice_stack[i].transferred) {
                int_stack_release(vm.choice_stack[i].data_stack);
            }
            free(vm.choice_stack[i].group_starts);
            free(vm.choice_stack[i].group_ends);
        }
        free(vm.choice_stack);
        free(vm.group_starts);
        free(vm.group_ends);
        
        if (result) return 1;
    }
    
    return 0;
}

void free_regex(CompiledRegex *compiled) {
    if (compiled) {
        free(compiled->code);
        free(compiled);
    }
}

void print_regex_bytecode(CompiledRegex *compiled) {
    printf("Bytecode (%d instructions):\n", compiled->code_len);
    for (int i = 0; i < compiled->code_len; i++) {
        printf("%3d: ", i);
        switch (compiled->code[i].op) {
            case OP_CHAR: printf("CHAR '%c'", compiled->code[i].c); break;
            case OP_DOT: printf("DOT"); break;
            case OP_CHARSET: 
                printf("CHARSET%s [", compiled->code[i].negate ? " (negated)" : "");
                for (int j = 0; j < 256; j++) {
                    if (compiled->code[i].charset[j / 8] & (1 << (j % 8))) {
                        if (j >= 32 && j < 127) {
                            printf("%c", j);
                        } else {
                            printf("\\x%02x", j);
                        }
                    }
                }
                printf("]");
                break;
            case OP_CHOICE: printf("CHOICE +%d (to %d)", compiled->code[i].addr, i + compiled->code[i].addr); break;
            case OP_BRANCH: printf("BRANCH +%d (to %d)", compiled->code[i].addr, i + compiled->code[i].addr); break;
            case OP_BRANCH_IF_NOT: printf("BRANCH_IF_NOT +%d (to %d)", compiled->code[i].addr, i + compiled->code[i].addr); break;
            case OP_SAVE_POINTER: printf("SAVE_POINTER"); break;
            case OP_RESTORE_POSITION: printf("RESTORE_POSITION"); break;
            case OP_ZERO_LENGTH: printf("ZERO_LENGTH"); break;
            case OP_SAVE_GROUP: printf("SAVE_GROUP %d %s", 
                compiled->code[i].group_num, 
                compiled->code[i].is_end ? "END" : "START"); break;
            case OP_ANCHOR_START: printf("ANCHOR_START"); break;
            case OP_ANCHOR_END: printf("ANCHOR_END"); break;
            case OP_MATCH: printf("MATCH"); break;
            default: printf("UNKNOWN(%d)", compiled->code[i].op); break;
        }
        printf("\n");
    }
}

// Compatibility API implementation
RegExp* regex_new(const char *pattern, const char *flags) {
    RegExp *regexp = malloc(sizeof(RegExp));
    regexp->pattern = strdup(pattern);
    regexp->flags = strdup(flags);
    regexp->last_index = 0;
    
    // Parse flags to integer
    int flag_bits = 0;
    if (flags) {
        for (int i = 0; flags[i]; i++) {
            switch (flags[i]) {
                case 's': flag_bits |= 1; break;  // dotall
                case 'i': flag_bits |= 2; break;  // ignorecase  
                case 'g': flag_bits |= 4; break;  // global
                case 'm': flag_bits |= 8; break;  // multiline
                // Add other flags as needed
            }
        }
    }
    
    regexp->compiled = compile_regex(pattern, flag_bits);
    
    return regexp;
}

int regex_test(RegExp *regexp, const char *text) {
    if (!regexp || !regexp->compiled || !text) return 0;
    return execute_regex(regexp->compiled, text, 0);
}

MatchResult* regex_exec(RegExp *regexp, const char *text) {
    if (!regexp || !regexp->compiled || !text) return NULL;
    
    int result = execute_regex(regexp->compiled, text, 0);
    if (!result) return NULL;
    
    // Create a basic match result - for now just the full match
    MatchResult *match = malloc(sizeof(MatchResult));
    match->group_count = 1;
    match->groups = malloc(sizeof(char*) * 1);
    match->groups[0] = strdup(text);  // For now, return the full input as match
    match->index = 0;
    match->input = strdup(text);
    
    return match;
}

void regex_free(RegExp *regexp) {
    if (regexp) {
        if (regexp->compiled) free_regex(regexp->compiled);
        if (regexp->pattern) free(regexp->pattern);
        if (regexp->flags) free(regexp->flags);
        free(regexp);
    }
}

void match_result_free(MatchResult *result) {
    if (result) {
        if (result->groups) {
            for (int i = 0; i < result->group_count; i++) {
                if (result->groups[i]) free(result->groups[i]);
            }
            free(result->groups);
        }
        if (result->input) free(result->input);
        free(result);
    }
}

// String methods - minimal implementations for compatibility
MatchResult* string_match(const char *text, RegExp *regexp) {
    if (!text || !regexp) return NULL;
    return regex_exec(regexp, text);
}

MatchIterator* string_match_all(const char *text, RegExp *regexp) {
    if (!text || !regexp) return NULL;
    
    MatchIterator *iter = malloc(sizeof(MatchIterator));
    iter->regexp = regexp;
    iter->text = text;
    iter->pos = 0;
    iter->done = 0;
    
    return iter;
}

MatchResult* match_iterator_next(MatchIterator *iter) {
    if (!iter || iter->done) return NULL;
    
    // For now, just return one match and mark done
    // This is a simplified implementation
    MatchResult *result = regex_exec(iter->regexp, iter->text);
    iter->done = 1;  // Mark as done after first match
    
    return result;
}

void match_iterator_free(MatchIterator *iter) {
    if (iter) {
        free(iter);
    }
}