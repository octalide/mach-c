# Mach Compiler

This is the bootstrap compiler for the Mach programming language, written in C.

## Language Definition

Below is the description of the entire syntax of the Mach language.

### Keywords

#### Builtin Type Keywords

| type     | description                  |
| -------- | ---------------------------- |
| `i8`     | 8-bit signed integer         |
| `i16`    | 16-bit signed integer        |
| `i32`    | 32-bit signed integer        |
| `i64`    | 64-bit signed integer        |
| `u8`     | 8-bit unsigned integer       |
| `u16`    | 16-bit unsigned integer      |
| `u32`    | 32-bit unsigned integer      |
| `u64`    | 64-bit unsigned integer      |
| `f32`    | 32-bit floating point number |
| `f64`    | 64-bit floating point number |

#### Decleration Keywords

| keyword | usage                                                                  |
| ------- | ---------------------------------------------------------------------- |
| `val`   | declares a constant variable, equivalent to `const` in other languages |
| `var`   | declares a mutable variable                                            |
| `ptr`   | declares a pointer type                                                |
| `fun`   | declares a function                                                    |
| `str`   | declares a structure                                                   |

Decleration keyword usage:

```mach
<keyword> <identifier> [: <signature>] [= <expression>]
```

Examples:

```mach
val x: i32 = 10;
var y: i32 = 20;
ptr z: i32 = 30; # `z` is a pointer to an `i32` with an initial value of 30
fun adder(val a: int, val b: int): i32;
str foo: {
    val add: adder
    var count: i32
}

fun add(val a: int, val b: int): i32 {
    val bar: foo = {
        add:   baz,
        count: 0
    }

    ret 0;
}
```

#### Control Flow Keywords

| keyword | usage                                                             |
| ------- | ----------------------------------------------------------------- |
| `if`    | if statement                                                      |
| `or`    | or statement (synonymous with if else or else in other languages) |
| `for`   | for loop                                                          |
| `end`   | end statement (equal to break in other languages)                 |
| `cnt`   | continue statement                                                |
| `ret`   | return statement                                                  |

Control flow keyword usage:

```mach
<keyword> <expression>
```

Examples:

```mach
if x == 10 {
    ret x;
}
or x == 20 {
    ret x;
}
or {
    ret 0;
}
```

```mach
for var i: i32 = 0; i < 10; i += 1 {
    # do something with i
}
```

```mach
end
cnt
ret 0
```

### Operators

| operator | description               |
| -------- | ------------------------- |
| `.`      | member access             |
| `:`      | type annotation           |
| `->`     | function return type      |
| `=`      | assignment                |
| `==`     | equality                  |
| `!=`     | inequality                |
| `>`      | greater than              |
| `>=`     | greater than or equal to  |
| `<`      | less than                 |
| `<=`     | less than or equal to     |
| `+`      | addition                  |
| `-`      | subtraction               |
| `*`      | multiplication            |
| `/`      | division                  |
| `%`      | modulo                    |
| `&`      | bitwise AND               |
| `\|`     | bitwise OR                |
| `^`      | bitwise XOR               |
| `!`      | logical NOT               |
| `&&`     | logical AND               |
| `\|\|`   | logical OR                |
| `~`      | bitwise NOT               |
| `<<`     | left shift                |
| `>>`     | right shift               |
| `+=`     | addition assignment       |
| `-=`     | subtraction assignment    |
| `*=`     | multiplication assignment |
| `/=`     | division assignment       |
| `%=`     | modulo assignment         |
| `&=`     | bitwise AND assignment    |
| `\|=`    | bitwise OR assignment     |
| `^=`     | bitwise XOR assignment    |
| `<<=`    | left shift assignment     |
| `>>=`    | right shift assignment    |
| `?`      | reference operator        |

Examples:

```mach
val x: i32 = 10;        # assignment
val y: i32 = 20;        # assignment
val z: i32 = x + y;     # addition
val a: i32 = x - y;     # subtraction
val b: i32 = x * y;     # multiplication
val c: i32 = x / y;     # division
val d: i32 = x % y;     # modulo
val e: i32 = x & y;     # bitwise AND
val f: i32 = x | y;     # bitwise OR
val g: i32 = x ^ y;     # bitwise XOR
val h: i32 = ~x;        # bitwise NOT
val i: i32 = x << y;    # left shift
val j: i32 = x >> y;    # right shift
x += y;                 # addition assignment
x -= y;                 # subtraction assignment
x *= y;                 # multiplication assignment
x /= y;                 # division assignment
x %= y;                 # modulo assignment
x &= y;                 # bitwise AND assignment
x |= y;                 # bitwise OR assignment
x ^= y;                 # bitwise XOR assignment
x <<= y;                # left shift assignment
x >>= y;                # right shift assignment

ptr p: i32 = 0  # pointer to an `i32` with the initial value `0`
ptr q: i32 = ?x # pointer to `x`
```

### Pointers

Pointers in Mach are a bit different than in other languages. They are a type
that can be assigned to a value or another pointer. They are created using the
`ptr` keyword.

Syntax:
```
ptr <identifier>: <type> = <expression>
```

Example pointer decleration:
```mach
ptr p: i32 = 0;  # pointer to an `i32` with the initial value `0`
ptr q: i32 = ?x; # pointer to `x`
```

Variables can be converted into a pointer value using the `?` operator.
The main difference between Mach and other languages lies in the fact that `ptr`
is its own type and is not just a modifier.

While `ptr` is its own type and has its own properties and methods to be used
for operations like pointer arithmetic, it can be directly used as if it were
the type it points to. For example:

```mach
ptr p: i32 = 1;      # pointer to an `i32` with the initial value `1`
val x: i32 = 2;      # integer with the value `2`
val y: i32 = p + x;  # `y` is `3`
```

In addition to this, pointers have the following builtin properties:
- `address`: the memory address of the pointer
- `value`: the dereferenced value of the pointer
- `type`: the type of the pointer

Example:
```mach
ptr p: i32 = 1;          # pointer to an `i32` with the initial value `1`
val x: i32 = p.value;    # `x` is `1`
val y: i32 = p.address;  # `y` is the memory address of `p`
val t: type = p.type;    # `t` is the type of `p`
```

## Example Code

Typical "Hello World" program:

```mach
use std;

str message: {
    var text: string
}

fun message.print() {
    print(this.text);
}

fun main: (args: *[]string) -> i32 {
    val msg: message = {
        text = "Hello World!"
    }

    msg.print();

    print("args:")
    for var i: i32 = 0; i < len(args); i += 1 {
        print(args[0]);
    }

    ret 0;
}
```
