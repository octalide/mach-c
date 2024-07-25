#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdbool.h>

#include "target.h"
#include "dependancy.h"
#include "compilation_type.h"

typedef struct Options
{
    const char *mach_installation;

    const char *version;

    // NOTE: these paths should all be absolute
    const char *path_project;
    const char *path_src;
    const char *path_dep;
    const char *path_out;
    const char *path_entry;

    bool dump_irl;
    bool dump_tac;
    bool dump_asm;
    bool verbose_compiler;
    bool verbose_parse;
    bool verbose_analysis;
    bool verbose_codegen;
    bool verbose_link;

    Target *targets;
    int target_count;

    Dependancy *dependancies;
    int dependancy_count;
} Options;

void options_default(Options *options);
void options_from_args(Options *options, int argc, char **argv);
void options_from_file(Options *options, const char *path);

void options_set_path_project(Options *options, const char *path_project);
void options_set_path_src(Options *options, const char *path_src);
void options_set_path_dep(Options *options, const char *path_dep);
void options_set_path_out(Options *options, const char *path_out);
void options_set_path_entry(Options *options, const char *path_entry);
void options_add_target(Options *options, Target target);
void options_add_dependancy(Options *options, Dependancy dependancy);

char *options_format_string_variables(Options *options, const char *str);

#endif // OPTIONS_H
