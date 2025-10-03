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
    bool shared;        // build shared library when building a library
} TargetConfig;

// explicit dependency specification (parsed from [deps] table)
typedef struct DepSpec
{
    char *name;       // dependency/package name (key)
    char *path;       // relative or absolute path (required for now)
    char *src_dir;    // source directory inside dependency (default: src)
    bool  is_runtime; // marked as runtime provider
} DepSpec;

// project configuration
typedef struct ModuleAlias
{
    char *name;   // alias exposed to source code
    char *target; // canonical package prefix (e.g., "dep.std")
} ModuleAlias;

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

    // explicit dependencies
    DepSpec **deps;      // dependency specs (excluding root project)
    int       dep_count; // number of deps

    // runtime configuration
    char *runtime_path;   // custom runtime path (deprecated, use runtime_module)
    char *runtime_module; // runtime module path (e.g., "dep.std.runtime", "mylib.custom_runtime")
    char *stdlib_path;    // standard library path

    // module alias configuration
    ModuleAlias *module_aliases;    // alias table for module path prefixes
    int          module_alias_count;
    int          module_alias_capacity;
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
// library type queries
bool config_is_shared_library(ProjectConfig *config, const char *target_name);
// derive default output names
char *config_default_executable_name(ProjectConfig *config);
char *config_default_library_name(ProjectConfig *config, bool shared);

// path resolution (target-specific)
char *config_resolve_main_file(ProjectConfig *config, const char *project_dir);
char *config_resolve_src_dir(ProjectConfig *config, const char *project_dir);
char *config_resolve_dep_dir(ProjectConfig *config, const char *project_dir);
char *config_resolve_lib_dir(ProjectConfig *config, const char *project_dir);
char *config_resolve_out_dir(ProjectConfig *config, const char *project_dir);
char *config_resolve_bin_dir(ProjectConfig *config, const char *project_dir, const char *target_name);
char *config_resolve_obj_dir(ProjectConfig *config, const char *project_dir, const char *target_name);
char *config_resolve_runtime_path(ProjectConfig *config, const char *project_dir);

// runtime module management
bool  config_set_runtime_module(ProjectConfig *config, const char *module_path);
char *config_get_runtime_module(ProjectConfig *config);
bool  config_has_runtime_module(ProjectConfig *config);

// dependency management
DepSpec *config_get_dep(ProjectConfig *config, const char *name);
bool     config_has_dep(ProjectConfig *config, const char *name);
// resolve package root directory (root project or dependency). returns malloc'd string
char *config_resolve_package_root(ProjectConfig *config, const char *project_dir, const char *package_name);
// get package src dir (may load dependency mach.toml lazily)
char *config_get_package_src_dir(ProjectConfig *config, const char *project_dir, const char *package_name);
// load dependency config to fill missing src_dir if needed
bool config_ensure_dep_loaded(ProjectConfig *config, const char *project_dir, DepSpec *dep);

// module alias utilities
bool        config_add_module_alias(ProjectConfig *config, const char *alias, const char *target);
const char *config_get_module_alias(ProjectConfig *config, const char *alias);
// normalize module paths using alias table and dependency information. returns malloc'd canonical path or NULL on failure.
char *config_expand_module_path(ProjectConfig *config, const char *module_path);

// canonical module resolution: given FQN pkg.segment1.segment2 -> absolute file path (.mach)
char *config_resolve_module_fqn(ProjectConfig *config, const char *project_dir, const char *fqn);

// directory management
bool config_ensure_directories(ProjectConfig *config, const char *project_dir);

// configuration validation
bool config_validate(ProjectConfig *config);

#endif
