#include "test_shared.h"

// Case insensitive flag tests
void test_case_insensitive_flag(void) {
    ASSERT_MATCH_WITH_FLAGS("hello", "i", "HELLO");
    ASSERT_MATCH_WITH_FLAGS("hello", "i", "Hello");
    ASSERT_MATCH_WITH_FLAGS("hello", "i", "HeLLo");
    ASSERT_MATCH_WITH_FLAGS("[a-z]+", "i", "HELLO");
}

// Note: test_case_insensitive_character_classes moved to test_charsets.c to avoid duplication

void test_global_flag_with_exec(void) {
    RegExp *re = regex_new("\\d+", "g");
    
    MatchResult *result1 = regex_exec(re, "123 abc 456 def 789");
    TEST_ASSERT_NOT_NULL_MESSAGE(result1, "First match should be found");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("123", result1->groups[0], "First match should be '123'");
    match_result_free(result1);
    
    MatchResult *result2 = regex_exec(re, "123 abc 456 def 789");
    TEST_ASSERT_NOT_NULL_MESSAGE(result2, "Second match should be found");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("456", result2->groups[0], "Second match should be '456'");
    match_result_free(result2);
    
    MatchResult *result3 = regex_exec(re, "123 abc 456 def 789");
    TEST_ASSERT_NOT_NULL_MESSAGE(result3, "Third match should be found");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("789", result3->groups[0], "Third match should be '789'");
    match_result_free(result3);
    
    MatchResult *result4 = regex_exec(re, "123 abc 456 def 789");
    TEST_ASSERT_NULL_MESSAGE(result4, "Fourth match should be NULL (end of matches)");
    
    // Reset and try again
    re->last_index = 0;
    MatchResult *result5 = regex_exec(re, "123 abc 456 def 789");
    TEST_ASSERT_NOT_NULL_MESSAGE(result5, "After reset, first match should be found again");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("123", result5->groups[0], "After reset, first match should be '123'");
    match_result_free(result5);
    
    regex_free(re);
}