#include "test_shared.h"

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

// Comprehensive alternation tests
void test_alternation_with_quantifiers(void) {
    // Alternation with quantifiers
    ASSERT_MATCH("a+|b*", "aaa");
    ASSERT_MATCH("a+|b*", "bbb");
    ASSERT_MATCH("a+|b*", "");  // b* matches empty
    ASSERT_MATCH("a+|b*", "ccc");  // b* matches empty at start
    
    // Alternation with optional quantifiers
    ASSERT_MATCH("colou?r|gray", "color");
    ASSERT_MATCH("colou?r|gray", "colour");
    ASSERT_MATCH("colou?r|gray", "gray");
    ASSERT_NO_MATCH("colou?r|gray", "blue");
}

void test_alternation_with_character_classes(void) {
    // Alternation with character classes
    ASSERT_MATCH("[0-9]+|[a-z]+", "123");
    ASSERT_MATCH("[0-9]+|[a-z]+", "abc");
    ASSERT_NO_MATCH("[0-9]+|[a-z]+", "ABC");
    
    // Mixed character classes and literals
    ASSERT_MATCH("[aeiou]|xyz", "a");        // Matches [aeiou]
    ASSERT_MATCH("[aeiou]|xyz", "xyz");      // Matches xyz
    ASSERT_MATCH("[aeiou]|xyz", "contains xyz here");  // Contains xyz
    ASSERT_NO_MATCH("[aeiou]|xyz", "x");     // 'x' alone doesn't match either alternative
    ASSERT_NO_MATCH("[aeiou]|xyz", "b");     // 'b' alone doesn't match either alternative
}

void test_alternation_with_anchors(void) {
    // Alternation with anchors
    ASSERT_MATCH("^start|end$", "start something");
    ASSERT_MATCH("^start|end$", "something end");
    // Note: ^start|end$ means (^start)|(end$), not ^(start|end)$
    ASSERT_MATCH("^start|end$", "middle end");  // This should match because of end$
}

void test_nested_alternation(void) {
    // Nested alternation in groups
    ASSERT_MATCH("(cat|dog)|(bird|fish)", "cat");
    ASSERT_MATCH("(cat|dog)|(bird|fish)", "dog");
    ASSERT_MATCH("(cat|dog)|(bird|fish)", "bird");
    ASSERT_MATCH("(cat|dog)|(bird|fish)", "fish");
    ASSERT_NO_MATCH("(cat|dog)|(bird|fish)", "mouse");
}

void test_alternation_with_escapes(void) {
    // Alternation with escape sequences
    ASSERT_MATCH("\\d+|\\w+|\\s+", "123");
    ASSERT_MATCH("\\d+|\\w+|\\s+", "abc");  
    ASSERT_MATCH("\\d+|\\w+|\\s+", "   ");
    
    // Alternation with literal escapes
    ASSERT_MATCH("\\n|\\t|\\r", "\n");
    ASSERT_MATCH("\\n|\\t|\\r", "\t");
    ASSERT_MATCH("\\n|\\t|\\r", "\r");
}

void test_complex_alternation_patterns(void) {
    // Complex real-world alternation patterns
    ASSERT_MATCH("(hello|hi) (world|earth)", "hello world");
    ASSERT_MATCH("(hello|hi) (world|earth)", "hi earth");
    ASSERT_NO_MATCH("(hello|hi) (world|earth)", "hey world");
    
    // Three or more alternatives  
    ASSERT_MATCH("red|green|blue|yellow", "red");
    ASSERT_MATCH("red|green|blue|yellow", "yellow");
    ASSERT_NO_MATCH("red|green|blue|yellow", "purple");
}