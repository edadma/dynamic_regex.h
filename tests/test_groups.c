#include "test_shared.h"

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