You MUST follow ALL guidelines under all circumstances.

General guidelines:
- Keep code clean, pruned, simple, and effective.
- Remove unnecessary comments and code.
- Only include error logging. Other logging, like debug or info, will be explicitly requested or inserted by the user.
- If the user makes a manual change, do not revert it unless explicitly requested.
- Prioritize modifying existing code over creating new code.
- Follow existing code styles and patterns.
- Follow instructions carefully.
- Do not attempt to execute code to test it. If you need to test code, ask the user to do so.
- Do NOT, under any circumstances, leave written code unimplemented.
- Brief, single-line comments should be `// lower case`

Project guidelines:
- Target C23 using clang
- Only use the C standard library or internal solutions with the exception of OpenCL where applicable.
- Standard `Makefile`s are the only acceptable build system (NO CMake).
- Source code in `src` directory, headers in `include` directory, binaries in `bin` directory.
