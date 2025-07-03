#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

// project configuration similar to go.mod
typedef struct ProjectConfig
{
    char *name;        // project name
    char *version;     // project version
    char *main_file;   // main source file (relative to src_dir)
    char *target_name; // output executable/library name

    // project directory structure
    char *src_dir; // source files directory
    char *dep_dir; // dependency source files directory
    char *lib_dir; // library/object dependencies directory
    char *out_dir; // output directory
    char *bin_dir; // binary output directory (relative to out_dir)
    char *obj_dir; // object file directory (relative to out_dir)

    // build options
    int  opt_level;     // optimization level (0-3)
    bool emit_ast;      // emit AST files
    bool emit_ir;       // emit LLVM IR
    bool emit_asm;      // emit assembly
    bool emit_object;   // emit object files
    bool build_library; // build as library
    bool no_pie;        // disable PIE

    // target configuration
    char *target_triple; // target architecture triple

    // dependencies
    char **dependencies; // project dependencies
    int    dep_count;    // number of dependencies

    // runtime configuration
    char *runtime_path; // custom runtime path
    char *stdlib_path;  // standard library path
} ProjectConfig;

// configuration file management
ProjectConfig *config_load(const char *config_path);
ProjectConfig *config_load_from_dir(const char *dir_path);
bool           config_save(ProjectConfig *config, const char *config_path);
ProjectConfig *config_create_default(const char *project_name);

// configuration lifecycle
void config_init(ProjectConfig *config);
void config_dnit(ProjectConfig *config);

// configuration queries
bool config_has_main_file(ProjectConfig *config);
bool config_should_emit_ast(ProjectConfig *config);
bool config_should_emit_ir(ProjectConfig *config);
bool config_should_emit_asm(ProjectConfig *config);
bool config_should_emit_object(ProjectConfig *config);
bool config_should_build_library(ProjectConfig *config);
bool config_should_link_executable(ProjectConfig *config);

// path resolution
char *config_resolve_main_file(ProjectConfig *config, const char *project_dir);
char *config_resolve_src_dir(ProjectConfig *config, const char *project_dir);
char *config_resolve_dep_dir(ProjectConfig *config, const char *project_dir);
char *config_resolve_lib_dir(ProjectConfig *config, const char *project_dir);
char *config_resolve_out_dir(ProjectConfig *config, const char *project_dir);
char *config_resolve_bin_dir(ProjectConfig *config, const char *project_dir);
char *config_resolve_obj_dir(ProjectConfig *config, const char *project_dir);
char *config_resolve_runtime_path(ProjectConfig *config, const char *project_dir);

// directory management
bool config_ensure_directories(ProjectConfig *config, const char *project_dir);

// configuration validation
bool config_validate(ProjectConfig *config);

#endif
