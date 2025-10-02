# CLI (`cmach`)

```
usage: cmach <command> [options]

commands:
  build <file> [options]     compile and optionally link a single file
  help                        show help message
```

## build options
- `-o <file>`: set output file name
- `-O<level>`: optimization level (0–3, default 2)
- `--emit-obj`: write object `.o` (skip linking)
- `--no-link`: do not link an executable
- `--no-pie`: disable PIE
- `--link <obj>`: link additional object file(s)
- `-I <dir>`: add module search directory
- `-M name=dir`: map module prefix to a base directory

Notes:
- This minimal CLI compiles a single source file; use `-I`/`-M` to resolve module imports.
- By default it links to an executable unless `--emit-obj` or `--no-link` is set.

Other commands like `init`, `run`, `examine`, and `dep` are not available in this streamlined build.
