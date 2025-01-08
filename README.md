# Mach Compiler

This is the bootstrap compiler for the Mach programming language, written in C.

To see the documentation and implementation of the standard library, see the [Mach repository](https://github.com/octalide/mach).

# WIP:

## General Checklist

- [ ] Array initialization (how did I miss this?)

## Semantic Analysis Checklist

- [ ] Build symbol table
  - [ ] Fill symbol table types
  - [ ] Fill any lazy types with additional passes
- [ ] Check type compatability
- [ ] Check all paths retur in non-void functions
- [ ] Validate control flow keyword usage
- [ ] Check for usage of uninitialized variables (?)
