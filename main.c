#include <stdio.h>
#include <stdlib.h>


#include <string.h>
#include <time.h>

#include "regex.h"
#include "devdeps/unity/unity.h"

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

// Unity setup/teardown
void setUp(void) {
    // Called before each test
}

void tearDown(void) {
    // Called after each test
}

// Basic literal matching tests
void test_literal_characters(void) {
    ASSERT_MATCH("hello", "hello");
    ASSERT_MATCH("hello", "say hello world");
    ASSERT_NO_MATCH("hello", "HELLO");
    ASSERT_NO_MATCH("hello", "help");
    ASSERT_NO_MATCH("hello", "");
}

void test_empty_pattern(void) {
    ASSERT_MATCH("", "");
    ASSERT_MATCH("", "anything");
}

// Dot wildcard tests
void test_dot_wildcard(void) {
    ASSERT_MATCH("h.llo", "hello");
    ASSERT_MATCH("h.llo", "hallo");
    ASSERT_MATCH("h.llo", "h@llo");
    ASSERT_NO_MATCH("h.llo", "hllo");
    ASSERT_NO_MATCH("h.llo", "h\nllo"); // Dot shouldn't match newline by default
}

void test_dot_with_dotall_flag(void) {
    ASSERT_MATCH_WITH_FLAGS("h.llo", "s", "h\nllo");
}

// Anchor tests
void test_start_anchor(void) {
    ASSERT_MATCH("^hello", "hello world");
    ASSERT_NO_MATCH("^hello", "say hello");
    ASSERT_MATCH("^$", "");
    ASSERT_NO_MATCH("^hello", "");
}

void test_end_anchor(void) {
    ASSERT_MATCH("world$", "hello world");
    ASSERT_NO_MATCH("world$", "world hello");
    ASSERT_MATCH("^hello$", "hello");
    ASSERT_NO_MATCH("^hello$", "hello world");
}

void test_multiline_anchors(void) {
    ASSERT_MATCH_WITH_FLAGS("^world", "m", "hello\nworld");
    ASSERT_MATCH_WITH_FLAGS("hello$", "m", "hello\nworld");
}

// Character class tests
void test_character_classes(void) {
    // Basic character classes
    ASSERT_MATCH("[abc]", "apple");
    ASSERT_MATCH("[abc]", "banana");
    ASSERT_MATCH("[abc]", "cherry");
    ASSERT_NO_MATCH("[abc]", "dog");

    // Single ranges
    ASSERT_MATCH("[a-z]", "hello");
    ASSERT_MATCH("[A-Z]", "HELLO");
    ASSERT_MATCH("[0-9]", "123");
    ASSERT_NO_MATCH("[a-z]", "HELLO");
    
    // Combined ranges and individual characters
    ASSERT_MATCH("[a-zA-Z]", "Hello");
    ASSERT_MATCH("[a-zA-Z]", "hello");
    ASSERT_MATCH("[a-zA-Z]", "HELLO");
    ASSERT_NO_MATCH("[a-zA-Z]", "123");
    
    // Multiple ranges with individual characters
    ASSERT_MATCH("[a-zA-Z0-9_]", "hello123");
    ASSERT_MATCH("[a-zA-Z0-9_]", "Test_42");
    ASSERT_MATCH("[a-zA-Z0-9_]", "_private");
    ASSERT_NO_MATCH("[a-zA-Z0-9_]", "@");
    
    // Mixed individual chars and ranges
    ASSERT_MATCH("[aeiou0-9]", "a1");
    ASSERT_MATCH("[aeiou0-9]", "e");
    ASSERT_MATCH("[aeiou0-9]", "5");
    ASSERT_NO_MATCH("[aeiou0-9]", "x");
    
    // Special characters in classes
    ASSERT_MATCH("[.-/]", ".txt");
    ASSERT_MATCH("[.-/]", "/path");
    ASSERT_NO_MATCH("[.-/]", "abc");
    
    // Escape sequences within character classes
    ASSERT_MATCH("[\\d\\w]", "5");      // Digit or word char
    ASSERT_MATCH("[\\d\\w]", "a");      // Digit or word char
    ASSERT_MATCH("[\\d\\w]", "_");      // Underscore is word char
    ASSERT_NO_MATCH("[\\d\\w]", "@");   // Special char not word/digit
    
    ASSERT_MATCH("[a-z\\s]", "hello");  // Letter or space
    ASSERT_MATCH("[a-z\\s]", " ");      // Space matches \\s
    ASSERT_MATCH("[a-z\\s]", "\t");     // Tab matches \\s
    ASSERT_NO_MATCH("[a-z\\s]", "A");   // Uppercase not in range
    
    ASSERT_MATCH("[\\n\\t\\r]", "\n");  // Literal escapes in class
    ASSERT_MATCH("[\\n\\t\\r]", "\t");  // Tab literal
    ASSERT_NO_MATCH("[\\n\\t\\r]", "a"); // Regular char doesn't match
}

void test_negated_character_classes(void) {
    // Basic negated classes
    ASSERT_MATCH("[^abc]", "dog");
    ASSERT_NO_MATCH("[^abc]", "a");
    ASSERT_MATCH("[^a-z]", "HELLO");
    ASSERT_MATCH("[^a-z]", "123");
    ASSERT_NO_MATCH("[^a-z]", "hello");
    
    // Negated combined ranges
    ASSERT_NO_MATCH("[^a-zA-Z0-9_]", "hello123");
    ASSERT_NO_MATCH("[^a-zA-Z0-9_]", "Test_42");
    ASSERT_MATCH("[^a-zA-Z0-9_]", "@");
    ASSERT_MATCH("[^a-zA-Z0-9_]", "!");
    
    // Negated mixed patterns
    ASSERT_NO_MATCH("[^aeiou0-9]", "a");
    ASSERT_NO_MATCH("[^aeiou0-9]", "5");
    ASSERT_MATCH("[^aeiou0-9]", "x");
    ASSERT_MATCH("[^aeiou0-9]", "z");
    
    // Negated with escape sequences
    ASSERT_NO_MATCH("[^\\d\\w]", "5");     // Digit matches, negated doesn't
    ASSERT_NO_MATCH("[^\\d\\w]", "a");     // Word char matches, negated doesn't
    ASSERT_MATCH("[^\\d\\w]", "@");        // Special char doesn't match, negated does
    
    ASSERT_NO_MATCH("[^\\s]", " ");        // Space matches \\s, negated doesn't
    ASSERT_NO_MATCH("[^\\s]", "\t");       // Tab matches \\s, negated doesn't
    ASSERT_MATCH("[^\\s]", "a");           // Letter doesn't match \\s, negated does
}

// Escape sequence tests
void test_digit_escape(void) {
    ASSERT_MATCH("\\d", "123");
    ASSERT_MATCH("\\d+", "123");
    ASSERT_NO_MATCH("\\d", "abc");

    ASSERT_MATCH("\\D", "abc");
    ASSERT_NO_MATCH("\\D", "123");
}

void test_word_escape(void) {
    ASSERT_MATCH("\\w", "hello");
    ASSERT_MATCH("\\w", "123");
    ASSERT_MATCH("\\w", "_test");
    ASSERT_NO_MATCH("\\w", "@#$");

    ASSERT_MATCH("\\W", "@#$");
    ASSERT_NO_MATCH("\\W", "hello");
}

void test_space_escape(void) {
    ASSERT_MATCH("\\s", " ");
    ASSERT_MATCH("\\s", "\t");
    ASSERT_MATCH("\\s", "\n");
    ASSERT_NO_MATCH("\\s", "a");

    ASSERT_MATCH("\\S", "a");
    ASSERT_NO_MATCH("\\S", " ");
}

void test_literal_escapes(void) {
    ASSERT_MATCH("\\n", "hello\nworld");
    ASSERT_MATCH("\\t", "hello\tworld");
    ASSERT_MATCH("\\r", "hello\rworld");
}

// Quantifier tests
void test_star_quantifier(void) {
    ASSERT_MATCH("a*", "");
    ASSERT_MATCH("a*", "a");
    ASSERT_MATCH("a*", "aaa");
    ASSERT_MATCH("ba*", "b");
    ASSERT_MATCH("ba*", "baaa");
    ASSERT_NO_MATCH("ba*", "ca");
}

void test_plus_quantifier(void) {
    ASSERT_NO_MATCH("a+", "");
    ASSERT_MATCH("a+", "a");
    ASSERT_MATCH("a+", "aaa");
    ASSERT_MATCH("ba+", "ba");
    ASSERT_MATCH("ba+", "baaa");
    ASSERT_NO_MATCH("ba+", "b");
}

void test_question_quantifier(void) {
    ASSERT_MATCH("a?", "");
    ASSERT_MATCH("a?", "a");
    ASSERT_MATCH("ba?", "b");
    ASSERT_MATCH("ba?", "ba");
    ASSERT_MATCH("colou?r", "color");
    ASSERT_MATCH("colou?r", "colour");
}

void test_exact_quantifiers(void) {
    ASSERT_MATCH("a{3}", "aaa");
    ASSERT_NO_MATCH("a{3}", "aa");
    ASSERT_MATCH("a{3}", "aaaa");  // Should match "aaa" within "aaaa"

    ASSERT_MATCH("a{2,4}", "aa");
    ASSERT_MATCH("a{2,4}", "aaa");
    ASSERT_MATCH("a{2,4}", "aaaa");
    ASSERT_NO_MATCH("a{2,4}", "a");
    ASSERT_MATCH("a{2,4}", "aaaaa");  // Should match "aaaa" within "aaaaa"

    ASSERT_MATCH("a{2,}", "aa");
    ASSERT_MATCH("a{2,}", "aaaaaa");
    ASSERT_NO_MATCH("a{2,}", "a");
    
    // Test exact full-string matches with anchors
    ASSERT_MATCH("a{3}", "aaa");
    ASSERT_NO_MATCH("^a{3}$", "aaaa");  // Anchored pattern should NOT match
    ASSERT_NO_MATCH("^a{2,4}$", "aaaaa");  // Anchored pattern should NOT match
}

// Group and capturing tests
void test_basic_groups(void) {
    ASSERT_GROUP_MATCH("(hello)", "hello world", 0, "hello");
    ASSERT_GROUP_MATCH("(hello)", "hello world", 1, "hello");
}

void test_multiple_groups(void) {
    ASSERT_GROUP_MATCH("(\\w+)\\s+(\\w+)", "hello world", 0, "hello world");
    ASSERT_GROUP_MATCH("(\\w+)\\s+(\\w+)", "hello world", 1, "hello");
    ASSERT_GROUP_MATCH("(\\w+)\\s+(\\w+)", "hello world", 2, "world");
}

void test_nested_groups(void) {
    ASSERT_GROUP_MATCH("((\\w+)\\s+(\\w+))", "hello world", 0, "hello world");
    ASSERT_GROUP_MATCH("((\\w+)\\s+(\\w+))", "hello world", 1, "hello world");
    ASSERT_GROUP_MATCH("((\\w+)\\s+(\\w+))", "hello world", 2, "hello");
    ASSERT_GROUP_MATCH("((\\w+)\\s+(\\w+))", "hello world", 3, "world");
}

// Alternation tests
void test_alternation(void) {
    ASSERT_MATCH("cat|dog", "I have a cat");
    ASSERT_MATCH("cat|dog", "I have a dog");
    ASSERT_NO_MATCH("cat|dog", "I have a bird");

    ASSERT_MATCH("red|green|blue", "red apple");
    ASSERT_MATCH("red|green|blue", "green leaf");
    ASSERT_MATCH("red|green|blue", "blue sky");
    ASSERT_NO_MATCH("red|green|blue", "yellow sun");
}

void test_alternation_with_groups(void) {
    ASSERT_GROUP_MATCH("(cat|dog)", "I have a cat", 1, "cat");
    ASSERT_GROUP_MATCH("(cat|dog)", "I have a dog", 1, "dog");
}

// Case insensitive flag tests
void test_case_insensitive_flag(void) {
    ASSERT_MATCH_WITH_FLAGS("hello", "i", "HELLO");
    ASSERT_MATCH_WITH_FLAGS("hello", "i", "Hello");
    ASSERT_MATCH_WITH_FLAGS("hello", "i", "HeLLo");
    ASSERT_MATCH_WITH_FLAGS("[a-z]+", "i", "HELLO");
}

// Global flag tests
void test_global_flag_with_exec(void) {
    RegExp *re = regex_new("\\w+", "g");
    MatchResult *result;

    // First match
    result = regex_exec(re, "hello world test");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("hello", result->groups[0]);
    TEST_ASSERT_EQUAL_INT(0, result->index);
    match_result_free(result);

    // Second match
    result = regex_exec(re, "hello world test");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("world", result->groups[0]);
    TEST_ASSERT_EQUAL_INT(6, result->index);
    match_result_free(result);

    // Third match
    result = regex_exec(re, "hello world test");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("test", result->groups[0]);
    TEST_ASSERT_EQUAL_INT(12, result->index);
    match_result_free(result);

    // No more matches
    result = regex_exec(re, "hello world test");
    TEST_ASSERT_NULL(result);

    regex_free(re);
}

// Match iterator tests (matchAll functionality)
void test_match_iterator(void) {
    RegExp *re = regex_new("\\w+", "g");
    MatchIterator *iter = string_match_all("hello world test", re);
    TEST_ASSERT_NOT_NULL(iter);

    MatchResult *result;
    int count = 0;
    const char *expected[] = {"hello", "world", "test"};

    while ((result = match_iterator_next(iter)) != NULL) {
        TEST_ASSERT_LESS_THAN(3, count);
        TEST_ASSERT_EQUAL_STRING(expected[count], result->groups[0]);
        match_result_free(result);
        count++;
    }

    TEST_ASSERT_EQUAL_INT(3, count);
    match_iterator_free(iter);
    regex_free(re);
}

void test_match_iterator_requires_global(void) {
    RegExp *re = regex_new("\\w+", ""); // No global flag
    MatchIterator *iter = string_match_all("hello world", re);
    TEST_ASSERT_NULL(iter); // Should fail without global flag
    regex_free(re);
}

// String methods tests
void test_string_match_method(void) {
    RegExp *re = regex_new("(\\w+)\\s+(\\w+)", "");
    MatchResult *result = string_match("hello world", re);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("hello world", result->groups[0]);
    TEST_ASSERT_EQUAL_STRING("hello", result->groups[1]);
    TEST_ASSERT_EQUAL_STRING("world", result->groups[2]);
    TEST_ASSERT_EQUAL_INT(0, result->index);

    match_result_free(result);
    regex_free(re);
}

// Complex pattern tests
void test_email_pattern(void) {
    const char *email_pattern = "[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}";

    ASSERT_MATCH(email_pattern, "test@example.com");
    ASSERT_MATCH(email_pattern, "user.name+tag@domain.co.uk");
    ASSERT_MATCH(email_pattern, "123@test-domain.org");
    ASSERT_NO_MATCH(email_pattern, "invalid.email");
    ASSERT_NO_MATCH(email_pattern, "@domain.com");
    ASSERT_NO_MATCH(email_pattern, "user@");
}

void test_phone_pattern(void) {
    const char *phone_pattern = "\\(?\\d{3}\\)?[- ]?\\d{3}[- ]?\\d{4}";

    ASSERT_MATCH(phone_pattern, "(555) 123-4567");
    ASSERT_MATCH(phone_pattern, "555-123-4567");
    ASSERT_MATCH(phone_pattern, "555 123 4567");
    ASSERT_MATCH(phone_pattern, "5551234567");
    ASSERT_NO_MATCH(phone_pattern, "123-4567");
    ASSERT_NO_MATCH(phone_pattern, "555-123-456");
}

void test_url_pattern(void) {
    const char *url_pattern = "https?://[\\w.-]+\\.[a-zA-Z]{2,}(/[\\w./?#&=]*)?";

    ASSERT_MATCH(url_pattern, "http://example.com");
    ASSERT_MATCH(url_pattern, "https://www.example.com/path");
    // ASSERT_MATCH(url_pattern, "https://sub.domain.com/path?query=1");
    // ASSERT_NO_MATCH(url_pattern, "ftp://example.com");
    // ASSERT_NO_MATCH(url_pattern, "http://");
}

// Error handling tests
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

// Performance stress tests
void test_pathological_patterns(void) {
    // These patterns can cause exponential backtracking in naive implementations
    // Our implementation should handle them reasonably

    RegExp *re1 = regex_new("a*a*a*a*a*a*a*b", "");
    TEST_ASSERT_FALSE(regex_test(re1, "aaaaaaaaaaaaaaaac")); // Should not hang
    regex_free(re1);

    RegExp *re2 = regex_new("(a+)+b", "");
    TEST_ASSERT_FALSE(regex_test(re2, "aaaaaaaaaaaac")); // Should not hang
    regex_free(re2);
}

void test_large_input(void) {
    // Test with large input string
    char *large_text = malloc(10000);
    memset(large_text, 'a', 9999);
    large_text[9999] = '\0';

    RegExp *re = regex_new("a+", "");
    TEST_ASSERT_TRUE(regex_test(re, large_text));
    regex_free(re);

    free(large_text);
}

// Memory management tests
void test_memory_cleanup(void) {
    // Test that multiple create/destroy cycles don't leak memory
    for (int i = 0; i < 100; i++) {
        RegExp *re = regex_new("test\\d+", "gi");
        MatchResult *result = regex_exec(re, "test123 test456");

        if (result) {
            match_result_free(result);
        }

        regex_free(re);
    }

    // Test match iterator cleanup
    RegExp *re = regex_new("\\d+", "g");
    MatchIterator *iter = string_match_all("123 456 789", re);

    MatchResult *result;
    while ((result = match_iterator_next(iter)) != NULL) {
        match_result_free(result);
    }

    match_iterator_free(iter);
    regex_free(re);
}

// Integration tests combining multiple features
void test_complex_integration(void) {
    // Test pattern with groups, alternation, quantifiers, and character classes
    const char *pattern = "([a-zA-Z]+)\\s+(\\d{1,3})\\s+(cat|dog|bird)s?";

    ASSERT_GROUP_MATCH(pattern, "John 25 cats", 0, "John 25 cats");
    ASSERT_GROUP_MATCH(pattern, "John 25 cats", 1, "John");
    ASSERT_GROUP_MATCH(pattern, "John 25 cats", 2, "25");
    ASSERT_GROUP_MATCH(pattern, "John 25 cats", 3, "cat");

    ASSERT_GROUP_MATCH(pattern, "Alice 99 dog", 1, "Alice");
    ASSERT_GROUP_MATCH(pattern, "Alice 99 dog", 2, "99");
    ASSERT_GROUP_MATCH(pattern, "Alice 99 dog", 3, "dog");
}

// Main test runner
int main(void) {
    UNITY_BEGIN();

    // Basic functionality
    RUN_TEST(test_literal_characters);
    RUN_TEST(test_empty_pattern);
    RUN_TEST(test_dot_wildcard);
    RUN_TEST(test_dot_with_dotall_flag);

    // Anchors
    RUN_TEST(test_start_anchor);
    RUN_TEST(test_end_anchor);
    RUN_TEST(test_multiline_anchors);

    // Character classes
    RUN_TEST(test_character_classes);
    RUN_TEST(test_negated_character_classes);

    // Escape sequences
    RUN_TEST(test_digit_escape);
    RUN_TEST(test_word_escape);
    RUN_TEST(test_space_escape);
    RUN_TEST(test_literal_escapes);

    // Quantifiers
    RUN_TEST(test_star_quantifier);
    RUN_TEST(test_plus_quantifier);
    RUN_TEST(test_question_quantifier);
    RUN_TEST(test_exact_quantifiers);

    // Groups and capturing
    RUN_TEST(test_basic_groups);
    RUN_TEST(test_multiple_groups);
    RUN_TEST(test_nested_groups);

    // // Alternation
    // RUN_TEST(test_alternation);
    // RUN_TEST(test_alternation_with_groups);
    //
    // // Flags
    // RUN_TEST(test_case_insensitive_flag);
    // RUN_TEST(test_global_flag_with_exec);
    //
    // // JavaScript API compatibility
    // RUN_TEST(test_match_iterator);
    // RUN_TEST(test_match_iterator_requires_global);
    // RUN_TEST(test_string_match_method);
    //
    // // Complex patterns
    // RUN_TEST(test_email_pattern);
    // RUN_TEST(test_phone_pattern);
    // RUN_TEST(test_url_pattern);
    //
    // // Error handling
    // RUN_TEST(test_null_inputs);
    // RUN_TEST(test_invalid_quantifiers);
    //
    // // Performance
    // RUN_TEST(test_pathological_patterns);
    // RUN_TEST(test_large_input);
    //
    // // Memory management
    // RUN_TEST(test_memory_cleanup);
    //
    // // Integration
    // RUN_TEST(test_complex_integration);

    return UNITY_END();
}

// Additional helper functions for extended testing
void run_benchmark_tests(void) {
    printf("\n=== Benchmark Tests ===\n");

    // Test compilation time
    clock_t start = clock();
    for (int i = 0; i < 1000; i++) {
        RegExp *re = regex_new("([a-zA-Z0-9._%+-]+)@([a-zA-Z0-9.-]+\\.[a-zA-Z]{2,})", "i");
        regex_free(re);
    }
    clock_t end = clock();
    double compile_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Compilation: 1000 patterns in %.3f seconds\n", compile_time);

    // Test execution time
    RegExp *re = regex_new("\\b\\w+@\\w+\\.\\w+\\b", "g");
    const char *text = "Contact us at support@example.com or sales@company.org for help";

    start = clock();
    for (int i = 0; i < 10000; i++) {
        re->last_index = 0; // Reset for each iteration
        MatchResult *result;
        while ((result = regex_exec(re, text)) != NULL) {
            match_result_free(result);
        }
    }
    end = clock();
    double exec_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Execution: 10000 runs in %.3f seconds\n", exec_time);

    regex_free(re);
}

// Test data generator for fuzzing
void generate_test_cases(FILE *output) {
    const char *patterns[] = {
        ".*",
        "\\d+",
        "[a-zA-Z]+",
        "(\\w+)@(\\w+)",
        "a{2,5}",
        "colou?r",
        "cat|dog|bird",
        "^start.*end$",
        "\\s*\\w+\\s*",
        "[^aeiou]+",
        NULL
    };

    const char *texts[] = {
        "hello world",
        "test@example.com",
        "123-456-7890",
        "The quick brown fox",
        "aaaaaa",
        "color colour",
        "I have a cat and a dog",
        "start something end",
        "   word   ",
        "bcdfg",
        "",
        "!@#$%^&*()",
        NULL
    };

    fprintf(output, "// Generated test cases\n");
    for (int i = 0; patterns[i]; i++) {
        for (int j = 0; texts[j]; j++) {
            fprintf(output, "{\"%s\", \"%s\"},\n", patterns[i], texts[j]);
        }
    }
}