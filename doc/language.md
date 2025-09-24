# Language

This document summarizes core syntax and semantics recognized by the compiler.

## Lexical
- Comments: line comments are supported and skipped by the lexer.
- Identifiers: `[A-Za-z_][A-Za-z0-9_]*`.
- Literals:
  - Integers: decimal, binary `0b...`, octal `0o...`, hex `0x...`; underscores allowed; type inferred.
  - Floats: digits with a single `.`; underscores allowed; default type `f64`.
  - Char: `'a'` with escapes `\n`, `\t`, `\r`, `\0`, `\'`, `\"`, `\\`.
  - String: `"..."` UTF-8; type is `[]u8` (aliasable as `string`).

## Keywords
- Declarations: `use`, `ext`, `def`, `val`, `var`, `fun`, `str`, `uni`.
- Control flow: `if`, `or` (else-if chain), `for`, `brk`, `cnt`, `ret`.

## Types (surface syntax)
- Named: identifiers (builtins include `u8,u16,u32,u64,i8,i16,i32,i64,f16,f32,f64`, `ptr`).
- Pointers: `*T` (typed pointer). Address-of `?expr`, dereference `@expr`.
- Arrays: `[N]T` or `[]T` (unbounded/fat pointer). Index with `a[i]`.
- Function types: `fun(T1, T2) R` or `fun() R`. Omit `R` for no return.
- Struct/Union types:
  - Named: `str Name { field: T; ... }`, `uni Name { field: T; ... }`.
  - Anonymous in types: `str { field: T; ... }`, `uni { field: T; ... }`.

## Declarations
- Module import: `use alias: pkg.path;` or `use pkg.path;`.
- External symbol: `ext "C:puts" puts: fun(*u8) i64;` (format `"CONV:SYMBOL"`; default convention `C`, symbol defaults to name).
- Type alias: `def name: T;`.
- Constant and variable:
  - `val NAME: T = expr;` (required init)
  - `var name: T;` or `var name: T = expr;`
- Function:
  - `fun name(p1: T1, p2: T2) R { ... }`
  - Return with `ret expr;` or `ret;` when `R` omitted.

## Statements
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
- Index/field: `a[i]`, `s.field`. Module member access uses the same `.` syntax.
- Cast: `expr :: T`.
- Unary: `+ - ! ~ ? @`.
  - `?x` address-of; `@p` dereference; `!x` logical not (returns `u8`).
- Binary precedence (low → high): `=`; `||`; `&&`; `|`; `^`; `&`; `== !=`; `< <= > >=`; `<< >>`; `+ -`; `* / %`.
- Typed literals:
  - Arrays: `[N]T{ a, b, c }` or `[]T{ a, b }`.
  - Structs: `TypeName{ field: expr, ... }` or `str { ... }{ ... }`.

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
