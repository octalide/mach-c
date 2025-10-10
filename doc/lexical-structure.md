# Lexical structure

Mach source text is tokenised by the bootstrap lexer (`src/lexer.c`). Understanding how the lexer behaves is essential for predicting how the parser will interpret your code.

## Character set

- Source files are read as UTF-8 byte sequences. Bytes outside ASCII are preserved but have no special meaning unless they occur inside string literals.
- Line endings may be `\n` or `\r\n`; both are treated the same.

## Whitespace and comments

- Whitespace (spaces, tabs, newlines, carriage returns) separates tokens but is otherwise ignored.
- Single-line comments start with `#` and continue to the next newline. Comments are removed before parsing; there is no block comment syntax.

## Identifiers

- The first character must be `A–Z`, `a–z`, or `_`.
- Subsequent characters may be `A–Z`, `a–z`, `0–9`, or `_`.
- Identifiers are case-sensitive and may be arbitrarily long.
- Identifiers that match reserved keywords are recognised as keywords (below) instead of user-defined names.

## Keywords

The following tokens are reserved and may not be used as identifiers:

| Keyword | Meaning |
|---------|---------|
| `use` | Import a module |
| `ext` | Declare an external symbol |
| `def` | Define a type alias |
| `pub` | Mark a declaration for export |
| `str` | Define a struct type |
| `uni` | Define a union type |
| `val` | Declare an immutable binding |
| `var` | Declare a mutable binding |
| `fun` | Define a function |
| `ret` | Return from a function |
| `if` | Conditional branch |
| `or` | Additional branch in an `if` chain (acts as `else`/`else if`) |
| `for` | Loop |
| `cnt` | Continue to the next loop iteration |
| `brk` | Break out of a loop |
| `asm` | Inline assembly block |
| `nil` | Null literal |

No other words are reserved.

## Literals

### Integer literals

- Digit separators `_` are ignored during lexing.
- Binary prefix: `0b` or `0B`.
- Octal prefix: `0o` or `0O`.
- Hexadecimal prefix: `0x` or `0X`.
- Decimal literals have no prefix. The lexer does not enforce digit grouping rules beyond the prefix constraints.
- Integer literals default to the smallest unsigned type that fits (`u8`, `u16`, `u32`, `u64`). During semantic analysis the literal may adopt a wider or signed type if required by context, or even a pointer type when the value is `0` and the target expects a pointer.

### Floating-point literals

- Contain a decimal point with digits on at least one side.
- Use the same digit separator rules as integers; there are no exponent forms yet.
- Default type is `f64` unless an expected float type is supplied by the surrounding context.

### Character literals

- Written as `'x'` (single quotes).
- Support escapes: `\'`, `\"`, `\\`, `\n`, `\t`, `\r`, `\0`.
- Default type is `u8`, or the expected integer type if it can represent the value.

### String literals

- Written as `"text"` (double quotes).
- Support the same escape sequences as character literals.
- The resulting type is `[]u8` (a fat pointer: data pointer plus length).

### Null literal

- `nil` is a literal that adopts pointer-like types. With no context it resolves to the built-in `ptr` type.

## Punctuation and operators

The lexer recognises the following punctuation and operator tokens:

```
( ) [ ] { } , ; : :: . .. ... _ ->
+ - * % ^ & | ~ < > <= >= == != << >>
= ! && || @ ? /
```

A solitary backslash (`\\`) is treated as a forward slash token (`/`). This keeps legacy escape usage working but should be avoided in new code.

## Token boundaries

Mach keeps a one-token lookahead. For example, `a- -b` lexes as `IDENTIFIER`, `-`, `-`, `IDENTIFIER`. When in doubt, add whitespace to disambiguate.
