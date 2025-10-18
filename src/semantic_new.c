#include "semantic_new.h"
#include "symbol.h"
#include "type.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// DIAGNOSTICS
// ============================================================================

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
    
    // copy token info if available
    if (node && node->token)
    {
        diag->token = malloc(sizeof(Token));
        if (diag->token)
        {
            diag->token->kind = node->token->kind;
            diag->token->pos = node->token->pos;
            diag->token->len = node->token->len;
        }
        diag->line = 0; // TODO: calculate from source
        diag->column = 0;
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
        const char *level_str = (diag->level == DIAG_ERROR) ? "error" : (diag->level == DIAG_WARNING) ? "warning" : "note";
        
        fprintf(stderr, "%s: %s\n", level_str, diag->message);
        if (diag->file_path)
        {
            fprintf(stderr, "%s:%d:%d\n", diag->file_path, diag->line, diag->column);
        }
    }
}

// ============================================================================
// GENERIC BINDINGS
// ============================================================================

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

// ============================================================================
// SPECIALIZATION CACHE
// ============================================================================

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

// ============================================================================
// INSTANTIATION QUEUE
// ============================================================================

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

// ============================================================================
// NAME MANGLING
// ============================================================================

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

// ============================================================================
// ANALYSIS CONTEXT
// ============================================================================

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

// ============================================================================
// SEMANTIC DRIVER
// ============================================================================

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

// ============================================================================
// TYPE RESOLUTION
// ============================================================================

Type *resolve_type_in_context(SemanticDriver *driver, const AnalysisContext *ctx, AstNode *type_node)
{
    if (!type_node)
        return NULL;

    // use existing cached type if available
    if (type_node->type)
        return type_node->type;

    switch (type_node->kind)
    {
    case AST_TYPE_NAME:
    {
        const char *name = type_node->type_name.name;

        // check generic bindings first
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

        // look up in symbol table
        Symbol *sym = symbol_lookup_scope(ctx->current_scope, name);
        if (!sym)
            sym = symbol_lookup_scope(ctx->module_scope, name);
        if (!sym)
            sym = symbol_lookup_scope(ctx->global_scope, name);

        if (sym && sym->kind == SYMBOL_TYPE)
        {
            // handle generic instantiation
            if (type_node->type_name.generic_args && type_node->type_name.generic_args->count > 0)
            {
                size_t expected = sym->type_def.generic_param_count;
                size_t provided = type_node->type_name.generic_args->count;

                if (provided != expected)
                {
                    diagnostic_emit(&driver->diagnostics, DIAG_ERROR, type_node, ctx->module_name,
                                    "generic type '%s' expects %zu type arguments (got %zu)", name, expected, provided);
                    return NULL;
                }

                Type **type_args = malloc(expected * sizeof(Type *));
                for (size_t i = 0; i < expected; i++)
                {
                    AstNode *arg_node = type_node->type_name.generic_args->items[i];
                    type_args[i] = resolve_type_in_context(driver, ctx, arg_node);
                    if (!type_args[i])
                    {
                        free(type_args);
                        return NULL;
                    }
                }

                // check cache first
                Symbol *existing = specialization_cache_find(&driver->spec_cache, sym, type_args, expected);
                if (existing)
                {
                    free(type_args);
                    type_node->type = existing->type;
                    return existing->type;
                }

                // enqueue instantiation
                InstantiationKind kind = (sym->type->kind == TYPE_STRUCT) ? INST_STRUCT : INST_UNION;
                instantiation_queue_push(&driver->inst_queue, kind, sym, type_args, expected, type_node);

                free(type_args);
                // return placeholder for now - will be resolved during instantiation pass
                return NULL;
            }

            type_node->type = sym->type;
            return sym->type;
        }

        diagnostic_emit(&driver->diagnostics, DIAG_ERROR, type_node, ctx->module_name, "unknown type '%s'", name);
        return NULL;
    }

    case AST_TYPE_PARAM:
    {
        Type *bound = generic_binding_ctx_lookup(&ctx->bindings, type_node->type_param.name);
        if (!bound)
        {
            diagnostic_emit(&driver->diagnostics, DIAG_ERROR, type_node, ctx->module_name,
                            "unbound generic type parameter '%s'", type_node->type_param.name);
            return NULL;
        }
        type_node->type = bound;
        return bound;
    }

    case AST_TYPE_PTR:
    {
        Type *base = resolve_type_in_context(driver, ctx, type_node->type_ptr.base);
        if (!base)
            return NULL;
        Type *ptr_type = type_pointer_create(base);
        type_node->type = ptr_type;
        return ptr_type;
    }

    case AST_TYPE_ARRAY:
    {
        Type *elem_type = resolve_type_in_context(driver, ctx, type_node->type_array.elem_type);
        if (!elem_type)
            return NULL;
        Type *array_type = type_array_create(elem_type);
        type_node->type = array_type;
        return array_type;
    }

    case AST_TYPE_FUN:
    {
        Type *return_type = NULL;
        if (type_node->type_fun.return_type)
        {
            return_type = resolve_type_in_context(driver, ctx, type_node->type_fun.return_type);
            if (!return_type)
                return NULL;
        }

        size_t param_count = type_node->type_fun.params ? type_node->type_fun.params->count : 0;
        Type **param_types = NULL;

        if (param_count > 0)
        {
            param_types = malloc(param_count * sizeof(Type *));
            for (size_t i = 0; i < param_count; i++)
            {
                AstNode *param_node = type_node->type_fun.params->items[i];
                param_types[i] = resolve_type_in_context(driver, ctx, param_node);
                if (!param_types[i])
                {
                    free(param_types);
                    return NULL;
                }
            }
        }

        Type *func_type = type_function_create(return_type, param_types, param_count, type_node->type_fun.is_variadic);
        free(param_types);
        type_node->type = func_type;
        return func_type;
    }

    default:
        break;
    }

    // fallback to existing type resolver
    Type *resolved = type_resolve(type_node, (SymbolTable *)&ctx->current_scope);
    if (resolved)
        type_node->type = resolved;
    return resolved;
}

// ============================================================================
// MAIN ANALYSIS ENTRY POINT (STUB)
// ============================================================================

bool semantic_driver_analyze(SemanticDriver *driver, AstNode *root, const char *entry_path)
{
    driver->program_root = root;
    driver->entry_module_name = entry_path;

    // TODO: implement multi-pass analysis
    // Pass 1: resolve dependencies, build module graph
    // Pass 2: declare all symbols (types, values, functions)
    // Pass 3: resolve type signatures
    // Pass 4: analyze function bodies, enqueue specializations
    // Pass 5: instantiation loop

    diagnostic_emit(&driver->diagnostics, DIAG_ERROR, NULL, entry_path,
                    "semantic_new: analysis not yet implemented");

    return !driver->diagnostics.has_errors;
}
