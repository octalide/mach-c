// minimal, config-free build entrypoint
#include "commands.h"
#include "ast.h"
#include "codegen.h"
#include "lexer.h"
#include "module.h"
#include "parser.h"
#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file_simple(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END); long size = ftell(file); fseek(file, 0, SEEK_SET);
    char *buf = malloc(size + 1); if (!buf) { fclose(file); return NULL; }
    size_t n = fread(buf, 1, size, file); buf[n] = '\0'; fclose(file); return buf;
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
}

int mach_cmd_build(int argc, char **argv)
{
    if (argc < 3) { mach_print_usage(argv[0]); return 1; }

    const char *filename = argv[2];
    const char *output_file = NULL; int opt_level = 2; int link_exe = 1; int no_pie = 0;

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
        else if (strcmp(argv[i], "--link") == 0) { if (i+1<argc) VPUSH(link_objs, argv[++i]); else { fprintf(stderr, "error: --link requires an object file\n"); return 1; } }
        else if (strcmp(argv[i], "-I") == 0) { if (i+1<argc) VPUSH(inc, argv[++i]); else { fprintf(stderr, "error: -I requires a directory\n"); return 1; } }
        else if (strcmp(argv[i], "-M") == 0) { if (i+1<argc) { char *a=argv[++i]; char *eq=strchr(a,'='); if(!eq){fprintf(stderr,"error: -M expects name=dir\n"); return 1;} *eq='\0'; APUSH(al, a, eq+1); } else { fprintf(stderr, "error: -M requires name=dir\n"); return 1; } }
        else { fprintf(stderr, "error: unknown option '%s'\n", argv[i]); return 1; }
    }

    char *source = read_file_simple(filename); if (!source) { fprintf(stderr, "error: could not read '%s'\n", filename); return 1; }

    Lexer lx; lexer_init(&lx, source); Parser ps; parser_init(&ps, &lx); AstNode *prog = parser_parse_program(&ps);
    if (ps.had_error) { fprintf(stderr, "parsing failed with %d error(s):\n", ps.errors.count); parser_error_list_print(&ps.errors, &lx, filename); parser_dnit(&ps); lexer_dnit(&lx); free(source); return 1; }

    SemanticAnalyzer an; semantic_analyzer_init(&an);
    for (int i = 0; i < inc.count; i++) module_manager_add_search_path(&an.module_manager, inc.items[i]);
    for (int i = 0; i < al.count; i++) module_manager_add_alias(&an.module_manager, al.names[i], al.dirs[i]);

    if (!semantic_analyze(&an, prog)) {
        if (an.errors.count > 0) { fprintf(stderr, "semantic analysis failed with %d error(s):\n", an.errors.count); semantic_print_errors(&an, &lx, filename); }
        if (an.module_manager.had_error) { fprintf(stderr, "module loading failed with %d error(s):\n", an.module_manager.errors.count); module_error_list_print(&an.module_manager.errors); }
        semantic_analyzer_dnit(&an); ast_node_dnit(prog); free(prog); parser_dnit(&ps); lexer_dnit(&lx); free(source); return 1; }

    CodegenContext cg; codegen_context_init(&cg, filename, no_pie); cg.opt_level = opt_level; cg.use_runtime = 0; cg.source_file = filename; cg.source_lexer = &lx;
    if (!codegen_generate(&cg, prog, &an)) { fprintf(stderr, "code generation failed:\n"); codegen_print_errors(&cg); codegen_context_dnit(&cg); semantic_analyzer_dnit(&an); ast_node_dnit(prog); free(prog); parser_dnit(&ps); lexer_dnit(&lx); free(source); return 1; }

    const char *obj_file = NULL; char *auto_obj = NULL;
    if (!output_file) { auto_obj = create_out_name(filename, ".o"); obj_file = auto_obj; } else obj_file = output_file;
    if (!codegen_emit_object(&cg, obj_file)) { fprintf(stderr, "error: failed to write object file '%s'\n", obj_file); }

    if (link_exe) {
        char *exe = NULL; if (!output_file) exe = get_base_filename_only(filename); else exe = strdup(output_file);
        size_t sz = 1024 + (link_objs.count * 256); char *cmd = malloc(sz);
        snprintf(cmd, sz, no_pie ? "cc -no-pie -o %s %s" : "cc -pie -o %s %s", exe, obj_file);
        for (int i = 0; i < link_objs.count; i++) { strcat(cmd, " "); strcat(cmd, link_objs.items[i]); }
        if (system(cmd) != 0) { fprintf(stderr, "error: failed to link executable '%s'\n", exe); }
        free(cmd); free(exe);
    }

    codegen_context_dnit(&cg);
    semantic_analyzer_dnit(&an);
    ast_node_dnit(prog); free(prog);
    parser_dnit(&ps); lexer_dnit(&lx);
    free(source); free(auto_obj);
    return 0;
}
