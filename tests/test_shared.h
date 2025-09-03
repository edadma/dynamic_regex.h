#ifndef TEST_SHARED_H
#define TEST_SHARED_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../regex.h"
#include "../devdeps/unity/unity.h"

// Test helper macros
#define ASSERT_MATCH(pattern, text) do { \
    RegExp *re = regex_new(pattern, ""); \
    TEST_ASSERT_TRUE_MESSAGE(regex_test(re, text), "Pattern should match text"); \
    regex_free(re); \
} while(0)

#define ASSERT_NO_MATCH(pattern, text) do { \
    RegExp *re = regex_new(pattern, ""); \
    TEST_ASSERT_FALSE_MESSAGE(regex_test(re, text), "Pattern should not match text"); \
    regex_free(re); \
} while(0)

#define ASSERT_MATCH_WITH_FLAGS(pattern, flags, text) do { \
    RegExp *re = regex_new(pattern, flags); \
    TEST_ASSERT_TRUE_MESSAGE(regex_test(re, text), "Pattern with flags should match text"); \
    regex_free(re); \
} while(0)

#define ASSERT_NO_MATCH_WITH_FLAGS(pattern, flags, text) do { \
    RegExp *re = regex_new(pattern, flags); \
    TEST_ASSERT_FALSE_MESSAGE(regex_test(re, text), "Pattern with flags should not match text"); \
    regex_free(re); \
} while(0)

#define ASSERT_GROUP_MATCH(pattern, text, group_idx, expected) do { \
    RegExp *re = regex_new(pattern, ""); \
    MatchResult *result = regex_exec(re, text); \
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "Should find match"); \
    TEST_ASSERT_LESS_THAN_MESSAGE(result->group_count, group_idx, "Group index within bounds"); \
    if (expected) { \
        TEST_ASSERT_NOT_NULL_MESSAGE(result->groups[group_idx], "Group should exist"); \
        TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, result->groups[group_idx], "Group content should match"); \
    } else { \
        TEST_ASSERT_NULL_MESSAGE(result->groups[group_idx], "Group should be null"); \
    } \
    match_result_free(result); \
    regex_free(re); \
} while(0)

// Unity setup/teardown functions
void setUp(void);
void tearDown(void);

#endif // TEST_SHARED_H