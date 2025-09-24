# CLI (`cmach`)

```
usage: cmach <command> [options]

commands:
  init                       initialize a new project
  build [file/dir] [options] build project or single file
  examine <file.mach>        parse + analyze a source file
  run                        build then execute project
  clean                      remove build artifacts
  dep <subcommand>           dependency management (info only)
  help [command]             show help message
```

## build options
- `-o <file>`: set output file name
- `-O<level>`: optimization level (0–3, default 2)
- `--lib`: build as library (shared by default)
- `--emit-ast`: write `.ast`
- `--emit-ir`: write LLVM IR `.ll`
- `--emit-asm`: write assembly `.s`
- `--emit-obj`: write object `.o` (skip linking)
- `--no-link`: do not link an executable
- `--no-pie`: disable PIE
- `--link <obj>`: link additional object file(s)
- `--no-runtime`: skip linking runtime support

Notes:
- When building a project directory, output is placed under `out/<target>/bin` and `obj`.
- Library builds can produce `.so` (shared) or `.a` (static) depending on target `shared`.

## init
Creates a new project scaffold, optionally using `MACH_STD` to seed a `std` dependency.

## run
Builds the project then runs the produced executable.

## dep
The `dep add/remove` commands are informational; dependencies are managed as git submodules and declared in `[deps]`.
