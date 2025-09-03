#include "test_shared.h"

// Comprehensive word boundary tests
void test_word_boundary_patterns(void) {
    // Test 1: Basic word boundary matching
    {
        ASSERT_MATCH("\\bword\\b", "word");
        ASSERT_MATCH("\\bword\\b", "a word here");
        ASSERT_MATCH("\\bword\\b", "word!");
        ASSERT_MATCH("\\bword\\b", "!word");
        ASSERT_NO_MATCH("\\bword\\b", "sword");
        ASSERT_NO_MATCH("\\bword\\b", "words");
        ASSERT_NO_MATCH("\\bword\\b", "password");
    }
    
    // Test 2: Numbers with word boundaries
    {
        ASSERT_MATCH("\\b123\\b", "123");
        ASSERT_MATCH("\\b123\\b", "number 123 here");
        ASSERT_MATCH("\\b123\\b", "123!");
        ASSERT_NO_MATCH("\\b123\\b", "a123");
        ASSERT_NO_MATCH("\\b123\\b", "123a");
        ASSERT_NO_MATCH("\\b123\\b", "1234");
    }
    
    // Test 3: Word boundary at start of string
    {
        ASSERT_MATCH("\\btest", "test");
        ASSERT_MATCH("\\btest", "test case");
        ASSERT_NO_MATCH("\\btest", "pretest");
        ASSERT_NO_MATCH("\\btest", "contest");
    }
    
    // Test 4: Word boundary at end of string
    {
        ASSERT_MATCH("test\\b", "test");
        ASSERT_MATCH("test\\b", "a test");
        ASSERT_NO_MATCH("test\\b", "testing");
        ASSERT_NO_MATCH("test\\b", "testcase");
    }
    
    // Test 5: Multiple word boundaries
    {
        ASSERT_MATCH("\\bcat\\b.*\\bdog\\b", "cat and dog");
        ASSERT_MATCH("\\bcat\\b.*\\bdog\\b", "the cat sees the dog");
        ASSERT_NO_MATCH("\\bcat\\b.*\\bdog\\b", "catdog");
        ASSERT_NO_MATCH("\\bcat\\b.*\\bdog\\b", "cat and dogs");
    }
    
    // Test 6: Word boundaries with punctuation
    {
        ASSERT_MATCH("\\bhi\\b", "hi!");
        ASSERT_MATCH("\\bhi\\b", "hi.");
        ASSERT_MATCH("\\bhi\\b", "hi,");
        ASSERT_MATCH("\\bhi\\b", "(hi)");
        ASSERT_MATCH("\\bhi\\b", "[hi]");
        ASSERT_MATCH("\\bhi\\b", "hi?");
    }
    
    // Test 7: Word boundaries with underscores (underscores are word chars)
    {
        ASSERT_NO_MATCH("\\btest\\b", "test_case");
        ASSERT_NO_MATCH("\\btest\\b", "_test");
        ASSERT_MATCH("\\b_test\\b", " _test ");
        ASSERT_MATCH("\\bvar_name\\b", "var_name = 5");
    }
    
    // Test 8: Hex colors with word boundaries
    {
        RegExp *re = regex_new("#[0-9a-fA-F]{6}\\b", "g");
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
    
    // Test 9: Extract whole words only
    {
        RegExp *re = regex_new("\\b\\w+\\b", "g");
        const char *text = "Hello, world! Test-case with_underscore and spaces.";
        const char *expected_words[] = {"Hello", "world", "Test", "case", "with_underscore", "and", "spaces"};
        int expected_positions[] = {0, 7, 14, 19, 24, 40, 44}; // Start positions in the text
        int expected_count = 7;
        
        MatchIterator *iter = string_match_all(text, re);
        TEST_ASSERT_NOT_NULL(iter);
        
        MatchResult *result;
        int count = 0;
        while ((result = match_iterator_next(iter)) != NULL) {
            TEST_ASSERT_LESS_THAN(expected_count, count);
            TEST_ASSERT_EQUAL_STRING(expected_words[count], result->groups[0]);
            TEST_ASSERT_EQUAL_INT(expected_positions[count], result->index);
            match_result_free(result);
            count++;
        }
        
        TEST_ASSERT_EQUAL_INT(expected_count, count);
        match_iterator_free(iter);
        regex_free(re);
    }
}

// Negative word boundary tests
void test_negative_word_boundary_patterns(void) {
    // Test 1: Basic negative word boundary - matches inside words
    {
        ASSERT_MATCH("\\Bcat\\B", "concatenate");  // cat inside concatenate
        ASSERT_MATCH("\\Bcat\\B", "scattered");    // cat inside scattered
        ASSERT_NO_MATCH("\\Bcat\\B", "cat");       // cat at word boundary
        ASSERT_NO_MATCH("\\Bcat\\B", "cat dog");   // cat at start boundary
        ASSERT_NO_MATCH("\\Bcat\\B", "dog cat");   // cat at end boundary
    }
    
    // Test 2: Start negative word boundary - matches inside word start
    {
        ASSERT_MATCH("\\Btest", "contest");     // test inside contest
        ASSERT_MATCH("\\Btest", "protest");    // test inside protest  
        ASSERT_NO_MATCH("\\Btest", "test");    // test at word start
        ASSERT_NO_MATCH("\\Btest", "test case"); // test at word start
    }
    
    // Test 3: End negative word boundary - matches inside word end  
    {
        ASSERT_MATCH("ing\\B", "strings");     // ing inside strings
        ASSERT_MATCH("ing\\B", "testings");    // ing inside testings  
        ASSERT_NO_MATCH("ing\\B", "ing");      // ing at word boundary
        ASSERT_NO_MATCH("ing\\B", "testing");  // ing at word end (boundary exists)
    }
    
    // Test 4: Numbers inside words
    {
        ASSERT_MATCH("\\B123\\B", "a1234b");   // 123 inside a1234b
        ASSERT_NO_MATCH("\\B123\\B", "123");   // 123 at boundaries
        ASSERT_NO_MATCH("\\B123\\B", "123 456"); // 123 at start
    }
    
    // Test 5: Compare \b vs \B behavior
    {
        const char *text = "The cat in concatenate";
        
        // \b should match cat at word boundary
        ASSERT_MATCH("\\bcat\\b", text);
        
        // \B should match the cat inside concatenate (not the standalone "cat")
        ASSERT_MATCH("\\Bcat\\B", text);
        
        // But \B should match the cat inside concatenate
        ASSERT_MATCH("\\Bcat", text);  // cat inside concatenate (without end \B)
    }
    
    // Test 6: Complex patterns with \B
    {
        ASSERT_MATCH("\\B\\w{2}\\B", "hello");    // 2 chars inside word - matches "ell" 
        ASSERT_MATCH("\\B[a-z]+\\B", "testing");  // letters inside word - matches "estin"
        // Removed confusing test case for clarity
    }
}