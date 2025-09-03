
static int execute(CompiledRegex *compiled, VM *vm) {
    int instruction_count = 0;
    const int max_instructions = 100000;

    while (vm->pc < compiled->code_len && instruction_count < max_instructions) {
        instruction_count++;
        Instruction *inst = &compiled->code[vm->pc];

        switch (inst->op) {
            case OP_CHAR:
                if (vm->pos >= vm->text_len) {
                    vm->last_operation_success = 0;
                    if (!pop_choice(vm)) return 0;
                    continue;
                }

                char text_char = vm->text[vm->pos];
                char pattern_char = inst->c;

                // Check for match (case sensitive or insensitive)
                int char_matches = 0;
                if (vm->flags & 2) { // Case insensitive flag 'i'
                    char_matches = (tolower(text_char) == tolower(pattern_char));
                } else {
                    char_matches = (text_char == pattern_char);
                }

                if (!char_matches) {
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
                int matches = 0;

                // Check direct character match
                int bit = (unsigned char)test_ch;
                matches = (inst->charset[bit / 8] & (1 << (bit % 8))) != 0;

                // If case insensitive flag is set and no match yet, try opposite case
                if (!matches && (vm->flags & 2)) { // Case insensitive flag 'i'
                    char opposite_case;
                    if (islower(test_ch)) {
                        opposite_case = toupper(test_ch);
                    } else if (isupper(test_ch)) {
                        opposite_case = tolower(test_ch);
                    } else {
                        opposite_case = test_ch; // No case change for non-letters
                    }

                    bit = (unsigned char)opposite_case;
                    matches = (inst->charset[bit / 8] & (1 << (bit % 8))) != 0;
                }

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
