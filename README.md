# Mach C Bootstrap Compiler

`mach-c` is the reference C23 implementation of the Mach language front-end.

This project exists both as an early adoption playground while the self-hosted toolchain is under construction and as the bootstrap compiler.

## Project status

- Targets 64-bit Linux; builds with Clang/LLVM 16+.
- Emits object files and executables via the host C toolchain (`cc`).
- The minimal standard library [`mach-std`](https://github.com/octalide/mach-std) is required to build CLI programs and exercise the language runtime.
- Language coverage is documented in [`doc/`](doc/README.md). Anything not captured there should be considered unsupported.

## Building

```bash
git clone https://github.com/octalide/mach-c.git
cd mach-c
make        # produces bin/cmach
```

The build expects `clang`, `lld`, and `llvm-config` on `$PATH`. The recommended validation is to compile the standard library ([`mach-std`](https://github.com/octalide/mach-std)).

## Tool usage

`cmach` exposes a single entry point:

```bash
bin/cmach build <path/to/source.mach> [options]
```

Key options:

- `--emit-obj --no-link` &mdash; stop after producing an object file.
- `-o <path>` &mdash; set the output path (object or executable, depending on the mode).
- `-M std=path` &mdash; map an import prefix to a filesystem directory. Every project should at least map `std` to the root of `mach-std/src`.

The compiler does not perform final linking on its own; hand the emitted objects to `cc` (or copy the build logic from the [`mach` Makefile](https://github.com/octalide/mach/blob/main/Makefile)).

## Documentation

The language surface implemented by this compiler is described in [`doc/`](doc/README.md). Start with `language-overview.md`, then deep-dive into the lexical structure, type system, and runtime conventions as needed. When the implementation evolves, update the docs in lockstep &mdash; they are the canonical reference for users coming from the public repositories.

## Related repositories

- [`mach`](https://github.com/octalide/mach) &mdash; future home of the self-hosted Mach compiler
- [`mach-std`](https://github.com/octalide/mach-std) &mdash; standard library

Issues and pull requests are welcome while the language is still settling. Please include the compiler revision and reproduction steps with any bug report.
