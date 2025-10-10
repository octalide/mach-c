# Mach Language Specification (Bootstrap Compiler)

This directory documents the Mach programming language as implemented by the bootstrap C compiler (`mach-c`). The intent is to capture the precise behavior exposed by the current toolchain so that the language can be used, tested, and evolved without guesswork.

The bootstrap compiler represents an **alpha** snapshot of the language. Future self-hosted compilers will grow additional tooling features (project integration, richer build pipelines, etc.), but the language rules laid out here define the surface users should rely on when targeting this compiler.

## Document map

- [Language overview](./language-overview.md) – goals, guiding principles, and a first working example.
- [Lexical structure](./lexical-structure.md) – characters, tokens, literals, and keywords.
- [Type system](./types.md) – built-in types, pointers, arrays, structs, unions, and aliases.
- [Declarations and modules](./declarations-and-modules.md) – top-level forms, visibility, and importing code.
- [Statements](./statements.md) – control flow, blocks, inline assembly, and expression statements.
- [Expressions](./expressions.md) – operators, precedence, literals, casts, and composite construction.
- [Intrinsics and runtime conventions](./intrinsics-and-runtime.md) – built-in functions, variadics, and interoperability hooks.

Each document is self-contained but cross-references related sections where appropriate. If the implementation and specification disagree, treat the implementation as authoritative and update the docs accordingly.
