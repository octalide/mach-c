# Statements

Mach statements form the control-flow backbone of the language. This chapter describes each statement recognised by the parser and the rules enforced during semantic analysis.

## Blocks

```
{
    // zero or more statements
}
```

- Entering a block pushes a new scope; leaving it pops the scope (all locals are destroyed).
- Blocks are used for function bodies, loop bodies, and conditional branches.

## Variable declarations inside blocks

`val`/`var` statements behave the same inside a block as at the top level (see [Declarations](./declarations-and-modules.md)). The only difference is the scope in which the symbol lives.

```
var counter: i32 = 0;
val limit: i32 = 128;
```

## Expression statements

Any expression followed by a semicolon becomes a statement:

```
call_something();
ptr = ?value;
```

The expression is evaluated for its side effects; the resulting value is discarded.

## `if` / `or`

```
if (condition) {
    // body
} or (elif_condition) {
    // optional branch with its own condition
} or {
    // optional catch-all (acts like "else")
}
```

- The initial `if` requires a parenthesised condition and a block body.
- Each `or` branch may optionally include a parenthesised condition. If omitted, the branch acts as a final `else`.
- Branch conditions are analysed in order. Once a branch executes, subsequent branches are skipped.
- Every branch introduces a new scope.
- Functions with non-void return types must ensure that all `if`/`or` paths return a value. The semantic analyser checks this by walking the chain.

## `for`

```
for {
    // infinite loop
}

for (condition) {
    // loop with condition checked before each iteration
}
```

- Mach supports while-style loops only. Loop initialisers and increment expressions are not part of the syntax.
- The condition (if present) must be enclosed in parentheses. When omitted, the loop is infinite until a `brk` executes.
- Inside the loop, the analyser tracks loop depth to validate `brk` and `cnt` usage.

## `brk` and `cnt`

```
brk;
cnt;
```

- `brk;` exits the nearest enclosing loop.
- `cnt;` continues to the next iteration of the nearest enclosing loop.
- Using either statement outside a loop triggers a semantic error.

## `ret`

```
ret value;
ret;
```

- `ret` ends the current function. With a value, the expression is type-checked against the function’s declared return type.
- Functions without a declared return type must use `ret;` with no value. Returning a value from such a function is an error.
- The analyser verifies that every code path in a non-void function ends with a `ret`.

## Inline assembly: `asm`

```
asm {
    mov rax, rdi
    call rsi
}
```

- Available both inside blocks and at the top level.
- The parser copies the raw text between the braces into the AST, trimming leading and trailing whitespace.
- No validation is performed; the emitted code is spliced directly into the generated LLVM IR. Use with care.

## Statement recovery

When the parser encounters an error inside a statement, it enters panic mode and synchronises at the next safe point (typically a semicolon or the start of a new declaration). This behaviour keeps error cascades manageable when editing code.
