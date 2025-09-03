#include "test_shared.h"

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