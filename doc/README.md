# Mach Language Documentation

Concise reference for the Mach language as implemented by `cmach`.

- language: `language.md` — syntax and semantics
- types: `types.md` — primitive and composite types
- modules: `modules.md` — imports and packaging
- FFI: `ffi.md` — calling external symbols
- intrinsics: `intrinsics.md` — built-in functions
- config: `config.md` — `mach.toml` options
- CLI: `cli.md` — `cmach` commands and flags
- examples: `examples.md` — small, focused snippets

Quick start:

```
use console: std.io.console;

fun main() u32 {
    console.print("Hello, world!\n");
    ret 0;
}
```

Build a project (with `mach.toml`):

```
cmach build
cmach run
```
