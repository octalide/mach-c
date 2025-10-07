# Modules

Mach organizes code into modules addressed by dotted paths.

## Importing
- `use pkg.path;` — imports the module and exposes its top-level symbols.
- Member access uses `.`: `pkg.foo`, `dep.std.console.print`.

## Resolution
- Module search paths:
  - Current file directory and project `src-dir`.
  - Project `dep-dir` for dependencies declared in `mach.toml`.
  - Optional standard library path (`runtime.stdlib-path`), or environment `MACH_STDLIB`.
- Dependency namespaces: dependencies are available as `dep.<name>.*` internally; import the concrete module with its full path (aliases are not yet supported).

## Runtime module
- Executables expect a runtime module providing `__mach_main` shim. The compiler can infer a runtime like `std.runtime` when configured.
- Configure explicitly in `[runtime]` (`config.md`).

## Library builds
- When building libraries, all modules inside the package `src-dir` can be preloaded so their symbols are available for consumers.
