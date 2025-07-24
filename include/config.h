#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

// target-specific configuration
typedef struct TargetConfig
{
    char *name;          // target name (e.g., "linux", "macos", "windows")
    char *target_triple; // target architecture triple

    // build options
    int  opt_level;     // optimization level (0-3)
    bool emit_ast;      // emit AST files
    bool emit_ir;       // emit LLVM IR
    bool emit_asm;      // emit assembly
    bool emit_object;   // emit object files
    bool build_library; // build as library
    bool no_pie;        // disable PIE
} TargetConfig;

// dependency configuration
typedef struct DependencyConfig
{
    char *name; // dependency name
    char *type; // dependency type ("local", "git", etc.)
    char *path; // source path or URL
} DependencyConfig;

// project configuration
typedef struct ProjectConfig
{
    char *name;           // project name
    char *version;        // project version
    char *main_file;      // main source file (relative to src_dir)
    char *target_name;    // output executable/library name
    char *default_target; // default target name (or "all")

    // project directory structure
    char *src_dir; // source files directory
    char *dep_dir; // dependency source files directory
    char *lib_dir; // library/object dependencies directory
    char *out_dir; // output directory

    // targets (computed paths: out_dir/target_name/bin, out_dir/target_name/obj)
    TargetConfig **targets;      // array of target configurations
    int            target_count; // number of targets

    // dependencies
    DependencyConfig **dependencies;     // project dependencies with full info
    int                dep_count;        // number of dependencies
    char             **lib_dependencies; // library dependencies (binary/object files)
    int                lib_dep_count;    // number of library dependencies

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

// target management
TargetConfig *target_config_create(const char *name, const char *target_triple);
void          target_config_init(TargetConfig *target);
void          target_config_dnit(TargetConfig *target);
bool          config_add_target(ProjectConfig *config, const char *name, const char *target_triple);
TargetConfig *config_get_target(ProjectConfig *config, const char *name);
TargetConfig *config_get_target_by_triple(ProjectConfig *config, const char *target_triple);
TargetConfig *config_get_default_target(ProjectConfig *config);
char        **config_get_target_names(ProjectConfig *config);
bool          config_is_build_all_targets(ProjectConfig *config);

// configuration queries (target-specific)
bool config_has_main_file(ProjectConfig *config);
bool config_should_emit_ast(ProjectConfig *config, const char *target_name);
bool config_should_emit_ir(ProjectConfig *config, const char *target_name);
bool config_should_emit_asm(ProjectConfig *config, const char *target_name);
bool config_should_emit_object(ProjectConfig *config, const char *target_name);
bool config_should_build_library(ProjectConfig *config, const char *target_name);
bool config_should_link_executable(ProjectConfig *config, const char *target_name);

// path resolution (target-specific)
char *config_resolve_main_file(ProjectConfig *config, const char *project_dir);
char *config_resolve_src_dir(ProjectConfig *config, const char *project_dir);
char *config_resolve_dep_dir(ProjectConfig *config, const char *project_dir);
char *config_resolve_lib_dir(ProjectConfig *config, const char *project_dir);
char *config_resolve_out_dir(ProjectConfig *config, const char *project_dir);
char *config_resolve_bin_dir(ProjectConfig *config, const char *project_dir, const char *target_name);
char *config_resolve_obj_dir(ProjectConfig *config, const char *project_dir, const char *target_name);
char *config_resolve_runtime_path(ProjectConfig *config, const char *project_dir);

// dependency management
DependencyConfig *dependency_config_create(const char *name, const char *type, const char *path);
void              dependency_config_init(DependencyConfig *dep);
void              dependency_config_dnit(DependencyConfig *dep);
bool              config_add_dependency_full(ProjectConfig *config, const char *name, const char *type, const char *path);
bool              config_add_dependency(ProjectConfig *config, const char *dep_name, const char *dep_source);
bool              config_add_lib_dependency(ProjectConfig *config, const char *lib_name, const char *lib_path);
bool              config_has_dependency(ProjectConfig *config, const char *dep_name);
bool              config_remove_dependency(ProjectConfig *config, const char *dep_name);
DependencyConfig *config_get_dependency(ProjectConfig *config, const char *dep_name);
char            **config_get_dependency_paths(ProjectConfig *config, const char *project_dir);

// directory management
bool config_ensure_directories(ProjectConfig *config, const char *project_dir);

// configuration validation
bool config_validate(ProjectConfig *config);

#endif
