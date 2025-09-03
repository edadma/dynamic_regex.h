#include "test_shared.h"

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

// Case insensitive character classes
void test_case_insensitive_character_classes(void) {
    // Test basic ranges with case insensitive flag
    ASSERT_MATCH_WITH_FLAGS("[a-z]", "i", "Hello");
    ASSERT_MATCH_WITH_FLAGS("[a-z]", "i", "WORLD");
    ASSERT_MATCH_WITH_FLAGS("[A-Z]", "i", "hello");
    ASSERT_MATCH_WITH_FLAGS("[A-Z]", "i", "world");
    
    // Test mixed case ranges
    ASSERT_MATCH_WITH_FLAGS("[a-zA-Z]", "i", "Hello");
    ASSERT_MATCH_WITH_FLAGS("[a-zA-Z]", "i", "WORLD");
    ASSERT_NO_MATCH_WITH_FLAGS("[a-zA-Z]", "i", "123");
    
    // Test specific character lists
    ASSERT_MATCH_WITH_FLAGS("[abc]", "i", "ABC");
    ASSERT_MATCH_WITH_FLAGS("[ABC]", "i", "abc");
    ASSERT_NO_MATCH_WITH_FLAGS("[abc]", "i", "def");
    ASSERT_NO_MATCH_WITH_FLAGS("[ABC]", "i", "DEF");
    
    // Test negated classes with case insensitive
    ASSERT_NO_MATCH_WITH_FLAGS("[^a-z]", "i", "Hello");
    ASSERT_NO_MATCH_WITH_FLAGS("[^A-Z]", "i", "world");
    ASSERT_MATCH_WITH_FLAGS("[^a-z]", "i", "123");
    
    // Test edge cases
    ASSERT_MATCH_WITH_FLAGS("[z]", "i", "Z");
    ASSERT_MATCH_WITH_FLAGS("[Z]", "i", "z");
    
    // Test complex patterns
    ASSERT_MATCH_WITH_FLAGS("[a-z0-9]", "i", "Hello123");
    ASSERT_MATCH_WITH_FLAGS("[A-Z0-9]", "i", "world456");
    ASSERT_NO_MATCH_WITH_FLAGS("[a-z0-9]", "i", "@#$");
    
    // Test with word boundaries and case insensitive classes
    ASSERT_MATCH_WITH_FLAGS("\\b[A-Z][a-z]+\\b", "i", "hello");
    ASSERT_MATCH_WITH_FLAGS("\\b[a-z][A-Z]+\\b", "i", "WORLD");
    
    // Test multiple character class matches in one pattern
    ASSERT_MATCH_WITH_FLAGS("[a-z]+[0-9]+", "i", "ABC123");
    ASSERT_MATCH_WITH_FLAGS("[A-Z]+[a-z]+", "i", "hello WORLD");
    
    // Test with quantifiers
    ASSERT_MATCH_WITH_FLAGS("[a-z]+", "i", "HELLO");
    ASSERT_MATCH_WITH_FLAGS("[A-Z]*", "i", "hello");
    ASSERT_MATCH_WITH_FLAGS("[a-z]?", "i", "X");
}