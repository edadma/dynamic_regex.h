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
        regex->code[0].op = OP_SAVE_GROUP;
        regex->code[0].group_num = 0;
        regex->code[0].is_end = 0;
        
        regex->code[1].op = OP_SAVE_GROUP;
        regex->code[1].group_num = 0;
        regex->code[1].is_end = 1;
        
        regex->code[2].op = OP_MATCH;
        regex->code_len = 3;
        return regex;
    }
    
    
    // General pattern compilation
    int pc = 0;
    
    // SAVE_GROUP 0 START
    regex->code[pc].op = OP_SAVE_GROUP;
    regex->code[pc].group_num = 0;
    regex->code[pc].is_end = 0;
    pc++;
    
    // Compile each character in the pattern
    int pattern_len = strlen(pattern);
    for (int i = 0; i < pattern_len; i++) {
        char ch = pattern[i];
        
        // Handle anchors
        if (ch == '^') {
            regex->code[pc].op = OP_ANCHOR_START;
            pc++;
            continue;
        }
        if (ch == '$') {
            regex->code[pc].op = OP_ANCHOR_END;
            pc++;
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
                regex->code[pc].op = OP_CHAR;
                regex->code[pc].c = ch;
                pc++;
                continue;
            }
            
            // Build character class
            regex->code[pc].op = OP_CHARSET;
            memset(regex->code[pc].charset, 0, sizeof(regex->code[pc].charset));
            regex->code[pc].negate = negate;
            
            // Parse the character class content
            for (int j = class_start; j < class_end; j++) {
                if (j + 2 < class_end && pattern[j + 1] == '-') {
                    // Character range: a-z (but make sure j+2 is not the last char)
                    char start_char = pattern[j];
                    char end_char = pattern[j + 2];
                    for (char c = start_char; c <= end_char; c++) {
                        int bit = (unsigned char)c;
                        regex->code[pc].charset[bit / 8] |= (1 << (bit % 8));
                    }
                    j += 2; // Skip the range
                } else {
                    // Individual character
                    char class_char = pattern[j];
                    int bit = (unsigned char)class_char;
                    regex->code[pc].charset[bit / 8] |= (1 << (bit % 8));
                }
            }
            
            pc++;
            i = class_end; // Skip to after the ]
            continue;
        }
        
        // Check for quantifiers - but character classes are already handled above
        if (i + 1 < pattern_len && pattern[i + 1] == '*') {
            // Zero-or-more quantifier: X*
            int choice_addr = pc;
            
            // CHOICE +N (to skip the loop)
            regex->code[pc].op = OP_CHOICE;
            pc++;
            int choice_pc = pc - 1;  // Remember location to patch
            
            // SAVE_POINTER
            regex->code[pc].op = OP_SAVE_POINTER;
            pc++;
            
            // The character/pattern to repeat
            if (ch == '.') {
                regex->code[pc].op = OP_DOT;
            } else {
                regex->code[pc].op = OP_CHAR;
                regex->code[pc].c = ch;
            }
            pc++;
            
            // ZERO_LENGTH
            regex->code[pc].op = OP_ZERO_LENGTH;
            pc++;
            
            // BRANCH_IF_NOT back to choice
            regex->code[pc].op = OP_BRANCH_IF_NOT;
            regex->code[pc].addr = choice_addr - pc;
            pc++;
            
            // Patch the CHOICE instruction to jump here
            regex->code[choice_pc].addr = pc - choice_pc;
            
            i++; // Skip the '*' character
        } else {
            // Regular character or dot (character classes handled above)
            if (ch == '.') {
                regex->code[pc].op = OP_DOT;
            } else {
                regex->code[pc].op = OP_CHAR;
                regex->code[pc].c = ch;
            }
            pc++;
        }
    }
    
    // SAVE_GROUP 0 END
    regex->code[pc].op = OP_SAVE_GROUP;
    regex->code[pc].group_num = 0;
    regex->code[pc].is_end = 1;
    pc++;
    
    // MATCH
    regex->code[pc].op = OP_MATCH;
    pc++;
    
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