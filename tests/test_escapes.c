#include "test_shared.h"

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

void test_additional_escape_sequences(void) {
    // Test \f (form feed)
    ASSERT_MATCH("\\f", "test\fvalue");
    ASSERT_NO_MATCH("\\f", "test value");
    
    // Test \v (vertical tab)  
    ASSERT_MATCH("\\v", "test\vvalue");
    ASSERT_NO_MATCH("\\v", "test value");
    
    // Test \0 (null character) - Skip due to C string termination issues
    // ASSERT_MATCH("test\\0", "test\0");  // Problematic with C strings
    
    // Test \x (hexadecimal characters)
    ASSERT_MATCH("\\x41", "testAvalue");  // \x41 = 'A'
    ASSERT_MATCH("\\x61", "testavalue");  // \x61 = 'a'
    ASSERT_MATCH("\\x30", "test0value");  // \x30 = '0'
    ASSERT_MATCH("\\x20", "test value"); // \x20 = ' '
    ASSERT_NO_MATCH("\\x41", "testavalue"); // \x41 is 'A' not 'a'
    
    // Test hex with both upper and lowercase hex digits
    ASSERT_MATCH("\\xFF", "test\xFFvalue");  // \xFF = 255
    ASSERT_MATCH("\\xab", "test\xabvalue");  // lowercase hex
    ASSERT_MATCH("\\xAB", "test\xABvalue");  // uppercase hex
    
    // Test invalid hex sequences (should treat as literal 'x')
    ASSERT_MATCH("\\xGG", "testxGGvalue");  // Invalid hex, becomes literal x
    ASSERT_MATCH("\\x1", "testx1value");    // Incomplete hex, becomes literal x
    
    // Test in character classes
    ASSERT_MATCH("[\\f\\v]", "test\fvalue");
    ASSERT_MATCH("[\\f\\v]", "test\vvalue");
    ASSERT_NO_MATCH("[\\f\\v]", "test\tvalue");
    
    ASSERT_MATCH("[\\x41-\\x5A]", "testAvalue"); // [A-Z] in hex
    ASSERT_MATCH("[\\x41-\\x5A]", "testZvalue"); // [A-Z] in hex
    ASSERT_NO_MATCH("[\\x41-\\x5A]", "testavalue"); // lowercase not in [A-Z]
    
    // Test combinations with other patterns
    ASSERT_MATCH("\\x48\\x65\\x6c\\x6c\\x6f", "Hello");  // "Hello" in hex
    ASSERT_MATCH("test\\f+end", "test\f\f\fend");        // \f with quantifier
    ASSERT_MATCH("begin\\v*middle", "beginmiddle");       // \v with zero matches
    ASSERT_MATCH("begin\\v*middle", "begin\v\vmiddle");   // \v with multiple matches
}