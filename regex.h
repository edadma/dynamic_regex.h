#ifndef REGEX_H
#define REGEX_H

#include "int_stack.h"
#include <stdint.h>

// AST Node Types for parsing
typedef enum {
    AST_CHAR,           // Single character
    AST_DOT,            // . (any character)
    AST_CHARSET,        // [abc] or \d, \w, \s
    AST_GROUP,          // (pattern)
    AST_SEQUENCE,       // pattern1 pattern2
    AST_QUANTIFIER,     // pattern*, pattern+, pattern?
    AST_ALTERNATION,    // pattern1|pattern2
    AST_ANCHOR_START,       // ^
    AST_ANCHOR_END,         // $
    AST_WORD_BOUNDARY,      // \b
    AST_WORD_BOUNDARY_NEG   // \B
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    union {
        char character;             // For AST_CHAR
        struct {                    // For AST_CHARSET
            uint8_t charset[32];    // 256-bit bitmap
            int negate;             // 1 if negated [^abc]
        } charset;
        struct {                    // For AST_GROUP
            struct ASTNode *content;
            int group_number;
        } group;
        struct {                    // For AST_SEQUENCE
            struct ASTNode **children;
            int child_count;
            int capacity;
        } sequence;
        struct {                    // For AST_QUANTIFIER
            struct ASTNode *target;
            char quantifier;        // '*', '+', '?', '{'
            int min_count;          // For {n} and {n,m}
            int max_count;          // For {n,m}, -1 for unlimited
        } quantifier;
        struct {                    // For AST_ALTERNATION
            struct ASTNode **alternatives;
            int alternative_count;
            int capacity;
        } alternation;
    } data;
} ASTNode;

// VM Instructions - same as v2 but simplified data stack
typedef enum {
    OP_CHAR,              // Match specific character
    OP_DOT,               // Match any character  
    OP_CHARSET,           // Match character class
    OP_CHOICE,            // Create choice point
    OP_BRANCH,            // Unconditional jump
    OP_BRANCH_IF_NOT,     // Jump if condition not met
    OP_SAVE_POINTER,      // Save position to integer data stack
    OP_RESTORE_POSITION,  // Restore position from integer data stack
    OP_SAVE_GROUP,        // Save capture group start/end
    OP_ZERO_LENGTH,       // Check for zero-length match
    OP_ANCHOR_START,      // Match start of string/line
    OP_ANCHOR_END,        // Match end of string/line
    OP_WORD_BOUNDARY,     // Match word boundary (\b)
    OP_WORD_BOUNDARY_NEG, // Match negative word boundary (\B)
    OP_MATCH,             // Success
    OP_FAIL               // Explicit failure
} OpCode;

typedef struct {
    OpCode op;
    union {
        char c;                    // For OP_CHAR
        int addr;                  // For jumps/branches
        struct {                   // For OP_CHARSET
            uint8_t charset[32];   // 256 bits
            int negate;
        };
        struct {                   // For OP_SAVE_GROUP
            int group_num;
            int is_end;
        };
    };
} Instruction;

// VM State with integer-only data stack
typedef struct {
    const char *text;
    int text_len;
    int pc;
    int pos;
    
    // Integer-only immutable data stack
    IntStack *data_stack;
    
    // Capture groups
    int *group_starts;
    int *group_ends;
    int group_count;
    
    // Choice point stack
    struct ChoicePoint {
        int pc;
        int pos;
        IntStack *data_stack;        // Immutable snapshot
        int *group_starts;
        int *group_ends;
        int flags;
        int last_operation_success;
        int transferred;             // Flag: 1 if data_stack was transferred back to VM
    } *choice_stack;
    int choice_top;
    int choice_capacity;
    
    // Limits
    int choice_count;
    int max_choices;
    int flags;
    int last_match_was_zero_length;
    int last_operation_success;
} VM;

typedef struct {
    Instruction *code;
    int code_len;
    int code_capacity;
    int group_count;
    int flags;
} CompiledRegex;

// Low-level VM API
CompiledRegex* compile_regex(const char *pattern, int flags);
int execute_regex(CompiledRegex *compiled, const char *text, int start_pos);
void free_regex(CompiledRegex *compiled);
void print_regex_bytecode(CompiledRegex *compiled);

// High-level compatibility API for main.c
typedef struct {
    CompiledRegex *compiled;
    char *pattern;
    char *flags;
    int last_index;
} RegExp;

typedef struct {
    char **groups;
    int group_count;
    int index;
    char *input;
} MatchResult;

typedef struct {
    RegExp *regexp;
    const char *text;
    int pos;
    int done;
} MatchIterator;

// Main API functions expected by main.c
RegExp* regex_new(const char *pattern, const char *flags);
int regex_test(RegExp *regexp, const char *text);
MatchResult* regex_exec(RegExp *regexp, const char *text);
void regex_free(RegExp *regexp);
void match_result_free(MatchResult *result);

// String methods (JavaScript-like API)
MatchResult* string_match(const char *text, RegExp *regexp);
MatchIterator* string_match_all(const char *text, RegExp *regexp);
MatchResult* match_iterator_next(MatchIterator *iter);
void match_iterator_free(MatchIterator *iter);

// AST parsing functions (old parse_pattern removed - using lexer+parser)
ASTNode* create_ast_node(ASTNodeType type);
void free_ast(ASTNode *node);
CompiledRegex* compile_ast(ASTNode *ast, int flags);

// Debug functions
void debug_display_token_stream(const char *pattern);
void debug_display_pattern_ast(const char *pattern);

#endif // REGEX_H
