#include "regex.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// Backtracking frame
typedef struct {
    int pc;                 // Program counter
    int text_pos;           // Text position
    int *group_starts;      // Saved group start positions
    int *group_ends;        // Saved group end positions
} BacktrackFrame;

// VM state
typedef struct {
    const char *text;
    int text_len;
    int text_pos;
    int pc;                 // Program counter
    int *group_starts;
    int *group_ends;
    int group_count;
    int flags;
    
    // Backtracking stack
    BacktrackFrame *stack;
    int stack_top;
    int stack_capacity;
    
    // Backtracking limit to prevent exponential behavior
    int backtrack_count;
    int max_backtracks;
    
    // Advanced loop detection
    int *loop_detection_pc;
    int *loop_detection_text;
    int *loop_detection_count;
    int loop_detection_size;
} VMState;

// Parser for building bytecode
typedef struct {
    const char *pattern;
    int pos;
    int len;
    int flags;
    int group_counter;
    
    // Bytecode building
    VMInstruction *code;
    int code_len;
    int code_capacity;
} BytecodeParser;

// Bytecode building helpers
static void emit_op(BytecodeParser *p, VMOpCode op) {
    if (p->code_len >= p->code_capacity) {
        p->code_capacity *= 2;
        p->code = realloc(p->code, p->code_capacity * sizeof(VMInstruction));
    }
    p->code[p->code_len].op = op;
    p->code_len++;
}

static void emit_char(BytecodeParser *p, VMOpCode op, char c) {
    emit_op(p, op);
    p->code[p->code_len - 1].c = c;
}

static void emit_split(BytecodeParser *p, int addr1, int addr2) {
    emit_op(p, OP_SPLIT);
    p->code[p->code_len - 1].addr1 = addr1;
    p->code[p->code_len - 1].addr2 = addr2;
}

static void emit_jump(BytecodeParser *p, int addr) {
    emit_op(p, OP_JUMP);
    p->code[p->code_len - 1].addr1 = addr;
}

static void emit_save(BytecodeParser *p, int group_num) {
    emit_op(p, OP_SAVE);
    p->code[p->code_len - 1].group_num = group_num;
}

// Character set helpers
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

// Forward declarations
static void parse_alternation(BytecodeParser *p);
static void parse_sequence(BytecodeParser *p);
static void parse_quantified_atom(BytecodeParser *p);
static void parse_atom(BytecodeParser *p);

// Parse character class [abc] or [^abc]
static void parse_charset(BytecodeParser *p) {
    if (p->pattern[p->pos] != '[') return;
    p->pos++; // Skip '['
    
    emit_op(p, OP_CHARSET);
    VMInstruction *inst = &p->code[p->code_len - 1];
    memset(inst->charset, 0, 32);
    inst->negate = 0;
    
    // Check for negation
    if (p->pos < p->len && p->pattern[p->pos] == '^') {
        inst->negate = 1;
        p->pos++;
    }
    
    // Parse character range
    while (p->pos < p->len && p->pattern[p->pos] != ']') {
        char start;
        
        // Handle escape sequences
        if (p->pattern[p->pos] == '\\' && p->pos + 1 < p->len) {
            p->pos++; // Skip '\'
            switch (p->pattern[p->pos]) {
                case 'd': // \d - digits
                    for (char c = '0'; c <= '9'; c++) {
                        charset_add_char(inst->charset, c);
                    }
                    p->pos++;
                    continue;
                case 'w': // \w - word chars
                    for (char c = 'a'; c <= 'z'; c++) charset_add_char(inst->charset, c);
                    for (char c = 'A'; c <= 'Z'; c++) charset_add_char(inst->charset, c);
                    for (char c = '0'; c <= '9'; c++) charset_add_char(inst->charset, c);
                    charset_add_char(inst->charset, '_');
                    p->pos++;
                    continue;
                case 's': // \s - whitespace
                    charset_add_char(inst->charset, ' ');
                    charset_add_char(inst->charset, '\t');
                    charset_add_char(inst->charset, '\n');
                    charset_add_char(inst->charset, '\r');
                    charset_add_char(inst->charset, '\f');
                    p->pos++;
                    continue;
                case 'n': start = '\n'; p->pos++; break;
                case 't': start = '\t'; p->pos++; break;
                case 'r': start = '\r'; p->pos++; break;
                default:
                    // Literal escaped character
                    start = p->pattern[p->pos++];
                    break;
            }
        } else {
            start = p->pattern[p->pos++];
        }
        
        // Check for range
        if (p->pos + 1 < p->len && p->pattern[p->pos] == '-' &&
            p->pattern[p->pos + 1] != ']') {
            // Range like a-z
            p->pos++; // Skip '-'
            char end = p->pattern[p->pos++];
            charset_add_range(inst->charset, start, end);
        } else {
            // Single character
            charset_add_char(inst->charset, start);
        }
    }
    
    if (p->pos < p->len) p->pos++; // Skip ']'
}

// Parse quantifier {n,m}
static int parse_quantifier_range(BytecodeParser *p, int *min, int *max) {
    if (p->pos >= p->len || p->pattern[p->pos] != '{') return 0;
    
    int start = ++p->pos;
    *min = 0;
    *max = -1; // -1 means unlimited
    
    // Parse minimum
    while (p->pos < p->len && isdigit(p->pattern[p->pos])) {
        *min = *min * 10 + (p->pattern[p->pos] - '0');
        p->pos++;
    }
    
    if (p->pos < p->len && p->pattern[p->pos] == ',') {
        p->pos++;
        if (p->pos < p->len && isdigit(p->pattern[p->pos])) {
            *max = 0;
            while (p->pos < p->len && isdigit(p->pattern[p->pos])) {
                *max = *max * 10 + (p->pattern[p->pos] - '0');
                p->pos++;
            }
        } // else unlimited
    } else {
        *max = *min; // {n} means exactly n
    }
    
    if (p->pos < p->len && p->pattern[p->pos] == '}') {
        p->pos++;
        return 1;
    }
    
    // Invalid quantifier, backtrack
    p->pos = start - 1;
    return 0;
}

// Parse single atom
static void parse_atom(BytecodeParser *p) {
    if (p->pos >= p->len) return;
    
    char c = p->pattern[p->pos];
    
    switch (c) {
        case '^':
            emit_op(p, OP_ANCHOR_START);
            p->pos++;
            break;
            
        case '$':
            emit_op(p, OP_ANCHOR_END);
            p->pos++;
            break;
            
        case '.':
            emit_op(p, OP_DOT);
            p->pos++;
            break;
            
        case '[':
            parse_charset(p);
            break;
            
        case '(':
            p->pos++; // Skip '('
            // Save group start
            emit_save(p, p->group_counter * 2); // Even numbers for starts
            int group_num = p->group_counter++;
            parse_alternation(p);
            // Save group end
            emit_save(p, group_num * 2 + 1); // Odd numbers for ends
            if (p->pos < p->len && p->pattern[p->pos] == ')') {
                p->pos++; // Skip ')'
            }
            break;
            
        case '\\':
            if (p->pos + 1 < p->len) {
                p->pos++; // Skip '\\'
                switch (p->pattern[p->pos]) {
                    case 'd': emit_op(p, OP_DIGIT); break;
                    case 'D': emit_op(p, OP_NOT_DIGIT); break;
                    case 'w': emit_op(p, OP_WORD); break;
                    case 'W': emit_op(p, OP_NOT_WORD); break;
                    case 's': emit_op(p, OP_SPACE); break;
                    case 'S': emit_op(p, OP_NOT_SPACE); break;
                    case 'n': emit_char(p, OP_CHAR, '\n'); break;
                    case 't': emit_char(p, OP_CHAR, '\t'); break;
                    case 'r': emit_char(p, OP_CHAR, '\r'); break;
                    default:
                        // Literal escaped character
                        emit_char(p, OP_CHAR, p->pattern[p->pos]);
                        break;
                }
                p->pos++;
            }
            break;
            
        // Special regex chars that end atoms
        case '*': case '+': case '?': case '{': 
        case '|': case ')':
            return; // Don't consume these
            
        default:
            // Regular character
            emit_char(p, OP_CHAR, c);
            p->pos++;
            break;
    }
}

// Parse atom with optional quantifier
static void parse_quantified_atom(BytecodeParser *p) {
    int atom_start = p->code_len;
    parse_atom(p);
    
    if (p->pos >= p->len || atom_start == p->code_len) return; // No atom was parsed
    
    // Check for quantifier
    char c = p->pattern[p->pos];
    
    switch (c) {
        case '*': { // Zero or more: a* becomes SPLIT(try, skip) try: a SPLIT(try, end) end:
            p->pos++;
            int split_addr = atom_start;
            int end_addr = p->code_len;
            
            // Insert SPLIT at beginning
            memmove(&p->code[split_addr + 1], &p->code[split_addr],
                    (p->code_len - split_addr) * sizeof(VMInstruction));
            p->code_len++;
            
            p->code[split_addr].op = OP_SPLIT;
            p->code[split_addr].addr1 = split_addr + 1; // Try matching
            p->code[split_addr].addr2 = end_addr + 2;   // Or skip to end
            
            // Add another SPLIT after the atom to handle repetition
            emit_split(p, split_addr, p->code_len + 1);
            break;
        }
        
        case '+': { // One or more: a+ becomes L1: a SPLIT(L1, L3) L3:
            p->pos++;
            int end_addr = p->code_len;
            emit_split(p, atom_start, p->code_len + 1);
            break;
        }
        
        case '?': { // Zero or one: a? becomes SPLIT(L1, L3) L1: a L3:
            p->pos++;
            int end_addr = p->code_len;
            
            // Insert SPLIT at beginning
            memmove(&p->code[atom_start + 1], &p->code[atom_start],
                    (p->code_len - atom_start) * sizeof(VMInstruction));
            p->code_len++;
            
            p->code[atom_start].op = OP_SPLIT;
            p->code[atom_start].addr1 = atom_start + 1; // Try matching
            p->code[atom_start].addr2 = end_addr + 1;   // Or skip
            break;
        }
        
        case '{': {
            int min, max;
            if (parse_quantifier_range(p, &min, &max)) {
                int atom_size = p->code_len - atom_start;
                
                // Duplicate the atom (min-1) additional times for required repetitions
                for (int i = 1; i < min; i++) {
                    if (p->code_len + atom_size >= p->code_capacity) {
                        p->code_capacity = (p->code_len + atom_size) * 2;
                        p->code = realloc(p->code, p->code_capacity * sizeof(VMInstruction));
                    }
                    memcpy(&p->code[p->code_len], &p->code[atom_start], 
                           atom_size * sizeof(VMInstruction));
                    p->code_len += atom_size;
                }
                
                // Add optional repetitions for (max - min) additional matches
                if (max == -1) {
                    // {n,} - unlimited repetitions after minimum: add * quantifier
                    // Pattern: L1: SPLIT(L2, L3) L2: atom JUMP(L1) L3:
                    int split_pos = p->code_len;
                    emit_split(p, split_pos + 1, 0); // addr2 will be patched
                    
                    // Copy the atom for the repeating part
                    if (p->code_len + atom_size >= p->code_capacity) {
                        p->code_capacity = (p->code_len + atom_size) * 2;
                        p->code = realloc(p->code, p->code_capacity * sizeof(VMInstruction));
                    }
                    memcpy(&p->code[p->code_len], &p->code[atom_start], 
                           atom_size * sizeof(VMInstruction));
                    p->code_len += atom_size;
                    
                    // Jump back to the SPLIT for the loop
                    emit_jump(p, split_pos);
                    
                    // Patch the SPLIT's second address to point after the loop
                    p->code[split_pos].addr2 = p->code_len;
                } else if (max > min) {
                    // {n,m} - add (max-min) optional repetitions
                    // Each optional repetition is wrapped in SPLIT(try, skip)
                    for (int i = min; i < max; i++) {
                        int optional_start = p->code_len;
                        
                        // Insert SPLIT before the optional repetition
                        if (p->code_len + atom_size + 1 >= p->code_capacity) {
                            p->code_capacity = (p->code_len + atom_size + 1) * 2;
                            p->code = realloc(p->code, p->code_capacity * sizeof(VMInstruction));
                        }
                        
                        // Make room for SPLIT instruction
                        memmove(&p->code[optional_start + 1], &p->code[optional_start],
                                (p->code_len - optional_start) * sizeof(VMInstruction));
                        p->code_len++;
                        
                        // Add SPLIT: try the optional repetition or skip it
                        p->code[optional_start].op = OP_SPLIT;
                        p->code[optional_start].addr1 = optional_start + 1; // Try repetition
                        p->code[optional_start].addr2 = optional_start + 1 + atom_size; // Skip repetition
                        
                        // Copy the atom for this optional repetition
                        memcpy(&p->code[optional_start + 1], &p->code[atom_start], 
                               atom_size * sizeof(VMInstruction));
                        p->code_len += atom_size;
                    }
                }
            } else {
                // Invalid quantifier - treat { as literal character
                // Since the atom was already parsed, we need to add the { after it
                emit_char(p, OP_CHAR, '{');
                p->pos++; // Consume the {
            }
            break;
        }
    }
}

// Parse sequence of atoms
static void parse_sequence(BytecodeParser *p) {
    while (p->pos < p->len && p->pattern[p->pos] != '|' && p->pattern[p->pos] != ')') {
        parse_quantified_atom(p);
    }
}

// Parse alternation a|b|c using simple recursive approach
static void parse_alternation(BytecodeParser *p) {
    int start_pos = p->code_len;
    parse_sequence(p);
    
    if (p->pos < p->len && p->pattern[p->pos] == '|') {
        // Move first alternative down by 1 to make room for SPLIT
        memmove(&p->code[start_pos + 1], &p->code[start_pos],
                (p->code_len - start_pos) * sizeof(VMInstruction));
        p->code_len++;
        
        // Insert SPLIT at start
        p->code[start_pos].op = OP_SPLIT;
        p->code[start_pos].addr1 = start_pos + 1; // First alternative
        
        // Add JUMP after first alternative
        int jump_addr = p->code_len;
        emit_jump(p, 0); // Will be patched to end
        
        // Parse rest of alternation (|b|c becomes b|c)
        int alt_start = p->code_len;
        p->code[start_pos].addr2 = alt_start; // SPLIT points to second alternative
        
        p->pos++; // Skip '|'
        parse_alternation(p); // Recursive call handles remaining alternatives
        
        // Patch the JUMP to point to end
        p->code[jump_addr].addr1 = p->code_len;
    }
}

// Compile pattern to bytecode
CompiledRegex* regex_compile(const char *pattern, int flags) {
    BytecodeParser parser = {
        .pattern = pattern,
        .pos = 0,
        .len = strlen(pattern),
        .flags = flags,
        .group_counter = 1, // Group 0 is full match
        .code_capacity = 64,
        .code_len = 0
    };
    
    parser.code = malloc(parser.code_capacity * sizeof(VMInstruction));
    
    // Save start of full match (group 0)
    emit_save(&parser, 0);
    
    parse_alternation(&parser);
    
    // Save end of full match (group 0) 
    emit_save(&parser, 1);
    emit_op(&parser, OP_MATCH);
    
    CompiledRegex *regex = malloc(sizeof(CompiledRegex));
    regex->code = parser.code;
    regex->code_len = parser.code_len;
    regex->code_capacity = parser.code_capacity;
    regex->group_count = parser.group_counter;
    regex->flags = flags;
    
    return regex;
}

// VM execution with backtracking
static void push_backtrack(VMState *state, int pc, int text_pos) {
    if (state->stack_top >= state->stack_capacity) {
        state->stack_capacity *= 2;
        state->stack = realloc(state->stack, state->stack_capacity * sizeof(BacktrackFrame));
    }
    
    BacktrackFrame *frame = &state->stack[state->stack_top++];
    frame->pc = pc;
    frame->text_pos = text_pos;
    
    // Save group positions
    frame->group_starts = malloc(state->group_count * sizeof(int));
    frame->group_ends = malloc(state->group_count * sizeof(int));
    memcpy(frame->group_starts, state->group_starts, state->group_count * sizeof(int));
    memcpy(frame->group_ends, state->group_ends, state->group_count * sizeof(int));
}

static int pop_backtrack(VMState *state) {
    if (state->stack_top <= 0) return 0;
    
    // Check backtracking limit to prevent exponential behavior
    state->backtrack_count++;
    if (state->backtrack_count > state->max_backtracks) {
        return 0; // Give up to prevent hang
    }
    
    BacktrackFrame *frame = &state->stack[state->stack_top - 1];
    
    // Advanced loop detection: track multiple PC/text_pos combinations
    int key = (frame->pc * 31 + frame->text_pos) % state->loop_detection_size;
    if (state->loop_detection_pc[key] == frame->pc && 
        state->loop_detection_text[key] == frame->text_pos) {
        state->loop_detection_count[key]++;
        if (state->loop_detection_count[key] > 20) { // Lower threshold for safety
            return 0; // Break the loop
        }
    } else {
        state->loop_detection_pc[key] = frame->pc;
        state->loop_detection_text[key] = frame->text_pos;
        state->loop_detection_count[key] = 1;
    }
    
    state->stack_top--;
    frame = &state->stack[state->stack_top];
    state->pc = frame->pc;
    state->text_pos = frame->text_pos;
    
    // Restore group positions
    memcpy(state->group_starts, frame->group_starts, state->group_count * sizeof(int));
    memcpy(state->group_ends, frame->group_ends, state->group_count * sizeof(int));
    
    free(frame->group_starts);
    free(frame->group_ends);
    
    return 1;
}

// Character matching with flags
static int vm_match_char(VMState *state, VMInstruction *inst, char c) {
    if (c == '\0') return 0; // End of input
    
    char test_c = c;
    if (state->flags & FLAG_IGNORECASE) {
        test_c = tolower(c);
    }
    
    switch (inst->op) {
        case OP_CHAR: {
            char pattern_c = inst->c;
            if (state->flags & FLAG_IGNORECASE) {
                pattern_c = tolower(pattern_c);
            }
            return pattern_c == test_c;
        }
        
        case OP_DOT:
            if (c == '\n' && !(state->flags & FLAG_DOTALL)) return 0;
            return 1;
            
        case OP_CHARSET: {
            int matches = charset_contains(inst->charset, test_c);
            if (state->flags & FLAG_IGNORECASE) {
                matches = matches || charset_contains(inst->charset, toupper(c));
            }
            return inst->negate ? !matches : matches;
        }
        
        case OP_DIGIT: return isdigit(c);
        case OP_NOT_DIGIT: return !isdigit(c);
        case OP_WORD: return isalnum(c) || c == '_';
        case OP_NOT_WORD: return !isalnum(c) && c != '_';
        case OP_SPACE: return isspace(c);
        case OP_NOT_SPACE: return !isspace(c);
    }
    
    return 0;
}

// Execute VM
static int vm_execute(CompiledRegex *regex, VMState *state) {
    int instruction_count = 0;
    const int max_instructions = 100000; // Prevent infinite loops in VM
    
    while (state->pc < regex->code_len && instruction_count < max_instructions) {
        instruction_count++;
        VMInstruction *inst = &regex->code[state->pc];
        
        switch (inst->op) {
            case OP_CHAR:
            case OP_DOT:
            case OP_CHARSET:
            case OP_DIGIT:
            case OP_NOT_DIGIT:
            case OP_WORD:
            case OP_NOT_WORD:
            case OP_SPACE:
            case OP_NOT_SPACE:
                if (state->text_pos >= state->text_len ||
                    !vm_match_char(state, inst, state->text[state->text_pos])) {
                    // Character didn't match, backtrack
                    if (!pop_backtrack(state)) {
                        return 0; // No more choices
                    }
                    continue;
                }
                state->text_pos++;
                state->pc++;
                break;
                
            case OP_ANCHOR_START:
                if (state->text_pos != 0 &&
                    (!(state->flags & FLAG_MULTILINE) || 
                     state->text[state->text_pos - 1] != '\n')) {
                    if (!pop_backtrack(state)) return 0;
                    continue;
                }
                state->pc++;
                break;
                
            case OP_ANCHOR_END:
                if (state->text_pos != state->text_len &&
                    (!(state->flags & FLAG_MULTILINE) ||
                     state->text[state->text_pos] != '\n')) {
                    if (!pop_backtrack(state)) return 0;
                    continue;
                }
                state->pc++;
                break;
                
            case OP_SPLIT:
                // Save backtrack point for second choice
                push_backtrack(state, inst->addr2, state->text_pos);
                // Take first choice
                state->pc = inst->addr1;
                break;
                
            case OP_JUMP:
                state->pc = inst->addr1;
                break;
                
            case OP_SAVE:
                if (inst->group_num / 2 < state->group_count) {
                    if (inst->group_num % 2 == 0) {
                        // Even: group start
                        state->group_starts[inst->group_num / 2] = state->text_pos;
                    } else {
                        // Odd: group end
                        state->group_ends[inst->group_num / 2] = state->text_pos;
                    }
                }
                state->pc++;
                break;
                
            case OP_MATCH:
                return 1; // Success!
                
            case OP_FAIL:
                if (!pop_backtrack(state)) return 0;
                break;
                
            default:
                state->pc++;
                break;
        }
    }
    
    return 0; // Fell off end without matching
}

// Execute regex against text starting at position
MatchResult* regex_execute(CompiledRegex *regex, const char *text, int start_pos) {
    if (!regex || !text) return NULL;
    
    int text_len = strlen(text);
    
    // Try matching at each position starting from start_pos
    for (int pos = start_pos; pos <= text_len; pos++) {
        VMState state = {
            .text = text,
            .text_len = text_len,
            .text_pos = pos,
            .pc = 0,
            .group_starts = calloc(regex->group_count, sizeof(int)),
            .group_ends = calloc(regex->group_count, sizeof(int)),
            .group_count = regex->group_count,
            .flags = regex->flags,
            .stack = malloc(64 * sizeof(BacktrackFrame)),
            .stack_top = 0,
            .stack_capacity = 64,
            .backtrack_count = 0,
            .max_backtracks = 1000,  // Much lower limit to prevent hangs
            .loop_detection_size = 256
        };
        
        // Initialize loop detection arrays
        state.loop_detection_pc = calloc(256, sizeof(int));
        state.loop_detection_text = calloc(256, sizeof(int));
        state.loop_detection_count = calloc(256, sizeof(int));
        for (int i = 0; i < 256; i++) {
            state.loop_detection_pc[i] = -1;
            state.loop_detection_text[i] = -1;
        };
        
        // Initialize group positions
        for (int i = 0; i < regex->group_count; i++) {
            state.group_starts[i] = -1;
            state.group_ends[i] = -1;
        }
        
        if (vm_execute(regex, &state)) {
            // Create match result
            MatchResult *result = malloc(sizeof(MatchResult));
            result->group_count = regex->group_count;
            result->groups = malloc(regex->group_count * sizeof(char*));
            result->group_starts = state.group_starts;
            result->group_ends = state.group_ends;
            result->index = pos;
            result->input = (char*)text; // Not owned
            
            // Extract group strings
            for (int i = 0; i < regex->group_count; i++) {
                if (state.group_starts[i] >= 0 && state.group_ends[i] >= 0) {
                    int len = state.group_ends[i] - state.group_starts[i];
                    result->groups[i] = malloc(len + 1);
                    strncpy(result->groups[i], text + state.group_starts[i], len);
                    result->groups[i][len] = '\0';
                } else {
                    result->groups[i] = NULL;
                }
            }
            
            // Clean up VM state
            for (int i = 0; i < state.stack_top; i++) {
                free(state.stack[i].group_starts);
                free(state.stack[i].group_ends);
            }
            free(state.stack);
            free(state.loop_detection_pc);
            free(state.loop_detection_text);
            free(state.loop_detection_count);
            
            return result;
        }
        
        // Clean up and try next position
        for (int i = 0; i < state.stack_top; i++) {
            free(state.stack[i].group_starts);
            free(state.stack[i].group_ends);
        }
        free(state.stack);
        free(state.group_starts);
        free(state.group_ends);
        free(state.loop_detection_pc);
        free(state.loop_detection_text);
        free(state.loop_detection_count);
        
        if (regex->flags & FLAG_STICKY) break; // Only try at exact position
    }
    
    return NULL;
}

// Public API implementation - unchanged
RegExp* regex_new(const char *pattern, const char *flags) {
    RegExp *regexp = malloc(sizeof(RegExp));
    
    // Parse flags
    int flag_bits = 0;
    if (flags) {
        for (const char *f = flags; *f; f++) {
            switch (*f) {
                case 'g': flag_bits |= FLAG_GLOBAL; break;
                case 'i': flag_bits |= FLAG_IGNORECASE; break;
                case 'm': flag_bits |= FLAG_MULTILINE; break;
                case 's': flag_bits |= FLAG_DOTALL; break;
                case 'u': flag_bits |= FLAG_UNICODE; break;
                case 'y': flag_bits |= FLAG_STICKY; break;
            }
        }
    }
    
    regexp->compiled = regex_compile(pattern, flag_bits);
    regexp->source = strdup(pattern);
    regexp->last_index = 0;
    regexp->flags = flag_bits;
    
    return regexp;
}

void regex_free(RegExp *regexp) {
    if (regexp) {
        if (regexp->compiled) {
            free(regexp->compiled->code);
            free(regexp->compiled);
        }
        free(regexp->source);
        free(regexp);
    }
}

int regex_test(RegExp *regexp, const char *text) {
    if (!regexp || !text) return 0;
    
    int start = (regexp->flags & FLAG_GLOBAL) ? regexp->last_index : 0;
    MatchResult *result = regex_execute(regexp->compiled, text, start);
    
    if (result) {
        if (regexp->flags & FLAG_GLOBAL) {
            regexp->last_index = result->group_ends[0];
        }
        match_result_free(result);
        return 1;
    }
    
    if (regexp->flags & FLAG_GLOBAL) {
        regexp->last_index = 0;
    }
    return 0;
}

MatchResult* regex_exec(RegExp *regexp, const char *text) {
    if (!regexp || !text) return NULL;
    
    int start = (regexp->flags & FLAG_GLOBAL) ? regexp->last_index : 0;
    MatchResult *result = regex_execute(regexp->compiled, text, start);
    
    if (result && (regexp->flags & FLAG_GLOBAL)) {
        regexp->last_index = result->group_ends[0];
    } else if (!result && (regexp->flags & FLAG_GLOBAL)) {
        regexp->last_index = 0;
    }
    
    return result;
}

MatchResult* string_match(const char *text, RegExp *regexp) {
    if (!text || !regexp) return NULL;
    
    // Reset lastIndex for non-global match
    if (!(regexp->flags & FLAG_GLOBAL)) {
        regexp->last_index = 0;
    }
    
    return regex_exec(regexp, text);
}

MatchIterator* string_match_all(const char *text, RegExp *regexp) {
    if (!text || !regexp) return NULL;
    
    // matchAll requires global flag
    if (!(regexp->flags & FLAG_GLOBAL)) return NULL;
    
    MatchIterator *iter = malloc(sizeof(MatchIterator));
    iter->regexp = regexp;
    iter->text = strdup(text);
    iter->done = 0;
    
    // Reset lastIndex
    regexp->last_index = 0;
    
    return iter;
}

MatchResult* match_iterator_next(MatchIterator *iter) {
    if (!iter || iter->done) return NULL;
    
    MatchResult *result = regex_exec(iter->regexp, iter->text);
    if (!result) {
        iter->done = 1;
        return NULL;
    }
    
    return result;
}

void match_iterator_free(MatchIterator *iter) {
    if (iter) {
        free(iter->text);
        free(iter);
    }
}

void match_result_free(MatchResult *result) {
    if (result) {
        for (int i = 0; i < result->group_count; i++) {
            free(result->groups[i]);
        }
        free(result->groups);
        free(result->group_starts);
        free(result->group_ends);
        free(result);
    }
}

// Debug utility
void regex_print_bytecode(CompiledRegex *regex) {
    printf("VM Bytecode (%d instructions):\n", regex->code_len);
    for (int i = 0; i < regex->code_len; i++) {
        printf("%3d: ", i);
        switch (regex->code[i].op) {
            case OP_CHAR: printf("CHAR '%c'", regex->code[i].c); break;
            case OP_DOT: printf("DOT"); break;
            case OP_CHARSET: printf("CHARSET"); break;
            case OP_DIGIT: printf("DIGIT"); break;
            case OP_WORD: printf("WORD"); break;
            case OP_SPACE: printf("SPACE"); break;
            case OP_SPLIT: printf("SPLIT %d %d", regex->code[i].addr1, regex->code[i].addr2); break;
            case OP_JUMP: printf("JUMP %d", regex->code[i].addr1); break;
            case OP_SAVE: printf("SAVE %d", regex->code[i].group_num); break;
            case OP_MATCH: printf("MATCH"); break;
            default: printf("UNKNOWN"); break;
        }
        printf("\n");
    }
}