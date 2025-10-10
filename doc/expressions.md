# Expressions

Expressions produce values and drive most compile-time reasoning. This chapter summarises the expression forms supported by the bootstrap compiler and the rules enforced during semantic analysis.

## Operator precedence and associativity

Mach uses a Pratt parser with the following precedence table (from lowest to highest). Operators at the same level associate left-to-right unless noted.

| Level | Operators | Notes |
|-------|-----------|-------|
| Assignment | `=` | Right-associative. LHS must be an lvalue. |
| Logical OR | `||` | Operands must be truthy; any type is allowed. |
| Logical AND | `&&` | Same truthiness rules as `||`. |
| Bitwise OR | `|` | Integer operands only. |
| Bitwise XOR | `^` | Integer operands only. |
| Bitwise AND | `&` | Integer operands only. |
| Equality | `==`, `!=` | Types must match exactly or be mutually castable. |
| Comparison | `<`, `<=`, `>`, `>=` | Numeric operands only. |
| Shift | `<<`, `>>` | Integer operands only. |
| Additive | `+`, `-` | Numeric operands. Pointer ± integer is permitted. |
| Multiplicative | `*`, `/`, `%` | Numeric operands only. |
| Unary | `+`, `-`, `!`, `~`, `?`, `@` | See below. |
| Postfix | Calls, indexing, field access, casts | Highest binding power. |

> **Note:** The parser recognises unary `*`, but the semantic analyser currently rejects it (`unknown unary operator`). Use `@expr` for dereferencing instead.

## Lvalues and assignment

- Valid lvalues include identifiers bound to variables/values/parameters, indexing expressions, field access on lvalues, and dereferenced pointers (`@expr`).
- `a = b` analyses the left-hand side first to obtain the target type, then checks that the right-hand side can be assigned using the rules from [Types](./types.md#type-equality-and-assignment-rules).
- The assignment expression evaluates to the assigned value’s type (matching the LHS).

## Unary operators

| Operator | Meaning | Restrictions |
|----------|---------|--------------|
| `+` | Numeric identity | Operand must be numeric. |
| `-` | Numeric negation | Operand must be numeric. |
| `!` | Logical not | Works on any type; result is `u8` (0 or 1). |
| `~` | Bitwise complement | Operand must be integer. |
| `?` | Address-of | Operand must be an lvalue. Result is `*T`. |
| `@` | Dereference | Operand must be pointer-like (`*T`, `ptr`, `[]T`, function pointer). Result is the pointed-to type. |

## Binary operators

- Arithmetic (`+`, `-`, `*`, `/`, `%`) promote numeric operands to a common type. Mixing integers and floats is not allowed; you must cast explicitly.
- Pointer arithmetic (`ptr + int`, `ptr - int`) returns the pointer type unchanged.
- Bitwise operators (`&`, `|`, `^`, `<<`, `>>`) require integer operands.
- Logical operators (`&&`, `||`) accept any operands; non-zero values are considered true.
- Equality operators allow comparison between equal types or types that can cast to one another. The result type is `u8`.

## Casts

`expr :: Type`

- Casts are postfix expressions triggered by `::`.
- The analyser checks `type_can_cast_to(from, to)`. The following conversions are permitted:
  - Between numeric types (integer ↔ integer, float ↔ float) regardless of size/sign.
  - Between pointer-like types (including `ptr`, `*T`, `[]T`, function pointers).
  - Integer ↔ pointer-like conversions.
- Array-to-pointer decay is **not** implicit and is rejected by the cast rules. Use `?array[0]` or construct a slice literal explicitly.

## Function calls

```
callee(arg0, arg1, ...)
```

- The callee expression must resolve to a function type. Aliases are resolved transparently.
- Fixed arguments are type-checked against the signature. Variadic arguments are analysed for side effects but are not type-checked beyond basic validity.
- To forward the current function’s variadic arguments, use the expression `...` as the final argument. This is valid only inside Mach-managed variadic functions and when calling another Mach-managed variadic function.

## Indexing

`sequence[index]`

- `sequence` must be an array (`[]T`) or pointer-like value.
- `index` must be an integer type.
- The result type is the element type (`T` for arrays, base type for pointers).
- No bounds checking is performed at compile time.

## Field access and module members

- `expr.field` accesses a struct/union field or a module export.
- When `expr` is a module identifier, the analyser looks up `field` inside the module’s scope.
- When `expr` is pointer-like, `expr->field` is desugared to `(@expr).field` during parsing.

## Literals

- Integer, float, character, string, and `nil` behave as described in [Lexical structure](./lexical-structure.md#literals).
- Struct literals: `Type{ field0: expr0, field1: expr1 }`. Every field must be named. The analyser checks that each provided field exists and that no extra fields appear. Missing fields default to zero-initialised memory at runtime.
- Array literals: `[]T{ elem0, elem1, ... }` or slice literals `[]T{ dataPtr, len }`.
- Union literals reuse the struct literal syntax but only one field should be initialised at a time.

## `nil`

- The `nil` literal adopts the surrounding pointer-like expected type. Without context it becomes the built-in `ptr` type.
- Assigning `nil` to non-pointer types is an error.

## The `...` expression

- Inside a Mach variadic function, `...` evaluates to the current function’s varargs pack. Its type is the untyped pointer `ptr`.
- It is valid only as the final argument in a function call and only when the callee also uses Mach-managed varargs.

## Built-in intrinsics

The following identifiers are recognised specially when used as simple function names:

| Name | Signature | Meaning |
|------|-----------|---------|
| `size_of(T)` | `fun(Type) u64` | Returns the size (in bytes) of the argument’s type. |
| `align_of(T)` | `fun(Type) u64` | Returns the alignment (in bytes) of the argument’s type. |
| `offset_of(T, field)` | `fun(Type, ident) u64` | Returns the offset of `field` within struct type `T`. |
| `type_of(expr)` | `fun(any) u64` | Returns a runtime type identifier for `expr`. |
| `va_count()` | `fun() u64` | Number of variadic arguments supplied to the current function. |
| `va_arg(index)` | `fun(u64) ptr` | Pointer to the `index`-th variadic argument. |

These intrinsics are handled directly by the semantic analyser; they bypass normal symbol lookup.
