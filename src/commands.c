// minimal, config-free build entrypoint
#include "commands.h"
#include "ast.h"
#include "codegen.h"
#include "lexer.h"
#include "config.h"
#include "module.h"
#include "parser.h"
#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// helper: sanitize module path to symbol-safe package name
static char *sanitize_pkg(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *buf = malloc(len + 1);
    if (!buf) return NULL;
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        buf[i] = (c == '.' || c == '/' || c == ':') ? '_' : c;
    }
    buf[len] = '\0';
    return buf;
}

static char *read_file_simple(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END); long size = ftell(file); fseek(file, 0, SEEK_SET);
    char *buf = malloc(size + 1); if (!buf) { fclose(file); return NULL; }
    size_t n = fread(buf, 1, size, file); buf[n] = '\0'; fclose(file); return buf;
}

static int file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static char *dirname_dup(const char *path)
{
    const char *slash = strrchr(path, '/');
    if (!slash) return strdup(".");
    size_t len = (size_t)(slash - path);
    char  *out = malloc(len + 1);
    memcpy(out, path, len);
    out[len] = '\0';
    return out;
}

static char *find_project_root(const char *start_path)
{
    // walk up from directory of start_path until a mach.toml is found or root reached
    char *dir = dirname_dup(start_path);
    for (int i = 0; i < 16 && dir; i++)
    {
        char cfg[1024];
        snprintf(cfg, sizeof(cfg), "%s/mach.toml", dir);
        if (file_exists(cfg))
            return dir;
        // go up one
        const char *slash = strrchr(dir, '/');
        if (!slash) { free(dir); return strdup("."); }
        if (slash == dir) { dir[1] = '\0'; return dir; }
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
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) return;
    mkdir(path, 0777);
}

static char *get_base_filename_only(const char *path)
{
    const char *last_slash = strrchr(path, '/');
    const char *filename   = last_slash ? last_slash + 1 : path;
    const char *last_dot   = strrchr(filename, '.');
    if (!last_dot) return strdup(filename);
    size_t len = (size_t)(last_dot - filename); char *base = malloc(len + 1);
    strncpy(base, filename, len); base[len] = '\0'; return base;
}

static char *create_out_name(const char *input_file, const char *ext)
{
    char *base = get_base_filename_only(input_file);
    size_t len = strlen(base) + strlen(ext) + 1; char *out = malloc(len);
    snprintf(out, len, "%s%s", base, ext); free(base); return out;
}

void mach_print_usage(const char *program_name)
{
    fprintf(stderr, "usage: %s build <file> [options]\n", program_name);
    fprintf(stderr, "options:\n");
    fprintf(stderr, "  -o <file>     set output file name\n");
    fprintf(stderr, "  -O<level>     optimization level (0-3, default: 2)\n");
    fprintf(stderr, "  --emit-obj    emit object file (.o file)\n");
    fprintf(stderr, "  --no-link     don't create executable (just compile)\n");
    fprintf(stderr, "  --no-pie      disable position independent executable\n");
    fprintf(stderr, "  --link <obj>  link with additional object file\n");
    fprintf(stderr, "  -I <dir>      add module search directory\n");
    fprintf(stderr, "  -M n=dir      map module prefix 'n' to base directory 'dir'\n");
    fprintf(stderr, "  --package <n> set explicit package name (for symbol mangling)\n");
}

int mach_cmd_build(int argc, char **argv)
{
    if (argc < 3) { mach_print_usage(argv[0]); return 1; }

    const char *filename = argv[2];
    const char *output_file = NULL; int opt_level = 2; int link_exe = 1; int no_pie = 0; const char *package_name_opt = NULL;

    typedef struct { char **items; int count; int cap; } Vec;
    Vec inc = (Vec){0}; Vec link_objs = (Vec){0};
    typedef struct { char **names; char **dirs; int count; int cap; } AVec; AVec al = (AVec){0};
    #define VPUSH(v, s) do { if ((v).count == (v).cap) { int n = (v).cap? (v).cap*2:4; void *nn = realloc((v).items, n*sizeof(char*)); if(!nn){fprintf(stderr,"error: oom\n"); return 1;} (v).items = nn; (v).cap = n;} (v).items[(v).count++] = (s); } while(0)
    #define APUSH(v, n, d) do { if ((v).count == (v).cap) { int ncap=(v).cap? (v).cap*2:4; char **nn=realloc((v).names,ncap*sizeof(char*)); char **nd=realloc((v).dirs,ncap*sizeof(char*)); if(!nn||!nd){fprintf(stderr,"error: oom\n"); return 1;} (v).names=nn; (v).dirs=nd; (v).cap=ncap;} (v).names[(v).count]=(n); (v).dirs[(v).count]=(d); (v).count++; } while(0)

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) { if (i+1<argc) output_file = argv[++i]; else { fprintf(stderr, "error: -o requires a filename\n"); return 1; } }
        else if (strncmp(argv[i], "-O", 2) == 0) { opt_level = atoi(argv[i] + 2); if (opt_level<0||opt_level>3){fprintf(stderr,"error: invalid optimization level\n"); return 1;} }
        else if (strcmp(argv[i], "--emit-obj") == 0) { link_exe = 0; }
        else if (strcmp(argv[i], "--no-link") == 0) { link_exe = 0; }
        else if (strcmp(argv[i], "--no-pie") == 0) { no_pie = 1; }
        else if (strcmp(argv[i], "--link") == 0) {
            if (i + 1 < argc) { VPUSH(link_objs, argv[++i]); }
            else { fprintf(stderr, "error: --link requires an object file\n"); return 1; }
        }
        else if (strcmp(argv[i], "--package") == 0) {
            if (i + 1 < argc) { package_name_opt = argv[++i]; }
            else { fprintf(stderr, "error: --package requires a name\n"); return 1; }
        }
        else if (strcmp(argv[i], "-I") == 0) { if (i+1<argc) VPUSH(inc, argv[++i]); else { fprintf(stderr, "error: -I requires a directory\n"); return 1; } }
        else if (strcmp(argv[i], "-M") == 0) { if (i+1<argc) { char *a=argv[++i]; char *eq=strchr(a,'='); if(!eq){fprintf(stderr,"error: -M expects name=dir\n"); return 1;} *eq='\0'; APUSH(al, a, eq+1); } else { fprintf(stderr, "error: -M requires name=dir\n"); return 1; } }
        else { fprintf(stderr, "error: unknown option '%s'\n", argv[i]); return 1; }
    }

    char *source = read_file_simple(filename); if (!source) { fprintf(stderr, "error: could not read '%s'\n", filename); return 1; }

    Lexer lx; lexer_init(&lx, source); Parser ps; parser_init(&ps, &lx); AstNode *prog = parser_parse_program(&ps);
    if (ps.had_error) { fprintf(stderr, "parsing failed with %d error(s):\n", ps.errors.count); parser_error_list_print(&ps.errors, &lx, filename); parser_dnit(&ps); lexer_dnit(&lx); free(source); return 1; }

    // find project root for config-based dependency resolution
    char *project_dir_root = find_project_root(filename);

    SemanticAnalyzer an; semantic_analyzer_init(&an);
    // try to load project config (mach.toml) to enable long-term dependency resolution
    ProjectConfig *cfg = config_load_from_dir(project_dir_root);
    if (cfg)
    {
        module_manager_set_config(&an.module_manager, cfg, project_dir_root);
        // proactively load runtime module so it's compiled and linked
        if (config_has_runtime_module(cfg))
        {
            char  *rt = config_get_runtime_module(cfg);
            Module *m  = module_manager_load_module(&an.module_manager, rt);
            if (m) m->needs_linking = true;
            free(rt);
        }
    }
    for (int i = 0; i < inc.count; i++) module_manager_add_search_path(&an.module_manager, inc.items[i]);
    for (int i = 0; i < al.count; i++) module_manager_add_alias(&an.module_manager, al.names[i], al.dirs[i]);

    if (!semantic_analyze(&an, prog)) {
        if (an.errors.count > 0) { fprintf(stderr, "semantic analysis failed with %d error(s):\n", an.errors.count); semantic_print_errors(&an, &lx, filename); }
        if (an.module_manager.had_error) { fprintf(stderr, "module loading failed with %d error(s):\n", an.module_manager.errors.count); module_error_list_print(&an.module_manager.errors); }
        if (cfg) { config_dnit(cfg); free(cfg); }
        semantic_analyzer_dnit(&an); ast_node_dnit(prog); free(prog); parser_dnit(&ps); lexer_dnit(&lx); free(source); return 1; }

    CodegenContext cg; codegen_context_init(&cg, filename, no_pie); cg.opt_level = opt_level; cg.use_runtime = (cfg && config_has_runtime_module(cfg)); cg.source_file = filename; cg.source_lexer = &lx;
    

    // set package name: explicit flag > project config name > inferred from -M mapping and source path
    if (package_name_opt) {
        cg.package_name = strdup(package_name_opt);
    } else if (cfg && cfg->name) {
        cg.package_name = strdup(cfg->name);
    } else if (al.count > 0) {
        // try to infer module fqn from the first matching alias whose dir prefixes the filename
    const char *best_alias = NULL;
        size_t      best_len   = 0;
        for (int i = 0; i < al.count; i++) {
            const char *dir = al.dirs[i];
            size_t dlen = strlen(dir);
            if (strncmp(filename, dir, dlen) == 0 && (filename[dlen] == '/' || filename[dlen] == '\\' || filename[dlen] == '\0')) {
                if (dlen > best_len) { best_len = dlen; best_alias = al.names[i]; }
            }
        }
        if (best_alias) {
            const char *rel = filename + best_len;
            if (*rel == '/' || *rel == '\\') rel++;
            size_t rlen = strlen(rel);
            // strip extension .mach if present
            size_t blen = rlen;
            if (rlen > 5 && strcmp(rel + rlen - 5, ".mach") == 0) blen = rlen - 5;
            char *rel_mod = malloc(blen + 1);
            if (rel_mod) {
                memcpy(rel_mod, rel, blen); rel_mod[blen] = '\0';
                for (size_t i = 0; i < blen; i++) if (rel_mod[i] == '/' || rel_mod[i] == '\\') rel_mod[i] = '.';
                size_t total = strlen(best_alias) + (blen ? 1 : 0) + blen + 1;
                char *fqn = malloc(total);
                if (fqn) {
                    if (blen)
                        snprintf(fqn, total, "%s.%s", best_alias, rel_mod);
                    else
                        snprintf(fqn, total, "%s", best_alias);
                    cg.package_name = sanitize_pkg(fqn);
                    free(fqn);
                }
                free(rel_mod);
            }
        }
    }
    if (!codegen_generate(&cg, prog, &an)) { fprintf(stderr, "code generation failed:\n"); codegen_print_errors(&cg); codegen_context_dnit(&cg); if (cfg) { config_dnit(cfg); free(cfg); } semantic_analyzer_dnit(&an); ast_node_dnit(prog); free(prog); parser_dnit(&ps); lexer_dnit(&lx); free(source); return 1; }

    const char *obj_file = NULL; char *auto_obj = NULL;
    if (!output_file) { auto_obj = create_out_name(filename, ".o"); obj_file = auto_obj; } else obj_file = output_file;
    if (!codegen_emit_object(&cg, obj_file)) { fprintf(stderr, "error: failed to write object file '%s'\n", obj_file); }

    // compile dependencies to objects if config is present
    char **dep_objs = NULL; int dep_count = 0; char dep_out_dir[1024]; dep_out_dir[0] = '\0';
    if (cfg)
    {
        snprintf(dep_out_dir, sizeof(dep_out_dir), "%s/out/obj", project_dir_root);
        ensure_dir(dep_out_dir);
        if (!module_manager_compile_dependencies(&an.module_manager, dep_out_dir, opt_level, no_pie))
        {
            fprintf(stderr, "error: failed to compile dependencies\n");
            if (cfg) { config_dnit(cfg); free(cfg); }
            semantic_analyzer_dnit(&an); ast_node_dnit(prog); free(prog); parser_dnit(&ps); lexer_dnit(&lx); free(source); free(project_dir_root); free(auto_obj); codegen_context_dnit(&cg); return 1;
        }
    module_manager_get_link_objects(&an.module_manager, &dep_objs, &dep_count);
    // no lockfile writing; reproducibility can be handled by VCS and pinned dep paths
        // ensure runtime module is part of link if configured
        if (config_has_runtime_module(cfg))
        {
            char *rt = config_get_runtime_module(cfg);
            Module *m  = module_manager_find_module(&an.module_manager, rt);
            if (!m) m = module_manager_load_module(&an.module_manager, rt);
            if (m)
            {
                m->needs_linking = true;
                // re-collect objects after marking runtime
                if (dep_objs) { for (int i = 0; i < dep_count; i++) free(dep_objs[i]); free(dep_objs); }
                module_manager_get_link_objects(&an.module_manager, &dep_objs, &dep_count);
            }
            free(rt);
        }
    }

    if (link_exe) {
    char *exe = NULL; if (!output_file) exe = get_base_filename_only(filename); else exe = strdup(output_file);
        size_t sz = 4096 + (link_objs.count * 256) + (dep_count * 256); char *cmd = malloc(sz);
        snprintf(cmd, sz, no_pie ? "cc -no-pie -o %s %s" : "cc -pie -o %s %s", exe, obj_file);
        for (int i = 0; i < dep_count; i++) { strcat(cmd, " "); strcat(cmd, dep_objs[i]); }
        for (int i = 0; i < link_objs.count; i++) { strcat(cmd, " "); strcat(cmd, link_objs.items[i]); }
        if (system(cmd) != 0) { fprintf(stderr, "error: failed to link executable '%s'\n", exe); }
        free(cmd); free(exe);
    }

    codegen_context_dnit(&cg);
    if (dep_objs) { for (int i = 0; i < dep_count; i++) free(dep_objs[i]); free(dep_objs); }
    if (cfg) { config_dnit(cfg); free(cfg); }
    semantic_analyzer_dnit(&an);
    ast_node_dnit(prog); free(prog);
    parser_dnit(&ps); lexer_dnit(&lx);
    free(source); free(auto_obj); free(project_dir_root);
    return 0;
}
