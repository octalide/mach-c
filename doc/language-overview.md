# Language overview

Mach is a compiled, statically typed systems language. The bootstrap compiler (`mach-c`) targets LLVM via C23 and focuses on a lean core language: explicit types, predictable control flow, and low-level data layout that pairs well with C interop. This document highlights the pieces you need to know before diving into the detailed specification.

## Goals and non-goals

- **Explicit control** – No hidden heap allocations or implicit lifetime rules. Pointers, structs, and unions behave like their C counterparts.
- **Predictable semantics** – Every expression has a well-defined type; conversions happen only when the rules in this spec allow them.
- **Toolable surface** – The language avoids context-sensitive syntax and is designed to be parsed and analyzed with straightforward passes.
- **Small standard library** – The bootstrap compiler ships with the basic `mach-std` modules but does not prescribe a runtime. You opt in through `use` statements.

Out of scope for the bootstrap compiler:

- Implicit module/package discovery beyond the configured search paths.
- Automatic build orchestration. The C toolchain relies on `make` (see the repository `Makefile`).
- Language features not described here (e.g. generics, pattern matching) are intentionally unsupported.

## Hello, Mach

```mach
use std.io.console;

pub fun main() i32 {
    print("Hello, Mach!\\n");
    ret 0;
}
```

Key points illustrated:

- Modules are imported with `use path.to.module;`. Dots separate path segments.
- Functions are declared with `fun name(parameters) returnType { ... }`. The return type is optional; missing return types mean no return value.
- Strings are `[]u8` fat pointers at compile time and can be passed to external functions that expect pointers to byte sequences.
- `ret` either returns a value (`ret 0;`) or exits a void function (`ret;`).

Compile and run:

```bash
make
./bin/cmach path/to/program.mach
```

## Source form

- Files are UTF-8 text. No BOM is required; if present, it is treated as part of the first token.
- The canonical file extension is `.mach`.
- Statements end in semicolons unless the construct is a block (`{ ... }`).
- Indentation is not significant but idiomatic code uses four spaces.

## Components of the language

1. **Lexical structure** – whitespace, comments, literals, and how identifiers are classified.
2. **Type system** – primitive integers/floats, generic pointers, typed pointers (`*T`), arrays (`[]T`), structs (`str`), unions (`uni`), and aliases (`def`).
3. **Declarations** – module imports (`use`), visibility (`pub`), external linkage (`ext`), type definitions, variables, values, and functions.
4. **Statements and control flow** – blocks, `if`/`or` chains, `for` loops, `brk`/`cnt`, `ret`, inline assembly, and expression statements.
5. **Expressions** – operators (arithmetic, bitwise, logical), casts (`::`), pointer operators (`?`, `@`, `->`), function calls, indexing, literals, and composite literals.
6. **Intrinsics and runtime hooks** – compile-time built-ins (`size_of`, `align_of`, `offset_of`, `type_of`) and variadic affordances (`va_count`, `va_arg`, `...`).

Each of these sections is detailed in the subsequent documents.
