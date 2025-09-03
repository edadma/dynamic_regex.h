#include "test_shared.h"

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