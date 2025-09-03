// ================================================================
// LEXER IMPLEMENTATION FOR REGEX PARSING
// This file is included in regex.c via #include "lexer.c"
// ================================================================

// Token types for regex lexing
typedef enum {
    TOK_EOF, TOK_CHAR, TOK_DOT, TOK_STAR, TOK_PLUS, TOK_QUESTION, TOK_PIPE,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACKET, TOK_RBRACKET, TOK_LBRACE, TOK_RBRACE,
    TOK_CARET, TOK_DOLLAR, TOK_CHARSET, TOK_QUANTIFIER, TOK_WORD_BOUNDARY, TOK_ERROR
} TokenType;

// Token structure
typedef struct {
    TokenType type;
    union {
        char character;
        struct {
            uint8_t charset[32];
            int negate;
        };
        struct {
            int min_count;
            int max_count;
        };
    } data;
    int position;
} Token;

// Lexer state
typedef struct {
    const char *input;
    int pos;
    int len;
    Token current_token;
    int has_token;
    int state;
    int paren_depth;
    int bracket_depth;
} Lexer;

// Forward declarations for lexer functions
static Token* lexer_peek(Lexer *lexer);
static Token* lexer_next(Lexer *lexer);
static Token* lexer_read_next_token(Lexer *lexer);

// Create new lexer
static Lexer* lexer_new(const char *input) {
    Lexer *lexer = malloc(sizeof(Lexer));
    lexer->input = input;
    lexer->pos = 0;
    lexer->len = strlen(input);
    lexer->state = 0; // LEX_NORMAL = 0
    lexer->paren_depth = 0;
    lexer->bracket_depth = 0;
    lexer->has_token = 0;
    return lexer;
}

// Free lexer  
static void lexer_free(Lexer *lexer) {
    if (lexer) free(lexer);
}

// Peek at next token without consuming it
static Token* lexer_peek(Lexer *lexer) {
    if (!lexer->has_token) {
        lexer_read_next_token(lexer);
        lexer->has_token = 1;
    }
    return &lexer->current_token;
}

// Get next token and advance
static Token* lexer_next(Lexer *lexer) {
    if (!lexer->has_token) {
        lexer_read_next_token(lexer);
    }
    lexer->has_token = 0;
    return &lexer->current_token;
}


// Read character class [...] 
static Token* lexer_read_charset(Lexer *lexer) {
    Token *token = &lexer->current_token;
    token->type = TOK_CHARSET;
    token->position = lexer->pos - 1; // Start at '['
    
    // Initialize charset bitmap
    memset(token->data.charset, 0, 32);
    token->data.negate = 0;
    
    // Check for negation
    if (lexer->pos < lexer->len && lexer->input[lexer->pos] == '^') {
        token->data.negate = 1;
        lexer->pos++;
    }
    
    // Parse character class content
    while (lexer->pos < lexer->len && lexer->input[lexer->pos] != ']') {
        char ch = lexer->input[lexer->pos];
        
        if (ch == '\\' && lexer->pos + 1 < lexer->len) {
            // Handle escape sequences in character class
            lexer->pos++;
            char escaped = lexer->input[lexer->pos];
            
            switch (escaped) {
                case 'd': // \d = [0-9]
                    for (char c = '0'; c <= '9'; c++) {
                        int bit = (unsigned char)c;
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    break;
                case 'w': // \w = [a-zA-Z0-9_]
                    for (char c = 'a'; c <= 'z'; c++) {
                        int bit = (unsigned char)c;
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    for (char c = 'A'; c <= 'Z'; c++) {
                        int bit = (unsigned char)c;
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    for (char c = '0'; c <= '9'; c++) {
                        int bit = (unsigned char)c;
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    {
                        int bit = (unsigned char)'_';
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    break;
                case 's': // \s = [ \t\n\r\f\v]
                    {
                        const char *whitespace = " \t\n\r\f\v";
                        for (int i = 0; whitespace[i]; i++) {
                            int bit = (unsigned char)whitespace[i];
                            token->data.charset[bit / 8] |= (1 << (bit % 8));
                        }
                    }
                    break;
                case 'n': // \n -> newline character
                    {
                        int bit = (unsigned char)'\n';
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    break;
                case 't': // \t -> tab character  
                    {
                        int bit = (unsigned char)'\t';
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    break;
                case 'r': // \r -> carriage return
                    {
                        int bit = (unsigned char)'\r';
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    break;
                default:
                    // Literal escaped character
                    {
                        int bit = (unsigned char)escaped;
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    break;
            }
            lexer->pos++;
        } else if (lexer->pos + 2 < lexer->len && lexer->input[lexer->pos + 1] == '-') {
            // Character range: a-z
            char start = ch;
            char end = lexer->input[lexer->pos + 2];
            for (char c = start; c <= end; c++) {
                int bit = (unsigned char)c;
                token->data.charset[bit / 8] |= (1 << (bit % 8));
            }
            lexer->pos += 3; // Skip start, -, end
        } else {
            // Single character
            int bit = (unsigned char)ch;
            token->data.charset[bit / 8] |= (1 << (bit % 8));
            lexer->pos++;
        }
    }
    
    // Skip closing ]
    if (lexer->pos < lexer->len && lexer->input[lexer->pos] == ']') {
        lexer->pos++;
    } else {
        token->type = TOK_ERROR;
        return token;
    }
    
    return token;
}

// Read quantifier {n,m}
static Token* lexer_read_quantifier(Lexer *lexer) {
    Token *token = &lexer->current_token;
    token->type = TOK_QUANTIFIER;
    token->position = lexer->pos - 1; // Start at '{'
    
    // Parse minimum count
    int min_count = 0;
    while (lexer->pos < lexer->len && lexer->input[lexer->pos] >= '0' && lexer->input[lexer->pos] <= '9') {
        min_count = min_count * 10 + (lexer->input[lexer->pos] - '0');
        lexer->pos++;
    }
    
    int max_count = min_count; // Default for {n}
    
    // Check for comma
    if (lexer->pos < lexer->len && lexer->input[lexer->pos] == ',') {
        lexer->pos++;
        
        // Check if there's a number after comma
        if (lexer->pos < lexer->len && lexer->input[lexer->pos] >= '0' && lexer->input[lexer->pos] <= '9') {
            max_count = 0;
            while (lexer->pos < lexer->len && lexer->input[lexer->pos] >= '0' && lexer->input[lexer->pos] <= '9') {
                max_count = max_count * 10 + (lexer->input[lexer->pos] - '0');
                lexer->pos++;
            }
        } else {
            max_count = -1; // {n,} = unlimited
        }
    }
    
    // Skip closing }
    if (lexer->pos < lexer->len && lexer->input[lexer->pos] == '}') {
        lexer->pos++;
    } else {
        token->type = TOK_ERROR;
        return token;
    }
    
    token->data.min_count = min_count;
    token->data.max_count = max_count;
    return token;
}

// Read escape sequence \x
static Token* lexer_read_escape_sequence(Lexer *lexer) {
    Token *token = &lexer->current_token;
    token->position = lexer->pos - 1; // Start at '\'
    
    if (lexer->pos >= lexer->len) {
        token->type = TOK_ERROR;
        return token;
    }
    
    char escaped = lexer->input[lexer->pos];
    lexer->pos++;
    
    switch (escaped) {
        case 'd': // \d -> character class
        case 'w': // \w -> character class  
        case 's': // \s -> character class
        case 'D': // \D -> negated character class
        case 'W': // \W -> negated character class
        case 'S': // \S -> negated character class
            token->type = TOK_CHARSET;
            memset(token->data.charset, 0, 32);
            
            switch (escaped) {
                case 'd':
                    token->data.negate = 0;
                    for (char c = '0'; c <= '9'; c++) {
                        int bit = (unsigned char)c;
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    break;
                case 'w':
                    token->data.negate = 0;
                    for (char c = 'a'; c <= 'z'; c++) {
                        int bit = (unsigned char)c;
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    for (char c = 'A'; c <= 'Z'; c++) {
                        int bit = (unsigned char)c;
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    for (char c = '0'; c <= '9'; c++) {
                        int bit = (unsigned char)c;
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    {
                        int bit = (unsigned char)'_';
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    break;
                case 's':
                    token->data.negate = 0;
                    {
                        const char *whitespace = " \t\n\r\f\v";
                        for (int i = 0; whitespace[i]; i++) {
                            int bit = (unsigned char)whitespace[i];
                            token->data.charset[bit / 8] |= (1 << (bit % 8));
                        }
                    }
                    break;
                case 'D':
                    token->data.negate = 1;
                    for (char c = '0'; c <= '9'; c++) {
                        int bit = (unsigned char)c;
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    break;
                case 'W':
                    token->data.negate = 1;
                    for (char c = 'a'; c <= 'z'; c++) {
                        int bit = (unsigned char)c;
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    for (char c = 'A'; c <= 'Z'; c++) {
                        int bit = (unsigned char)c;
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    for (char c = '0'; c <= '9'; c++) {
                        int bit = (unsigned char)c;
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    {
                        int bit = (unsigned char)'_';
                        token->data.charset[bit / 8] |= (1 << (bit % 8));
                    }
                    break;
                case 'S':
                    token->data.negate = 1;
                    {
                        const char *whitespace = " \t\n\r\f\v";
                        for (int i = 0; whitespace[i]; i++) {
                            int bit = (unsigned char)whitespace[i];
                            token->data.charset[bit / 8] |= (1 << (bit % 8));
                        }
                    }
                    break;
            }
            break;
            
        case 'b': // \b -> word boundary
            token->type = TOK_WORD_BOUNDARY;
            break;
            
        case 'n': // \n -> newline character
            token->type = TOK_CHAR;
            token->data.character = '\n';
            break;
        case 't': // \t -> tab character  
            token->type = TOK_CHAR;
            token->data.character = '\t';
            break;
        case 'r': // \r -> carriage return
            token->type = TOK_CHAR;
            token->data.character = '\r';
            break;
        default:
            // Regular escaped character
            token->type = TOK_CHAR;
            token->data.character = escaped;
            break;
    }
    
    return token;
}

// Read next token from input
static Token* lexer_read_next_token(Lexer *lexer) {
    Token *token = &lexer->current_token;
    
    if (lexer->pos >= lexer->len) {
        token->type = TOK_EOF;
        token->position = lexer->pos;
        return token;
    }
    
    char ch = lexer->input[lexer->pos];
    token->position = lexer->pos;
    lexer->pos++;
    
    switch (ch) {
        case '.':
            token->type = TOK_DOT;
            break;
        case '*':
            token->type = TOK_STAR;
            break;
        case '+':
            token->type = TOK_PLUS;
            break;
        case '?':
            token->type = TOK_QUESTION;
            break;
        case '|':
            token->type = TOK_PIPE;
            break;
        case '(':
            token->type = TOK_LPAREN;
            lexer->paren_depth++;
            break;
        case ')':
            token->type = TOK_RPAREN;
            lexer->paren_depth--;
            break;
        case '[':
            // Read complete character class
            return lexer_read_charset(lexer);
        case ']':
            token->type = TOK_RBRACKET;
            break;
        case '{':
            // Read complete quantifier
            return lexer_read_quantifier(lexer);
        case '}':
            token->type = TOK_RBRACE;
            break;
        case '^':
            token->type = TOK_CARET;
            break;
        case '$':
            token->type = TOK_DOLLAR;
            break;
        case '\\':
            // Read escape sequence
            return lexer_read_escape_sequence(lexer);
        default:
            // Regular character
            token->type = TOK_CHAR;
            token->data.character = ch;
            break;
    }
    
    return token;
}