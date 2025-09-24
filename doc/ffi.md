# FFI (External Symbols)

Declare external functions with `ext`.

## Syntax
```
ext "C:printf" printf: fun(*u8) i64;
```
- Optional string prefix: `"CONV:SYMBOL"`
  - `CONV` — calling convention; default `"C"`.
  - `SYMBOL` — linker symbol; defaults to the declared name.
- Then the public name, a colon, and the function type.

Examples:
```
ext "C:malloc"  malloc:  fun(i64) *u8;
ext "C:free"    free:    fun(*u8);
ext "C:realloc" realloc: fun(*u8, i64) *u8;
```

Use like any function after import:
```
use C: std.c; // module defines the above externs

fun demo(p: *u8) {
    C.free(p);
}
```
