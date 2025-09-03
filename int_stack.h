#ifndef INT_STACK_H
#define INT_STACK_H

// Immutable integer stack with reference counting
// Empty stack is NULL, non-empty stack is a node
typedef struct IntStack {
    int value;
    struct IntStack *tail;
    int ref_count;
} IntStack;

// Stack operations
IntStack* int_stack_new(void);
IntStack* int_stack_push(IntStack *stack, int value);
IntStack* int_stack_pop(IntStack *stack, int *value);
void int_stack_retain(IntStack *stack);
void int_stack_release(IntStack *stack);
int int_stack_is_empty(IntStack *stack);
int int_stack_peek(IntStack *stack);

#endif