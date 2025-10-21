// minimal, config-free build entrypoint
#include "commands.h"
#include "ast.h"
#include "codegen.h"
#include "config.h"
#include "lexer.h"
#include "module.h"
#include "parser.h"
#include "preprocessor.h"
#include "semantic_new.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct
{
    char **items;
    int    count;
    int    cap;
} StringVec;

typedef struct
{
    char **names;
    char **dirs;
    int    count;
    int    cap;
} AliasVec;

static char *derive_module_name_from_aliases(const char *filename, const AliasVec *aliases)
{
    if (!filename || !aliases || aliases->count == 0)
        return NULL;

    char *abs_file = realpath(filename, NULL);
    if (!abs_file)
        return NULL;

    char *module_name = NULL;
    for (int i = 0; i < aliases->count && !module_name; i++)
    {
        char *abs_dir = realpath(aliases->dirs[i], NULL);
        if (!abs_dir)
            continue;

        size_t dir_len  = strlen(abs_dir);
        size_t file_len = strlen(abs_file);
        if (file_len >= dir_len && strncmp(abs_file, abs_dir, dir_len) == 0 && (abs_file[dir_len] == '/' || abs_file[dir_len] == '\\' || abs_file[dir_len] == '\0'))
        {
            const char *rel = abs_file + dir_len;
            if (*rel == '/' || *rel == '\\')
                rel++;

            size_t rel_len = strlen(rel);
            if (rel_len >= 5 && strcmp(rel + rel_len - 5, ".mach") == 0)
                rel_len -= 5;

            size_t alias_len = strlen(aliases->names[i]);
            size_t total_len = alias_len + (rel_len > 0 ? (1 + rel_len) : 0);
            module_name      = malloc(total_len + 1);
            if (module_name)
            {
                memcpy(module_name, aliases->names[i], alias_len);
                size_t pos = alias_len;
                if (rel_len > 0)
                {
                    module_name[pos++] = '.';
                    for (size_t j = 0; j < rel_len; j++)
                    {
                        char c = rel[j];
                        if (c == '/' || c == '\\')
                            c = '.';
                        module_name[pos++] = c;
                    }
                }
                module_name[pos] = '\0';
            }
        }

        free(abs_dir);
    }

    free(abs_file);
    return module_name;
}

static char *read_file_simple(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return NULL;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (!buf)
    {
        fclose(file);
        return NULL;
    }
    size_t n = fread(buf, 1, size, file);
    buf[n]   = '\0';
    fclose(file);
    return buf;
}

static int file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static char *dirname_dup(const char *path)
{
    const char *slash = strrchr(path, '/');
    if (!slash)
        return strdup(".");
    size_t len = (size_t)(slash - path);
    char  *out = malloc(len + 1);
    memcpy(out, path, len);
    out[len] = '\0';
    return out;
}

static char *find_project_root(const char *start_path)
{
    char resolved[PATH_MAX];
    if (realpath(start_path, resolved))
    {
        struct stat st;
        if (stat(resolved, &st) != 0)
            return strdup(".");

        if (!S_ISDIR(st.st_mode))
        {
            char *slash = strrchr(resolved, '/');
            if (slash)
                *slash = '\0';
        }

        if (resolved[0] == '\0')
            strcpy(resolved, "/");

        char *dir = strdup(resolved);
        if (!dir)
            return NULL;

        for (int depth = 0; depth < 64; depth++)
        {
            char cfg[PATH_MAX];
            snprintf(cfg, sizeof(cfg), "%s/mach.toml", dir);
            if (file_exists(cfg))
                return dir;

            if (strcmp(dir, "/") == 0 || dir[0] == '\0')
                break;

            char *slash = strrchr(dir, '/');
            if (!slash)
            {
                dir[0] = '\0';
            }
            else if (slash == dir)
            {
                slash[1] = '\0';
            }
            else
            {
                *slash = '\0';
            }
        }

        free(dir);
        return strdup(".");
    }

    // fallback to relative walk if realpath fails
    char *dir = dirname_dup(start_path);
    for (int i = 0; i < 16 && dir; i++)
    {
        char cfg[1024];
        snprintf(cfg, sizeof(cfg), "%s/mach.toml", dir);
        if (file_exists(cfg))
            return dir;
        const char *slash = strrchr(dir, '/');
        if (!slash)
        {
            free(dir);
            return strdup(".");
        }
        if (slash == dir)
        {
            dir[1] = '\0';
            return dir;
        }
        size_t nlen = (size_t)(slash - dir);
        char  *up   = malloc(nlen + 1);
        memcpy(up, dir, nlen);
        up[nlen] = '\0';
        free(dir);
        dir = up;
    }
    return dir;
}

static void ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return;
    mkdir(path, 0777);
}

static char *get_base_filename_only(const char *path)
{
    const char *last_slash = strrchr(path, '/');
    const char *filename   = last_slash ? last_slash + 1 : path;
    const char *last_dot   = strrchr(filename, '.');
    if (!last_dot)
        return strdup(filename);
    size_t len  = (size_t)(last_dot - filename);
    char  *base = malloc(len + 1);
    strncpy(base, filename, len);
    base[len] = '\0';
    return base;
}

static char *create_out_name(const char *input_file, const char *ext)
{
    char  *base = get_base_filename_only(input_file);
    size_t len  = strlen(base) + strlen(ext) + 1;
    char  *out  = malloc(len);
    snprintf(out, len, "%s%s", base, ext);
    free(base);
    return out;
}

void mach_print_usage(const char *program_name)
{
    fprintf(stderr, "usage: %s build <file> [options]\n", program_name);
    fprintf(stderr, "options:\n");
    fprintf(stderr, "  -o <file>     set output file name\n");
    fprintf(stderr, "  -O<level>     optimization level (0-3, default: 2)\n");
    fprintf(stderr, "  --emit-obj    emit object file (.o file)\n");
    fprintf(stderr, "  --emit-ast[=<file>]  dump parsed AST for debugging\n");
    fprintf(stderr, "  --emit-ir[=<file>]   dump LLVM IR\n");
    fprintf(stderr, "  --emit-asm[=<file>]  dump target assembly\n");
    fprintf(stderr, "  --no-link     don't create executable (just compile)\n");
    fprintf(stderr, "  --no-pie      disable position independent executable\n");
    fprintf(stderr, "  --link <obj>  link with additional object file\n");
    fprintf(stderr, "  -g, --debug   include debug info (default)\n");
    fprintf(stderr, "  --no-debug    disable debug info\n");
    fprintf(stderr, "  -I <dir>      add module search directory\n");
    fprintf(stderr, "  -M n=dir      map module prefix 'n' to base directory 'dir'\n");
}

int mach_cmd_build(int argc, char **argv)
{
    if (argc < 3)
    {
        mach_print_usage(argv[0]);
        return 1;
    }

    const char *filename      = argv[2];
    const char *output_file   = NULL;
    int         opt_level     = 2;
    int         link_exe      = 1;
    int         no_pie        = 0;
    int         debug_info    = 1;
    int         emit_ast      = 0;
    int         emit_ir       = 0;
    int         emit_asm      = 0;
    const char *emit_ast_path = NULL;
    const char *emit_ir_path  = NULL;
    const char *emit_asm_path = NULL;

    StringVec inc       = (StringVec){0};
    StringVec link_objs = (StringVec){0};
    AliasVec  al        = (AliasVec){0};
#define VPUSH(v, s)                                            \
    do                                                         \
    {                                                          \
        if ((v).count == (v).cap)                              \
        {                                                      \
            int   n  = (v).cap ? (v).cap * 2 : 4;              \
            void *nn = realloc((v).items, n * sizeof(char *)); \
            if (!nn)                                           \
            {                                                  \
                fprintf(stderr, "error: oom\n");               \
                return 1;                                      \
            }                                                  \
            (v).items = nn;                                    \
            (v).cap   = n;                                     \
        }                                                      \
        (v).items[(v).count++] = (s);                          \
    } while (0)
#define APUSH(v, n, d)                                               \
    do                                                               \
    {                                                                \
        if ((v).count == (v).cap)                                    \
        {                                                            \
            int    ncap = (v).cap ? (v).cap * 2 : 4;                 \
            char **nn   = realloc((v).names, ncap * sizeof(char *)); \
            char **nd   = realloc((v).dirs, ncap * sizeof(char *));  \
            if (!nn || !nd)                                          \
            {                                                        \
                fprintf(stderr, "error: oom\n");                     \
                return 1;                                            \
            }                                                        \
            (v).names = nn;                                          \
            (v).dirs  = nd;                                          \
            (v).cap   = ncap;                                        \
        }                                                            \
        (v).names[(v).count] = (n);                                  \
        (v).dirs[(v).count]  = (d);                                  \
        (v).count++;                                                 \
    } while (0)

    for (int i = 3; i < argc; i++)
    {
        if (strcmp(argv[i], "-o") == 0)
        {
            if (i + 1 < argc)
                output_file = argv[++i];
            else
            {
                fprintf(stderr, "error: -o requires a filename\n");
                return 1;
            }
        }
        else if (strncmp(argv[i], "-O", 2) == 0)
        {
            opt_level = atoi(argv[i] + 2);
            if (opt_level < 0 || opt_level > 3)
            {
                fprintf(stderr, "error: invalid optimization level\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "--emit-obj") == 0)
        {
            link_exe = 0;
        }
        else if (strcmp(argv[i], "--no-link") == 0)
        {
            link_exe = 0;
        }
        else if (strncmp(argv[i], "--emit-ast", 10) == 0)
        {
            emit_ast = 1;
            if (argv[i][10] == '=' && argv[i][11] != '\0')
                emit_ast_path = argv[i] + 11;
        }
        else if (strncmp(argv[i], "--emit-ir", 9) == 0)
        {
            emit_ir = 1;
            if (argv[i][9] == '=' && argv[i][10] != '\0')
                emit_ir_path = argv[i] + 10;
        }
        else if (strncmp(argv[i], "--emit-asm", 10) == 0)
        {
            emit_asm = 1;
            if (argv[i][10] == '=' && argv[i][11] != '\0')
                emit_asm_path = argv[i] + 11;
        }
        else if (strcmp(argv[i], "--no-pie") == 0)
        {
            no_pie = 1;
        }
        else if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--debug") == 0)
        {
            debug_info = 1;
        }
        else if (strcmp(argv[i], "--no-debug") == 0)
        {
            debug_info = 0;
        }
        else if (strcmp(argv[i], "--link") == 0)
        {
            if (i + 1 < argc)
            {
                VPUSH(link_objs, argv[++i]);
            }
            else
            {
                fprintf(stderr, "error: --link requires an object file\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-I") == 0)
        {
            if (i + 1 < argc)
                VPUSH(inc, argv[++i]);
            else
            {
                fprintf(stderr, "error: -I requires a directory\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-M") == 0)
        {
            if (i + 1 < argc)
            {
                char *a  = argv[++i];
                char *eq = strchr(a, '=');
                if (!eq)
                {
                    fprintf(stderr, "error: -M expects name=dir\n");
                    return 1;
                }
                *eq = '\0';
                APUSH(al, a, eq + 1);
            }
            else
            {
                fprintf(stderr, "error: -M requires name=dir\n");
                return 1;
            }
        }
        else
        {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    char *source = read_file_simple(filename);
    if (!source)
    {
        fprintf(stderr, "error: could not read '%s'\n", filename);
        return 1;
    }

    SemanticDriver *driver = semantic_driver_create();

    // find project root for config-based dependency resolution
    char *project_dir_root = find_project_root(filename);

    // try to load project config (mach.toml) to enable long-term dependency resolution
    ProjectConfig *cfg = config_load_from_dir(project_dir_root);
    if (cfg)
    {
        module_manager_set_config(&driver->module_manager, cfg, project_dir_root);
    }
    for (int i = 0; i < inc.count; i++)
        module_manager_add_search_path(&driver->module_manager, inc.items[i]);
    for (int i = 0; i < al.count; i++)
        module_manager_add_alias(&driver->module_manager, al.names[i], al.dirs[i]);

    PreprocessorConstant constants[32];
    size_t               constant_count = module_manager_collect_constants(&driver->module_manager, constants, sizeof(constants) / sizeof(constants[0]));

    PreprocessorOutput pp_output;
    preprocessor_output_init(&pp_output);
    if (!preprocessor_run(source, constants, constant_count, &pp_output))
    {
        fprintf(stderr, "#! directive error in '%s' line %d: %s\n", filename, pp_output.line, pp_output.message ? pp_output.message : "unknown");
        preprocessor_output_dnit(&pp_output);
        if (cfg)
        {
            config_dnit(cfg);
            free(cfg);
        }
        semantic_driver_destroy(driver);
        free(project_dir_root);
        for (int i = 0; i < inc.count; i++)
            free(inc.items[i]);
        free(inc.items);
        for (int i = 0; i < al.count; i++)
        {
            free(al.names[i]);
            free(al.dirs[i]);
        }
        free(al.names);
        free(al.dirs);
        free(source);
        return 1;
    }

    free(source);
    source           = pp_output.source;
    pp_output.source = NULL;
    preprocessor_output_dnit(&pp_output);

    Lexer lx;
    lexer_init(&lx, source);
    Parser ps;
    parser_init(&ps, &lx);
    AstNode *prog = parser_parse_program(&ps);
    if (ps.had_error)
    {
        fprintf(stderr, "parsing failed with %d error(s):\n", ps.errors.count);
        parser_error_list_print(&ps.errors, &lx, filename);
        parser_dnit(&ps);
        lexer_dnit(&lx);
        free(source);
        if (cfg)
        {
            config_dnit(cfg);
            free(cfg);
        }
        semantic_driver_destroy(driver);
        free(project_dir_root);
        for (int i = 0; i < inc.count; i++)
            free(inc.items[i]);
        free(inc.items);
        for (int i = 0; i < al.count; i++)
        {
            free(al.names[i]);
            free(al.dirs[i]);
        }
        free(al.names);
        free(al.dirs);
        return 1;
    }

    char *module_name_override = derive_module_name_from_aliases(filename, &al);

    const char *semantic_module_name = module_name_override ? module_name_override : filename;

    if (!semantic_driver_analyze(driver, prog, semantic_module_name, filename))
    {
        // diagnostics are already printed by semantic_driver_analyze
        if (driver->module_manager.had_error)
        {
            fprintf(stderr, "module loading failed with %d error(s):\n", driver->module_manager.errors.count);
            module_error_list_print(&driver->module_manager.errors);
        }
        if (cfg)
        {
            config_dnit(cfg);
            free(cfg);
        }
        free(module_name_override);
        semantic_driver_destroy(driver);
        ast_node_dnit(prog);
        free(prog);
        parser_dnit(&ps);
        lexer_dnit(&lx);
        free(source);
        return 1;
    }

    const char *config_target_name = NULL;
    if (cfg)
    {
        TargetConfig *def_target = config_get_default_target(cfg);
        if (def_target)
            config_target_name = def_target->name;
    }
    if (cfg && config_target_name)
    {
        if (config_should_emit_ast(cfg, config_target_name))
            emit_ast = 1;
        if (config_should_emit_ir(cfg, config_target_name))
            emit_ir = 1;
        if (config_should_emit_asm(cfg, config_target_name))
            emit_asm = 1;
    }

    char *auto_ast = NULL;
    if (emit_ast)
    {
        const char *ast_path = emit_ast_path;
        if (!ast_path || ast_path[0] == '\0')
        {
            auto_ast = create_out_name(filename, ".ast");
            ast_path = auto_ast;
        }
        if (!ast_emit(prog, ast_path))
        {
            fprintf(stderr, "error: failed to emit ast file '%s'\n", ast_path);
        }
    }

    CodegenContext cg;
    codegen_context_init(&cg, semantic_module_name, no_pie);
    cg.opt_level    = opt_level;
    cg.debug_info   = debug_info;
    cg.source_file  = filename;
    cg.source_lexer = &lx;

    if (!codegen_generate(&cg, prog, driver))
    {
        fprintf(stderr, "code generation failed:\n");
        codegen_print_errors(&cg);
        codegen_context_dnit(&cg);
        if (cfg)
        {
            config_dnit(cfg);
            free(cfg);
        }
        free(module_name_override);
        semantic_driver_destroy(driver);
        ast_node_dnit(prog);
        free(prog);
        parser_dnit(&ps);
        lexer_dnit(&lx);
        free(source);
        free(auto_ast);
        return 1;
    }

    const char *obj_file = NULL;
    char       *auto_obj = NULL;
    char       *auto_ir  = NULL;
    char       *auto_asm = NULL;
    if (!output_file)
    {
        auto_obj = create_out_name(filename, ".o");
        obj_file = auto_obj;
    }
    else
        obj_file = output_file;
    if (!codegen_emit_object(&cg, obj_file))
    {
        fprintf(stderr, "error: failed to write object file '%s'\n", obj_file);
    }

    if (emit_ir)
    {
        const char *ir_path = emit_ir_path;
        if (!ir_path || ir_path[0] == '\0')
        {
            auto_ir = create_out_name(filename, ".ll");
            ir_path = auto_ir;
        }
        if (!codegen_emit_llvm_ir(&cg, ir_path))
        {
            fprintf(stderr, "error: failed to emit llvm ir '%s'\n", ir_path);
        }
    }

    if (emit_asm)
    {
        const char *asm_path = emit_asm_path;
        if (!asm_path || asm_path[0] == '\0')
        {
            auto_asm = create_out_name(filename, ".s");
            asm_path = auto_asm;
        }
        if (!codegen_emit_assembly(&cg, asm_path))
        {
            fprintf(stderr, "error: failed to emit assembly '%s'\n", asm_path);
        }
    }

    // compile dependencies to objects if config is present
    char **dep_objs  = NULL;
    int    dep_count = 0;
    char   dep_out_dir[1024];
    dep_out_dir[0] = '\0';
    if (cfg)
    {
        snprintf(dep_out_dir, sizeof(dep_out_dir), "%s/out/obj", project_dir_root);
        ensure_dir(dep_out_dir);
        if (!module_manager_compile_dependencies(&driver->module_manager, dep_out_dir, opt_level, no_pie, debug_info))
        {
            fprintf(stderr, "error: failed to compile dependencies\n");
            if (cfg)
            {
                config_dnit(cfg);
                free(cfg);
            }
            free(module_name_override);
            semantic_driver_destroy(driver);
            ast_node_dnit(prog);
            free(prog);
            parser_dnit(&ps);
            lexer_dnit(&lx);
            free(source);
            free(project_dir_root);
            free(auto_obj);
            free(auto_ir);
            free(auto_asm);
            free(auto_ast);
            codegen_context_dnit(&cg);
            return 1;
        }
        module_manager_get_link_objects(&driver->module_manager, &dep_objs, &dep_count);
    }

    if (link_exe)
    {
        char *exe = NULL;
        if (!output_file)
            exe = get_base_filename_only(filename);
        else
            exe = strdup(output_file);
        size_t sz  = 4096 + (link_objs.count * 256) + (dep_count * 256);
        char  *cmd = malloc(sz);
        if (!cmd)
        {
            fprintf(stderr, "error: oom during link command setup\n");
            free(exe);
            exe = NULL;
        }
        else
        {
            cmd[0] = '\0';
            strcat(cmd, "cc -nostartfiles -nostdlib");
            if (no_pie)
                strcat(cmd, " -no-pie");
            else
                strcat(cmd, " -pie");
            if (debug_info)
                strcat(cmd, " -g");
            strcat(cmd, " -o ");
            strcat(cmd, exe);
            strcat(cmd, " ");
            strcat(cmd, obj_file);
            for (int i = 0; i < dep_count; i++)
            {
                strcat(cmd, " ");
                strcat(cmd, dep_objs[i]);
            }
            for (int i = 0; i < link_objs.count; i++)
            {
                strcat(cmd, " ");
                strcat(cmd, link_objs.items[i]);
            }
            if (system(cmd) != 0)
            {
                fprintf(stderr, "error: failed to link executable '%s'\n", exe);
            }
            free(cmd);
        }
        if (exe)
            free(exe);
    }

    codegen_context_dnit(&cg);
    if (dep_objs)
    {
        for (int i = 0; i < dep_count; i++)
            free(dep_objs[i]);
        free(dep_objs);
    }
    if (cfg)
    {
        config_dnit(cfg);
        free(cfg);
    }
    semantic_driver_destroy(driver);
    ast_node_dnit(prog);
    free(prog);
    parser_dnit(&ps);
    lexer_dnit(&lx);
    free(source);
    free(auto_obj);
    free(auto_ir);
    free(auto_asm);
    free(auto_ast);
    free(project_dir_root);
    free(module_name_override);
    return 0;
}
