# Intrinsics

Built-in functions recognized by the compiler during semantic analysis.

- `len(x)` → `u64`
  - Valid on arrays; returns length.
- `size_of(x)` → `u64`
- `align_of(x)` → `u64`
- `offset_of(Type, field)` → `u64`
  - First arg must be a struct type, second a field name.
- `type_of(x)` → `u64` (implementation-defined type id)
- `va_count()` → `u64`
  - Only valid inside a variadic `fun`; returns the number of extra arguments supplied at the callsite.
- `va_arg(i)` → `ptr`
  - Only valid inside a variadic `fun`. `i` must be an integer literal or value.<br>
    Returns a `ptr` pointing at the i-th extra argument (0-based). Cast and dereference manually.

Notes:
- Intrinsics are ordinary call syntax but checked specially.
- Some accept either a value or type expression; the compiler resolves and validates.
