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
// New AST-based compile_regex function
CompiledRegex* compile_regex(const char *pattern, int flags) {
    // Handle empty pattern
    if (!pattern || strlen(pattern) == 0) {
        CompiledRegex *regex = malloc(sizeof(CompiledRegex));
        regex->code = malloc(sizeof(Instruction) * 4);
        regex->code_len = 0;
        regex->code_capacity = 4;
        regex->group_count = 1;
        regex->flags = flags;
        
        int start_pc = emit_ast_instruction(regex, OP_SAVE_GROUP);
        regex->code[start_pc].group_num = 0;
        regex->code[start_pc].is_end = 0;
        
        int end_pc = emit_ast_instruction(regex, OP_SAVE_GROUP);
        regex->code[end_pc].group_num = 0;
        regex->code[end_pc].is_end = 1;
        
        emit_ast_instruction(regex, OP_MATCH);
        return regex;
    }
    
    // Parse pattern to AST
    int group_counter = 0;
    ASTNode *ast = parse_pattern(pattern, &group_counter);
    
    // Compile AST to bytecode
    CompiledRegex *compiled = compile_ast(ast, flags);
    
    // Clean up AST
    free_ast(ast);
    
    return compiled;
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

// New function that returns detailed match information
typedef struct {
    int matched;
    int match_start;
    int match_end;
    int *group_starts;
    int *group_ends;
    int group_count;
} DetailedMatch;

DetailedMatch execute_regex_detailed(CompiledRegex *compiled, const char *text, int start_pos) {
    DetailedMatch result = {0};
    if (!compiled || !text) return result;
    
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
        
        int match_result = vm_execute(compiled, &vm);
        
        if (match_result) {
            // Success - copy group data
            result.matched = 1;
            result.match_start = pos;
            result.match_end = vm.group_ends[0];  // Group 0 is the full match
            result.group_count = compiled->group_count;
            result.group_starts = malloc(compiled->group_count * sizeof(int));
            result.group_ends = malloc(compiled->group_count * sizeof(int));
            memcpy(result.group_starts, vm.group_starts, compiled->group_count * sizeof(int));
            memcpy(result.group_ends, vm.group_ends, compiled->group_count * sizeof(int));
        }
        
        // Clean up VM
        int_stack_release(vm.data_stack);
        for (int i = 0; i < vm.choice_top; i++) {
            struct ChoicePoint *cp = &vm.choice_stack[i];
            if (!cp->transferred) {
                int_stack_release(cp->data_stack);
            }
            free(cp->group_starts);
            free(cp->group_ends);
        }
        free(vm.choice_stack);
        free(vm.group_starts);
        free(vm.group_ends);
        
        if (match_result) return result;
    }
    
    return result;
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
    
    DetailedMatch detailed = execute_regex_detailed(regexp->compiled, text, 0);
    if (!detailed.matched) return NULL;
    
    // Create MatchResult with captured groups
    MatchResult *match = malloc(sizeof(MatchResult));
    match->group_count = detailed.group_count;
    match->groups = malloc(sizeof(char*) * detailed.group_count);
    match->index = detailed.match_start;
    match->input = strdup(text);
    
    // Extract captured group strings from text
    for (int i = 0; i < detailed.group_count; i++) {
        if (detailed.group_starts[i] >= 0 && detailed.group_ends[i] >= 0) {
            int len = detailed.group_ends[i] - detailed.group_starts[i];
            match->groups[i] = malloc(len + 1);
            strncpy(match->groups[i], text + detailed.group_starts[i], len);
            match->groups[i][len] = '\0';
        } else {
            match->groups[i] = NULL;  // Group didn't match
        }
    }
    
    // Clean up detailed match
    free(detailed.group_starts);
    free(detailed.group_ends);
    
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

// AST Implementation

ASTNode* create_ast_node(ASTNodeType type) {
    ASTNode *node = malloc(sizeof(ASTNode));
    node->type = type;
    
    // Initialize based on type
    switch (type) {
        case AST_SEQUENCE:
            node->data.sequence.children = NULL;
            node->data.sequence.child_count = 0;
            node->data.sequence.capacity = 0;
            break;
        case AST_GROUP:
            node->data.group.content = NULL;
            node->data.group.group_number = 0;
            break;
        case AST_QUANTIFIER:
            node->data.quantifier.target = NULL;
            node->data.quantifier.quantifier = '\0';
            node->data.quantifier.min_count = 0;
            node->data.quantifier.max_count = 0;
            break;
        case AST_CHARSET:
            memset(node->data.charset.charset, 0, 32);
            node->data.charset.negate = 0;
            break;
        default:
            // Other types don't need initialization
            break;
    }
    
    return node;
}

void free_ast(ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_SEQUENCE:
            for (int i = 0; i < node->data.sequence.child_count; i++) {
                free_ast(node->data.sequence.children[i]);
            }
            free(node->data.sequence.children);
            break;
        case AST_GROUP:
            free_ast(node->data.group.content);
            break;
        case AST_QUANTIFIER:
            free_ast(node->data.quantifier.target);
            break;
        default:
            // Other types don't have child nodes to free
            break;
    }
    
    free(node);
}

// Helper function to add a child to a sequence node
void add_sequence_child(ASTNode *sequence, ASTNode *child) {
    if (sequence->type != AST_SEQUENCE) return;
    
    // Resize if needed
    if (sequence->data.sequence.child_count >= sequence->data.sequence.capacity) {
        int new_capacity = sequence->data.sequence.capacity == 0 ? 4 : sequence->data.sequence.capacity * 2;
        sequence->data.sequence.children = realloc(sequence->data.sequence.children, 
                                                  new_capacity * sizeof(ASTNode*));
        sequence->data.sequence.capacity = new_capacity;
    }
    
    sequence->data.sequence.children[sequence->data.sequence.child_count++] = child;
}

// Parse a regex pattern into an AST
ASTNode* parse_pattern(const char *pattern, int *group_counter) {
    int len = strlen(pattern);
    if (len == 0) {
        // Empty pattern - create empty sequence
        return create_ast_node(AST_SEQUENCE);
    }
    
    ASTNode *sequence = create_ast_node(AST_SEQUENCE);
    
    for (int i = 0; i < len; i++) {
        char ch = pattern[i];
        ASTNode *node = NULL;
        
        // Handle escape sequences
        if (ch == '\\' && i + 1 < len) {
            char escaped = pattern[i + 1];
            
            if (escaped == 'd') {
                // Digit character class [0-9]
                node = create_ast_node(AST_CHARSET);
                node->data.charset.negate = 0;
                for (int d = '0'; d <= '9'; d++) {
                    int bit = (unsigned char)d;
                    node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                }
                i++; // Skip escaped character
            } else if (escaped == 'w') {
                // Word character class [a-zA-Z0-9_]
                node = create_ast_node(AST_CHARSET);
                node->data.charset.negate = 0;
                for (int c = 'a'; c <= 'z'; c++) {
                    int bit = (unsigned char)c;
                    node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                }
                for (int c = 'A'; c <= 'Z'; c++) {
                    int bit = (unsigned char)c;
                    node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                }
                for (int c = '0'; c <= '9'; c++) {
                    int bit = (unsigned char)c;
                    node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                }
                int bit = (unsigned char)'_';
                node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                i++; // Skip escaped character
            } else if (escaped == 's') {
                // Whitespace character class
                node = create_ast_node(AST_CHARSET);
                node->data.charset.negate = 0;
                const char *whitespace = " \t\n\r\f\v";
                for (int w = 0; whitespace[w]; w++) {
                    int bit = (unsigned char)whitespace[w];
                    node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                }
                i++; // Skip escaped character
            } else if (escaped == 'D') {
                // Non-digit character class [^0-9]
                node = create_ast_node(AST_CHARSET);
                node->data.charset.negate = 1;
                for (int d = '0'; d <= '9'; d++) {
                    int bit = (unsigned char)d;
                    node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                }
                i++; // Skip escaped character
            } else if (escaped == 'W') {
                // Non-word character class [^a-zA-Z0-9_]
                node = create_ast_node(AST_CHARSET);
                node->data.charset.negate = 1;
                for (int c = 'a'; c <= 'z'; c++) {
                    int bit = (unsigned char)c;
                    node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                }
                for (int c = 'A'; c <= 'Z'; c++) {
                    int bit = (unsigned char)c;
                    node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                }
                for (int c = '0'; c <= '9'; c++) {
                    int bit = (unsigned char)c;
                    node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                }
                int bit = (unsigned char)'_';
                node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                i++; // Skip escaped character
            } else if (escaped == 'S') {
                // Non-whitespace character class [^ \t\n\r\f\v]
                node = create_ast_node(AST_CHARSET);
                node->data.charset.negate = 1;
                const char *whitespace = " \t\n\r\f\v";
                for (int w = 0; whitespace[w]; w++) {
                    int bit = (unsigned char)whitespace[w];
                    node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                }
                i++; // Skip escaped character
            } else if (escaped == 'n') {
                // Newline
                node = create_ast_node(AST_CHAR);
                node->data.character = '\n';
                i++; // Skip escaped character
            } else if (escaped == 't') {
                // Tab
                node = create_ast_node(AST_CHAR);
                node->data.character = '\t';
                i++; // Skip escaped character
            } else if (escaped == 'r') {
                // Carriage return
                node = create_ast_node(AST_CHAR);
                node->data.character = '\r';
                i++; // Skip escaped character
            } else {
                // Regular escaped character
                node = create_ast_node(AST_CHAR);
                node->data.character = escaped;
                i++; // Skip escaped character
            }
        } else if (ch == '(') {
            // Group start - find matching closing paren
            int paren_depth = 1;
            int start = i + 1;
            int end = i + 1;
            
            while (end < len && paren_depth > 0) {
                if (pattern[end] == '(') {
                    paren_depth++;
                } else if (pattern[end] == ')') {
                    paren_depth--;
                }
                end++;
            }
            
            if (paren_depth > 0) {
                // Unmatched paren - treat as literal
                node = create_ast_node(AST_CHAR);
                node->data.character = ch;
            } else {
                // Create group node
                node = create_ast_node(AST_GROUP);
                (*group_counter)++;
                node->data.group.group_number = *group_counter;
                
                // Parse group content
                int group_len = end - start - 1;
                char *group_content = malloc(group_len + 1);
                strncpy(group_content, pattern + start, group_len);
                group_content[group_len] = '\0';
                
                node->data.group.content = parse_pattern(group_content, group_counter);
                
                free(group_content);
                i = end - 1; // Skip to closing paren (will be incremented by loop)
            }
        } else if (ch == '[') {
            // Character class [abc] or [a-z] or [^abc]
            int class_start = i + 1;
            int negate = 0;
            
            // Check for negation
            if (class_start < len && pattern[class_start] == '^') {
                negate = 1;
                class_start++;
            }
            
            // Find closing ]
            int class_end = class_start;
            while (class_end < len && pattern[class_end] != ']') {
                class_end++;
            }
            
            if (class_end >= len) {
                // Malformed character class - treat [ as literal
                node = create_ast_node(AST_CHAR);
                node->data.character = ch;
            } else {
                // Build character class
                node = create_ast_node(AST_CHARSET);
                memset(node->data.charset.charset, 0, 32);
                node->data.charset.negate = negate;
                
                // Parse character class content
                for (int j = class_start; j < class_end; j++) {
                    // Handle escape sequences within character classes
                    if (pattern[j] == '\\' && j + 1 < class_end) {
                        char escape_char = pattern[j + 1];
                        
                        if (escape_char == 'd') {
                            // \d matches [0-9]
                            for (char c = '0'; c <= '9'; c++) {
                                int bit = (unsigned char)c;
                                node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                            }
                            j++; // Skip escaped character
                        } else if (escape_char == 'w') {
                            // \w matches [a-zA-Z0-9_]
                            for (char c = 'a'; c <= 'z'; c++) {
                                int bit = (unsigned char)c;
                                node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                            }
                            for (char c = 'A'; c <= 'Z'; c++) {
                                int bit = (unsigned char)c;
                                node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                            }
                            for (char c = '0'; c <= '9'; c++) {
                                int bit = (unsigned char)c;
                                node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                            }
                            int bit = (unsigned char)'_';
                            node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                            j++; // Skip escaped character
                        } else if (escape_char == 's') {
                            // \s matches [ \t\n\r\f\v]
                            const char *whitespace = " \t\n\r\f\v";
                            for (int w = 0; whitespace[w]; w++) {
                                int bit = (unsigned char)whitespace[w];
                                node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                            }
                            j++; // Skip escaped character
                        } else if (escape_char == 'n') {
                            int bit = (unsigned char)'\n';
                            node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                            j++; // Skip escaped character
                        } else if (escape_char == 't') {
                            int bit = (unsigned char)'\t';
                            node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                            j++; // Skip escaped character
                        } else if (escape_char == 'r') {
                            int bit = (unsigned char)'\r';
                            node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                            j++; // Skip escaped character
                        } else {
                            // Treat as literal escaped character
                            int bit = (unsigned char)escape_char;
                            node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                            j++; // Skip escaped character
                        }
                    } else if (j + 2 < class_end && pattern[j + 1] == '-') {
                        // Character range: a-z
                        char start_char = pattern[j];
                        char end_char = pattern[j + 2];
                        for (char c = start_char; c <= end_char; c++) {
                            int bit = (unsigned char)c;
                            node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                        }
                        j += 2; // Skip the range
                    } else {
                        // Individual character
                        char class_char = pattern[j];
                        int bit = (unsigned char)class_char;
                        node->data.charset.charset[bit / 8] |= (1 << (bit % 8));
                    }
                }
                
                i = class_end; // Skip to closing ] (will be incremented by loop)
            }
        } else if (ch == '.') {
            node = create_ast_node(AST_DOT);
        } else if (ch == '^') {
            node = create_ast_node(AST_ANCHOR_START);
        } else if (ch == '$') {
            node = create_ast_node(AST_ANCHOR_END);
        } else {
            // Regular character
            node = create_ast_node(AST_CHAR);
            node->data.character = ch;
        }
        
        if (node) {
            // Check for quantifiers
            if (i + 1 < len && (pattern[i + 1] == '*' || pattern[i + 1] == '+' || pattern[i + 1] == '?' || pattern[i + 1] == '{')) {
                char quantifier = pattern[i + 1];
                ASTNode *quantifier_node = create_ast_node(AST_QUANTIFIER);
                quantifier_node->data.quantifier.target = node;
                quantifier_node->data.quantifier.quantifier = quantifier;
                
                if (quantifier == '{') {
                    // Parse {n} or {n,m} quantifier
                    int brace_start = i + 2;
                    int brace_end = brace_start;
                    
                    // Find closing }
                    while (brace_end < len && pattern[brace_end] != '}') {
                        brace_end++;
                    }
                    
                    if (brace_end >= len) {
                        // Malformed quantifier - treat as literal
                        free_ast(quantifier_node);
                        node = create_ast_node(AST_CHAR);
                        node->data.character = pattern[i + 1];
                    } else {
                        // Parse the content between braces
                        int min_count = 0, max_count = 0;
                        int has_comma = 0;
                        
                        // Parse numbers
                        int j = brace_start;
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
                        } else {
                            max_count = min_count; // {n} same as {n,n}
                        }
                        
                        quantifier_node->data.quantifier.min_count = min_count;
                        quantifier_node->data.quantifier.max_count = has_comma && max_count == 0 ? -1 : max_count; // -1 for {n,}
                        
                        i = brace_end; // Skip to closing }
                    }
                } else {
                    // Simple quantifiers *, +, ?
                    quantifier_node->data.quantifier.min_count = 0;
                    quantifier_node->data.quantifier.max_count = 0;
                    i++; // Skip quantifier
                }
                
                node = quantifier_node;
            }
            
            add_sequence_child(sequence, node);
        }
    }
    
    return sequence;
}

// Helper function to count the maximum group number in an AST
int count_groups(ASTNode *node) {
    if (!node) return 0;
    
    int max_group = 0;
    
    switch (node->type) {
        case AST_GROUP:
            max_group = node->data.group.group_number;
            int child_max = count_groups(node->data.group.content);
            if (child_max > max_group) max_group = child_max;
            break;
            
        case AST_SEQUENCE:
            for (int i = 0; i < node->data.sequence.child_count; i++) {
                int child_max = count_groups(node->data.sequence.children[i]);
                if (child_max > max_group) max_group = child_max;
            }
            break;
            
        case AST_QUANTIFIER:
            max_group = count_groups(node->data.quantifier.target);
            break;
            
        default:
            // Other node types don't contain groups
            break;
    }
    
    return max_group;
}

// Helper function to ensure capacity in CompiledRegex during AST compilation
void ensure_ast_capacity(CompiledRegex *regex, int additional) {
    while (regex->code_len + additional >= regex->code_capacity) {
        regex->code_capacity *= 2;
        regex->code = realloc(regex->code, regex->code_capacity * sizeof(Instruction));
    }
}

// Helper function to emit instruction during AST compilation
int emit_ast_instruction(CompiledRegex *regex, OpCode op) {
    ensure_ast_capacity(regex, 1);
    regex->code[regex->code_len].op = op;
    return regex->code_len++;
}

// Compile an AST node to bytecode
void compile_ast_node(ASTNode *node, CompiledRegex *regex) {
    if (!node) return;
    
    switch (node->type) {
        case AST_CHAR: {
            int pc = emit_ast_instruction(regex, OP_CHAR);
            regex->code[pc].c = node->data.character;
            break;
        }
        
        case AST_DOT: {
            emit_ast_instruction(regex, OP_DOT);
            break;
        }
        
        case AST_CHARSET: {
            int pc = emit_ast_instruction(regex, OP_CHARSET);
            memcpy(regex->code[pc].charset, node->data.charset.charset, 32);
            regex->code[pc].negate = node->data.charset.negate;
            break;
        }
        
        case AST_SEQUENCE: {
            for (int i = 0; i < node->data.sequence.child_count; i++) {
                compile_ast_node(node->data.sequence.children[i], regex);
            }
            break;
        }
        
        case AST_GROUP: {
            // Emit SAVE_GROUP start
            int start_pc = emit_ast_instruction(regex, OP_SAVE_GROUP);
            regex->code[start_pc].group_num = node->data.group.group_number;
            regex->code[start_pc].is_end = 0;
            
            // Compile group content
            compile_ast_node(node->data.group.content, regex);
            
            // Emit SAVE_GROUP end
            int end_pc = emit_ast_instruction(regex, OP_SAVE_GROUP);
            regex->code[end_pc].group_num = node->data.group.group_number;
            regex->code[end_pc].is_end = 1;
            break;
        }
        
        case AST_QUANTIFIER: {
            char quantifier = node->data.quantifier.quantifier;
            
            if (quantifier == '*') {
                // Zero-or-more: CHOICE +N, SAVE_POINTER, [pattern], ZERO_LENGTH, BRANCH_IF_NOT -N
                int choice_addr = regex->code_len;
                int choice_pc = emit_ast_instruction(regex, OP_CHOICE);
                
                emit_ast_instruction(regex, OP_SAVE_POINTER);
                
                // Compile the target pattern
                compile_ast_node(node->data.quantifier.target, regex);
                
                emit_ast_instruction(regex, OP_ZERO_LENGTH);
                
                int branch_pc = emit_ast_instruction(regex, OP_BRANCH_IF_NOT);
                regex->code[branch_pc].addr = choice_addr - branch_pc;
                
                // Update CHOICE to skip to here
                regex->code[choice_addr].addr = regex->code_len - choice_addr;
                
            } else if (quantifier == '+') {
                // One-or-more: [pattern], CHOICE -N
                int loop_start = regex->code_len;
                
                // Compile the target pattern
                compile_ast_node(node->data.quantifier.target, regex);
                
                int choice_pc = emit_ast_instruction(regex, OP_CHOICE);
                regex->code[choice_pc].addr = loop_start - choice_pc;
                
            } else if (quantifier == '?') {
                // Zero-or-one: CHOICE +N, [pattern]
                int choice_pc = emit_ast_instruction(regex, OP_CHOICE);
                
                // Compile the target pattern
                compile_ast_node(node->data.quantifier.target, regex);
                
                // Update CHOICE to skip to here
                regex->code[choice_pc].addr = regex->code_len - choice_pc;
                
            } else if (quantifier == '{') {
                // Exact quantifiers: {n}, {n,m}, {n,}
                int min_count = node->data.quantifier.min_count;
                int max_count = node->data.quantifier.max_count;
                
                // Generate required matches (min_count times)
                for (int rep = 0; rep < min_count; rep++) {
                    compile_ast_node(node->data.quantifier.target, regex);
                }
                
                // Handle additional optional matches
                if (max_count == -1) {
                    // {n,} case - unlimited additional matches (like *)
                    int choice_addr = regex->code_len;
                    int choice_pc = emit_ast_instruction(regex, OP_CHOICE);
                    
                    emit_ast_instruction(regex, OP_SAVE_POINTER);
                    
                    // The pattern to repeat
                    compile_ast_node(node->data.quantifier.target, regex);
                    
                    emit_ast_instruction(regex, OP_ZERO_LENGTH);
                    
                    int branch_pc = emit_ast_instruction(regex, OP_BRANCH_IF_NOT);
                    regex->code[branch_pc].addr = choice_addr - branch_pc;
                    
                    // Update CHOICE to skip to here
                    regex->code[choice_addr].addr = regex->code_len - choice_addr;
                    
                } else if (max_count > min_count) {
                    // {n,m} case - limited additional matches
                    for (int rep = min_count; rep < max_count; rep++) {
                        // CHOICE +N (to skip this optional match)
                        int choice_pc = emit_ast_instruction(regex, OP_CHOICE);
                        
                        // The optional pattern
                        compile_ast_node(node->data.quantifier.target, regex);
                        
                        // Update CHOICE to skip to here
                        regex->code[choice_pc].addr = regex->code_len - choice_pc;
                    }
                }
            }
            break;
        }
        
        case AST_ANCHOR_START: {
            emit_ast_instruction(regex, OP_ANCHOR_START);
            break;
        }
        
        case AST_ANCHOR_END: {
            emit_ast_instruction(regex, OP_ANCHOR_END);
            break;
        }
    }
}

// Compile an AST to bytecode
CompiledRegex* compile_ast(ASTNode *ast, int flags) {
    CompiledRegex *regex = malloc(sizeof(CompiledRegex));
    regex->code = malloc(sizeof(Instruction) * 16);
    regex->code_len = 0;
    regex->code_capacity = 16;
    regex->group_count = 0;
    regex->flags = flags;
    
    // Emit SAVE_GROUP for group 0 (full match) start
    int start_pc = emit_ast_instruction(regex, OP_SAVE_GROUP);
    regex->code[start_pc].group_num = 0;
    regex->code[start_pc].is_end = 0;
    
    // Compile the AST
    compile_ast_node(ast, regex);
    
    // Count groups by traversing the AST
    int max_group = count_groups(ast);
    regex->group_count = max_group + 1; // +1 for group 0
    
    // Emit SAVE_GROUP for group 0 end
    int end_pc = emit_ast_instruction(regex, OP_SAVE_GROUP);
    regex->code[end_pc].group_num = 0;
    regex->code[end_pc].is_end = 1;
    
    // Emit MATCH instruction
    emit_ast_instruction(regex, OP_MATCH);
    
    return regex;
}