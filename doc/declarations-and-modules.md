# Declarations and modules

This document covers the syntactic forms that introduce new symbols as well as the module system that stitches compilation units together.

## Visibility with `pub`

- `pub` may precede any top-level declaration except `use`, `asm`, or stray expressions.
- Declaring `pub` elevates the symbol for export: the semantic analyser marks the symbol as public when it appears in the global or module scope.
- `pub` is not allowed inside function bodies.

## Module imports: `use`

```
use package.path.to.module;
```

- The path is a dot-separated list of identifiers. Each component is appended with a `.` during parsing.
- Semicolons are mandatory.
- Import aliases (e.g. `use foo.bar as baz;`) are not yet supported; attempting to write `use foo:bar;` emits an error.
- The semantic analyser resolves module paths via the project configuration (`mach.toml`) and the `ModuleManager` search paths. If resolution fails, compilation stops with an error.
- On success, all `pub` symbols from the imported module are injected into the current scope and marked as imported (`symbol->is_imported`). Subsequent `use` of the same module is idempotent.
- Module symbols themselves may be referenced in expressions. Accessing `module.member` performs a scoped lookup in the imported module.

## External declarations: `ext`

```
ext "C:puts" puts: fun(*u8) i32;
ext "systemv" syscall: fun(i64, ...);
ext printf: fun(*u8, ...);
```

- Optional leading string literal configures the calling convention and/or symbol name. The string is split on `:`:
  - `"C:puts"` → convention `C`, symbol name `puts`.
  - `"systemv"` → convention `systemv`, symbol inferred from the Mach name.
- With no string literal, the compiler assumes convention `C` and symbol name equal to the declared identifier.
- The type expression must be a function type; `ext` currently supports only function imports.
- `ext` declarations cannot be followed by bodies.
- Mark the declaration `pub` to re-export the symbol from the current module.

## Type aliases: `def`

```
def Size: u64;
```

- Defines a new name for an existing type. Aliases are first-class types with their own identity but share the target's layout.
- Aliases may be `pub` and participate in module exports.

## Structs: `str`

```
str Point {
    x: f32;
    y: f32;
}
```

- Declares a struct type and its field list.
- Each field requires a trailing semicolon.
- Use `pub str` to export the type.
- Anonymous struct types can appear in type positions: `str { x: f32; y: f32; }`.

## Unions: `uni`

```
uni Value {
    int: i64;
    ptr: *ptr;
}
```

- Similar rules to structs, but all fields share offset 0.
- Anonymous union types are available via `uni { ... }`.

## Variables and values: `var` / `val`

```
val answer: i32 = 42;
var counter: i64;
var pointer: *ptr = nil;
```

- Both forms require an explicit type annotation (`name: Type`).
- `val` bindings are immutable and must be initialised.
- `var` bindings are mutable; the initializer is optional.
- Top-level `var`/`val` declarations may be marked `pub`. Inside blocks, the keyword applies only to the immediate scope.
- The semantic analyser checks for redefinitions within the current scope.

## Functions: `fun`

```
pub fun main(argc: i32, argv: *ptr) i32 {
    ret 0;
}

fun log(message: []u8) {
    console.print message;
    ret;
}

fun variadic(format: []u8, ...) {
    ret;
}
```

- The parameter list is enclosed in parentheses. Parameter declarations use the same `name: Type` syntax as variable declarations.
- Append `...` as the final parameter to declare a Mach-managed variadic function. The `...` token does not create a named parameter.
- The return type is optional. When omitted, the function must use `ret;` (no value) or fall off the end.
- Functions may forward variadic arguments using the expression `...` (see [Expressions](./expressions.md)).
- Functions can be forward-declared by repeating the signature without a body; the second declaration must include the body.
- Nested functions are not supported.

## Inline assembly: top-level `asm`

```
asm {
    ; emitted verbatim into the generated module
}
```

- Available at top level and inside blocks.
- The contents between `{` and `}` are not parsed; they are copied verbatim (after trimming leading/trailing whitespace) into the generated LLVM assembly fragment.
- `pub asm { ... }` is invalid.

## Scopes and symbol tables

- The global scope contains all top-level symbols and imported modules.
- `use` introduces module symbols and public members into the current scope.
- Blocks (`{ ... }`) push a new scope; leaving the block pops it. Redeclaring names in an inner scope hides outer bindings, but reusing the same name within a single scope is an error.
- Modules themselves create their own scope (`module_scope`), enabling `module.member` access without polluting the global namespace.

## Module resolution pipeline

1. Parse every `use` statement at the top level.
2. The semantic pass resolves module paths by consulting:
   - The current `ProjectConfig` (if present via `mach.toml`).
   - Explicit search paths registered with the `ModuleManager`.
   - Directory aliases (so packages can be remapped to arbitrary folders).
3. Once the module is found, its exported symbols are merged into the importer’s scope. Name conflicts between imported symbols and existing symbols raise errors.
4. Imported modules are analysed before the importing file continues, ensuring that dependent types and values are available for type checking.

Keep this order in mind when structuring projects: any module referenced by `use` must be locatable when the importer is analysed.
