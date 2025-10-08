# Language

This document summarizes core syntax and semantics recognized by the compiler.

## Lexical
- Comments: line comments start with `#` and run until the end of the line.
- Identifiers: `[A-Za-z_][A-Za-z0-9_]*`.
- Literals:
  - Integers: decimal, binary `0b...`, octal `0o...`, hex `0x...`; underscores allowed; type inferred.
  - Floats: digits with a single `.`; underscores allowed; default type `f64`.
  - Char: `'a'` with escapes `\n`, `\t`, `\r`, `\0`, `\'`, `\"`, `\\`.
  - String: `"..."` UTF-8; type is `[]u8` (aliasable as `string`).

- Keywords
  - Visibility: `pub` (may precede declarations except `use` and `asm`).
  - Declarations: `use`, `ext`, `def`, `val`, `var`, `fun`, `str`, `uni`.
  - Control flow: `if`, `or` (else-if chain), `for`, `brk`, `cnt`, `ret`.
  - Miscellaneous: `asm` (inline assembly), `nil` (null literal sugar).

## Types (surface syntax)
- Named: identifiers (builtins include `u8,u16,u32,u64,i8,i16,i32,i64,f16,f32,f64`, `ptr`).
- Pointers: `*T` (typed pointer). Address-of `?expr`, dereference `@expr`.
- Arrays: `[N]T` or `[]T` (unbounded/fat pointer). Index with `a[i]`.
- Function types: `fun(T1, T2) R` or `fun() R`. Omit `R` for no return.
  - Variadic: append `, ...` as last parameter (e.g., `fun(i32, ...) i32`). In declarations `fun name(a: i32, ...) { }` or external types `ext "C:printf" printf: fun(*u8, ...) i32;`.
  - Inside a variadic function, intrinsics:
    - `va_count()` -> `u64` number of extra arguments
    - `va_arg(i)` -> `ptr` pointer to i-th extra argument (0-based). The pointed memory contains the original argument value; user must cast and dereference.
- Struct/Union types:
  - Named: `str Name { field: T; ... }`, `uni Name { field: T; ... }`.
  - Anonymous in types: `str { field: T; ... }`, `uni { field: T; ... }`.

## Declarations
- Module import: `use pkg.path;`. Import aliases are not yet supported.
- Visibility: prefix a declaration with `pub` to export it from the current module. `pub` is rejected on `use` statements and inline `asm` blocks.
- External symbol: `ext "C:puts" puts: fun(*u8) i64;` (format `"CONV:SYMBOL"`; default convention `C`, symbol defaults to name).
- Type alias: `def name: T;`.
- Constant and variable:
  - `val NAME: T = expr;` (required init)
  - `var name: T;` or `var name: T = expr;`
- Function:
  - `fun name(p1: T1, p2: T2) R { ... }`
  - Return with `ret expr;` or `ret;` when `R` omitted.
  - Top-level `ext` declarations use the same function type syntax and accept an optional convention string literal: `pub ext "C:puts" puts: fun(*u8) i64;`.

- Inline assembly:
  - `asm <rest of line>` copies the trimmed text after `asm` directly into the generated assembly stream. No `pub` prefix is allowed.
- Block: `{ ... }` (lexical scope).
- If/else-if chain:
  ```
  if (cond1) { ... }
  or (cond2) { ... }
  or { ... } // else branch when condition omitted
  ```
- For loop:
  - `for { ... }` (infinite)
  - `for (cond) { ... }`
  - `brk;` breaks, `cnt;` continues.
- Expression statement: `expr;`.

## Expressions
- Primary: identifiers, literals, parenthesized `(...)`.
- Calls: `f(a, b)`; arguments must match parameter types.
- Index/field: `a[i]`, `s.field`. Pointers use `p->field` as shorthand for `(@p).field`. Module member access uses the same `.` syntax.
- Cast: `expr :: T`.
- Unary: `+ - ! ~ ? @`.
  - `?x` address-of; `@p` dereference; `!x` logical not (returns `u8`).
- Assignment: `a = b` evaluates the right-hand side first, then stores into the left-hand side. The expression result is the assigned value.
- Binary precedence (low → high): `=`; `||`; `&&`; `|`; `^`; `&`; `== !=`; `< <= > >=`; `<< >>`; `+ -`; `* / %`.
- Typed literals:
  - Arrays: `[N]T{ a, b, c }` or `[]T{ a, b }`.
  - Structs: `TypeName{ field: expr, ... }` or `str { ... }{ ... }`.
- Nil literal: `nil` produces a null pointer; its type is inferred from context or defaults to `ptr`.

## Type semantics
- Integer widening and float promotion follow usual rules; binary ops promote to a common type.
- Equality permits equivalent or cast-compatible types.
- Truthiness: any non-zero value is true; logical ops produce `u8`.
- Pointers: pointer arithmetic with `+`/`-` and integer; `@` requires pointer-like operand.
- Arrays are fat pointers (`{data, len}`) at the type level; `len(x)` returns `u64`.

## Name and scope
- Lexical scoping with nested blocks.
- Symbol kinds: variables, constants, functions, types, fields, params, modules.
- Module-qualified access: `pkg.symbol`.

## Errors
- The compiler reports parse and semantic errors with source locations when available.

## Platform Target Module
The compiler synthesizes a builtin module named `target` containing immutable
platform constants for conditional logic and compile-time decisions. Import it
with a normal use statement:

```
use target;

fun main() {
  if (OS == OS_LINUX) {
    // linux-specific path
  } or if (OS == OS_WINDOWS) {
    // windows-specific path
  } or {
    // fallback
  }
  ret;
}
```

### Provided constants
Numeric enum-style ids (stable once published):
```
val OS_LINUX: u32 = 1;
val OS_DARWIN: u32 = 2;
val OS_WINDOWS: u32 = 3;
val OS_FREEBSD: u32 = 4;
val OS_NETBSD: u32 = 5;
val OS_OPENBSD: u32 = 6;
val OS_DRAGONFLY: u32 = 7;
val OS_WASM: u32 = 8;
val OS_UNKNOWN: u32 = 255;

val ARCH_X86_64: u32 = 1;
val ARCH_AARCH64: u32 = 2;
val ARCH_RISCV64: u32 = 3;
val ARCH_WASM32: u32 = 4;
val ARCH_WASM64: u32 = 5;
val ARCH_UNKNOWN: u32 = 255;
```

Concrete build values:
```
val OS: u32;         // one of OS_* ids
val ARCH: u32;       // one of ARCH_* ids
val PTR_WIDTH: u8;   // pointer bit width (e.g. 64)
val ENDIAN: u8;      // 0 little, 1 big
val DEBUG: u8;       // 1 if debug build (reserved, currently 0)
val OS_NAME: []u8;   // lowercase OS name string
val ARCH_NAME: []u8; // architecture name string
```

### Usage patterns
Derive helper booleans or select code paths:
```
use target;
val IS_UNIX: u8 = (OS == OS_LINUX or OS == OS_DARWIN or OS == OS_FREEBSD or OS == OS_NETBSD or OS == OS_OPENBSD or OS == OS_DRAGONFLY);

fun init() {
  if (IS_UNIX) { /* unix setup */ } or { /* fallback */ }
  ret;
}
```

Keep platform-specific numeric constants in ordinary modules using `target`:
```
use target;
val SIGINT: i32 = if (OS == OS_WINDOWS) { 15 } or { 2 };
```

No macro system or file suffix variants are required; normal `if` chains are
used and can later be optimized by constant folding.
