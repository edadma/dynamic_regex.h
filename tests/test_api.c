#include "test_shared.h"

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

// Comprehensive match iterator test with non-trivial patterns
void test_comprehensive_match_iterator(void) {
    // Test 1: Email addresses in a sentence
    {
        RegExp *re = regex_new("[a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,}", "g");
        const char *text = "Contact us at support@example.com, sales@company.org, or admin@test.co.uk for assistance.";
        const char *expected_emails[] = {"support@example.com", "sales@company.org", "admin@test.co.uk"};
        int expected_positions[] = {14, 35, 57}; // Start positions in the text
        int expected_count = 3;
        
        MatchIterator *iter = string_match_all(text, re);
        TEST_ASSERT_NOT_NULL(iter);
        
        MatchResult *result;
        int count = 0;
        while ((result = match_iterator_next(iter)) != NULL) {
            TEST_ASSERT_LESS_THAN(expected_count, count);
            TEST_ASSERT_EQUAL_STRING(expected_emails[count], result->groups[0]);
            TEST_ASSERT_EQUAL_INT(expected_positions[count], result->index);
            match_result_free(result);
            count++;
        }
        
        TEST_ASSERT_EQUAL_INT(expected_count, count);
        match_iterator_free(iter);
        regex_free(re);
    }
    
    // Test 2: Phone numbers in various formats
    {
        RegExp *re = regex_new("\\d{3}[- ]?\\d{3}[- ]?\\d{4}", "g");
        const char *text = "Call 555-123-4567 or 800 555 1234, alternatively try 9876543210.";
        const char *expected_phones[] = {"555-123-4567", "800 555 1234", "9876543210"};
        int expected_positions[] = {5, 21, 53}; // Start positions in the text
        int expected_count = 3;
        
        MatchIterator *iter = string_match_all(text, re);
        TEST_ASSERT_NOT_NULL(iter);
        
        MatchResult *result;
        int count = 0;
        while ((result = match_iterator_next(iter)) != NULL) {
            TEST_ASSERT_LESS_THAN(expected_count, count);
            TEST_ASSERT_EQUAL_STRING(expected_phones[count], result->groups[0]);
            TEST_ASSERT_EQUAL_INT(expected_positions[count], result->index);
            match_result_free(result);
            count++;
        }
        
        TEST_ASSERT_EQUAL_INT(expected_count, count);
        match_iterator_free(iter);
        regex_free(re);
    }
    
    // Test 3: URLs with different protocols
    {
        RegExp *re = regex_new("https?://[\\w.\\-]+\\.[a-zA-Z]{2,}(/[\\w./?#&=\\-]*)?", "g");
        const char *text = "Visit https://www.example.com or http://test-site.org/page?id=123 for more info.";
        const char *expected_urls[] = {"https://www.example.com", "http://test-site.org/page?id=123"};
        int expected_positions[] = {6, 33}; // Start positions in the text
        int expected_count = 2;
        
        MatchIterator *iter = string_match_all(text, re);
        TEST_ASSERT_NOT_NULL(iter);
        
        MatchResult *result;
        int count = 0;
        while ((result = match_iterator_next(iter)) != NULL) {
            TEST_ASSERT_LESS_THAN(expected_count, count);
            TEST_ASSERT_EQUAL_STRING(expected_urls[count], result->groups[0]);
            TEST_ASSERT_EQUAL_INT(expected_positions[count], result->index);
            match_result_free(result);
            count++;
        }
        
        TEST_ASSERT_EQUAL_INT(expected_count, count);
        match_iterator_free(iter);
        regex_free(re);
    }
    
    // Test 4: IP addresses
    {
        RegExp *re = regex_new("\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}", "g");
        const char *text = "Server IPs: 192.168.1.1, 10.0.0.1, and 255.255.255.0 are configured.";
        const char *expected_ips[] = {"192.168.1.1", "10.0.0.1", "255.255.255.0"};
        int expected_positions[] = {12, 25, 39}; // Start positions in the text
        int expected_count = 3;
        
        MatchIterator *iter = string_match_all(text, re);
        TEST_ASSERT_NOT_NULL(iter);
        
        MatchResult *result;
        int count = 0;
        while ((result = match_iterator_next(iter)) != NULL) {
            TEST_ASSERT_LESS_THAN(expected_count, count);
            TEST_ASSERT_EQUAL_STRING(expected_ips[count], result->groups[0]);
            TEST_ASSERT_EQUAL_INT(expected_positions[count], result->index);
            match_result_free(result);
            count++;
        }
        
        TEST_ASSERT_EQUAL_INT(expected_count, count);
        match_iterator_free(iter);
        regex_free(re);
    }
    
    // Test 5: Hexadecimal color codes
    {
        RegExp *re = regex_new("#[0-9a-fA-F]{6}", "g");
        const char *text = "Colors: #FF0000 (red), #00FF00 (green), #0000FF (blue), #FFFFFF (white).";
        const char *expected_colors[] = {"#FF0000", "#00FF00", "#0000FF", "#FFFFFF"};
        int expected_positions[] = {8, 23, 40, 56}; // Start positions in the text
        int expected_count = 4;
        
        MatchIterator *iter = string_match_all(text, re);
        TEST_ASSERT_NOT_NULL(iter);
        
        MatchResult *result;
        int count = 0;
        while ((result = match_iterator_next(iter)) != NULL) {
            TEST_ASSERT_LESS_THAN(expected_count, count);
            TEST_ASSERT_EQUAL_STRING(expected_colors[count], result->groups[0]);
            TEST_ASSERT_EQUAL_INT(expected_positions[count], result->index);
            match_result_free(result);
            count++;
        }
        
        TEST_ASSERT_EQUAL_INT(expected_count, count);
        match_iterator_free(iter);
        regex_free(re);
    }
}