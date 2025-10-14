# Intrinsics and runtime conventions

The bootstrap compiler exposes a small set of built-in operations and calling conventions that extend the core language. These features bridge the gap between pure Mach code and the underlying runtime (C ABI, stack layout, etc.).

## Built-in functions (intrinsics)

These identifiers are recognised specially when used as simple function calls. They are not ordinary symbols and cannot be shadowed.

| Name | Expected arguments | Result | Description |
|------|---------------------|--------|-------------|
| `size_of(expr)` | Any expression or type literal | `u64` | Returns the size in bytes of the operand’s type. The operand is analysed but not evaluated. |
| `align_of(expr)` | Any expression or type literal | `u64` | Returns the alignment requirement of the operand’s type. |
| `offset_of(typeExpr, fieldName)` | Struct-type expression, identifier literal | `u64` | Returns the byte offset of `fieldName` within the struct type. Errors if the field is missing or the type is not a struct. |
| `type_of(expr)` | Any expression | `u64` | Produces a runtime identifier describing the expression’s type. Currently implemented as a hash/ID; no further reflection is available. |
| `va_count()` | *(none)* | `u64` | Valid only inside a Mach-managed variadic function. Returns the number of trailing variadic arguments supplied at the call site. |
| `va_arg(index)` | Integer expression | `ptr` | Also restricted to Mach variadic functions. Returns an opaque pointer to the `index`-th variadic argument. You must cast to the desired type manually before use. |

Rules:

- Intrinsics participate in overload resolution before standard lookup. If the callee is not one of the recognised names, normal function resolution applies.
- Intrinsics perform semantic checks (argument count, type categories) and emit errors on misuse.
- `offset_of` requires the second argument to be an identifier expression. String literals are not accepted.

## Variadic functions and `...`

Mach distinguishes between two categories of variadic calls:

1. **Mach-managed variadics** – Functions declared with `...` in their signature. Example:
   ```mach
   fun log(format: []u8, ...) {
       ret;
   }
   ```
   - The analyser marks such functions with `uses_mach_varargs`.
   - Call sites may pass any number of extra arguments after the fixed parameters.
   - Inside the function, `va_count()` and `va_arg(index)` inspect the argument pack.
   - To forward the current function’s variadic arguments to another Mach variadic function, supply `...` as the final argument:
     ```mach
     fun log_forward(format: []u8, ...) {
         other_logger(format, ...);
         ret;
     }
     ```
     This is only valid when the callee also uses Mach-managed variadics.

2. **Foreign (C-style) variadics** – Declared via `ext` and relying on the target ABI. Example:
   ```mach
   ext "C:printf" printf: fun(*u8, ...);
   ```
   - These functions set the variadic flag on the type but are marked as *not* using Mach-managed varargs.
   - Calls with `...` forwarding are disallowed.
   - Arguments beyond the fixed parameters are passed directly to the foreign function with no compile-time checks.

The semantic analyser enforces the following:

- `...` may only appear in parameter lists as the final token.
- Within function bodies, the expression `...` is valid only inside a Mach variadic function and only as the final argument in a call to another Mach variadic function.
- When forwarding `...`, no additional arguments may follow it.

## Calling conventions and external linkage

External functions declared with `ext` may specify a calling convention and symbol name. The string literal is optional and interpreted as `"convention:symbol"` (symbol part optional).

- Supported conventions currently include `C` (default) and pass-through strings used to tag LLVM call attributes. Unrecognised conventions are still emitted but may not map to a real ABI.
- The `symbol` component lets you override the exported name (useful for linking against C libraries with different identifiers).

Example:

```mach
ext "C:puts" puts: fun(*u8) i32;
ext "systemv" syscall: fun(i64, ...);
ext custom_log: fun(*u8);
```

## Module import side effects

The `ModuleManager` resolves `use` statements before analysing declarations. Imported modules are compiled (if needed) and their public symbols are merged into the importer’s scope. Cycles are not yet supported; recursive imports will cause undefined behaviour or stack overflow in the importer.

Imported modules originate from:

- Paths expanded via `mach.toml` (`ProjectConfig`).
- Alias mappings configured in the project.
- Fallback search paths (typically `mach-std`).

If resolution fails, the analyser emits a diagnostic pointing to the failing `use` statement.

## Runtime layout commitments

- Structs follow standard C layout rules: each field is aligned to its natural alignment; padding is inserted as needed; the overall size is rounded up to the max alignment.
- Unions store the maximum field size, aligned to the maximum field alignment. All fields start at offset 0.
- Arrays/slices are fat pointers `{ data: *T, len: u64 }`. Passing an array to external C functions usually requires extracting the data pointer manually (`?arr[0]`).
- All pointers (`ptr` and `*T`) are 64-bit. The compiler currently assumes a 64-bit target; other architectures require adjustments in `type.c` and the LLVM target configuration.

## Runtime panic handling

- The standard runtime now exports `mach_panic(message: []u8)` and an `abort()` fallback. Both write a short diagnostic to `STDERR` and terminate the process via the platform-specific exit shim.
- Bounds checks and other compiler-inserted guards call `abort` when a violation is detected. Providing `abort` inside the runtime keeps the generated objects self-contained even when linking without the C runtime.
- `mach_panic` is available to user code as well; it is the unified way to surface unrecoverable errors until richer error handling is implemented.

## Error reporting and diagnostics

The semantic analyser reports errors with token locations. When an error originates in an imported module, diagnostics fall back to less detailed output (`module:pos:n`). Inline assembly blocks bypass semantic checks; errors may surface during LLVM IR verification or at link time.

## Future compatibility notes

- The self-hosted compiler intends to reuse these language rules. Additional tooling (such as richer project configuration and automated builds) will be layered on top without breaking the semantics documented here.
- Intrinsic behaviour is subject to refinement, but all existing names will remain reserved to avoid source breakage.
