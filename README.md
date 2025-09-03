# dynamic_regex.h

A single-header C library implementing regular expressions with a custom bytecode compiler and virtual machine. Provides a familiar API with methods like `test()`, `exec()`, and string methods like `match()` and `matchAll()`.

## Features

- **Familiar API**: Clean, intuitive interface for regex operations
- **Single-header Library**: Easy integration with `#include "regex.h"`
- **Backtracking Engine**: Recursive backtracking with bytecode execution
- **Unicode Support**: UTF-8 string handling with codepoint-aware operations
- **Memory Safe**: Automatic memory management with proper cleanup
- **Comprehensive Flags**: Support for global, ignorecase, multiline, dotall modes
- **Capturing Groups**: Full support for parenthetical captures
- **Real-world Tested**: Handles complex patterns like email, URL, and phone validation

## Quick Start

```c
#include "regex.h"

int main() {
    // Create a regex
    RegExp* email_regex = regex_new("\\w+@\\w+\\.\\w+", "gi");
    
    // Test if pattern matches
    if (regex_test(email_regex, "Contact us: user@example.com")) {
        printf("Email found!\n");
    }
    
    // Extract match details
    MatchResult* result = regex_exec(email_regex, "user@example.com");
    if (result) {
        printf("Matched: %s\n", result->groups[0]);
        match_result_free(result);
    }
    
    regex_free(email_regex);
    return 0;
}
```

## Building

### Requirements

- C11 compatible compiler (GCC, Clang, MSVC)
- CMake 3.10+ (for building tests)

### Build Instructions

```bash
# Clone and build
git clone <repository>
cd dynamic_regex.h
cmake -B cmake-build-debug
cmake --build cmake-build-debug

# Run test suite
./cmake-build-debug/dynamic_regex_h
```

### Integration

For simple integration, just copy `regex.h` and `regex.c` into your project:

```c
#include "regex.h"
// Your code here
```

For advanced string handling, include the dynamic_string.h dependency:

```c
#define DS_IMPLEMENTATION  // Include in exactly ONE .c file
#include "deps/dynamic_string.h/dynamic_string.h"
#include "regex.h"
```

## API Reference

### RegExp Object Methods

```c
// Create new RegExp with pattern and flags
RegExp* regex_new(const char* pattern, const char* flags);

// Test if pattern matches (returns boolean)
bool regex_test(RegExp* regexp, const char* text);

// Execute and return match details
MatchResult* regex_exec(RegExp* regexp, const char* text);

// Clean up RegExp object
void regex_free(RegExp* regexp);
```

### String Methods

```c
// Find matches in text
MatchResult* string_match(const char* text, RegExp* regexp);

// Find all matches in text
MatchIterator* string_match_all(const char* text, RegExp* regexp);
```

### Supported Flags

- `g` (global): Multiple matches with stateful lastIndex
- `i` (ignorecase): Case-insensitive matching  
- `m` (multiline): ^ and $ match line boundaries
- `s` (dotall): . matches newlines
- `u` (unicode): Unicode support
- `y` (sticky): Sticky matching

### Pattern Features

- **Literals**: `abc`, `123`
- **Wildcards**: `.` matches any character
- **Character Classes**: `[a-z]`, `[^0-9]`, `\w`, `\d`, `\s`
- **Quantifiers**: `*`, `+`, `?`, `{n,m}`
- **Anchors**: `^` start, `$` end, `\b` word boundary
- **Groups**: `(pattern)` capturing, `(?:pattern)` non-capturing
- **Alternation**: `|` for OR patterns
- **Escapes**: `\t`, `\n`, `\r`, `\\`, `\.`, etc.

## Examples

### Basic Matching

```c
RegExp* regex = regex_new("hello", "i");
bool matches = regex_test(regex, "Hello World");  // true
regex_free(regex);
```

### Capturing Groups

```c
RegExp* regex = regex_new("(\\w+)@(\\w+)", "");
MatchResult* result = regex_exec(regex, "user@example.com");
if (result) {
    printf("Full match: %s\n", result->groups[0]);     // user@example.com
    printf("Username: %s\n", result->groups[1]);       // user
    printf("Domain: %s\n", result->groups[2]);         // example
    match_result_free(result);
}
regex_free(regex);
```

### Global Matching

```c
RegExp* regex = regex_new("\\d+", "g");
MatchResult* result;

while ((result = regex_exec(regex, "Numbers: 123, 456, 789")) != NULL) {
    printf("Found: %s\n", result->groups[0]);
    match_result_free(result);
}
regex_free(regex);
```

### Complex Patterns

```c
// Email validation
RegExp* email = regex_new("^[\\w._%+-]+@[\\w.-]+\\.[A-Za-z]{2,}$", "");

// Phone number extraction  
RegExp* phone = regex_new("\\(?\\d{3}\\)?[-\\s]?\\d{3}[-\\s]?\\d{4}", "g");

// URL matching
RegExp* url = regex_new("https?://[\\w.-]+(?:\\.[\\w.-]+)+[\\w\\-\\._~:/?#[\\]@!\\$&'\\(\\)\\*\\+,;=.]+", "gi");
```

## Memory Management

Always free allocated objects:

```c
RegExp* regex = regex_new("pattern", "flags");
MatchResult* result = regex_exec(regex, "text");
MatchIterator* iter = string_match_all("text", regex);

// Clean up
match_result_free(result);
match_iterator_free(iter);
regex_free(regex);
```

## Performance

- Uses backtracking-based matching with bytecode execution
- Performance depends on pattern complexity and input size
- Supports large input strings (10K+ characters)
- Character classes use 256-bit bitmaps for O(1) lookup
- Dynamic string integration provides copy-on-write optimization

## Dependencies

### Runtime Dependencies
- **dynamic_string.h**: Reference-counted string library (optional but recommended)

### Development Dependencies  
- **Unity**: Testing framework for comprehensive test coverage

## Testing

The library includes comprehensive tests covering:

- Basic pattern matching (literals, wildcards, anchors)
- Character classes and escape sequences
- Quantifiers and repetition
- Capturing groups and alternation
- Regular expression flags
- Complex real-world patterns
- Error handling and edge cases
- Performance and memory management

Run tests with:
```bash
./cmake-build-debug/dynamic_regex_h
```

## Architecture

The regex engine uses a four-stage compilation pipeline:

1. **Lexer**: Pattern string → Token stream
2. **Parser**: Token stream → AST (with operator precedence)
3. **Compiler**: AST → Bytecode instructions
4. **Executor**: Bytecode + input text → Match results

## License

This project is dual-licensed under your choice of:

- MIT License
- The Unlicense (public domain)

See LICENSE file for details.

## Contributing

Contributions are welcome! The codebase uses an amalgamation architecture where components are developed in separate files but compiled together. Please ensure all tests pass before submitting changes.