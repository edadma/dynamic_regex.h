#include "test_shared.h"

void test_email_pattern(void) {
    RegExp *re = regex_new("[a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,}", "");
    ASSERT_MATCH("[a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,}", "user@example.com");
    ASSERT_MATCH("[a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,}", "test.email+tag@sub.domain.org");
    ASSERT_NO_MATCH("[a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,}", "invalid.email");
    ASSERT_NO_MATCH("[a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,}", "@domain.com");
    regex_free(re);
}

void test_phone_pattern(void) {
    RegExp *re = regex_new("\\d{3}[- ]?\\d{3}[- ]?\\d{4}", "");
    ASSERT_MATCH("\\d{3}[- ]?\\d{3}[- ]?\\d{4}", "555-123-4567");
    ASSERT_MATCH("\\d{3}[- ]?\\d{3}[- ]?\\d{4}", "555 123 4567");
    ASSERT_MATCH("\\d{3}[- ]?\\d{3}[- ]?\\d{4}", "5551234567");
    ASSERT_NO_MATCH("\\d{3}[- ]?\\d{3}[- ]?\\d{4}", "555-12-4567");
    regex_free(re);
}

void test_url_pattern(void) {
    RegExp *re = regex_new("https?://[\\w.\\-]+\\.[a-zA-Z]{2,}(/[\\w./?#&=\\-]*)?", "");
    ASSERT_MATCH("https?://[\\w.\\-]+\\.[a-zA-Z]{2,}(/[\\w./?#&=\\-]*)?", "https://www.example.com");
    ASSERT_MATCH("https?://[\\w.\\-]+\\.[a-zA-Z]{2,}(/[\\w./?#&=\\-]*)?", "http://test-site.org/page?id=123");
    ASSERT_NO_MATCH("https?://[\\w.\\-]+\\.[a-zA-Z]{2,}(/[\\w./?#&=\\-]*)?", "ftp://example.com");
    regex_free(re);
}

void test_complex_integration(void) {
    // Test complex pattern with multiple features
    RegExp *re = regex_new("^(\\d{4})-(\\d{2})-(\\d{2})\\s+(\\d{2}):(\\d{2}):(\\d{2})$", "");
    
    MatchResult *result = regex_exec(re, "2023-12-25 14:30:45");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("2023-12-25 14:30:45", result->groups[0]);
    TEST_ASSERT_EQUAL_STRING("2023", result->groups[1]);
    TEST_ASSERT_EQUAL_STRING("12", result->groups[2]);
    TEST_ASSERT_EQUAL_STRING("25", result->groups[3]);
    TEST_ASSERT_EQUAL_STRING("14", result->groups[4]);
    TEST_ASSERT_EQUAL_STRING("30", result->groups[5]);
    TEST_ASSERT_EQUAL_STRING("45", result->groups[6]);
    
    match_result_free(result);
    regex_free(re);
}