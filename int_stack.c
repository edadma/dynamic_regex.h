#include "int_stack.h"
#include <stdlib.h>

// Create new empty stack
IntStack* int_stack_new(void) {
    return NULL;  // Empty stack is just NULL
}

// Retain a stack (increment reference count)
void int_stack_retain(IntStack *stack) {
    if (stack) {
        stack->ref_count++;
    }
}

// Release a stack
void int_stack_release(IntStack *stack) {
    if (!stack) return;
    
    stack->ref_count--;
    if (stack->ref_count <= 0) {
        IntStack *tail = stack->tail;
        free(stack);
        int_stack_release(tail);  // Recursive release
    }
}

// Push operation - creates new stack sharing tail
IntStack* int_stack_push(IntStack *stack, int value) {
    IntStack *new_stack = malloc(sizeof(IntStack));
    new_stack->value = value;
    new_stack->tail = stack;
    new_stack->ref_count = 1;
    
    // Increment reference count of shared tail
    if (new_stack->tail) {
        new_stack->tail->ref_count++;
    }
    
    return new_stack;
}

// Pop operation - creates new stack pointing to tail
IntStack* int_stack_pop(IntStack *stack, int *value) {
    if (!stack) {
        if (value) *value = 0;
        return NULL;  // Empty stack
    }
    
    if (value) *value = stack->value;
    
    IntStack *tail = stack->tail;
    
    // Increment reference count of tail
    if (tail) {
        tail->ref_count++;
    }
    
    return tail;
}

int int_stack_is_empty(IntStack *stack) {
    return stack == NULL;
}

int int_stack_peek(IntStack *stack) {
    if (!stack) return 0;
    return stack->value;
}