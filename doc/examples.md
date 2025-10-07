# Examples

## Hello world
```
use std.io.console;

pub fun main() u32 {
    print("Hello, world!\n");
    ret 0;
}
```

## Types and variables
```
def bool: u8;
val true: bool = 1;
val false: bool = 0;

var x: i32 = 42;
var p: *i32 = ?x;
var y: i32 = @p;
var dangling: *i32 = nil; // sugar for ?0
```

## Arrays and strings
```
def char: u8;
def string: []char;

val s: string = "hi";
val a: [3]i32{ 1, 2, 3 };
val b: []i32{ 4, 5 };
val n: u64 = len(a);
```

## Structs and unions
```
str Point { x: f64; y: f64; }

pub fun length(p: Point) f64 {
    ret (p.x*p.x + p.y*p.y) :: f64;
}
```

## Control flow
```
fun countdown(n: i32) {
    for (n > 0) {
        n = n - 1;
        if (n == 2) { cnt; }
        if (n == 1) { brk; }
    }
}
```

## Inline assembly
```
fun spin_once() {
    asm pause;
    # emits a single x86 pause instruction
}
```

## External functions
```
ext "C:write" write: fun(i64, *u8, i64) i64;
```
