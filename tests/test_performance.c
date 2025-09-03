#include "test_shared.h"

void test_pathological_patterns(void) {
    // Test patterns that could cause exponential backtracking in naive implementations
    // Our NFA-based engine should handle these efficiently
    
    RegExp *re = regex_new("(a+)+b", "");
    TEST_ASSERT_NOT_NULL(re);
    
    // This should not hang or take excessive time
    int result = regex_test(re, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaac");
    TEST_ASSERT_FALSE(result);  // Should not match
    
    regex_free(re);
    
    // Another pathological case
    re = regex_new("(a|a)*b", "");
    TEST_ASSERT_NOT_NULL(re);
    
    result = regex_test(re, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab");
    TEST_ASSERT_TRUE(result);  // Should match
    
    regex_free(re);
}

void test_large_input(void) {
    RegExp *re = regex_new("\\d+", "g");
    TEST_ASSERT_NOT_NULL(re);
    
    // Create a large string with numbers
    char *large_text = malloc(10000);
    for (int i = 0; i < 9999; i++) {
        large_text[i] = (i % 10) + '0';  // Fill with digits 0-9
    }
    large_text[9999] = '\0';
    
    // Should be able to handle large input
    int result = regex_test(re, large_text);
    TEST_ASSERT_TRUE(result);
    
    free(large_text);
    regex_free(re);
}