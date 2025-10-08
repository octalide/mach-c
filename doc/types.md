# Types

Primitives and composites exposed by the implementation.

## Primitive integers
- Unsigned: `u8, u16, u32, u64`
- Signed: `i8, i16, i32, i64`

## Primitive floats
- `f16, f32, f64`

## Pointers
- Untyped pointer: `ptr` (generic) exists internally; use `*T` for typed pointers.
- Address-of `?expr` yields `*T` when `expr: T`.
- Dereference `@p` yields `T` when `p: *T`.
- The literal `nil` evaluates to a null pointer; it type-checks against pointer-like types and defaults to `ptr` when no context is available.

## Arrays
- Syntax: `[]T` (unbounded) or `[N]T`.
- Value semantics are fat pointer-like with length; index yields element type `T`.

## Structs and Unions
- `str Name { field: T; ... }`
- `uni Name { field: T; ... }`
- Offsets/layout: `offset_of(Type, field)` returns `u64`.

## Functions
- Type: `fun(T1, T2) R` or `fun() R`; omit `R` for no return.
- Calls perform argument count and type checks; implicit literal fitting permitted.

## Aliases
- `def alias: TargetType;` defines a named alias; use `type_resolve_alias` semantics internally.

## Built-in aliases (std)
- From common std patterns:
  - `def char: u8;`
  - `def string: []char;`
  - `def bool: u8;` with `true = 1`, `false = 0`.

## Type utilities
- `size_of(x)` → `u64`
- `align_of(x)` → `u64`
- `type_of(x)` → `u64` (implementation id)
- `len(a)` → `u64` for arrays

