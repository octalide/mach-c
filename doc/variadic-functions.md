# Variadic Functions in Mach

## Overview

Mach supports variadic functions using a clean, array-like syntax that provides bounds safety while maintaining full C FFI compatibility. Unlike traditional C variadic functions, Mach's approach treats variadic arguments as a named, indexable parameter.

## Syntax

```mach
fun function_name(param1: type1, param2: type2, args: ...) return_type {
    # Access variadic arguments using array-like syntax
    val count: u32 = len(args);           # Get argument count
    val first_arg: T = args[0]::T;        # Extract argument with explicit cast
    # ... function body
}
```

## Key Features

### 1. Named Variadic Parameter
- Variadic arguments are accessed through a named parameter (e.g., `args: ...`)
- No anonymous `...` - always use a parameter name

### 2. Array-like Access
- `len(args)` returns the number of variadic arguments
- `args[i]::T` extracts argument at index `i` with explicit type cast
- Bounds checking prevents accessing `args[len(args)]` or beyond

### 3. Explicit Type Casting
- All variadic argument access requires explicit type casting using `::`
- Consistent with Mach's explicit casting philosophy
- Example: `args[0]::i32` extracts first argument as i32

### 4. C FFI Compatibility
- External C functions use traditional `...` syntax: `ext printf: fun(*u8, ...) i32;`
- Direct calls to C variadic functions work without conversion
- Mach variadic functions can forward arguments to C functions

## Implementation Strategy

### LLVM Backend
- Uses LLVM's `@llvm.va_start`, `@llvm.va_arg`, `@llvm.va_end` intrinsics
- `len(args)` compiles to argument count tracking
- `args[i]::T` compiles to bounds-checked `va_arg` extraction
- Zero overhead for external C function calls

### Memory Layout
```llvm
; Function signature
define i32 @my_function(i32 %param, ...) {
  ; Initialize va_list
  %va_list = alloca i8*, align 8
  call void @llvm.va_start(i8* %va_list)
  
  ; args[i]::T becomes optimized va_arg calls
  %arg_val = va_arg i8** %va_list, i32
  
  ; Cleanup
  call void @llvm.va_end(i8* %va_list)
}
```

### Bounds Checking
- Compile-time optimization eliminates bounds checks when safe
- Runtime bounds checking for dynamic access patterns
- Panic on out-of-bounds access (configurable behavior)

## Examples

### Basic Integer Sum
```mach
fun sum_ints(count: i32, args: ...) i32 {
    var total: i32 = 0;
    var i: u32 = 0;
    
    for (i < len(args)) {
        total = total + args[i]::i32;
        i = i + 1;
    }
    
    ret total;
}

# Usage
val result: i32 = sum_ints(3, 10, 20, 30);
```

### Printf-style Formatting
```mach
fun debug_log(level: i32, format: [_]u8, args: ...) void {
    if (level > 0) {
        var arg_index: u32 = 0;
        var i: u32 = 0;
        
        for (i < len(format)) {
            if (format[i] == '%' && i + 1 < len(format)) {
                i = i + 1;
                if (format[i] == 'd' && arg_index < len(args)) {
                    var value: i32 = args[arg_index]::i32;
                    # Process integer
                    arg_index = arg_index + 1;
                } or (format[i] == 's' && arg_index < len(args)) {
                    var value: [_]u8 = args[arg_index]::[_]u8;
                    # Process string
                    arg_index = arg_index + 1;
                }
            }
            i = i + 1;
        }
    }
}

# Usage
debug_log(1, "User %s has %d points", "Alice", 100);
```

### C FFI Integration
```mach
# External C function
ext printf: fun(*u8, ...) i32;

# Mach wrapper function
fun mach_printf(format: [_]u8, args: ...) i32 {
    # Convert Mach string to C string
    var c_format: *u8 = to_cstring(format);
    
    # Forward to C printf (compiler magic for argument forwarding)
    ret printf(c_format, args...);
}
```

## Function Type Definitions
```mach
# Variadic function types
def logger: fun([_]u8, ...) void;
def formatter: fun([_]u8, ...) [_]u8;

# Usage in callbacks
var my_logger: logger = debug_log;
```

## Advantages

1. **Type Safety**: Explicit casting makes type errors visible
2. **Bounds Safety**: Array-like access prevents buffer overruns
3. **Familiar Syntax**: Uses existing array indexing conventions
4. **C Compatibility**: Zero-cost interop with C variadic functions
5. **Performance**: LLVM optimizations eliminate overhead
6. **Readability**: Named parameters make code more self-documenting

## Implementation Notes

### Phase 1: Basic Support
- Implement `args: ...` parameter syntax
- Support `len(args)` and `args[i]::T` operations
- LLVM backend with va_list intrinsics
- Basic bounds checking

### Phase 2: Optimization
- Compile-time argument count when possible
- Eliminate bounds checks for provably safe access
- Optimize common patterns (e.g., simple loops)

### Phase 3: Advanced Features
- Argument forwarding syntax (`args...`)
- Type information preservation for better diagnostics
- Integration with Mach's type reflection system

## Related Features

- **External Functions**: `ext` keyword for C function declarations
- **Explicit Casting**: `::` operator for type reinterpretation
- **Array Types**: `[_]T` for dynamic arrays, `[N]T` for fixed arrays
- **Function Types**: Support for variadic function type definitions
