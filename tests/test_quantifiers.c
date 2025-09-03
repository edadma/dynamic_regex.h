#include "test_shared.h"

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