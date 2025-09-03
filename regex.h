#ifndef REGEX_H
#define REGEX_H

#include <stdint.h>

// VM bytecode opcodes
typedef enum {
    OP_CHAR,            // Match exact character
    OP_DOT,             // Match any character
    OP_CHARSET,         // Character class [abc]
    OP_DIGIT,           // \d
    OP_WORD,            // \w  
    OP_SPACE,           // \s
    OP_NOT_DIGIT,       // \D
    OP_NOT_WORD,        // \W
    OP_NOT_SPACE,       // \S
    OP_ANCHOR_START,    // ^
    OP_ANCHOR_END,      // $
    OP_SPLIT,           // Split execution (for alternation and quantifiers)
    OP_JUMP,            // Unconditional jump
    OP_SAVE,            // Save group position
    OP_MATCH,           // Success - pattern matched
    OP_FAIL             // Explicit failure
} VMOpCode;

// VM instruction
typedef struct {
    VMOpCode op;
    union {
        char c;                     // OP_CHAR
        struct {                    // OP_SPLIT, OP_JUMP
            int addr1;              // Primary target
            int addr2;              // Secondary target (for SPLIT)
        };
        struct {                    // OP_CHARSET
            uint8_t charset[32];    // 256-bit bitmap
            int negate;             // 1 if negated [^abc]
        };
        int group_num;              // OP_SAVE
    };
} VMInstruction;

// Compiled regex - contains bytecode
typedef struct {
    VMInstruction *code;
    int code_len;
    int code_capacity;
    int group_count;
    int flags;
} CompiledRegex;

// Regex flags
#define FLAG_GLOBAL      0x01  // g
#define FLAG_IGNORECASE  0x02  // i
#define FLAG_MULTILINE   0x04  // m
#define FLAG_DOTALL      0x08  // s
#define FLAG_UNICODE     0x10  // u
#define FLAG_STICKY      0x20  // y

// Match result
typedef struct {
    char **groups;       // Captured groups
    int *group_starts;   // Start positions
    int *group_ends;     // End positions
    int group_count;
    int index;           // Match start index
    char *input;         // Original input (not owned)
} MatchResult;

// RegExp object (JavaScript-like)
typedef struct {
    CompiledRegex *compiled;
    char *source;
    int last_index;
    int flags;
} RegExp;

// Match iterator for matchAll functionality
typedef struct {
    RegExp *regexp;
    char *text;
    int done;
} MatchIterator;

// Core API functions
RegExp* regex_new(const char *pattern, const char *flags);
void regex_free(RegExp *regexp);

// JavaScript RegExp methods
int regex_test(RegExp *regexp, const char *text);
MatchResult* regex_exec(RegExp *regexp, const char *text);

// String methods that use regex
MatchResult* string_match(const char *text, RegExp *regexp);
MatchIterator* string_match_all(const char *text, RegExp *regexp);
MatchResult* match_iterator_next(MatchIterator *iter);
void match_iterator_free(MatchIterator *iter);

// Utility functions
void match_result_free(MatchResult *result);
void regex_print_bytecode(CompiledRegex *regex);

// Internal functions
CompiledRegex* regex_compile(const char *pattern, int flags);
MatchResult* regex_execute(CompiledRegex *regex, const char *text, int start_pos);

#endif // REGEX_H