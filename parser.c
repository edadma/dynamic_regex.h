// ================================================================
// PARSER IMPLEMENTATION FOR REGEX PARSING
// This file is included in regex.c via #include "parser.c"
// ================================================================

// Parser state
typedef struct {
    Lexer *lexer;
    int *group_counter;
    Token *current_token;
    int error;
    const char *error_message;
} Parser;

// Forward declarations for parser functions
static ASTNode* parse_alternation(Parser *parser);
static ASTNode* parse_concatenation(Parser *parser);
static ASTNode* parse_quantified(Parser *parser);
static ASTNode* parse_atom(Parser *parser);
static int parser_expect(Parser *parser, TokenType type);
static void parser_error(Parser *parser, const char *message);
static int parser_is_at_end(Parser *parser);

// Create new parser
static Parser* parser_new(Lexer *lexer, int *group_counter) {
    Parser *parser = malloc(sizeof(Parser));
    parser->lexer = lexer;
    parser->group_counter = group_counter;
    parser->current_token = lexer_peek(lexer);
    parser->error = 0;
    parser->error_message = NULL;
    return parser;
}

// Free parser
static void parser_free(Parser *parser) {
    if (parser) {
        free(parser);
    }
}

// Main parsing entry point
ASTNode* parser_parse(Parser *parser) {
    ASTNode *root = parse_alternation(parser);
    
    if (parser->error) {
        if (root) {
            free_ast(root);
        }
        return NULL;
    }
    
    if (!parser_is_at_end(parser)) {
        parser_error(parser, "Unexpected token at end of pattern");
        if (root) {
            free_ast(root);
        }
        return NULL;
    }
    
    return root;
}

// Parse alternation (lowest precedence): concatenation ('|' concatenation)*
ASTNode* parse_alternation(Parser *parser) {
    ASTNode *left = parse_concatenation(parser);
    if (!left || parser->error) return left;
    
    // Check if we have alternation
    if (parser->current_token->type != TOK_PIPE) {
        return left; // No alternation, just return the concatenation
    }
    
    // We have alternation - create alternation node
    ASTNode *alternation = create_ast_node(AST_ALTERNATION);
    add_alternation_child(alternation, left);
    
    while (parser->current_token->type == TOK_PIPE) {
        // Consume '|'
        lexer_next(parser->lexer);
        parser->current_token = lexer_peek(parser->lexer);
        
        // Parse next alternative
        ASTNode *right = parse_concatenation(parser);
        if (!right || parser->error) {
            free_ast(alternation);
            return NULL;
        }
        
        add_alternation_child(alternation, right);
    }
    
    return alternation;
}

// Parse concatenation: quantified+
ASTNode* parse_concatenation(Parser *parser) {
    ASTNode *sequence = create_ast_node(AST_SEQUENCE);
    
    // Parse sequence of quantified items
    while (!parser_is_at_end(parser) && 
           parser->current_token->type != TOK_PIPE &&
           parser->current_token->type != TOK_RPAREN) {
        
        ASTNode *item = parse_quantified(parser);
        if (!item || parser->error) {
            free_ast(sequence);
            return NULL;
        }
        
        add_sequence_child(sequence, item);
    }
    
    // If sequence is empty, return NULL
    if (sequence->data.sequence.child_count == 0) {
        free_ast(sequence);
        return create_ast_node(AST_SEQUENCE); // Empty sequence
    }
    
    // If sequence has only one child, return the child directly
    if (sequence->data.sequence.child_count == 1) {
        ASTNode *child = sequence->data.sequence.children[0];
        sequence->data.sequence.children[0] = NULL; // Prevent double-free
        free_ast(sequence);
        return child;
    }
    
    return sequence;
}

// Parse quantified: atom quantifier?
ASTNode* parse_quantified(Parser *parser) {
    ASTNode *atom = parse_atom(parser);
    if (!atom || parser->error) return atom;
    
    // Check for quantifier
    TokenType type = parser->current_token->type;
    if (type == TOK_STAR || type == TOK_PLUS || type == TOK_QUESTION || type == TOK_QUANTIFIER) {
        Token *quantifier_token = lexer_next(parser->lexer);
        parser->current_token = lexer_peek(parser->lexer);
        
        ASTNode *quantifier = create_ast_node(AST_QUANTIFIER);
        quantifier->data.quantifier.target = atom;
        
        switch (type) {
            case TOK_STAR:
                quantifier->data.quantifier.quantifier = '*';
                quantifier->data.quantifier.min_count = 0;
                quantifier->data.quantifier.max_count = -1;
                break;
            case TOK_PLUS:
                quantifier->data.quantifier.quantifier = '+';
                quantifier->data.quantifier.min_count = 1;
                quantifier->data.quantifier.max_count = -1;
                break;
            case TOK_QUESTION:
                quantifier->data.quantifier.quantifier = '?';
                quantifier->data.quantifier.min_count = 0;
                quantifier->data.quantifier.max_count = 1;
                break;
            case TOK_QUANTIFIER:
                quantifier->data.quantifier.quantifier = '{';
                quantifier->data.quantifier.min_count = quantifier_token->data.min_count;
                quantifier->data.quantifier.max_count = quantifier_token->data.max_count;
                break;
            default:
                break;
        }
        
        return quantifier;
    }
    
    return atom;
}

// Parse atom: char | charset | group | anchor | '.'
ASTNode* parse_atom(Parser *parser) {
    Token *token = parser->current_token;
    
    switch (token->type) {
        case TOK_CHAR: {
            // Save character before advancing lexer
            char character = token->data.character;
            
            lexer_next(parser->lexer);
            parser->current_token = lexer_peek(parser->lexer);
            
            ASTNode *node = create_ast_node(AST_CHAR);
            node->data.character = character;
            return node;
        }
        
        case TOK_DOT: {
            lexer_next(parser->lexer);
            parser->current_token = lexer_peek(parser->lexer);
            
            return create_ast_node(AST_DOT);
        }
        
        case TOK_CHARSET: {
            // Save charset data before advancing lexer
            uint8_t charset[32];
            memcpy(charset, token->data.charset, 32);
            int negate = token->data.negate;
            
            lexer_next(parser->lexer);
            parser->current_token = lexer_peek(parser->lexer);
            
            ASTNode *node = create_ast_node(AST_CHARSET);
            memcpy(node->data.charset.charset, charset, 32);
            node->data.charset.negate = negate;
            return node;
        }
        
        case TOK_CARET: {
            lexer_next(parser->lexer);
            parser->current_token = lexer_peek(parser->lexer);
            
            return create_ast_node(AST_ANCHOR_START);
        }
        
        case TOK_DOLLAR: {
            lexer_next(parser->lexer);
            parser->current_token = lexer_peek(parser->lexer);
            
            return create_ast_node(AST_ANCHOR_END);
        }
        
        case TOK_LPAREN: {
            // Parse group: '(' alternation ')'
            lexer_next(parser->lexer); // consume '('
            parser->current_token = lexer_peek(parser->lexer);
            
            // Create group node
            ASTNode *group = create_ast_node(AST_GROUP);
            (*parser->group_counter)++;
            group->data.group.group_number = *parser->group_counter;
            
            // Parse group content
            group->data.group.content = parse_alternation(parser);
            if (!group->data.group.content || parser->error) {
                free_ast(group);
                return NULL;
            }
            
            // Expect closing ')'
            if (!parser_expect(parser, TOK_RPAREN)) {
                parser_error(parser, "Expected ')' after group");
                free_ast(group);
                return NULL;
            }
            
            return group;
        }
        
        default:
            parser_error(parser, "Expected atom (character, charset, group, or anchor)");
            return NULL;
    }
}

// Utility functions
int parser_expect(Parser *parser, TokenType type) {
    if (parser->current_token->type == type) {
        lexer_next(parser->lexer);
        parser->current_token = lexer_peek(parser->lexer);
        return 1;
    }
    return 0;
}

void parser_error(Parser *parser, const char *message) {
    parser->error = 1;
    parser->error_message = message;
}

int parser_is_at_end(Parser *parser) {
    return parser->current_token->type == TOK_EOF;
}

// Entry point for parsing a pattern with the new lexer+parser
static ASTNode* parse_pattern_with_lexer(const char *pattern, int *group_counter) {
    Lexer *lexer = lexer_new(pattern);
    Parser *parser = parser_new(lexer, group_counter);
    
    ASTNode *ast = parser_parse(parser);
    
    parser_free(parser);
    lexer_free(lexer);
    
    return ast;
}