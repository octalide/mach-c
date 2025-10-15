# Type system

The bootstrap compiler models types explicitly. Every expression has a concrete type by the end of semantic analysis. This document enumerates the kinds of types supported, how they are constructed, and the rules that govern them.

## Built-in primitives

The compiler creates the following primitives during `type_system_init`:

| Kind | Size (bytes) | Alignment | Notes |
|------|--------------|-----------|-------|
| `u8`, `u16`, `u32`, `u64` | 1, 2, 4, 8 | same as size | Unsigned integers |
| `i8`, `i16`, `i32`, `i64` | 1, 2, 4, 8 | same as size | Signed integers |
| `f16`, `f32`, `f64` | 2, 4, 8 | same as size | IEEE floats |
| `ptr` | 8 | 8 | Untyped (opaque) pointer |

Primitive names are reserved identifiers. They may be referred to directly in source.

## Typed pointers

Typed pointers are written as `*T` where `T` is any other type.

- Size and alignment are 8 bytes (64-bit pointers).
- Pointer types are cached, so repeated `*T` constructions reuse the same descriptor.
- Pointer arithmetic is limited to adding/subtracting integer offsets (see [Expressions](./expressions.md)).

The address-of operator (`?expr`) yields a typed pointer when the operand is an lvalue. Dereferencing is performed with `@expr` or the `->` sugar for accessing fields on pointers to structs.

### `ptr` versus `*ptr`

`ptr` is a standalone built-in type that represents an opaque pointer – it is the Mach analogue of `void*` in C. Because Mach has no `void` type, `ptr` exists as the canonical “untyped pointer” that you can use for FFI boundaries, raw memory buffers, or when type information is unavailable.

Writing `*ptr` builds a *typed* pointer whose base type is `ptr`; it therefore corresponds to `void**` in C. Examples such as `argv: *ptr` model the classic `char **argv` signature by saying “a pointer to an array of untyped pointers.”

So while `ptr` behaves similarly to an untyped pointer, it is not syntactic sugar for `*void` – there is no implicit desugaring step. You can freely create additional levels of indirection (`**ptr`, etc.) using the same pointer construction rules as with any other type.

## Arrays

Array types are written as `[N]T` or `[_]T` / `[]T`:

- Regardless of syntax, arrays are *fat pointers*: `{ data: *T, len: u64 }`. They are first-class values that carry both a pointer and a length.
- The optional `N` expression is currently parsed but not enforced by the type system. It is best used for documentation; semantic checks still treat all arrays as unsized slices.
- `[]T` (or `[_]T`) omits the bound and produces the same fat-pointer representation.
- Strings are identical to `[]u8` values.

Array literals accept two shapes:

1. `[]T{ value0, value1, ... }` – constructs a new array whose elements are type-checked against `T`.
2. `[]T{ dataPtr, length }` – marks the literal as a slice literal, reusing an existing pointer/length pair. The first element must be pointer-compatible with `*T`; the second must be integer-compatible with `u64`.

When a type alias resolves to an array type, `Alias{ ... }` uses the same rules.

> **Dynamic growth.** The core language does not auto-resize slices; any append-like operation must allocate explicitly. The bundled standard library module `std.types.array` exposes helper functions—`array_append`, `array_append_slice`, `array_reserve`, `array_shrink_to_fit`, `array_clear`, and `array_free`—that manage capacity by storing bookkeeping metadata in front of the slice’s data pointer. Each helper returns a new slice value (potentially with a different backing pointer), so call sites must reassign the result:

```mach
use std.types.array;

var bytes: []u8 = []u8{ nil, 0 };
bytes = array_append<u8>(bytes, 42);
bytes = array_append_slice<u8>(bytes, []u8{ 'O', 'K', 0 });

# release the allocation once it is no longer needed
bytes = array_free<u8>(bytes);
```

Treat these helpers as part of the contract of the standard library. Code that bypasses them must allocate and free the slice storage manually.

## Structs

Struct types are introduced with `str` declarations:

```mach
pub str Vec2 {
    x: f32;
    y: f32;
}
```

Semantics:

- Fields are declared as `name: Type;` within braces. Semicolons between fields are required.
- The compiler computes offsets using standard alignment rules: each field is aligned to its natural alignment; the overall struct size is rounded up to the maximum field alignment.
- Struct symbols carry a linked list of field descriptors (`Symbol_FIELD`) that records names, types, and offsets.
- Struct types can be referenced by name or constructed anonymously with `str { field: Type; ... }` in type positions.

Struct literals use the `Type{ field: expr, ... }` form. Every field must be named; positional initialisers are not supported.

## Unions

Union types are introduced with `uni` declarations:

```mach
uni Value {
    int: i64;
    ptr: *ptr;
}
```

- All fields start at offset 0.
- The union size is the maximum of the field sizes, rounded up to the maximum alignment among its fields.
- Like structs, unions can be named or anonymous (`uni { ... }`).

Union literals currently reuse the struct literal machinery. Named field initialisers are required, and only one field may be initialised at a time.

## Function types

Function types appear in two places: 

- Explicit function signatures: `fun name(params) ReturnType { ... }`.
- Standalone type expressions: `fun(paramType0, paramType1, ... ) ReturnType`.

Function type details:

- Parameters are ordered and stored as an array of types.
- A function may be marked variadic by including `...` as the final element in the parameter list. Variadic signatures activate Mach-managed varargs (see [Intrinsics and runtime conventions](./intrinsics-and-runtime.md)).
- The return type is optional. Missing return types are treated as `void` (represented internally as `NULL`).

## Type aliases

`def Name: Type;` creates a named alias. Aliases:

- Retain their own size/alignment fields, mirroring the target type.
- Are resolved lazily; `type_resolve_alias` follows alias chains when needed.
- Participate in equality checks by name: two struct aliases with the same underlying type but different names are considered different types.

## Type equality and assignment rules

- `type_equals` resolves aliases before comparison.
- Numeric types are grouped for conversion: integer ↔ integer (widening only) and float ↔ float. Implicit integer ↔ float assignments are rejected.
- Pointer-like categories include `ptr`, `*T`, `[]T`, and function pointers. Arrays can assign to pointers if their element type matches.
- Pointer ↔ integer assignments are forbidden; explicit casts are required.

See [Expressions](./expressions.md) for the list of casts and conversions available to user code.
