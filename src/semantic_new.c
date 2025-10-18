#include "semantic_new.h"
#include "symbol.h"
#include "type.h"
#include "ioutil.h"
#include "lexer.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void diagnostic_sink_init(DiagnosticSink *sink)
{
    sink->entries     = NULL;
    sink->count       = 0;
    sink->capacity    = 0;
    sink->has_errors  = false;
    sink->has_fatal   = false;
}

void diagnostic_sink_dnit(DiagnosticSink *sink)
{
    for (size_t i = 0; i < sink->count; i++)
    {
        free(sink->entries[i].message);
        free(sink->entries[i].file_path);
        if (sink->entries[i].token)
            free(sink->entries[i].token);
    }
    free(sink->entries);
    sink->entries = NULL;
    sink->count   = 0;
    sink->capacity = 0;
}

void diagnostic_emit(DiagnosticSink *sink, DiagnosticLevel level, AstNode *node, const char *file_path, const char *fmt, ...)
{
    if (sink->count >= sink->capacity)
    {
        size_t new_capacity = sink->capacity ? sink->capacity * 2 : 16;
        Diagnostic *new_entries = realloc(sink->entries, new_capacity * sizeof(Diagnostic));
        if (!new_entries)
            return;
        sink->entries = new_entries;
        sink->capacity = new_capacity;
    }

    va_list args;
    va_start(args, fmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    Diagnostic *diag = &sink->entries[sink->count++];
    diag->level      = level;
    diag->message    = strdup(buffer);
    diag->file_path  = file_path ? strdup(file_path) : NULL;
    
    // copy token info if available (position calculated at print time)
    if (node && node->token)
    {
        diag->token = malloc(sizeof(Token));
        if (diag->token)
        {
            diag->token->kind = node->token->kind;
            diag->token->pos = node->token->pos;
            diag->token->len = node->token->len;
        }
        diag->line = 0;    // calculated at print time
        diag->column = 0;  // calculated at print time
    }
    else
    {
        diag->token = NULL;
        diag->line = 0;
        diag->column = 0;
    }

    if (level == DIAG_ERROR)
        sink->has_errors = true;
}

void diagnostic_print_all(DiagnosticSink *sink)
{
    for (size_t i = 0; i < sink->count; i++)
    {
        Diagnostic *diag = &sink->entries[i];
        const char *level_str = (diag->level == DIAG_ERROR) ? "error" : 
                                (diag->level == DIAG_WARNING) ? "warning" : "note";
        
        fprintf(stderr, "%s: %s\n", level_str, diag->message);
        
        // if we have token and file info, calculate and display location
        if (diag->token && diag->file_path)
        {
            // read the source file
            char *source = read_file((char *)diag->file_path);
            if (source)
            {
                // create a temporary lexer for this file to calculate position
                Lexer lexer;
                lexer_init(&lexer, source);
                
                int line = lexer_get_pos_line(&lexer, diag->token->pos);
                int col = lexer_get_pos_line_offset(&lexer, diag->token->pos);
                char *line_text = lexer_get_line_text(&lexer, line);
                
                if (line_text)
                {
                    fprintf(stderr, "%s:%d:%d\n", diag->file_path, line + 1, col);
                    fprintf(stderr, "%5d | %s\n", line + 1, line_text);
                    fprintf(stderr, "      | %*s^\n", col > 1 ? col - 1 : 0, "");
                    free(line_text);
                }
                else
                {
                    // fallback if we can't get line text
                    fprintf(stderr, "%s:%d:%d\n", diag->file_path, line + 1, col);
                }
                
                lexer_dnit(&lexer);
                free(source);
            }
            else
            {
                // fallback if we can't open the file
                fprintf(stderr, "%s:<unknown position>\n", diag->file_path);
            }
        }
        else if (diag->file_path)
        {
            // no token info, just show file
            fprintf(stderr, "%s\n", diag->file_path);
        }
    }
}

GenericBindingCtx generic_binding_ctx_create(void)
{
    GenericBindingCtx ctx;
    ctx.bindings = NULL;
    ctx.count    = 0;
    ctx.capacity = 0;
    return ctx;
}

void generic_binding_ctx_destroy(GenericBindingCtx *ctx)
{
    free(ctx->bindings);
    ctx->bindings = NULL;
    ctx->count    = 0;
    ctx->capacity = 0;
}

GenericBindingCtx generic_binding_ctx_push(GenericBindingCtx *parent, const char *param_name, Type *concrete_type)
{
    GenericBindingCtx new_ctx;
    new_ctx.count    = parent->count + 1;
    new_ctx.capacity = new_ctx.count;
    new_ctx.bindings = malloc(new_ctx.capacity * sizeof(GenericBinding));

    // copy parent bindings
    if (parent->count > 0)
        memcpy(new_ctx.bindings, parent->bindings, parent->count * sizeof(GenericBinding));

    // add new binding
    new_ctx.bindings[parent->count].param_name    = param_name;
    new_ctx.bindings[parent->count].concrete_type = concrete_type;

    return new_ctx;
}

Type *generic_binding_ctx_lookup(const GenericBindingCtx *ctx, const char *param_name)
{
    if (!param_name)
        return NULL;

    for (size_t i = ctx->count; i > 0; i--)
    {
        if (ctx->bindings[i - 1].param_name && strcmp(ctx->bindings[i - 1].param_name, param_name) == 0)
            return ctx->bindings[i - 1].concrete_type;
    }

    return NULL;
}

#define SPEC_CACHE_INITIAL_BUCKETS 64

static size_t hash_specialization_key(Symbol *generic_symbol, Type **type_args, size_t type_arg_count)
{
    size_t hash = (size_t)generic_symbol;
    for (size_t i = 0; i < type_arg_count; i++)
    {
        hash = hash * 31 + (size_t)type_args[i];
    }
    return hash;
}

static bool keys_equal(const SpecializationKey *a, const SpecializationKey *b)
{
    if (a->generic_symbol != b->generic_symbol)
        return false;
    if (a->type_arg_count != b->type_arg_count)
        return false;
    for (size_t i = 0; i < a->type_arg_count; i++)
    {
        if (!type_equals(a->type_args[i], b->type_args[i]))
            return false;
    }
    return true;
}

void specialization_cache_init(SpecializationCache *cache)
{
    cache->bucket_count = SPEC_CACHE_INITIAL_BUCKETS;
    cache->buckets      = calloc(cache->bucket_count, sizeof(SpecializationEntry *));
    cache->entry_count  = 0;
}

void specialization_cache_dnit(SpecializationCache *cache)
{
    for (size_t i = 0; i < cache->bucket_count; i++)
    {
        SpecializationEntry *entry = cache->buckets[i];
        while (entry)
        {
            SpecializationEntry *next = entry->next;
            free(entry->key.type_args);
            free(entry);
            entry = next;
        }
    }
    free(cache->buckets);
    cache->buckets = NULL;
    cache->bucket_count = 0;
    cache->entry_count  = 0;
}

Symbol *specialization_cache_find(SpecializationCache *cache, Symbol *generic_symbol, Type **type_args, size_t type_arg_count)
{
    size_t hash   = hash_specialization_key(generic_symbol, type_args, type_arg_count);
    size_t bucket = hash % cache->bucket_count;

    SpecializationKey search_key = {generic_symbol, type_args, type_arg_count};

    for (SpecializationEntry *entry = cache->buckets[bucket]; entry; entry = entry->next)
    {
        if (keys_equal(&entry->key, &search_key))
            return entry->specialized_symbol;
    }

    return NULL;
}

void specialization_cache_insert(SpecializationCache *cache, Symbol *generic_symbol, Type **type_args, size_t type_arg_count, Symbol *specialized)
{
    size_t hash   = hash_specialization_key(generic_symbol, type_args, type_arg_count);
    size_t bucket = hash % cache->bucket_count;

    SpecializationEntry *entry = malloc(sizeof(SpecializationEntry));
    entry->key.generic_symbol  = generic_symbol;
    entry->key.type_arg_count  = type_arg_count;
    entry->key.type_args       = malloc(type_arg_count * sizeof(Type *));
    memcpy(entry->key.type_args, type_args, type_arg_count * sizeof(Type *));
    entry->specialized_symbol = specialized;
    entry->next               = cache->buckets[bucket];
    cache->buckets[bucket]    = entry;
    cache->entry_count++;
}

void instantiation_queue_init(InstantiationQueue *queue)
{
    queue->head  = NULL;
    queue->tail  = NULL;
    queue->count = 0;
}

void instantiation_queue_dnit(InstantiationQueue *queue)
{
    InstantiationRequest *req = queue->head;
    while (req)
    {
        InstantiationRequest *next = req->next;
        free(req->type_args);
        free(req);
        req = next;
    }
    queue->head  = NULL;
    queue->tail  = NULL;
    queue->count = 0;
}

void instantiation_queue_push(InstantiationQueue *queue, InstantiationKind kind, Symbol *generic_symbol, Type **type_args, size_t type_arg_count, AstNode *call_site)
{
    InstantiationRequest *req = malloc(sizeof(InstantiationRequest));
    req->kind           = kind;
    req->generic_symbol = generic_symbol;
    req->type_arg_count = type_arg_count;
    req->type_args      = malloc(type_arg_count * sizeof(Type *));
    memcpy(req->type_args, type_args, type_arg_count * sizeof(Type *));
    req->call_site = call_site;
    req->next      = NULL;

    if (queue->tail)
    {
        queue->tail->next = req;
        queue->tail       = req;
    }
    else
    {
        queue->head = queue->tail = req;
    }
    queue->count++;
}

InstantiationRequest *instantiation_queue_pop(InstantiationQueue *queue)
{
    if (!queue->head)
        return NULL;

    InstantiationRequest *req = queue->head;
    queue->head = req->next;
    if (!queue->head)
        queue->tail = NULL;
    queue->count--;

    return req;
}

static char *sanitize_type_name(const char *name)
{
    if (!name)
        return strdup("anon");

    size_t len = strlen(name);
    char  *result = malloc(len + 1);
    char  *dst = result;

    for (size_t i = 0; i < len; i++)
    {
        char c = name[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')
        {
            *dst++ = c;
        }
    }
    *dst = '\0';

    return result;
}

static char *type_to_mangled_string(Type *type)
{
    if (!type)
        return strdup("unknown");

    char buffer[256];
    switch (type->kind)
    {
    case TYPE_U8:
    case TYPE_U16:
    case TYPE_U32:
    case TYPE_U64:
    case TYPE_I8:
    case TYPE_I16:
    case TYPE_I32:
    case TYPE_I64:
    case TYPE_F16:
    case TYPE_F32:
    case TYPE_F64:
    case TYPE_PTR:
        return strdup(type->name ? type->name : "type");

    case TYPE_POINTER:
        snprintf(buffer, sizeof(buffer), "ptr_%s", type_to_mangled_string(type->pointer.base));
        return strdup(buffer);

    case TYPE_ARRAY:
        snprintf(buffer, sizeof(buffer), "arr_%s", type_to_mangled_string(type->array.elem_type));
        return strdup(buffer);

    case TYPE_STRUCT:
    case TYPE_UNION:
        return sanitize_type_name(type->name);

    default:
        return strdup("unknown");
    }
}

char *mangle_generic_type(const char *module_name, const char *base_name, Type **type_args, size_t type_arg_count)
{
    if (!base_name)
        base_name = "type";

    char *sanitized_base = sanitize_type_name(base_name);
    
    // calculate total length with module prefix
    size_t total_len = 0;
    char *module_prefix = NULL;
    if (module_name && strlen(module_name) > 0)
    {
        // sanitize module path: std.types.result → std__types__result
        size_t mod_len = strlen(module_name);
        module_prefix = malloc(mod_len + 1);
        char *dst = module_prefix;
        for (size_t i = 0; i < mod_len; i++)
        {
            char c = module_name[i];
            if (c == '.')
                *dst++ = '_';
            else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')
                *dst++ = c;
        }
        *dst = '\0';
        total_len = strlen(module_prefix) + 2; // module + "__"
    }
    
    total_len += strlen(sanitized_base);

    char **arg_strs = malloc(type_arg_count * sizeof(char *));
    for (size_t i = 0; i < type_arg_count; i++)
    {
        arg_strs[i] = type_to_mangled_string(type_args[i]);
        total_len += 1 + strlen(arg_strs[i]); // '$' + arg
    }

    char *result = malloc(total_len + 1);
    size_t offset = 0;
    
    // add module prefix
    if (module_prefix)
    {
        strcpy(result, module_prefix);
        offset = strlen(module_prefix);
        result[offset++] = '_';
        result[offset++] = '_';
        free(module_prefix);
    }
    
    // add base name
    strcpy(result + offset, sanitized_base);
    offset += strlen(sanitized_base);

    // add type arguments
    for (size_t i = 0; i < type_arg_count; i++)
    {
        result[offset++] = '$';
        strcpy(result + offset, arg_strs[i]);
        offset += strlen(arg_strs[i]);
        free(arg_strs[i]);
    }
    result[offset] = '\0';

    free(arg_strs);
    free(sanitized_base);
    return result;
}

char *mangle_generic_function(const char *module_name, const char *base_name, Type **type_args, size_t type_arg_count)
{
    // functions use same mangling as types
    return mangle_generic_type(module_name, base_name, type_args, type_arg_count);
}

char *mangle_method(const char *module_name, const char *owner_name, const char *method_name, bool receiver_is_pointer)
{
    const char *mod = module_name ? module_name : "mod";
    const char *own = owner_name ? owner_name : "owner";
    const char *mth = method_name ? method_name : "method";
    const char *ptr_suffix = receiver_is_pointer ? "_ptr" : "";

    size_t len = strlen(mod) + 2 + strlen(own) + 2 + strlen(mth) + strlen(ptr_suffix) + 1;
    char *result = malloc(len);
    snprintf(result, len, "%s__%s__%s%s", mod, own, mth, ptr_suffix);
    return result;
}

char *mangle_global_symbol(const char *module_name, const char *symbol_name)
{
    const char *mod = module_name ? module_name : "mod";
    const char *sym = symbol_name ? symbol_name : "symbol";

    size_t len = strlen(mod) + 2 + strlen(sym) + 1;
    char *result = malloc(len);
    snprintf(result, len, "%s__%s", mod, sym);
    return result;
}

AnalysisContext analysis_context_create(Scope *global_scope, Scope *module_scope, const char *module_name)
{
    AnalysisContext ctx;
    ctx.current_scope    = module_scope ? module_scope : global_scope;
    ctx.module_scope     = module_scope;
    ctx.global_scope     = global_scope;
    ctx.bindings         = generic_binding_ctx_create();
    ctx.module_name      = module_name;
    ctx.current_function = NULL;
    return ctx;
}

AnalysisContext analysis_context_with_scope(const AnalysisContext *parent, Scope *new_scope)
{
    AnalysisContext ctx = *parent;
    ctx.current_scope = new_scope;
    return ctx;
}

AnalysisContext analysis_context_with_bindings(const AnalysisContext *parent, GenericBindingCtx new_bindings)
{
    AnalysisContext ctx = *parent;
    ctx.bindings = new_bindings;
    return ctx;
}

AnalysisContext analysis_context_with_function(const AnalysisContext *parent, Symbol *function)
{
    AnalysisContext ctx = *parent;
    ctx.current_function = function;
    return ctx;
}

SemanticDriver *semantic_driver_create(void)
{
    SemanticDriver *driver = malloc(sizeof(SemanticDriver));
    module_manager_init(&driver->module_manager);
    specialization_cache_init(&driver->spec_cache);
    instantiation_queue_init(&driver->inst_queue);
    diagnostic_sink_init(&driver->diagnostics);
    driver->program_root = NULL;
    driver->entry_module_name = NULL;
    return driver;
}

void semantic_driver_destroy(SemanticDriver *driver)
{
    module_manager_dnit(&driver->module_manager);
    specialization_cache_dnit(&driver->spec_cache);
    instantiation_queue_dnit(&driver->inst_queue);
    diagnostic_sink_dnit(&driver->diagnostics);
    free(driver);
}

static bool analyze_pass_a_declarations(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *root);
static bool analyze_pass_b_signatures(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *root);
static bool analyze_pass_c_bodies(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *root);
static Symbol *request_generic_type_instantiation(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *type_node);
static bool analyze_function_body(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt);

static Type *resolve_type_in_context(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *type_node)
{
    if (!type_node)
        return NULL;

    // cache if already resolved
    if (type_node->type)
        return type_node->type;

    switch (type_node->kind)
    {
    case AST_TYPE_NAME:
    {
        const char *name = type_node->type_name.name;
        
        // check for generic instantiation
        if (type_node->type_name.generic_args && type_node->type_name.generic_args->count > 0)
        {
            Symbol *specialized = request_generic_type_instantiation(driver, ctx, type_node);
            if (specialized && specialized->type)
            {
                type_node->type = specialized->type;
                return specialized->type;
            }
            diagnostic_emit(&driver->diagnostics, DIAG_ERROR, type_node, ctx->module_name,
                          "failed to instantiate generic type '%s'", name);
            return NULL;
        }
        
        // check for generic binding first
        Type *bound = generic_binding_ctx_lookup(&ctx->bindings, name);
        if (bound)
        {
            type_node->type = bound;
            return bound;
        }

        // check builtin types
        Type *builtin = type_lookup_builtin(name);
        if (builtin)
        {
            type_node->type = builtin;
            return builtin;
        }

        // look up user-defined type in scope
        Symbol *type_sym = symbol_lookup_scope(ctx->current_scope, name);
        if (!type_sym)
            type_sym = symbol_lookup_scope(ctx->module_scope, name);
        if (!type_sym)
            type_sym = symbol_lookup_scope(ctx->global_scope, name);

        if (type_sym && type_sym->kind == SYMBOL_TYPE)
        {
            type_node->type = type_sym->type;
            return type_sym->type;
        }

        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, type_node, ctx->module_name,
                       "unknown type '%s'", name);
        return NULL;
    }

    case AST_TYPE_PTR:
    {
        Type *base = resolve_type_in_context(driver, ctx, type_node->type_ptr.base);
        if (!base)
            return NULL;
        Type *ptr = type_pointer_create(base);
        type_node->type = ptr;
        return ptr;
    }

    case AST_TYPE_ARRAY:
    {
        Type *elem = resolve_type_in_context(driver, ctx, type_node->type_array.elem_type);
        if (!elem)
            return NULL;
        Type *arr = type_array_create(elem);
        type_node->type = arr;
        return arr;
    }

    case AST_TYPE_FUN:
    {
        Type *ret_type = NULL;
        if (type_node->type_fun.return_type)
        {
            ret_type = resolve_type_in_context(driver, ctx, type_node->type_fun.return_type);
            if (!ret_type)
                return NULL;
        }

        size_t param_count = type_node->type_fun.params ? type_node->type_fun.params->count : 0;
        Type **param_types = NULL;
        if (param_count > 0)
        {
            param_types = malloc(sizeof(Type *) * param_count);
            for (size_t i = 0; i < param_count; i++)
            {
                param_types[i] = resolve_type_in_context(driver, ctx, type_node->type_fun.params->items[i]);
                if (!param_types[i])
                {
                    free(param_types);
                    return NULL;
                }
            }
        }

        Type *func = type_function_create(ret_type, param_types, param_count, type_node->type_fun.is_variadic);
        free(param_types);
        type_node->type = func;
        return func;
    }

    default:
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, type_node, ctx->module_name,
                       "unsupported type kind in resolution");
        return NULL;
    }
}

// load and analyze a module
static Module *load_module(SemanticDriver *driver, const char *module_path)
{
    // check if already loaded
    Module *existing = module_manager_find_module(&driver->module_manager, module_path);
    if (existing)
        return existing;
    
    // use existing module manager to load
    Module *module = module_manager_load_module(&driver->module_manager, module_path);
    if (!module)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, NULL, module_path,
                      "failed to load module '%s'", module_path);
        return NULL;
    }
    
    // recursively analyze the module with new semantic analyzer
    if (!module->is_analyzed)
    {
        // check for circular dependencies
        // TODO: proper circular dependency detection
        
        // create analysis context for module
        if (!module->symbols || !module->symbols->global_scope)
        {
            diagnostic_emit(&driver->diagnostics, DIAG_ERROR, NULL, module_path,
                          "module '%s' has invalid symbol table", module_path);
            return NULL;
        }
        
        Scope *module_scope = module->symbols->global_scope;
        AnalysisContext module_ctx = analysis_context_create(module_scope, module_scope, module_path);
        
        // analyze module with all three passes
        bool success = true;
        success = success && analyze_pass_a_declarations(driver, &module_ctx, module->ast);
        success = success && analyze_pass_b_signatures(driver, &module_ctx, module->ast);
        success = success && analyze_pass_c_bodies(driver, &module_ctx, module->ast);
        
        if (!success)
        {
            diagnostic_emit(&driver->diagnostics, DIAG_ERROR, NULL, module_path,
                          "semantic analysis failed for module '%s'", module_path);
            return NULL;
        }
        
        module->is_analyzed = true;
    }
    
    return module;
}

// import public symbols from module into current scope
static bool import_public_symbols(Scope *dest_scope, Module *src_module)
{
    if (!dest_scope || !src_module || !src_module->symbols)
        return false;
    
    // iterate through module's symbol table and import public symbols
    // the symbol table has a global_scope field
    Scope *src_scope = src_module->symbols->global_scope;
    if (!src_scope)
        return false;
    
    for (Symbol *sym = src_scope->symbols; sym; sym = sym->next)
    {
        if (sym->is_public)
        {
            // add reference to symbol in destination scope
            // (don't clone, just reference the same symbol)
            symbol_add(dest_scope, sym);
        }
    }
    
    return true;
}

// process use statement
static bool process_use_statement(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    const char *module_path = stmt->use_stmt.module_path;
    const char *alias = stmt->use_stmt.alias;
    
    // load the module
    Module *module = load_module(driver, module_path);
    if (!module)
        return false;
    
    if (alias)
    {
        // aliased import: use io: std.io
        // create module namespace symbol
        Symbol *ns_symbol = symbol_create(SYMBOL_MODULE, alias, NULL, stmt);
        ns_symbol->module.path = strdup(module_path);
        ns_symbol->module.scope = module->symbols->global_scope;
        ns_symbol->is_public = false; // aliases are local to importing module
        
        // check for name collision
        Symbol *existing = symbol_lookup_scope(ctx->current_scope, alias);
        if (existing)
        {
            diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                          "alias '%s' conflicts with existing symbol", alias);
            return false;
        }
        
        symbol_add(ctx->current_scope, ns_symbol);
        
        diagnostic_emit(&driver->diagnostics, DIAG_NOTE, stmt, ctx->module_name,
                      "module '%s' aliased as '%s'", module_path, alias);
    }
    else
    {
        // direct import: use std.io
        // import all public symbols directly into current scope
        if (!import_public_symbols(ctx->current_scope, module))
        {
            diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                          "failed to import symbols from module '%s'", module_path);
            return false;
        }
        
        diagnostic_emit(&driver->diagnostics, DIAG_NOTE, stmt, ctx->module_name,
                      "imported public symbols from module '%s'", module_path);
    }
    
    return true;
}

// declare type alias
static bool declare_def_stmt(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    const char *name = stmt->def_stmt.name;
    
    // check for redefinition
    Symbol *existing = symbol_lookup_scope(ctx->current_scope, name);
    if (existing)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                       "redefinition of '%s'", name);
        return false;
    }

    // create placeholder - signature will be resolved in pass B
    Type *placeholder = type_alias_create(name, NULL);
    Symbol *symbol = symbol_create(SYMBOL_TYPE, name, placeholder, stmt);
    symbol->type_def.is_alias = true;
    symbol->type_def.is_generic = false;
    symbol->is_public = stmt->def_stmt.is_public;
    symbol->module_name = ctx->module_name ? strdup(ctx->module_name) : NULL;
    symbol->home_scope = ctx->current_scope;
    
    symbol_add(ctx->current_scope, symbol);
    stmt->symbol = symbol;
    return true;
}

// declare struct
static bool declare_str_stmt(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    const char *name = stmt->str_stmt.name;
    
    // check if generic
    if (stmt->str_stmt.generics && stmt->str_stmt.generics->count > 0)
    {
        // register generic template - instantiation handled later
        diagnostic_emit(&driver->diagnostics, DIAG_NOTE, stmt, ctx->module_name,
                       "generic struct '%s' registered", name);
        return true;
    }

    // check for redefinition
    Symbol *existing = symbol_lookup_scope(ctx->current_scope, name);
    if (existing)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                       "redefinition of struct '%s'", name);
        return false;
    }

    // create struct type - fields resolved in pass B
    Type *struct_type = type_struct_create(name);
    Symbol *symbol = symbol_create(SYMBOL_TYPE, name, struct_type, stmt);
    symbol->type_def.is_alias = false;
    symbol->type_def.is_generic = false;
    symbol->is_public = stmt->str_stmt.is_public;
    symbol->module_name = ctx->module_name ? strdup(ctx->module_name) : NULL;
    symbol->home_scope = ctx->current_scope;
    
    symbol_add(ctx->current_scope, symbol);
    stmt->symbol = symbol;
    return true;
}

// declare union
static bool declare_uni_stmt(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    const char *name = stmt->uni_stmt.name;
    
    // check if generic
    if (stmt->uni_stmt.generics && stmt->uni_stmt.generics->count > 0)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_NOTE, stmt, ctx->module_name,
                       "generic union '%s' registered", name);
        return true;
    }

    // check for redefinition
    Symbol *existing = symbol_lookup_scope(ctx->current_scope, name);
    if (existing)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                       "redefinition of union '%s'", name);
        return false;
    }

    // create union type - fields resolved in pass B
    Type *union_type = type_union_create(name);
    Symbol *symbol = symbol_create(SYMBOL_TYPE, name, union_type, stmt);
    symbol->type_def.is_alias = false;
    symbol->type_def.is_generic = false;
    symbol->is_public = stmt->uni_stmt.is_public;
    symbol->module_name = ctx->module_name ? strdup(ctx->module_name) : NULL;
    symbol->home_scope = ctx->current_scope;
    
    symbol_add(ctx->current_scope, symbol);
    stmt->symbol = symbol;
    return true;
}

// declare method
static bool declare_method_stmt(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    const char *method_name = stmt->fun_stmt.name;
    
    // extract owner type name from method_receiver
    if (!stmt->fun_stmt.method_receiver || stmt->fun_stmt.method_receiver->kind != AST_TYPE_NAME)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                       "method '%s' missing valid receiver type", method_name);
        return false;
    }
    
    const char *owner_name = stmt->fun_stmt.method_receiver->type_name.name;
    
    // find owner type symbol
    Symbol *owner_sym = symbol_lookup_scope(ctx->current_scope, owner_name);
    if (!owner_sym)
        owner_sym = symbol_lookup_scope(ctx->module_scope, owner_name);
    if (!owner_sym)
        owner_sym = symbol_lookup_scope(ctx->global_scope, owner_name);
    
    if (!owner_sym || owner_sym->kind != SYMBOL_TYPE)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                       "method owner type '%s' not found", owner_name);
        return false;
    }
    
    // check if generic
    if (stmt->fun_stmt.generics && stmt->fun_stmt.generics->count > 0)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_NOTE, stmt, ctx->module_name,
                       "generic method '%s.%s' registered", owner_name, method_name);
        return true;
    }
    
    // create method symbol - signature resolved in pass B
    // mangle method name with owner type: Type.method → Type__method
    char mangled[512];
    snprintf(mangled, sizeof(mangled), "%s__%s", owner_name, method_name);
    
    Type *placeholder = type_function_create(NULL, NULL, 0, stmt->fun_stmt.is_variadic);
    Symbol *symbol = symbol_create(SYMBOL_FUNC, strdup(mangled), placeholder, stmt);
    symbol->func.is_external = false;
    symbol->func.is_defined = (stmt->fun_stmt.body != NULL);
    symbol->func.is_generic = false;
    symbol->is_public = stmt->fun_stmt.is_public;
    symbol->module_name = ctx->module_name ? strdup(ctx->module_name) : NULL;
    symbol->home_scope = ctx->current_scope;
    
    // add to current scope with mangled name
    symbol_add(ctx->current_scope, symbol);
    
    stmt->symbol = symbol;
    return true;
}

// declare function
static bool declare_fun_stmt(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    const char *name = stmt->fun_stmt.name;
    
    // check if generic
    if (stmt->fun_stmt.generics && stmt->fun_stmt.generics->count > 0)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_NOTE, stmt, ctx->module_name,
                       "generic function '%s' registered", name);
        return true;
    }

    // check if method - handle separately
    if (stmt->fun_stmt.is_method)
        return declare_method_stmt(driver, ctx, stmt);

    // check for redefinition
    Symbol *existing = symbol_lookup_scope(ctx->current_scope, name);
    if (existing)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                       "redefinition of function '%s'", name);
        return false;
    }

    // create function symbol - signature resolved in pass B
    Type *placeholder = type_function_create(NULL, NULL, 0, stmt->fun_stmt.is_variadic);
    Symbol *symbol = symbol_create(SYMBOL_FUNC, name, placeholder, stmt);
    symbol->func.is_external = false;  // regular functions are not external
    symbol->func.is_defined = (stmt->fun_stmt.body != NULL);
    symbol->func.is_generic = false;
    symbol->is_public = stmt->fun_stmt.is_public;
    symbol->module_name = ctx->module_name ? strdup(ctx->module_name) : NULL;
    symbol->home_scope = ctx->current_scope;
    
    symbol_add(ctx->current_scope, symbol);
    stmt->symbol = symbol;
    return true;
}

// declare external function
static bool declare_ext_stmt(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    const char *name = stmt->ext_stmt.name;
    
    // check for redefinition
    Symbol *existing = symbol_lookup_scope(ctx->current_scope, name);
    if (existing)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                       "redefinition of external function '%s'", name);
        return false;
    }

    // create external function symbol - signature resolved in pass B
    Type *placeholder = type_function_create(NULL, NULL, 0, false);
    Symbol *symbol = symbol_create(SYMBOL_FUNC, name, placeholder, stmt);
    symbol->func.is_external = true;
    symbol->func.is_defined = false;  // external functions have no body
    symbol->func.is_generic = false;
    symbol->is_public = stmt->ext_stmt.is_public;
    symbol->module_name = ctx->module_name ? strdup(ctx->module_name) : NULL;
    symbol->home_scope = ctx->current_scope;
    
    symbol_add(ctx->current_scope, symbol);
    stmt->symbol = symbol;
    return true;
}

// declare global variable
static bool declare_var_stmt(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    const char *name = stmt->var_stmt.name;
    
    // check for redefinition
    Symbol *existing = symbol_lookup_scope(ctx->current_scope, name);
    if (existing)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                       "redefinition of '%s'", name);
        return false;
    }

    // create variable symbol - type resolved in pass B
    Symbol *symbol = symbol_create(stmt->var_stmt.is_val ? SYMBOL_VAL : SYMBOL_VAR, name, NULL, stmt);
    symbol->var.is_global = true;
    symbol->var.is_const = stmt->var_stmt.is_val;
    symbol->is_public = stmt->var_stmt.is_public;
    symbol->module_name = ctx->module_name ? strdup(ctx->module_name) : NULL;
    symbol->home_scope = ctx->current_scope;
    
    symbol_add(ctx->current_scope, symbol);
    stmt->symbol = symbol;
    return true;
}

static bool analyze_pass_a_declarations(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *root)
{
    if (!root || root->kind != AST_PROGRAM)
        return false;

    bool success = true;
    
    for (int i = 0; i < root->program.stmts->count; i++)
    {
        AstNode *stmt = root->program.stmts->items[i];
        
        switch (stmt->kind)
        {
        case AST_STMT_DEF:
            if (!declare_def_stmt(driver, ctx, stmt))
                success = false;
            break;
            
        case AST_STMT_STR:
            if (!declare_str_stmt(driver, ctx, stmt))
                success = false;
            break;
            
        case AST_STMT_UNI:
            if (!declare_uni_stmt(driver, ctx, stmt))
                success = false;
            break;
            
        case AST_STMT_FUN:
            if (!declare_fun_stmt(driver, ctx, stmt))
                success = false;
            break;
            
        case AST_STMT_EXT:
            if (!declare_ext_stmt(driver, ctx, stmt))
                success = false;
            break;
            
        case AST_STMT_VAL:
        case AST_STMT_VAR:
            if (!declare_var_stmt(driver, ctx, stmt))
                success = false;
            break;
            
        case AST_STMT_USE:
            if (!process_use_statement(driver, ctx, stmt))
                success = false;
            break;
            
        default:
            break;
        }
    }
    
    return success;
}

// resolve struct fields
static bool resolve_str_fields(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    if (!stmt->symbol || !stmt->symbol->type || stmt->symbol->type->kind != TYPE_STRUCT)
        return false;

    Type *struct_type = stmt->symbol->type;
    
    if (!stmt->str_stmt.fields || stmt->str_stmt.fields->count == 0)
        return true; // empty struct is valid

    Symbol *field_list_head = NULL;
    Symbol *field_list_tail = NULL;
    size_t field_count = 0;
    size_t offset = 0;
    size_t max_alignment = 1;

    for (int i = 0; i < stmt->str_stmt.fields->count; i++)
    {
        AstNode *field_node = stmt->str_stmt.fields->items[i];
        if (field_node->kind != AST_STMT_FIELD)
            continue;

        const char *field_name = field_node->field_stmt.name;
        Type *field_type = resolve_type_in_context(driver, ctx, field_node->field_stmt.type);
        
        if (!field_type)
        {
            diagnostic_emit(&driver->diagnostics, DIAG_ERROR, field_node, ctx->module_name,
                          "cannot resolve type for field '%s'", field_name);
            return false;
        }

        // create field symbol
        Symbol *field_sym = symbol_create(SYMBOL_FIELD, field_name, field_type, field_node);
        
        // calculate offset with alignment
        size_t field_align = type_alignof(field_type);
        if (field_align > 0)
        {
            size_t remainder = offset % field_align;
            if (remainder != 0)
                offset += field_align - remainder;
        }
        field_sym->field.offset = offset;

        // link to list
        if (!field_list_head)
        {
            field_list_head = field_list_tail = field_sym;
        }
        else
        {
            field_list_tail->next = field_sym;
            field_list_tail = field_sym;
        }

        offset += type_sizeof(field_type);
        if (field_align > max_alignment)
            max_alignment = field_align;
        field_count++;
        
        field_node->symbol = field_sym;
        field_node->type = field_type;
    }

    // align final struct size
    if (max_alignment > 0)
    {
        size_t remainder = offset % max_alignment;
        if (remainder != 0)
            offset += max_alignment - remainder;
    }

    struct_type->composite.fields = field_list_head;
    struct_type->composite.field_count = field_count;
    struct_type->size = offset;
    struct_type->alignment = max_alignment;

    return true;
}

// resolve union variants
static bool resolve_uni_fields(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    if (!stmt->symbol || !stmt->symbol->type || stmt->symbol->type->kind != TYPE_UNION)
        return false;

    Type *union_type = stmt->symbol->type;
    
    if (!stmt->uni_stmt.fields || stmt->uni_stmt.fields->count == 0)
        return true; // empty union is valid

    Symbol *field_list_head = NULL;
    Symbol *field_list_tail = NULL;
    size_t field_count = 0;
    size_t max_size = 0;
    size_t max_alignment = 1;

    for (int i = 0; i < stmt->uni_stmt.fields->count; i++)
    {
        AstNode *field_node = stmt->uni_stmt.fields->items[i];
        if (field_node->kind != AST_STMT_FIELD)
            continue;

        const char *field_name = field_node->field_stmt.name;
        Type *field_type = resolve_type_in_context(driver, ctx, field_node->field_stmt.type);
        
        if (!field_type)
        {
            diagnostic_emit(&driver->diagnostics, DIAG_ERROR, field_node, ctx->module_name,
                          "cannot resolve type for field '%s'", field_name);
            return false;
        }

        // create field symbol (all at offset 0 in union)
        Symbol *field_sym = symbol_create(SYMBOL_FIELD, field_name, field_type, field_node);
        field_sym->field.offset = 0;

        // link to list
        if (!field_list_head)
        {
            field_list_head = field_list_tail = field_sym;
        }
        else
        {
            field_list_tail->next = field_sym;
            field_list_tail = field_sym;
        }

        size_t field_size = type_sizeof(field_type);
        size_t field_align = type_alignof(field_type);
        
        if (field_size > max_size)
            max_size = field_size;
        if (field_align > max_alignment)
            max_alignment = field_align;
        field_count++;
        
        field_node->symbol = field_sym;
        field_node->type = field_type;
    }

    // align final union size
    if (max_alignment > 0)
    {
        size_t remainder = max_size % max_alignment;
        if (remainder != 0)
            max_size += max_alignment - remainder;
    }

    union_type->composite.fields = field_list_head;
    union_type->composite.field_count = field_count;
    union_type->size = max_size;
    union_type->alignment = max_alignment;

    return true;
}

// resolve function signature
static bool resolve_fun_signature(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    if (!stmt->symbol || stmt->symbol->kind != SYMBOL_FUNC)
        return false;

    // resolve return type
    Type *return_type = NULL;
    if (stmt->fun_stmt.return_type)
    {
        return_type = resolve_type_in_context(driver, ctx, stmt->fun_stmt.return_type);
        if (!return_type)
        {
            diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                          "cannot resolve return type for function '%s'", stmt->fun_stmt.name);
            return false;
        }
    }

    // resolve parameters
    size_t param_count = stmt->fun_stmt.params ? stmt->fun_stmt.params->count : 0;
    Type **param_types = NULL;
    
    if (param_count > 0)
    {
        param_types = malloc(sizeof(Type *) * param_count);
        
        for (size_t i = 0; i < param_count; i++)
        {
            AstNode *param = stmt->fun_stmt.params->items[i];
            if (param->kind != AST_STMT_PARAM)
            {
                free(param_types);
                return false;
            }

            // skip variadic sentinel
            if (param->param_stmt.is_variadic)
            {
                param_types[i] = NULL;
                continue;
            }

            Type *param_type = resolve_type_in_context(driver, ctx, param->param_stmt.type);
            if (!param_type)
            {
                diagnostic_emit(&driver->diagnostics, DIAG_ERROR, param, ctx->module_name,
                              "cannot resolve type for parameter '%s'", param->param_stmt.name);
                free(param_types);
                return false;
            }
            
            param_types[i] = param_type;
            param->type = param_type;
        }
    }

    // create function type
    Type *func_type = type_function_create(return_type, param_types, param_count, stmt->fun_stmt.is_variadic);
    free(param_types);

    // update symbol
    stmt->symbol->type = func_type;
    stmt->type = func_type;

    return true;
}

// resolve external function signature
static bool resolve_ext_signature(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    if (!stmt->symbol || stmt->symbol->kind != SYMBOL_FUNC)
        return false;

    // resolve return type from ext_stmt.type (full function type)
    Type *func_type = resolve_type_in_context(driver, ctx, stmt->ext_stmt.type);
    if (!func_type || func_type->kind != TYPE_FUNCTION)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                      "invalid type for external function '%s'", stmt->ext_stmt.name);
        return false;
    }

    stmt->symbol->type = func_type;
    stmt->type = func_type;
    
    return true;
}

// resolve variable type
static bool resolve_var_type(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    if (!stmt->symbol)
        return false;

    Type *var_type = NULL;

    // explicit type annotation
    if (stmt->var_stmt.type)
    {
        var_type = resolve_type_in_context(driver, ctx, stmt->var_stmt.type);
        if (!var_type)
        {
            diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                          "cannot resolve type for '%s'", stmt->var_stmt.name);
            return false;
        }
    }
    else if (stmt->var_stmt.init)
    {
        // type inference from initializer - defer to pass C
        // for now, require explicit types
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                      "type inference not yet implemented for '%s'", stmt->var_stmt.name);
        return false;
    }
    else
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                      "variable '%s' must have explicit type or initializer", stmt->var_stmt.name);
        return false;
    }

    stmt->symbol->type = var_type;
    stmt->type = var_type;

    return true;
}

static bool analyze_pass_b_signatures(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *root)
{
    if (!root || root->kind != AST_PROGRAM)
        return false;

    bool success = true;

    // resolve type aliases
    for (int i = 0; i < root->program.stmts->count; i++)
    {
        AstNode *stmt = root->program.stmts->items[i];
        
        if (stmt->kind == AST_STMT_DEF && stmt->symbol)
        {
            Type *resolved = resolve_type_in_context(driver, ctx, stmt->def_stmt.type);
            if (resolved)
            {
                stmt->symbol->type->alias.target = resolved;
                stmt->type = resolved;
            }
            else
            {
                success = false;
            }
        }
    }

    // resolve struct fields
    for (int i = 0; i < root->program.stmts->count; i++)
    {
        AstNode *stmt = root->program.stmts->items[i];
        
        if (stmt->kind == AST_STMT_STR && stmt->symbol && !stmt->symbol->type_def.is_generic)
        {
            if (!resolve_str_fields(driver, ctx, stmt))
                success = false;
        }
    }

    // resolve union fields
    for (int i = 0; i < root->program.stmts->count; i++)
    {
        AstNode *stmt = root->program.stmts->items[i];
        
        if (stmt->kind == AST_STMT_UNI && stmt->symbol && !stmt->symbol->type_def.is_generic)
        {
            if (!resolve_uni_fields(driver, ctx, stmt))
                success = false;
        }
    }

    // resolve function signatures
    for (int i = 0; i < root->program.stmts->count; i++)
    {
        AstNode *stmt = root->program.stmts->items[i];
        
        if (stmt->kind == AST_STMT_FUN && stmt->symbol && !stmt->symbol->func.is_generic)
        {
            // handle both regular functions and methods
            if (!resolve_fun_signature(driver, ctx, stmt))
                success = false;
        }
        
        // also handle external functions
        if (stmt->kind == AST_STMT_EXT && stmt->symbol)
        {
            if (!resolve_ext_signature(driver, ctx, stmt))
                success = false;
        }
    }

    // resolve variable types
    for (int i = 0; i < root->program.stmts->count; i++)
    {
        AstNode *stmt = root->program.stmts->items[i];
        
        if ((stmt->kind == AST_STMT_VAL || stmt->kind == AST_STMT_VAR) && stmt->symbol)
        {
            if (!resolve_var_type(driver, ctx, stmt))
                success = false;
        }
    }
    
    return success;
}

// instantiate generic struct
static Symbol *instantiate_generic_struct(SemanticDriver *driver, const AnalysisContext *ctx, 
                                         Symbol *generic_sym, Type **type_args, size_t arg_count)
{
    if (!generic_sym || !generic_sym->decl || arg_count == 0)
        return NULL;
    
    // check cache first
    Symbol *cached = specialization_cache_find(&driver->spec_cache, generic_sym, type_args, arg_count);
    if (cached)
        return cached;
    
    // create specialized type name
    const char *module_name = generic_sym->module_name ? generic_sym->module_name : ctx->module_name;
    char *specialized_name = mangle_generic_type(module_name, generic_sym->name, type_args, arg_count);
    if (!specialized_name)
        return NULL;
    
    // create specialized struct type
    Type *specialized_type = type_struct_create(specialized_name);
    Symbol *specialized_sym = symbol_create(SYMBOL_TYPE, specialized_name, specialized_type, generic_sym->decl);
    specialized_sym->type_def.is_alias = false;
    specialized_sym->type_def.is_generic = false;
    specialized_sym->is_public = generic_sym->is_public;
    specialized_sym->module_name = module_name ? strdup(module_name) : NULL;
    specialized_sym->home_scope = generic_sym->home_scope;
    
    // build binding context from type parameters
    AstNode *generic_decl = generic_sym->decl;
    GenericBindingCtx bindings = ctx->bindings;
    
    if (generic_decl->kind == AST_STMT_STR && generic_decl->str_stmt.generics)
    {
        for (size_t i = 0; i < arg_count && i < (size_t)generic_decl->str_stmt.generics->count; i++)
        {
            AstNode *param = generic_decl->str_stmt.generics->items[i];
            const char *param_name = param->type_param.name;
            bindings = generic_binding_ctx_push(&bindings, param_name, type_args[i]);
        }
    }
    
    // create context with bindings
    AnalysisContext specialized_ctx = *ctx;
    specialized_ctx.bindings = bindings;
    
    // resolve fields with specialized context
    if (generic_decl->str_stmt.fields)
    {
        Symbol *field_head = NULL;
        Symbol *field_tail = NULL;
        size_t offset = 0;
        size_t max_align = 1;
        size_t field_count = 0;
        
        for (int i = 0; i < generic_decl->str_stmt.fields->count; i++)
        {
            AstNode *field_node = generic_decl->str_stmt.fields->items[i];
            Type *field_type = resolve_type_in_context(driver, &specialized_ctx, field_node->field_stmt.type);
            
            if (!field_type)
            {
                free(specialized_name);
                return NULL;
            }
            
            Symbol *field_sym = symbol_create(SYMBOL_FIELD, field_node->field_stmt.name, field_type, field_node);
            
            size_t field_align = type_alignof(field_type);
            if (field_align > 0)
            {
                size_t remainder = offset % field_align;
                if (remainder != 0)
                    offset += field_align - remainder;
            }
            field_sym->field.offset = offset;
            
            if (!field_head)
                field_head = field_tail = field_sym;
            else
            {
                field_tail->next = field_sym;
                field_tail = field_sym;
            }
            
            offset += type_sizeof(field_type);
            if (field_align > max_align)
                max_align = field_align;
            field_count++;
        }
        
        if (max_align > 0)
        {
            size_t remainder = offset % max_align;
            if (remainder != 0)
                offset += max_align - remainder;
        }
        
        specialized_type->composite.fields = field_head;
        specialized_type->composite.field_count = field_count;
        specialized_type->size = offset;
        specialized_type->alignment = max_align;
    }
    
    // cache the specialization
    specialization_cache_insert(&driver->spec_cache, generic_sym, type_args, arg_count, specialized_sym);
    
    free(specialized_name);
    return specialized_sym;
}

// instantiate generic union
static Symbol *instantiate_generic_union(SemanticDriver *driver, const AnalysisContext *ctx,
                                        Symbol *generic_sym, Type **type_args, size_t arg_count)
{
    if (!generic_sym || !generic_sym->decl || arg_count == 0)
        return NULL;
    
    // check cache first
    Symbol *cached = specialization_cache_find(&driver->spec_cache, generic_sym, type_args, arg_count);
    if (cached)
        return cached;
    
    // create specialized type name
    const char *module_name = generic_sym->module_name ? generic_sym->module_name : ctx->module_name;
    char *specialized_name = mangle_generic_type(module_name, generic_sym->name, type_args, arg_count);
    if (!specialized_name)
        return NULL;
    
    // create specialized union type
    Type *specialized_type = type_union_create(specialized_name);
    Symbol *specialized_sym = symbol_create(SYMBOL_TYPE, specialized_name, specialized_type, generic_sym->decl);
    specialized_sym->type_def.is_alias = false;
    specialized_sym->type_def.is_generic = false;
    specialized_sym->is_public = generic_sym->is_public;
    specialized_sym->module_name = module_name ? strdup(module_name) : NULL;
    specialized_sym->home_scope = generic_sym->home_scope;
    
    // build binding context
    AstNode *generic_decl = generic_sym->decl;
    GenericBindingCtx bindings = ctx->bindings;
    
    if (generic_decl->kind == AST_STMT_UNI && generic_decl->uni_stmt.generics)
    {
        for (size_t i = 0; i < arg_count && i < (size_t)generic_decl->uni_stmt.generics->count; i++)
        {
            AstNode *param = generic_decl->uni_stmt.generics->items[i];
            const char *param_name = param->type_param.name;
            bindings = generic_binding_ctx_push(&bindings, param_name, type_args[i]);
        }
    }
    
    AnalysisContext specialized_ctx = *ctx;
    specialized_ctx.bindings = bindings;
    
    // resolve variants with specialized context
    if (generic_decl->uni_stmt.fields)
    {
        Symbol *field_head = NULL;
        Symbol *field_tail = NULL;
        size_t max_size = 0;
        size_t max_align = 1;
        size_t field_count = 0;
        
        for (int i = 0; i < generic_decl->uni_stmt.fields->count; i++)
        {
            AstNode *field_node = generic_decl->uni_stmt.fields->items[i];
            Type *field_type = resolve_type_in_context(driver, &specialized_ctx, field_node->field_stmt.type);
            
            if (!field_type)
            {
                free(specialized_name);
                return NULL;
            }
            
            Symbol *field_sym = symbol_create(SYMBOL_FIELD, field_node->field_stmt.name, field_type, field_node);
            field_sym->field.offset = 0;
            
            if (!field_head)
                field_head = field_tail = field_sym;
            else
            {
                field_tail->next = field_sym;
                field_tail = field_sym;
            }
            
            size_t field_size = type_sizeof(field_type);
            size_t field_align = type_alignof(field_type);
            
            if (field_size > max_size)
                max_size = field_size;
            if (field_align > max_align)
                max_align = field_align;
            field_count++;
        }
        
        if (max_align > 0)
        {
            size_t remainder = max_size % max_align;
            if (remainder != 0)
                max_size += max_align - remainder;
        }
        
        specialized_type->composite.fields = field_head;
        specialized_type->composite.field_count = field_count;
        specialized_type->size = max_size;
        specialized_type->alignment = max_align;
    }
    
    // cache the specialization
    specialization_cache_insert(&driver->spec_cache, generic_sym, type_args, arg_count, specialized_sym);
    
    free(specialized_name);
    return specialized_sym;
}

// instantiate generic function
static Symbol *instantiate_generic_function(SemanticDriver *driver, const AnalysisContext *ctx,
                                           Symbol *generic_sym, Type **type_args, size_t arg_count)
{
    if (!generic_sym || !generic_sym->decl || arg_count == 0)
        return NULL;
    
    // check cache first
    Symbol *cached = specialization_cache_find(&driver->spec_cache, generic_sym, type_args, arg_count);
    if (cached)
        return cached;
    
    // create specialized function name
    const char *module_name = generic_sym->module_name ? generic_sym->module_name : ctx->module_name;
    char *specialized_name = mangle_generic_function(module_name, generic_sym->name, type_args, arg_count);
    if (!specialized_name)
        return NULL;
    
    // build binding context
    AstNode *generic_decl = generic_sym->decl;
    GenericBindingCtx bindings = ctx->bindings;
    
    if (generic_decl->kind == AST_STMT_FUN && generic_decl->fun_stmt.generics)
    {
        for (size_t i = 0; i < arg_count && i < (size_t)generic_decl->fun_stmt.generics->count; i++)
        {
            AstNode *param = generic_decl->fun_stmt.generics->items[i];
            const char *param_name = param->type_param.name;
            bindings = generic_binding_ctx_push(&bindings, param_name, type_args[i]);
        }
    }
    
    AnalysisContext specialized_ctx = *ctx;
    specialized_ctx.bindings = bindings;
    
    // resolve return type
    Type *ret_type = NULL;
    if (generic_decl->fun_stmt.return_type)
    {
        ret_type = resolve_type_in_context(driver, &specialized_ctx, generic_decl->fun_stmt.return_type);
        if (!ret_type)
        {
            free(specialized_name);
            return NULL;
        }
    }
    
    // resolve parameters
    size_t param_count = generic_decl->fun_stmt.params ? generic_decl->fun_stmt.params->count : 0;
    Type **param_types = NULL;
    
    if (param_count > 0)
    {
        param_types = malloc(sizeof(Type *) * param_count);
        for (size_t i = 0; i < param_count; i++)
        {
            AstNode *param = generic_decl->fun_stmt.params->items[i];
            if (param->param_stmt.is_variadic)
            {
                param_types[i] = NULL;
                continue;
            }
            
            param_types[i] = resolve_type_in_context(driver, &specialized_ctx, param->param_stmt.type);
            if (!param_types[i])
            {
                free(param_types);
                free(specialized_name);
                return NULL;
            }
        }
    }
    
    // create specialized function type
    Type *func_type = type_function_create(ret_type, param_types, param_count, generic_decl->fun_stmt.is_variadic);
    free(param_types);
    
    // create specialized symbol
    Symbol *specialized_sym = symbol_create(SYMBOL_FUNC, specialized_name, func_type, generic_decl);
    specialized_sym->func.is_external = generic_sym->func.is_external;
    specialized_sym->func.is_defined = generic_sym->func.is_defined;
    specialized_sym->func.is_generic = false;
    specialized_sym->is_public = generic_sym->is_public;
    specialized_sym->module_name = module_name ? strdup(module_name) : NULL;
    specialized_sym->home_scope = generic_sym->home_scope;
    
    // cache the specialization
    specialization_cache_insert(&driver->spec_cache, generic_sym, type_args, arg_count, specialized_sym);
    
    // if function has body, analyze it with specialized context
    if (generic_decl->fun_stmt.body && generic_sym->func.is_defined)
    {
        // create temporary AST node for specialized function (reuse generic_decl but with specialized symbol)
        AstNode specialized_decl = *generic_decl;
        specialized_decl.symbol = specialized_sym;
        
        if (!analyze_function_body(driver, &specialized_ctx, &specialized_decl))
        {
            free(specialized_name);
            return NULL;  // body analysis failed
        }
    }
    
    free(specialized_name);
    return specialized_sym;
}

// process instantiation queue
static bool process_instantiation_queue(SemanticDriver *driver, const AnalysisContext *ctx)
{
    bool success = true;
    size_t iterations = 0;
    const size_t max_iterations = 1000; // prevent infinite loops
    
    while (driver->inst_queue.count > 0 && iterations < max_iterations)
    {
        InstantiationRequest *req = instantiation_queue_pop(&driver->inst_queue);
        if (!req)
            break;
        
        Symbol *specialized = NULL;
        
        switch (req->kind)
        {
        case INST_STRUCT:
            specialized = instantiate_generic_struct(driver, ctx, req->generic_symbol, 
                                                    req->type_args, req->type_arg_count);
            break;
            
        case INST_UNION:
            specialized = instantiate_generic_union(driver, ctx, req->generic_symbol,
                                                   req->type_args, req->type_arg_count);
            break;
            
        case INST_FUNCTION:
            specialized = instantiate_generic_function(driver, ctx, req->generic_symbol,
                                                      req->type_args, req->type_arg_count);
            break;
        }
        
        if (!specialized)
        {
            diagnostic_emit(&driver->diagnostics, DIAG_ERROR, req->call_site, ctx->module_name,
                          "failed to instantiate generic");
            success = false;
        }
        
        // clean up request
        free(req->type_args);
        free(req);
        
        iterations++;
    }
    
    if (iterations >= max_iterations)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, NULL, ctx->module_name,
                      "generic instantiation exceeded maximum depth (possible infinite recursion)");
        success = false;
    }
    
    return success;
}

// helper to request instantiation for type node with generic args
static Symbol *request_generic_type_instantiation(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *type_node)
{
    if (!type_node || type_node->kind != AST_TYPE_NAME)
        return NULL;
    
    if (!type_node->type_name.generic_args || type_node->type_name.generic_args->count == 0)
        return NULL;
    
    // look up base generic symbol
    Symbol *generic_sym = symbol_lookup_scope(ctx->current_scope, type_node->type_name.name);
    if (!generic_sym)
        generic_sym = symbol_lookup_scope(ctx->module_scope, type_node->type_name.name);
    if (!generic_sym)
        generic_sym = symbol_lookup_scope(ctx->global_scope, type_node->type_name.name);
    
    if (!generic_sym || generic_sym->kind != SYMBOL_TYPE)
        return NULL;
    
    // resolve type arguments
    size_t arg_count = type_node->type_name.generic_args->count;
    Type **type_args = malloc(sizeof(Type *) * arg_count);
    
    for (size_t i = 0; i < arg_count; i++)
    {
        type_args[i] = resolve_type_in_context(driver, ctx, type_node->type_name.generic_args->items[i]);
        if (!type_args[i])
        {
            free(type_args);
            return NULL;
        }
    }
    
    // check if already specialized
    Symbol *specialized = specialization_cache_find(&driver->spec_cache, generic_sym, type_args, arg_count);
    if (specialized)
    {
        free(type_args);
        return specialized;
    }
    
    // determine kind and instantiate immediately (synchronous for types)
    InstantiationKind kind = INST_STRUCT;
    if (generic_sym->type && generic_sym->type->kind == TYPE_UNION)
        kind = INST_UNION;
    
    if (kind == INST_STRUCT)
        specialized = instantiate_generic_struct(driver, ctx, generic_sym, type_args, arg_count);
    else
        specialized = instantiate_generic_union(driver, ctx, generic_sym, type_args, arg_count);
    
    free(type_args);
    return specialized;
}

// forward declarations for mutual recursion
static Type *analyze_expr(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *expr);
static bool analyze_stmt(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt);

// expression analysis
static Type *analyze_lit_expr(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *expr)
{
    (void)driver;
    (void)ctx;
    
    switch (expr->lit_expr.kind)
    {
    case TOKEN_LIT_INT:
        expr->type = type_i64();
        return expr->type;
    case TOKEN_LIT_FLOAT:
        expr->type = type_f64();
        return expr->type;
    case TOKEN_LIT_CHAR:
        expr->type = type_u8();
        return expr->type;
    case TOKEN_LIT_STRING:
        // string literals are fat pointers: {*u8, u64}
        expr->type = type_array_create(type_u8());
        return expr->type;
    default:
        return NULL;
    }
}

static Type *analyze_ident_expr(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *expr)
{
    const char *name = expr->ident_expr.name;
    
    Symbol *sym = symbol_lookup_scope(ctx->current_scope, name);
    if (!sym)
        sym = symbol_lookup_scope(ctx->module_scope, name);
    if (!sym)
        sym = symbol_lookup_scope(ctx->global_scope, name);
    
    if (!sym)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, expr, ctx->module_name,
                      "undefined identifier '%s'", name);
        return NULL;
    }
    
    expr->symbol = sym;
    expr->type = sym->type;
    return sym->type;
}

static Type *analyze_binary_expr(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *expr)
{
    Type *left = analyze_expr(driver, ctx, expr->binary_expr.left);
    Type *right = analyze_expr(driver, ctx, expr->binary_expr.right);
    
    if (!left || !right)
        return NULL;
    
    TokenKind op = expr->binary_expr.op;
    
    // arithmetic operators
    if (op == TOKEN_PLUS || op == TOKEN_MINUS || op == TOKEN_STAR || op == TOKEN_SLASH || op == TOKEN_PERCENT)
    {
        if (!type_is_numeric(left) || !type_is_numeric(right))
        {
            diagnostic_emit(&driver->diagnostics, DIAG_ERROR, expr, ctx->module_name,
                          "arithmetic requires numeric operands");
            return NULL;
        }
        
        // type promotion: choose wider type
        Type *result_type = left;
        
        // if sizes differ, use the larger
        if (left->size < right->size)
            result_type = right;
        else if (left->size == right->size)
        {
            // same size: prefer unsigned over signed
            if (type_is_integer(left) && type_is_integer(right))
            {
                // if one is unsigned and one is signed, use unsigned
                bool left_signed = (strcmp(left->name, "i8") == 0 || 
                                   strcmp(left->name, "i16") == 0 ||
                                   strcmp(left->name, "i32") == 0 ||
                                   strcmp(left->name, "i64") == 0);
                bool right_signed = (strcmp(right->name, "i8") == 0 || 
                                    strcmp(right->name, "i16") == 0 ||
                                    strcmp(right->name, "i32") == 0 ||
                                    strcmp(right->name, "i64") == 0);
                
                if (!left_signed && right_signed)
                    result_type = left;  // left is unsigned
                else if (left_signed && !right_signed)
                    result_type = right; // right is unsigned
            }
        }
        
        expr->type = result_type;
        return expr->type;
    }
    
    // comparison operators
    if (op == TOKEN_EQUAL_EQUAL || op == TOKEN_BANG_EQUAL || 
        op == TOKEN_LESS || op == TOKEN_LESS_EQUAL || 
        op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL)
    {
        expr->type = type_u8();  // boolean result as u8
        return expr->type;
    }
    
    // logical operators
    if (op == TOKEN_AMPERSAND_AMPERSAND || op == TOKEN_PIPE_PIPE)
    {
        expr->type = type_u8();  // boolean result as u8
        return expr->type;
    }
    
    // assignment
    if (op == TOKEN_EQUAL)
    {
        if (!type_equals(left, right) && !type_can_assign_to(right, left))
        {
            diagnostic_emit(&driver->diagnostics, DIAG_ERROR, expr, ctx->module_name,
                          "incompatible types in assignment");
            return NULL;
        }
        expr->type = left;
        return expr->type;
    }
    
    diagnostic_emit(&driver->diagnostics, DIAG_ERROR, expr, ctx->module_name,
                  "unsupported binary operator");
    return NULL;
}

static Type *analyze_unary_expr(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *expr)
{
    Type *operand = analyze_expr(driver, ctx, expr->unary_expr.expr);
    if (!operand)
        return NULL;
    
    TokenKind op = expr->unary_expr.op;
    
    if (op == TOKEN_MINUS || op == TOKEN_PLUS)
    {
        if (!type_is_numeric(operand))
        {
            diagnostic_emit(&driver->diagnostics, DIAG_ERROR, expr, ctx->module_name,
                          "unary arithmetic requires numeric operand");
            return NULL;
        }
        expr->type = operand;
        return expr->type;
    }
    
    if (op == TOKEN_BANG)
    {
        expr->type = type_u8();  // logical not result as u8
        return expr->type;
    }
    
    if (op == TOKEN_AT) // dereference (@expr)
    {
        if (operand->kind != TYPE_POINTER)
        {
            diagnostic_emit(&driver->diagnostics, DIAG_ERROR, expr, ctx->module_name,
                          "cannot dereference non-pointer type");
            return NULL;
        }
        expr->type = operand->pointer.base;
        return expr->type;
    }
    
    if (op == TOKEN_QUESTION) // address-of (?expr)
    {
        expr->type = type_pointer_create(operand);
        return expr->type;
    }
    
    diagnostic_emit(&driver->diagnostics, DIAG_ERROR, expr, ctx->module_name,
                  "unsupported unary operator");
    return NULL;
}

static Type *analyze_call_expr(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *expr)
{
    Type *func_type = analyze_expr(driver, ctx, expr->call_expr.func);
    if (!func_type)
        return NULL;
    
    if (func_type->kind != TYPE_FUNCTION)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, expr, ctx->module_name,
                      "cannot call non-function type");
        return NULL;
    }
    
    // check argument count
    size_t expected = func_type->function.param_count;
    size_t provided = expr->call_expr.args ? expr->call_expr.args->count : 0;
    
    if (!func_type->function.is_variadic && provided != expected)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, expr, ctx->module_name,
                      "function expects %zu arguments, got %zu", expected, provided);
        return NULL;
    }
    
    // type check arguments
    for (size_t i = 0; i < provided && i < expected; i++)
    {
        Type *arg_type = analyze_expr(driver, ctx, expr->call_expr.args->items[i]);
        if (!arg_type)
            return NULL;
            
        Type *param_type = func_type->function.param_types[i];
        if (!type_equals(arg_type, param_type) && !type_can_assign_to(arg_type, param_type))
        {
            diagnostic_emit(&driver->diagnostics, DIAG_ERROR, expr->call_expr.args->items[i], ctx->module_name,
                          "argument %zu has incompatible type", i + 1);
            return NULL;
        }
    }
    
    expr->type = func_type->function.return_type;
    return expr->type;
}

static Type *analyze_field_expr(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *expr)
{
    // check if object is an identifier (could be module alias)
    if (expr->field_expr.object->kind == AST_EXPR_IDENT)
    {
        const char *name = expr->field_expr.object->ident_expr.name;
        Symbol *sym = symbol_lookup_scope(ctx->current_scope, name);
        if (!sym)
            sym = symbol_lookup_scope(ctx->module_scope, name);
        if (!sym)
            sym = symbol_lookup_scope(ctx->global_scope, name);
        
        // handle module member access: alias.symbol
        if (sym && sym->kind == SYMBOL_MODULE)
        {
            const char *member_name = expr->field_expr.field;
            Symbol *member = symbol_lookup_scope(sym->module.scope, member_name);
            
            if (!member)
            {
                diagnostic_emit(&driver->diagnostics, DIAG_ERROR, expr, ctx->module_name,
                              "module '%s' has no member '%s'", name, member_name);
                return NULL;
            }
            
            if (!member->is_public)
            {
                diagnostic_emit(&driver->diagnostics, DIAG_ERROR, expr, ctx->module_name,
                              "member '%s' of module '%s' is not public", member_name, name);
                return NULL;
            }
            
            expr->symbol = member;
            expr->type = member->type;
            return member->type;
        }
    }
    
    // regular field access on typed expressions
    Type *object_type = analyze_expr(driver, ctx, expr->field_expr.object);
    if (!object_type)
        return NULL;
    
    // dereference pointer
    if (object_type->kind == TYPE_POINTER)
        object_type = object_type->pointer.base;
    
    // array special members
    if (object_type->kind == TYPE_ARRAY)
    {
        if (strcmp(expr->field_expr.field, "len") == 0)
        {
            expr->type = type_u64();
            return expr->type;
        }
        if (strcmp(expr->field_expr.field, "data") == 0)
        {
            expr->type = type_pointer_create(object_type->array.elem_type);
            return expr->type;
        }
    }
    
    // struct/union field access
    if (object_type->kind != TYPE_STRUCT && object_type->kind != TYPE_UNION)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, expr, ctx->module_name,
                      "member access on non-composite type");
        return NULL;
    }
    
    for (Symbol *field = object_type->composite.fields; field; field = field->next)
    {
        if (field->name && strcmp(field->name, expr->field_expr.field) == 0)
        {
            expr->symbol = field;
            expr->type = field->type;
            return field->type;
        }
    }
    
    diagnostic_emit(&driver->diagnostics, DIAG_ERROR, expr, ctx->module_name,
                  "no member named '%s'", expr->field_expr.field);
    return NULL;
}

static Type *analyze_index_expr(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *expr)
{
    Type *array_type = analyze_expr(driver, ctx, expr->index_expr.array);
    Type *index_type = analyze_expr(driver, ctx, expr->index_expr.index);
    
    if (!array_type || !index_type)
        return NULL;
    
    // dereference pointer
    if (array_type->kind == TYPE_POINTER)
        array_type = array_type->pointer.base;
    
    if (array_type->kind != TYPE_ARRAY)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, expr, ctx->module_name,
                      "cannot index non-array type");
        return NULL;
    }
    
    if (!type_is_integer(index_type))
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, expr->index_expr.index, ctx->module_name,
                      "array index must be integer");
        return NULL;
    }
    
    expr->type = array_type->array.elem_type;
    return expr->type;
}

static Type *analyze_cast_expr(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *expr)
{
    Type *source = analyze_expr(driver, ctx, expr->cast_expr.expr);
    Type *target = resolve_type_in_context(driver, ctx, expr->cast_expr.type);
    
    if (!source || !target)
        return NULL;
    
    // validate cast is legal
    bool valid = false;
    
    // same size types can always be cast (reinterpret bits)
    if (source->size == target->size)
        valid = true;
    
    // numeric to numeric (different sizes - truncate or extend)
    else if (type_is_numeric(source) && type_is_numeric(target))
        valid = true;
    
    // pointer to pointer (always allowed)
    else if (source->kind == TYPE_POINTER && target->kind == TYPE_POINTER)
        valid = true;
    
    // integer to pointer or pointer to integer (unsafe but allowed)
    else if ((type_is_integer(source) && target->kind == TYPE_POINTER) ||
             (source->kind == TYPE_POINTER && type_is_integer(target)))
        valid = true;
    
    if (!valid)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, expr, ctx->module_name,
                      "invalid cast from '%s' to '%s'", source->name, target->name);
        return NULL;
    }
    
    expr->type = target;
    return target;
}

static Type *analyze_expr(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *expr)
{
    if (!expr)
        return NULL;
    
    switch (expr->kind)
    {
    case AST_EXPR_LIT:
        return analyze_lit_expr(driver, ctx, expr);
    case AST_EXPR_IDENT:
        return analyze_ident_expr(driver, ctx, expr);
    case AST_EXPR_BINARY:
        return analyze_binary_expr(driver, ctx, expr);
    case AST_EXPR_UNARY:
        return analyze_unary_expr(driver, ctx, expr);
    case AST_EXPR_CALL:
        return analyze_call_expr(driver, ctx, expr);
    case AST_EXPR_FIELD:
        return analyze_field_expr(driver, ctx, expr);
    case AST_EXPR_INDEX:
        return analyze_index_expr(driver, ctx, expr);
    case AST_EXPR_CAST:
        return analyze_cast_expr(driver, ctx, expr);
    case AST_EXPR_NULL:
        expr->type = type_ptr();
        return expr->type;
    default:
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, expr, ctx->module_name,
                      "unsupported expression kind");
        return NULL;
    }
}

// statement analysis
static bool analyze_block_stmt(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    if (!stmt->block_stmt.stmts)
        return true;
    
    bool success = true;
    for (int i = 0; i < stmt->block_stmt.stmts->count; i++)
    {
        if (!analyze_stmt(driver, ctx, stmt->block_stmt.stmts->items[i]))
            success = false;
    }
    return success;
}

static bool analyze_expr_stmt(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    return analyze_expr(driver, ctx, stmt->expr_stmt.expr) != NULL;
}

static bool analyze_var_stmt_body(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    if (!stmt->var_stmt.init)
        return true;
    
    Type *init_type = analyze_expr(driver, ctx, stmt->var_stmt.init);
    if (!init_type)
        return false;
    
    if (stmt->type && !type_equals(stmt->type, init_type) && !type_can_assign_to(init_type, stmt->type))
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                      "incompatible initializer type for variable '%s'", stmt->var_stmt.name);
        return false;
    }
    
    return true;
}

static bool analyze_ret_stmt(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    if (!ctx->current_function)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                      "return statement outside function");
        return false;
    }
    
    Type *func_ret = ctx->current_function->type->function.return_type;
    
    if (!stmt->ret_stmt.expr)
    {
        if (func_ret)
        {
            diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                          "function must return a value");
            return false;
        }
        return true;
    }
    
    Type *ret_type = analyze_expr(driver, ctx, stmt->ret_stmt.expr);
    if (!ret_type)
        return false;
    
    if (!func_ret)
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                      "void function cannot return a value");
        return false;
    }
    
    if (!type_equals(ret_type, func_ret) && !type_can_assign_to(ret_type, func_ret))
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, stmt, ctx->module_name,
                      "incompatible return type");
        return false;
    }
    
    return true;
}

static bool analyze_if_stmt(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    Type *cond_type = analyze_expr(driver, ctx, stmt->cond_stmt.cond);
    if (!cond_type)
        return false;
    
    bool success = analyze_stmt(driver, ctx, stmt->cond_stmt.body);
    
    if (stmt->cond_stmt.stmt_or)
        success = analyze_stmt(driver, ctx, stmt->cond_stmt.stmt_or) && success;
    
    return success;
}

static bool analyze_for_stmt(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    if (stmt->for_stmt.cond)
    {
        Type *cond_type = analyze_expr(driver, ctx, stmt->for_stmt.cond);
        if (!cond_type)
            return false;
    }
    
    return analyze_stmt(driver, ctx, stmt->for_stmt.body);
}

static bool analyze_stmt(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    if (!stmt)
        return true;
    
    switch (stmt->kind)
    {
    case AST_STMT_BLOCK:
        return analyze_block_stmt(driver, ctx, stmt);
    case AST_STMT_EXPR:
        return analyze_expr_stmt(driver, ctx, stmt);
    case AST_STMT_VAR:
    case AST_STMT_VAL:
        return analyze_var_stmt_body(driver, ctx, stmt);
    case AST_STMT_RET:
        return analyze_ret_stmt(driver, ctx, stmt);
    case AST_STMT_IF:
    case AST_STMT_OR:
        return analyze_if_stmt(driver, ctx, stmt);
    case AST_STMT_FOR:
        return analyze_for_stmt(driver, ctx, stmt);
    default:
        return true;
    }
}

static bool analyze_function_body(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *stmt)
{
    if (!stmt->fun_stmt.body)
        return true; // external function
    
    // create function context with current function set
    AnalysisContext func_ctx = analysis_context_with_function(ctx, stmt->symbol);
    
    // add parameters to function local scope
    if (stmt->fun_stmt.params)
    {
        for (int i = 0; i < stmt->fun_stmt.params->count; i++)
        {
            AstNode *param = stmt->fun_stmt.params->items[i];
            if (param->kind != AST_STMT_PARAM)
                continue;
            
            // parameter type already resolved in Pass B
            Type *param_type = param->type;
            if (!param_type)
            {
                diagnostic_emit(&driver->diagnostics, DIAG_ERROR, param, ctx->module_name,
                              "parameter '%s' has no resolved type", param->param_stmt.name);
                continue;
            }
            
            // create parameter symbol in function scope
            Symbol *param_sym = symbol_create(SYMBOL_VAR, param->param_stmt.name, param_type, param);
            symbol_add(func_ctx.current_scope, param_sym);
        }
    }
    
    return analyze_stmt(driver, &func_ctx, stmt->fun_stmt.body);
}

static bool analyze_pass_c_bodies(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *root)
{
    if (!root || root->kind != AST_PROGRAM)
        return false;

    bool success = true;
    
    // analyze global initializers
    for (int i = 0; i < root->program.stmts->count; i++)
    {
        AstNode *stmt = root->program.stmts->items[i];
        
        if ((stmt->kind == AST_STMT_VAL || stmt->kind == AST_STMT_VAR) && stmt->var_stmt.init)
        {
            if (!analyze_var_stmt_body(driver, ctx, stmt))
                success = false;
        }
    }
    
    // analyze function bodies (including methods)
    for (int i = 0; i < root->program.stmts->count; i++)
    {
        AstNode *stmt = root->program.stmts->items[i];
        
        if (stmt->kind == AST_STMT_FUN && stmt->symbol && !stmt->symbol->func.is_generic)
        {
            if (!analyze_function_body(driver, ctx, stmt))
                success = false;
        }
    }
    
    return success;
}

bool semantic_driver_analyze(SemanticDriver *driver, AstNode *root, const char *entry_path)
{
    driver->program_root = root;
    driver->entry_module_name = entry_path;

    // initialize type system
    type_system_init();

    // create global scope
    SymbolTable symbol_table;
    symbol_table_init(&symbol_table);
    
    Scope *global_scope = symbol_table.global_scope;
    AnalysisContext ctx = analysis_context_create(global_scope, NULL, entry_path);

    // three-pass analysis
    bool success = true;
    
    // pass A: declare all symbols
    if (!analyze_pass_a_declarations(driver, &ctx, root))
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, NULL, entry_path,
                       "declaration pass failed");
        success = false;
    }

    // pass B: resolve signatures
    if (success && !analyze_pass_b_signatures(driver, &ctx, root))
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, NULL, entry_path,
                       "signature resolution pass failed");
        success = false;
    }

    // pass C: analyze bodies
    if (success && !analyze_pass_c_bodies(driver, &ctx, root))
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, NULL, entry_path,
                       "body analysis pass failed");
        success = false;
    }

    // process generic instantiation queue
    if (success && !process_instantiation_queue(driver, &ctx))
    {
        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, NULL, entry_path,
                       "generic instantiation failed");
        success = false;
    }

    // print diagnostics
    if (driver->diagnostics.count > 0)
    {
        diagnostic_print_all(&driver->diagnostics);
    }

    return success && !driver->diagnostics.has_errors;
}
