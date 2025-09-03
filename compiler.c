// ================================================================
// AST COMPILER - Compiles AST nodes to bytecode instructions
// ================================================================
// This file is amalgamated into regex.c - do not compile separately

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
            
        case AST_ALTERNATION:
            for (int i = 0; i < node->data.alternation.alternative_count; i++) {
                int child_max = count_groups(node->data.alternation.alternatives[i]);
                if (child_max > max_group) max_group = child_max;
            }
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
                // One-or-more: [pattern], CHOICE +2, BRANCH -N
                int loop_start = regex->code_len;
                
                // Compile the target pattern (required first match)
                compile_ast_node(node->data.quantifier.target, regex);
                
                // CHOICE: either continue to next instruction (exit) or jump to pattern again
                int choice_pc = emit_ast_instruction(regex, OP_CHOICE);
                regex->code[choice_pc].addr = 2; // Skip BRANCH to exit
                
                // BRANCH back to pattern for additional matches
                int branch_pc = emit_ast_instruction(regex, OP_BRANCH);
                regex->code[branch_pc].addr = loop_start - branch_pc;
                
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
        
        case AST_WORD_BOUNDARY: {
            emit_ast_instruction(regex, OP_WORD_BOUNDARY);
            break;
        }
        
        case AST_ALTERNATION: {
            // Alternation: CHOICE +skip1, [alt1], BRANCH +end, CHOICE +skip2, [alt2], BRANCH +end, ..., [lastalt]
            int alternative_count = node->data.alternation.alternative_count;
            int *branch_addrs = malloc(alternative_count * sizeof(int));
            
            // Compile all alternatives except the last
            for (int i = 0; i < alternative_count - 1; i++) {
                // Create choice point to skip to next alternative
                int choice_pc = emit_ast_instruction(regex, OP_CHOICE);
                
                // Compile this alternative
                compile_ast_node(node->data.alternation.alternatives[i], regex);
                
                // Branch to end of alternation
                branch_addrs[i] = emit_ast_instruction(regex, OP_BRANCH);
                
                // Update CHOICE to skip to next alternative (right here)
                regex->code[choice_pc].addr = regex->code_len - choice_pc;
            }
            
            // Compile the last alternative (no CHOICE needed)
            compile_ast_node(node->data.alternation.alternatives[alternative_count - 1], regex);
            
            // Update all BRANCH instructions to jump to here (end of alternation)
            for (int i = 0; i < alternative_count - 1; i++) {
                regex->code[branch_addrs[i]].addr = regex->code_len - branch_addrs[i];
            }
            
            free(branch_addrs);
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