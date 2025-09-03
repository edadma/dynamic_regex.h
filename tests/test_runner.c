#include "test_shared.h"

// Forward declarations for all test functions

// Basic functionality tests
void test_literal_characters(void);
void test_empty_pattern(void);
void test_dot_wildcard(void);
void test_dot_with_dotall_flag(void);

// Anchor tests
void test_start_anchor(void);
void test_end_anchor(void);
void test_multiline_anchors(void);

// Character class tests
void test_character_classes(void);
void test_negated_character_classes(void);

// Escape sequence tests
void test_digit_escape(void);
void test_word_escape(void);
void test_space_escape(void);
void test_literal_escapes(void);
void test_additional_escape_sequences(void);

// Quantifier tests
void test_star_quantifier(void);
void test_plus_quantifier(void);
void test_question_quantifier(void);
void test_exact_quantifiers(void);

// Group tests
void test_basic_groups(void);
void test_multiple_groups(void);
void test_nested_groups(void);

// Alternation tests
void test_alternation(void);
void test_alternation_with_groups(void);
void test_alternation_with_quantifiers(void);
void test_alternation_with_character_classes(void);
void test_alternation_with_anchors(void);
void test_nested_alternation(void);
void test_alternation_with_escapes(void);
void test_complex_alternation_patterns(void);

// Flag tests
void test_case_insensitive_flag(void);
void test_global_flag_with_exec(void);

// API compatibility tests
void test_match_iterator(void);
void test_match_iterator_requires_global(void);
void test_comprehensive_match_iterator(void);
void test_string_match_method(void);

// Word boundary tests
void test_word_boundary_patterns(void);
void test_negative_word_boundary_patterns(void);

// Complex pattern tests
void test_email_pattern(void);
void test_phone_pattern(void);
void test_url_pattern(void);

// Edge case tests
void test_null_inputs(void);
void test_invalid_quantifiers(void);
void test_memory_cleanup(void);

// Performance tests
void test_pathological_patterns(void);
void test_large_input(void);

// Integration tests
void test_complex_integration(void);

// Unity setup/teardown
void setUp(void) {
    // Called before each test
}

void tearDown(void) {
    // Called after each test
}

// Benchmark function
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
    RUN_TEST(test_additional_escape_sequences);

    // Quantifiers
    RUN_TEST(test_star_quantifier);
    RUN_TEST(test_plus_quantifier);
    RUN_TEST(test_question_quantifier);
    RUN_TEST(test_exact_quantifiers);

    // Groups and capturing
    RUN_TEST(test_basic_groups);
    RUN_TEST(test_multiple_groups);
    RUN_TEST(test_nested_groups);

    // Alternation
    RUN_TEST(test_alternation);
    RUN_TEST(test_alternation_with_groups);
    RUN_TEST(test_alternation_with_quantifiers);
    RUN_TEST(test_alternation_with_character_classes);
    RUN_TEST(test_alternation_with_anchors);
    RUN_TEST(test_nested_alternation);
    RUN_TEST(test_alternation_with_escapes);
    RUN_TEST(test_complex_alternation_patterns);

    // Flags
    RUN_TEST(test_case_insensitive_flag);
    RUN_TEST(test_global_flag_with_exec);

    // Language API compatibility
    RUN_TEST(test_match_iterator);
    RUN_TEST(test_match_iterator_requires_global);
    RUN_TEST(test_comprehensive_match_iterator);
    RUN_TEST(test_string_match_method);

    // Word boundary tests
    RUN_TEST(test_word_boundary_patterns);
    RUN_TEST(test_negative_word_boundary_patterns);

    // Complex patterns
    RUN_TEST(test_email_pattern);
    RUN_TEST(test_phone_pattern);
    RUN_TEST(test_url_pattern);

    // Error handling
    RUN_TEST(test_null_inputs);
    RUN_TEST(test_invalid_quantifiers);

    // Performance
    RUN_TEST(test_pathological_patterns);
    RUN_TEST(test_large_input);

    // Memory management
    RUN_TEST(test_memory_cleanup);

    // Integration
    RUN_TEST(test_complex_integration);

    int result = UNITY_END();
    
    // Run benchmark tests after main tests
    run_benchmark_tests();
    
    return result;
}