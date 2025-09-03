#include "test_shared.h"

void test_null_inputs(void) {
    RegExp *re = regex_new("test", "");

    TEST_ASSERT_FALSE(regex_test(NULL, "text"));
    TEST_ASSERT_FALSE(regex_test(re, NULL));
    TEST_ASSERT_NULL(regex_exec(NULL, "text"));
    TEST_ASSERT_NULL(regex_exec(re, NULL));
    TEST_ASSERT_NULL(string_match(NULL, re));
    TEST_ASSERT_NULL(string_match("text", NULL));

    regex_free(re);
    regex_free(NULL); // Should not crash
}

void test_invalid_quantifiers(void) {
    // These should not crash, might match literally or fail gracefully
    RegExp *re1 = regex_new("a{", "");
    TEST_ASSERT_NOT_NULL(re1);
    regex_free(re1);

    RegExp *re2 = regex_new("a{5,3}", ""); // Invalid range
    TEST_ASSERT_NOT_NULL(re2);
    regex_free(re2);
}

void test_memory_cleanup(void) {
    // Test that we don't leak memory on multiple operations
    for (int i = 0; i < 100; i++) {
        RegExp *re = regex_new("test\\d+", "gi");
        TEST_ASSERT_NOT_NULL(re);
        
        MatchResult *result = regex_exec(re, "test123 test456");
        if (result) {
            match_result_free(result);
        }
        
        regex_free(re);
    }
}