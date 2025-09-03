#include "regex.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

// Forward declaration for AST instruction emission
int emit_ast_instruction(CompiledRegex *regex, OpCode op);
// Forward declaration for new lexer+parser (defined in parser.c)
static ASTNode* parse_pattern(const char *pattern, int *group_counter);

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
    
    // NEW: Use lexer + parser approach with proper precedence
    // Parse pattern to AST using new lexer+parser with proper precedence
    // DEBUGGING: Switch to new parser to investigate exact quantifier issue
    int group_counter = 0;
    ASTNode *ast = parse_pattern(pattern, &group_counter);
    if (!ast) {
        return NULL; // Parse error
    }
    
    // Compile AST to bytecode (unchanged)
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

#include "execute.c" // AMALGAMATE

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
        
        int match_result = execute(compiled, &vm);
        
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
        
        int result = execute(compiled, &vm);
        
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
    
    // Check if this is a global regex and should continue from last_index
    int start_pos = 0;
    if (regexp->compiled->flags & 4) { // Global flag 'g'
        start_pos = regexp->last_index;
        
        // If last_index is beyond the text, return NULL (no more matches)
        if (start_pos >= (int)strlen(text)) {
            return NULL;
        }
    }
    
    DetailedMatch detailed = execute_regex_detailed(regexp->compiled, text, start_pos);
    if (!detailed.matched) {
        // For global regex, reset last_index when no match found
        if (regexp->compiled->flags & 4) {
            regexp->last_index = 0;
        }
        return NULL;
    }
    
    // For global regex, update last_index to end of this match
    if (regexp->compiled->flags & 4) {
        regexp->last_index = detailed.match_end;
    }
    
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
    
    // matchAll requires global flag
    if (!(regexp->compiled->flags & 4)) { // Check for global flag 'g'
        return NULL;
    }
    
    // Reset lastIndex to start from beginning
    regexp->last_index = 0;
    
    MatchIterator *iter = malloc(sizeof(MatchIterator));
    iter->regexp = regexp;
    iter->text = strdup(text);  // Keep our own copy
    iter->pos = 0;
    iter->done = 0;
    
    return iter;
}

MatchResult* match_iterator_next(MatchIterator *iter) {
    if (!iter || iter->done) return NULL;
    
    // Use regex_exec which handles global flag and lastIndex
    MatchResult *result = regex_exec(iter->regexp, iter->text);
    
    if (!result) {
        iter->done = 1;  // No more matches
    }
    
    return result;
}

void match_iterator_free(MatchIterator *iter) {
    if (iter) {
        free((char*)iter->text);  // Free our copy of text
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
        case AST_ALTERNATION:
            node->data.alternation.alternatives = NULL;
            node->data.alternation.alternative_count = 0;
            node->data.alternation.capacity = 0;
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
        case AST_ALTERNATION:
            for (int i = 0; i < node->data.alternation.alternative_count; i++) {
                free_ast(node->data.alternation.alternatives[i]);
            }
            free(node->data.alternation.alternatives);
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
// Helper function to add alternation child
void add_alternation_child(ASTNode *alternation, ASTNode *child) {
    if (alternation->data.alternation.alternative_count >= alternation->data.alternation.capacity) {
        alternation->data.alternation.capacity = alternation->data.alternation.capacity ? alternation->data.alternation.capacity * 2 : 4;
        alternation->data.alternation.alternatives = realloc(alternation->data.alternation.alternatives, 
                                                               alternation->data.alternation.capacity * sizeof(ASTNode*));
    }
    alternation->data.alternation.alternatives[alternation->data.alternation.alternative_count++] = child;
}

#include "lexer.c" // AMALGAMATE

#include "parser.c" // AMALGAMATE

#include "compiler.c" // AMALGAMATE

// Debug function to display AST recursively
void debug_display_ast(ASTNode *node, int depth) {
    if (!node) return;
    
    // Print indentation
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }
    
    switch (node->type) {
        case AST_CHAR:
            printf("CHAR '%c'\n", node->data.character);
            break;
        case AST_DOT:
            printf("DOT\n");
            break;
        case AST_CHARSET:
            printf("CHARSET [%s]\n", node->data.charset.negate ? "negated" : "normal");
            break;
        case AST_SEQUENCE:
            printf("SEQUENCE (%d children)\n", node->data.sequence.child_count);
            for (int i = 0; i < node->data.sequence.child_count; i++) {
                debug_display_ast(node->data.sequence.children[i], depth + 1);
            }
            break;
        case AST_ALTERNATION:
            printf("ALTERNATION (%d alternatives)\n", node->data.alternation.alternative_count);
            for (int i = 0; i < node->data.alternation.alternative_count; i++) {
                debug_display_ast(node->data.alternation.alternatives[i], depth + 1);
            }
            break;
        case AST_QUANTIFIER:
            printf("QUANTIFIER '%c' min=%d max=%d\n", 
                   node->data.quantifier.quantifier,
                   node->data.quantifier.min_count,
                   node->data.quantifier.max_count);
            debug_display_ast(node->data.quantifier.target, depth + 1);
            break;
        case AST_GROUP:
            printf("GROUP #%d\n", node->data.group.group_number);
            debug_display_ast(node->data.group.content, depth + 1);
            break;
        case AST_ANCHOR_START:
            printf("ANCHOR_START\n");
            break;
        case AST_ANCHOR_END:
            printf("ANCHOR_END\n");
            break;
        case AST_WORD_BOUNDARY:
            printf("WORD_BOUNDARY\n");
            break;
        default:
            printf("UNKNOWN(%d)\n", node->type);
    }
}

// Debug function to parse and display AST
void debug_display_pattern_ast(const char *pattern) {
    printf("=== NEW PARSER AST for: %s ===\n", pattern);
    
    int group_counter = 0;
    ASTNode *ast = parse_pattern(pattern, &group_counter);
    
    if (ast) {
        debug_display_ast(ast, 0);
        free_ast(ast);
    } else {
        printf("Parse failed\n");
    }
    printf("\n");
}

// Debug function to display token stream
void debug_display_token_stream(const char *pattern) {
    printf("=== Token stream for: %s ===\n", pattern);
    
    Lexer *lexer = lexer_new(pattern);
    Token *token;
    int token_count = 0;
    
    while ((token = lexer_peek(lexer)) && token->type != TOK_EOF) {
        printf("Token %d: ", token_count++);
        
        switch (token->type) {
            case TOK_CHAR:
                printf("CHAR '%c'", token->data.character);
                break;
            case TOK_DOT:
                printf("DOT");
                break;
            case TOK_STAR:
                printf("STAR");
                break;
            case TOK_PLUS:
                printf("PLUS");
                break;
            case TOK_QUESTION:
                printf("QUESTION");
                break;
            case TOK_QUANTIFIER:
                printf("QUANTIFIER {%d,%d}", token->data.min_count, token->data.max_count);
                break;
            case TOK_CHARSET:
                printf("CHARSET [%s]", token->data.negate ? "negated" : "normal");
                break;
            case TOK_CARET:
                printf("CARET");
                break;
            case TOK_DOLLAR:
                printf("DOLLAR");
                break;
            case TOK_WORD_BOUNDARY:
                printf("WORD_BOUNDARY");
                break;
            case TOK_PIPE:
                printf("PIPE");
                break;
            case TOK_LPAREN:
                printf("LPAREN");
                break;
            case TOK_RPAREN:
                printf("RPAREN");
                break;
            case TOK_LBRACKET:
                printf("LBRACKET");
                break;
            case TOK_RBRACKET:
                printf("RBRACKET");
                break;
            case TOK_LBRACE:
                printf("LBRACE");
                break;
            case TOK_RBRACE:
                printf("RBRACE");
                break;
            case TOK_ERROR:
                printf("ERROR");
                break;
            default:
                printf("UNKNOWN(%d)", token->type);
        }
        printf("\n");
        
        lexer_next(lexer); // Advance to next token
    }
    
    printf("Token %d: EOF\n", token_count);
    lexer_free(lexer);
    printf("\n");
}
