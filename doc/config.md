# Project Configuration (`mach.toml`)

`cmach` reads `mach.toml` to configure builds.

## Sections

### [project]
- `name`: project name
- `version`: semantic version string
- `entrypoint`: main source file within `src-dir` (omit for library-only projects)
- `target-name`: output artifact base name
- `default-target`: name of default target to build, or `"all"`

### [directories]
- `src-dir`: source directory (default `src`)
- `dep-dir`: dependency source directory (default `dep`)
- `lib-dir`: binary/object deps directory (default `lib`)
- `out-dir`: build output root (default `out`)

### [targets.<name>]
- `target`: target triple (e.g., `x86_64-pc-linux-gnu`)
- `opt-level`: 0–3 (default 2)
- `emit-ast`: bool
- `emit-ir`: bool
- `emit-asm`: bool
- `emit-object`: bool (default true)
- `build-library`: bool (library instead of executable)
- `shared`: bool (shared `.so` vs static `.a` when building library; default true)
- `no-pie`: bool (disable PIE for executables)

### [runtime]
- `runtime-path`: legacy path to runtime sources
- `runtime` / `runtime-module`: module path, e.g. `std.runtime`
- `stdlib-path`: additional stdlib search path

### [modules]
- Map short aliases to full package roots, e.g. `std = "dep.std"`.

### [deps]
Inline table per dependency:
```
[deps]
std = { path = "dep/std", src = "src", runtime = true }
```
- `path`: location of dependency
- `src` / `src-dir`: subdir containing sources (default `src`)
- `runtime`: marks this dep as a runtime provider; compiler may infer `runtime = "<dep>.runtime"`

## Derived directories
- Binaries: `out/<target-name>/bin`
- Objects: `out/<target-name>/obj`

## Module resolution
- Package root for a dependency is `path` under project dir (unless absolute).
- Module FQN: `pkg.segment1.segment2` → `<package-src-dir>/segment1/segment2.mach`.
