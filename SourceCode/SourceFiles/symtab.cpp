/*
 * symtab.cpp – Symbol table + semantic analysis for Lexico.
 */

#include "symtab.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Safe helpers ───────────────────────────────────────────────── */

static void oom(void) {
    fprintf(stderr, "Fatal: out of memory.\n");
    exit(EXIT_FAILURE);
}
static void *xmalloc(size_t n)          { void *p = malloc(n);          if (!p && n) oom(); return p; }
static void *xrealloc(void *p, size_t n){ void *q = realloc(p, n);      if (!q && n) oom(); return q; }
static char *xstrdup(const char *s)     { if (!s) return NULL; size_t l = strlen(s); char *d = static_cast<char *>(xmalloc(l + 1)); memcpy(d, s, l + 1); return d; }

#define TYPE_DYNAMIC ((TypeKind)1000)

/* ── Function signatures (semantic-only table) ─────────────────── */

typedef struct {
    char     *name;
    TypeKind  return_type;
    TypeKind *param_types;
    size_t    param_count;
    int       line;
} FuncSymbol;

typedef struct {
    FuncSymbol *entries;
    size_t      count;
    size_t      cap;
} FuncTable;

static void functab_init(FuncTable *ft) {
    ft->entries = NULL;
    ft->count = 0;
    ft->cap = 0;
}

static void functab_free(FuncTable *ft) {
    for (size_t i = 0; i < ft->count; i++) {
        free(ft->entries[i].name);
        free(ft->entries[i].param_types);
    }
    free(ft->entries);
}

static FuncSymbol *functab_find(FuncTable *ft, const char *name) {
    for (size_t i = 0; i < ft->count; i++) {
        if (strcmp(ft->entries[i].name, name) == 0)
            return &ft->entries[i];
    }
    return NULL;
}

static int functab_add(FuncTable *ft, const FuncDefStmt *fd, int line) {
    FuncSymbol *existing = functab_find(ft, fd->name);
    if (existing) {
        fprintf(stderr,
                "Error (line %d): duplicate function '%s' (previously declared on line %d).\n",
                line, fd->name, existing->line);
        return -1;
    }

    if (ft->count == ft->cap) {
        ft->cap = ft->cap ? ft->cap * 2 : 8;
        ft->entries = static_cast<FuncSymbol *>(xrealloc(ft->entries, ft->cap * sizeof(FuncSymbol)));
    }

    FuncSymbol *f = &ft->entries[ft->count++];
    f->name = xstrdup(fd->name);
    f->return_type = fd->return_type;
    f->param_count = fd->param_count;
    f->line = line;

    if (fd->param_count == 0) {
        f->param_types = NULL;
    } else {
        f->param_types = static_cast<TypeKind *>(xmalloc(fd->param_count * sizeof(TypeKind)));
        for (size_t i = 0; i < fd->param_count; i++)
            f->param_types[i] = fd->params[i].type;
    }
    return 0;
}

/* ── Symbol table life-cycle ────────────────────────────────────── */

SymTable *symtab_new(void) {
    SymTable *t = static_cast<SymTable *>(xmalloc(sizeof *t));
    t->entries = NULL;
    t->count   = 0;
    t->cap     = 0;
    return t;
}

void symtab_free(SymTable *t) {
    if (!t) return;
    for (size_t i = 0; i < t->count; i++)
        free(t->entries[i].name);
    free(t->entries);
    free(t);
}

/* ── Lookup / insert ────────────────────────────────────────────── */

Symbol *symtab_lookup(SymTable *t, const char *name) {
    if (!t || !name) return NULL;
    for (size_t i = 0; i < t->count; i++) {
        if (strcmp(t->entries[i].name, name) == 0)
            return &t->entries[i];
    }
    return NULL;
}

static int symtab_add_kind(SymTable *t, const char *name,
                           SymbolKind kind, TypeKind type, int line) {
    Symbol *existing = symtab_lookup(t, name);
    if (existing) {
        fprintf(stderr,
                "Error (line %d): duplicate declaration of '%s' (previously declared on line %d).\n",
                line, name, existing->line);
        return -1;
    }
    if (t->count == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 8;
        t->entries = static_cast<Symbol *>(xrealloc(t->entries, t->cap * sizeof(Symbol)));
    }
    Symbol *s = &t->entries[t->count++];
    s->name = xstrdup(name);
    s->kind = kind;
    s->type = type;
    s->line = line;
    return 0;
}

int symtab_add(SymTable *t, const char *name, TypeKind type, int line) {
    return symtab_add_kind(t, name, SYM_VAR, type, line);
}

/* ── Type helpers ───────────────────────────────────────────────── */

static const char *type_name(TypeKind k) {
    switch (k) {
        case TYPE_INT:    return "int";
        case TYPE_FLOAT:  return "float";
        case TYPE_CHAR:   return "char";
        case TYPE_STRING: return "string";
        case TYPE_BOOL:   return "bool";
    }
    if (k == TYPE_DYNAMIC) return "dynamic";
    return "unknown";
}

static int type_compatible(TypeKind expected, TypeKind actual) {
    if (expected == actual) return 1;
    if (expected == TYPE_STRING && actual == TYPE_CHAR) return 1;
    if (expected == TYPE_CHAR && actual == TYPE_STRING) return 1;
    if (expected == TYPE_FLOAT && actual == TYPE_INT) return 1;
    if (expected == TYPE_INT && actual == TYPE_FLOAT) return 1;
    return 0;
}

static int text_mesh_compatible(TypeKind actual) {
    return actual != TYPE_DYNAMIC;
}

static int scalar_type(TypeKind t) {
    return t == TYPE_INT || t == TYPE_FLOAT || t == TYPE_CHAR ||
           t == TYPE_STRING || t == TYPE_BOOL;
}

static int convert_compatible(TypeKind from, TypeKind to) {
    if (!scalar_type(from)) return 0;
    if (to == TYPE_INT || to == TYPE_FLOAT || to == TYPE_CHAR || to == TYPE_STRING)
        return 1;
    return 0;
}

/* ── Forward declarations ───────────────────────────────────────── */

static int analyze_stmts(SymTable *t, FuncTable *ft,
                         Stmt *const *stmts, size_t count,
                         int in_function, TypeKind current_return,
                         int allow_function_defs,
                         int in_file_block,
                         const char *collection_context,
                         const char *collection_index_context,
                         const char *matrix_context);

/* ── Expression / call checks ───────────────────────────────────── */

static TypeKind infer_expr_type(SymTable *t, FuncTable *ft,
                                const Expr *e, int line, int *errors);

static TypeKind check_call(SymTable *t, FuncTable *ft,
                           const char *name,
                           Expr *const *args, size_t arg_count,
                           int line, int *errors) {
    FuncSymbol *f = functab_find(ft, name);
    if (!f) {
        fprintf(stderr,
                "Error (line %d): call to undeclared function '%s'.\n",
                line, name);
        (*errors)++;
        for (size_t i = 0; i < arg_count; i++)
            (void)infer_expr_type(t, ft, args[i], line, errors);
        return TYPE_INT;
    }

    if (arg_count != f->param_count) {
        fprintf(stderr,
                "Error (line %d): function '%s' expects %zu argument(s), got %zu.\n",
                line, name, f->param_count, arg_count);
        (*errors)++;
    }

    size_t n = arg_count < f->param_count ? arg_count : f->param_count;
    for (size_t i = 0; i < n; i++) {
        TypeKind at = infer_expr_type(t, ft, args[i], line, errors);
        if (!type_compatible(f->param_types[i], at)) {
            fprintf(stderr,
                    "Error (line %d): argument %zu of '%s' has type %s, expected %s.\n",
                    line, i + 1, name, type_name(at), type_name(f->param_types[i]));
            (*errors)++;
        }
    }
    for (size_t i = n; i < arg_count; i++)
        (void)infer_expr_type(t, ft, args[i], line, errors);

    return f->return_type;
}

static TypeKind infer_expr_type(SymTable *t, FuncTable *ft,
                                const Expr *e, int line, int *errors) {
    if (!e) return TYPE_INT;

    switch (e->kind) {
    case EXPR_LITERAL:
        switch (e->as.lit.kind) {
            case LIT_INT:    return TYPE_INT;
            case LIT_FLOAT:  return TYPE_FLOAT;
            case LIT_CHAR:   return TYPE_CHAR;
            case LIT_STRING: return TYPE_STRING;
            case LIT_BOOL:   return TYPE_BOOL;
        }
        return TYPE_INT;

    case EXPR_IDENT: {
        Symbol *sym = symtab_lookup(t, e->as.ident);
        if (!sym) {
            fprintf(stderr,
                    "Error (line %d): use of undeclared identifier '%s'.\n",
                    line, e->as.ident);
            (*errors)++;
            return TYPE_INT;
        }
        if (sym->kind != SYM_VAR) {
            fprintf(stderr,
                    "Error (line %d): '%s' is a collection, not a scalar value.\n",
                    line, e->as.ident);
            (*errors)++;
            return TYPE_INT;
        }
        return sym->type;
    }

    case EXPR_BINARY: {
        TypeKind lt = infer_expr_type(t, ft, e->as.bin.left, line, errors);
        TypeKind rt = infer_expr_type(t, ft, e->as.bin.right, line, errors);
        if (lt == TYPE_DYNAMIC || rt == TYPE_DYNAMIC) {
            fprintf(stderr,
                    "Error (line %d): arithmetic on dynamic table values is not allowed directly.\n",
                    line);
            (*errors)++;
            return TYPE_INT;
        }
        if (lt == TYPE_STRING || rt == TYPE_STRING) {
            if ((lt == TYPE_STRING && rt == TYPE_STRING ||
                 lt == TYPE_STRING && rt == TYPE_CHAR  ||
                 lt == TYPE_CHAR  && rt == TYPE_STRING) &&
                (e->as.bin.op == OP_ADD || e->as.bin.op == OP_SUB)) {
                return TYPE_STRING;
            }
            fprintf(stderr,
                    "Error (line %d): string operations support only 'plus' and 'minus' between two strings.\n",
                    line);
            (*errors)++;
            return TYPE_INT;
        }
        if (lt == TYPE_BOOL || rt == TYPE_BOOL) {
            fprintf(stderr,
                    "Error (line %d): arithmetic requires numeric/char operands, got %s and %s.\n",
                    line, type_name(lt), type_name(rt));
            (*errors)++;
            return TYPE_INT;
        }
        if (lt == TYPE_FLOAT || rt == TYPE_FLOAT) return TYPE_FLOAT;
        return TYPE_INT;
    }

    case EXPR_UNARY_NEG: {
        TypeKind ot = infer_expr_type(t, ft, e->as.operand, line, errors);
        if (ot == TYPE_DYNAMIC) {
            fprintf(stderr,
                    "Error (line %d): table dynamic values cannot be negated directly.\n",
                    line);
            (*errors)++;
            return TYPE_INT;
        }
        if (ot == TYPE_STRING || ot == TYPE_BOOL) {
            fprintf(stderr,
                    "Error (line %d): unary minus requires numeric/char operand, got %s.\n",
                    line, type_name(ot));
            (*errors)++;
            return TYPE_INT;
        }
        return ot;
    }

    case EXPR_CALL:
        return check_call(t, ft, e->as.call.name,
                          e->as.call.args, e->as.call.count,
                          line, errors);

    case EXPR_INDEX: {
        Symbol *sym = symtab_lookup(t, e->as.index.name);
        if (!sym) {
            fprintf(stderr,
                    "Error (line %d): '%s' is undeclared.\n",
                    line, e->as.index.name);
            (*errors)++;
            (void)infer_expr_type(t, ft, e->as.index.index, line, errors);
            return TYPE_INT;
        }
        if (sym->kind != SYM_ARRAY && sym->kind != SYM_TABLE &&
            !(sym->kind == SYM_VAR && sym->type == TYPE_STRING)) {
            fprintf(stderr,
                    "Error (line %d): '%s' is not indexable (expected mesh/table/string).\n",
                    line, e->as.index.name);
            (*errors)++;
        }
        TypeKind ik = infer_expr_type(t, ft, e->as.index.index, line, errors);
        if (ik != TYPE_INT) {
            fprintf(stderr,
                    "Error (line %d): index for '%s' must be int.\n",
                    line, e->as.index.name);
            (*errors)++;
        }
        if (sym->kind == SYM_ARRAY) return sym->type;
        if (sym->kind == SYM_VAR && sym->type == TYPE_STRING) return TYPE_CHAR;
        return TYPE_DYNAMIC;
    }

    case EXPR_MATRIX_INDEX: {
        Symbol *sym = symtab_lookup(t, e->as.matrix_index.name);
        if (!sym) {
            fprintf(stderr,
                    "Error (line %d): matrix '%s' is undeclared.\n",
                    line, e->as.matrix_index.name);
            (*errors)++;
            (void)infer_expr_type(t, ft, e->as.matrix_index.row, line, errors);
            (void)infer_expr_type(t, ft, e->as.matrix_index.col, line, errors);
            return TYPE_INT;
        }
        if (sym->kind != SYM_MATRIX) {
            fprintf(stderr,
                    "Error (line %d): '%s' is not a matrix.\n",
                    line, e->as.matrix_index.name);
            (*errors)++;
        }
        TypeKind rk = infer_expr_type(t, ft, e->as.matrix_index.row, line, errors);
        TypeKind ck = infer_expr_type(t, ft, e->as.matrix_index.col, line, errors);
        if (rk != TYPE_INT) {
            fprintf(stderr,
                    "Error (line %d): matrix row index for '%s' must be int.\n",
                    line, e->as.matrix_index.name);
            (*errors)++;
        }
        if (ck != TYPE_INT) {
            fprintf(stderr,
                    "Error (line %d): matrix column index for '%s' must be int.\n",
                    line, e->as.matrix_index.name);
            (*errors)++;
        }
        if (sym->type == TYPE_DYNAMIC) {
            fprintf(stderr,
                    "Error (line %d): indexing dynamic matrix '%s' in scalar expressions is not supported yet.\n",
                    line, e->as.matrix_index.name);
            (*errors)++;
            return TYPE_INT;
        }
        return sym->type;
    }

    case EXPR_LENGTH: {
        Symbol *sym = symtab_lookup(t, e->as.length_name);
        if (!sym) {
            fprintf(stderr,
                    "Error (line %d): collection '%s' is undeclared.\n",
                    line, e->as.length_name);
            (*errors)++;
            return TYPE_INT;
        }
        if (sym->kind != SYM_ARRAY && sym->kind != SYM_TABLE && sym->kind != SYM_MATRIX) {
            fprintf(stderr,
                    "Error (line %d): size works only on collections, but '%s' is not an array/table.\n",
                    line, e->as.length_name);
            (*errors)++;
        }
        return TYPE_INT;
    }

    case EXPR_ROW_LENGTH: {
        Symbol *sym = symtab_lookup(t, e->as.row_length_name);
        if (!sym) {
            fprintf(stderr,
                    "Error (line %d): matrix '%s' is undeclared.\n",
                    line, e->as.row_length_name);
            (*errors)++;
            return TYPE_INT;
        }
        if (sym->kind != SYM_MATRIX) {
            fprintf(stderr,
                    "Error (line %d): row length works only on matrix mesh, but '%s' is not a matrix.\n",
                    line, e->as.row_length_name);
            (*errors)++;
        }
        return TYPE_INT;
    }

    case EXPR_COL_LENGTH: {
        Symbol *sym = symtab_lookup(t, e->as.col_length_name);
        if (!sym) {
            fprintf(stderr,
                    "Error (line %d): matrix '%s' is undeclared.\n",
                    line, e->as.col_length_name);
            (*errors)++;
            return TYPE_INT;
        }
        if (sym->kind != SYM_MATRIX) {
            fprintf(stderr,
                    "Error (line %d): column length works only on matrix mesh, but '%s' is not a matrix.\n",
                    line, e->as.col_length_name);
            (*errors)++;
        }
        return TYPE_INT;
    }

    case EXPR_STRLEN: {
        TypeKind vk = infer_expr_type(t, ft, e->as.strlen_value, line, errors);
        if (vk != TYPE_STRING) {
            fprintf(stderr,
                    "Error (line %d): length works only on string expressions, got %s.\n",
                    line, type_name(vk));
            (*errors)++;
        }
        return TYPE_INT;
    }

    case EXPR_EXTRACT: {
        Symbol *sym = symtab_lookup(t, e->as.extract.source_name);
        TypeKind vk = infer_expr_type(t, ft, e->as.extract.value, line, errors);
        if (!sym) {
            fprintf(stderr,
                    "Error (line %d): string '%s' is undeclared.\n",
                    line, e->as.extract.source_name);
            (*errors)++;
            return TYPE_STRING;
        }
        if (sym->kind != SYM_VAR || sym->type != TYPE_STRING) {
            fprintf(stderr,
                    "Error (line %d): '%s' must be a string variable for extract.\n",
                    line, e->as.extract.source_name);
            (*errors)++;
        }
        if (vk != TYPE_STRING) {
            fprintf(stderr,
                    "Error (line %d): extract value must be string, got %s.\n",
                    line, type_name(vk));
            (*errors)++;
        }
        return TYPE_STRING;
    }

    case EXPR_EXTRACT_RANGE: {
        Symbol *sym = symtab_lookup(t, e->as.extract_range.source_name);
        TypeKind fk = infer_expr_type(t, ft, e->as.extract_range.from, line, errors);
        TypeKind tk = infer_expr_type(t, ft, e->as.extract_range.to, line, errors);
        if (!sym) {
            fprintf(stderr,
                    "Error (line %d): string '%s' is undeclared.\n",
                    line, e->as.extract_range.source_name);
            (*errors)++;
            return TYPE_STRING;
        }
        if (sym->kind != SYM_VAR || sym->type != TYPE_STRING) {
            fprintf(stderr,
                    "Error (line %d): '%s' must be a string variable for extract range.\n",
                    line, e->as.extract_range.source_name);
            (*errors)++;
        }
        if (fk != TYPE_INT || tk != TYPE_INT) {
            fprintf(stderr,
                    "Error (line %d): extract range bounds must be int values.\n",
                    line);
            (*errors)++;
        }
        return TYPE_STRING;
    }

    case EXPR_COUNT: {
        Symbol *sym = symtab_lookup(t, e->as.count.collection_name);
        TypeKind vk = infer_expr_type(t, ft, e->as.count.value, line, errors);
        if (!sym) {
            fprintf(stderr,
                    "Error (line %d): collection '%s' is undeclared.\n",
                    line, e->as.count.collection_name);
            (*errors)++;
            return TYPE_INT;
        }
        if (sym->kind != SYM_ARRAY && sym->kind != SYM_TABLE && sym->kind != SYM_MATRIX) {
            fprintf(stderr,
                "Error (line %d): '%s' is not an array/table/matrix.\n",
                    line, e->as.count.collection_name);
            (*errors)++;
            return TYPE_INT;
        }

        if (vk == TYPE_DYNAMIC) {
            fprintf(stderr,
                    "Error (line %d): dynamic table values cannot be used as count target values directly.\n",
                    line);
            (*errors)++;
            return TYPE_INT;
        }

                if ((sym->kind == SYM_ARRAY || sym->kind == SYM_MATRIX) &&
                        sym->type != TYPE_DYNAMIC &&
                        !(((sym->type == TYPE_STRING || sym->type == TYPE_CHAR) && text_mesh_compatible(vk)) ||
                            type_compatible(sym->type, vk))) {
            fprintf(stderr,
                    "Error (line %d): count value for '%s' has type %s, expected %s.\n",
                    line, e->as.count.collection_name, type_name(vk), type_name(sym->type));
            (*errors)++;
        }

        return TYPE_INT;
    }

    case EXPR_CHECK: {
        Symbol *sym = symtab_lookup(t, e->as.check.collection_name);
        TypeKind vk = infer_expr_type(t, ft, e->as.check.value, line, errors);
        if (!sym) {
            fprintf(stderr,
                    "Error (line %d): collection '%s' is undeclared.\n",
                    line, e->as.check.collection_name);
            (*errors)++;
            return TYPE_BOOL;
        }
        if (sym->kind == SYM_VAR && sym->type == TYPE_STRING) {
            if (!(vk == TYPE_STRING || vk == TYPE_CHAR)) {
                fprintf(stderr,
                        "Error (line %d): check value for string '%s' must be char/string, got %s.\n",
                        line, e->as.check.collection_name, type_name(vk));
                (*errors)++;
            }
            return TYPE_BOOL;
        }

        if (sym->kind != SYM_ARRAY && sym->kind != SYM_TABLE && sym->kind != SYM_MATRIX) {
            fprintf(stderr,
                "Error (line %d): '%s' is not an array/table/matrix.\n",
                    line, e->as.check.collection_name);
            (*errors)++;
            return TYPE_BOOL;
        }

        if (vk == TYPE_DYNAMIC) {
            fprintf(stderr,
                    "Error (line %d): dynamic table values cannot be used as check target values directly.\n",
                    line);
            (*errors)++;
            return TYPE_BOOL;
        }

                if ((sym->kind == SYM_ARRAY || sym->kind == SYM_MATRIX) &&
                        sym->type != TYPE_DYNAMIC &&
                        !(((sym->type == TYPE_STRING || sym->type == TYPE_CHAR) && text_mesh_compatible(vk)) ||
                            type_compatible(sym->type, vk))) {
            fprintf(stderr,
                    "Error (line %d): check value for '%s' has type %s, expected %s.\n",
                    line, e->as.check.collection_name, type_name(vk), type_name(sym->type));
            (*errors)++;
        }

        return TYPE_BOOL;
    }

    case EXPR_POSITION: {
        Symbol *sym = symtab_lookup(t, e->as.position.collection_name);
        TypeKind vk = infer_expr_type(t, ft, e->as.position.value, line, errors);
        if (!sym) {
            fprintf(stderr,
                    "Error (line %d): collection '%s' is undeclared.\n",
                    line, e->as.position.collection_name);
            (*errors)++;
            return TYPE_INT;
        }

        if (sym->kind == SYM_VAR && sym->type == TYPE_STRING) {
            if (!(vk == TYPE_STRING || vk == TYPE_CHAR)) {
                fprintf(stderr,
                        "Error (line %d): position value for string '%s' must be char/string, got %s.\n",
                        line, e->as.position.collection_name, type_name(vk));
                (*errors)++;
            }
            return TYPE_INT;
        }

        if (sym->kind != SYM_ARRAY && sym->kind != SYM_TABLE) {
            fprintf(stderr,
                    "Error (line %d): '%s' is not an array/table.\n",
                    line, e->as.position.collection_name);
            (*errors)++;
            return TYPE_INT;
        }

        if (vk == TYPE_DYNAMIC) {
            fprintf(stderr,
                    "Error (line %d): dynamic table values cannot be used as position target values directly.\n",
                    line);
            (*errors)++;
            return TYPE_INT;
        }

        if (sym->kind == SYM_ARRAY &&
            !(((sym->type == TYPE_STRING || sym->type == TYPE_CHAR) && text_mesh_compatible(vk)) ||
              type_compatible(sym->type, vk))) {
            fprintf(stderr,
                    "Error (line %d): position value for '%s' has type %s, expected %s.\n",
                    line, e->as.position.collection_name, type_name(vk), type_name(sym->type));
            (*errors)++;
        }

        return TYPE_INT;
    }

    case EXPR_CONVERT: {
        TypeKind from_t = infer_expr_type(t, ft, e->as.convert.value, line, errors);
        if (!convert_compatible(from_t, e->as.convert.to_type)) {
            fprintf(stderr,
                    "Error (line %d): cannot convert expression from %s to %s.\n",
                    line, type_name(from_t), type_name(e->as.convert.to_type));
            (*errors)++;
            return TYPE_INT;
        }
        return e->as.convert.to_type;
    }
    }

    return TYPE_INT;
}

static int check_expr(SymTable *t, FuncTable *ft, const Expr *e, int line) {
    int errors = 0;
    (void)infer_expr_type(t, ft, e, line, &errors);
    return errors;
}

/* ── Condition checks ───────────────────────────────────────────── */

static int check_cond(SymTable *t, FuncTable *ft, const CondExpr *c, int line) {
    if (!c) return 0;
    int errors = 0;
    switch (c->kind) {
    case COND_CMP:
        errors += check_expr(t, ft, c->as.cmp.left, line);
        errors += check_expr(t, ft, c->as.cmp.right, line);
        break;

    case COND_AND:
    case COND_OR:
        errors += check_cond(t, ft, c->as.logic.left, line);
        errors += check_cond(t, ft, c->as.logic.right, line);
        break;

    case COND_NOT:
        errors += check_cond(t, ft, c->as.operand, line);
        break;
    }
    return errors;
}

static int analyze_stmts(SymTable *t, FuncTable *ft,
                         Stmt *const *stmts, size_t count,
                         int in_function, TypeKind current_return,
                         int allow_function_defs,
                         int in_file_block,
                         const char *collection_context,
                         const char *collection_index_context,
                         const char *matrix_context) {
    int errors = 0;

    for (size_t i = 0; i < count; i++) {
        Stmt *s = stmts[i];
        if (!s) continue;

        switch (s->kind) {
        case STMT_DECL: {
            const DeclStmt *d = &s->as.decl;
            for (size_t j = 0; j < d->count; j++) {
                if (symtab_add(t, d->names[j], d->type, s->line) != 0)
                    errors++;

                if (d->values[j]) {
                    TypeKind rhs = infer_expr_type(t, ft, d->values[j], s->line, &errors);
                    if (!type_compatible(d->type, rhs)) {
                        fprintf(stderr,
                                "Error (line %d): initializer for '%s' has type %s, expected %s.\n",
                                s->line, d->names[j], type_name(rhs), type_name(d->type));
                        errors++;
                    }
                }
            }
            break;
        }

        case STMT_ARRAY_DECL: {
            const ArrayDeclStmt *ad = &s->as.array_decl;
            if (symtab_add_kind(t, ad->name, SYM_ARRAY, ad->elem_type, s->line) != 0)
                errors++;
            TypeKind sk = infer_expr_type(t, ft, ad->size, s->line, &errors);
            if (sk != TYPE_INT) {
                fprintf(stderr,
                        "Error (line %d): array size for '%s' must be int.\n",
                        s->line, ad->name);
                errors++;
            }
            break;
        }

        case STMT_TABLE_DECL: {
            const TableDeclStmt *td = &s->as.table_decl;
            if (symtab_add_kind(t, td->name, SYM_TABLE, TYPE_INT, s->line) != 0)
                errors++;
            TypeKind sk = infer_expr_type(t, ft, td->size, s->line, &errors);
            if (sk != TYPE_INT) {
                fprintf(stderr,
                        "Error (line %d): table size for '%s' must be int.\n",
                        s->line, td->name);
                errors++;
            }
            break;
        }

        case STMT_MATRIX_DECL: {
            const MatrixDeclStmt *md = &s->as.matrix_decl;
            TypeKind decl_type = md->is_dynamic ? TYPE_DYNAMIC : md->elem_type;
            if (symtab_add_kind(t, md->name, SYM_MATRIX, decl_type, s->line) != 0)
                errors++;

            TypeKind rk = infer_expr_type(t, ft, md->rows, s->line, &errors);
            TypeKind ck = infer_expr_type(t, ft, md->cols, s->line, &errors);
            if (rk != TYPE_INT) {
                fprintf(stderr,
                        "Error (line %d): rows for matrix '%s' must be int.\n",
                        s->line, md->name);
                errors++;
            }
            if (ck != TYPE_INT) {
                fprintf(stderr,
                        "Error (line %d): cols for matrix '%s' must be int.\n",
                        s->line, md->name);
                errors++;
            }
            break;
        }

        case STMT_COLLECTION_SET: {
            const CollectionSetStmt *cs = &s->as.collection_set;
            int in_collection_loop = (collection_context != NULL);
            if (in_collection_loop && (cs->name != NULL || cs->index != NULL)) {
                fprintf(stderr,
                        "Error (line %d): inside 'for ... in <mesh>' use only 'set value to <expr>'.\n",
                        s->line);
                errors++;
            }
            const char *target = cs->name ? cs->name : collection_context;
            if (!target) {
                fprintf(stderr,
                "Error (line %d): 'set value ...' used outside a bound collection loop.\n",
                        s->line);
                errors++;
            if (cs->index) errors += check_expr(t, ft, cs->index, s->line);
                errors += check_expr(t, ft, cs->value, s->line);
                break;
            }

            Symbol *sym = symtab_lookup(t, target);
            if (!sym) {
                fprintf(stderr,
                        "Error (line %d): collection '%s' is undeclared.\n",
                        s->line, target);
                errors++;
                if (cs->index) errors += check_expr(t, ft, cs->index, s->line);
                errors += check_expr(t, ft, cs->value, s->line);
                break;
            }
            if (sym->kind != SYM_ARRAY && sym->kind != SYM_TABLE && sym->kind != SYM_MATRIX) {
                fprintf(stderr,
                        "Error (line %d): '%s' is not an array/table/matrix.\n",
                        s->line, target);
                errors++;
            }

            if (cs->index) {
                TypeKind ik = infer_expr_type(t, ft, cs->index, s->line, &errors);
                if (ik != TYPE_INT) {
                    fprintf(stderr,
                            "Error (line %d): collection index for '%s' must be int.\n",
                            s->line, target);
                    errors++;
                }
            } else if (!collection_index_context) {
                fprintf(stderr,
                        "Error (line %d): contextual 'set value to ...' requires a bound loop index.\n",
                        s->line);
                errors++;
            }

            TypeKind vk = infer_expr_type(t, ft, cs->value, s->line, &errors);
                        if (sym->kind == SYM_ARRAY &&
                            !(((sym->type == TYPE_STRING || sym->type == TYPE_CHAR) && text_mesh_compatible(vk)) ||
                                    type_compatible(sym->type, vk))) {
                fprintf(stderr,
                        "Error (line %d): array '%s' expects %s values, got %s.\n",
                        s->line, target, type_name(sym->type), type_name(vk));
                errors++;
            }
            break;
        }

        case STMT_MATRIX_SET: {
            const MatrixSetStmt *ms = &s->as.matrix_set;
            int in_matrix_loop = (matrix_context != NULL);
            if (in_matrix_loop && ms->name != NULL) {
                fprintf(stderr,
                        "Error (line %d): inside 'for ... in <matrix>' use 'set <value> at <row>,<col>' without matrix name.\n",
                        s->line);
                errors++;
            }
            if (!in_matrix_loop && ms->name == NULL) {
                fprintf(stderr,
                        "Error (line %d): contextual matrix set is only valid inside a matrix loop.\n",
                        s->line);
                errors++;
            }

            const char *target = ms->name ? ms->name : matrix_context;
            if (!target) {
                errors += check_expr(t, ft, ms->row, s->line);
                errors += check_expr(t, ft, ms->col, s->line);
                errors += check_expr(t, ft, ms->value, s->line);
                break;
            }

            Symbol *sym = symtab_lookup(t, target);
            if (!sym) {
                fprintf(stderr,
                        "Error (line %d): matrix '%s' is undeclared.\n",
                        s->line, target);
                errors++;
                errors += check_expr(t, ft, ms->row, s->line);
                errors += check_expr(t, ft, ms->col, s->line);
                errors += check_expr(t, ft, ms->value, s->line);
                break;
            }
            if (sym->kind != SYM_MATRIX) {
                fprintf(stderr,
                        "Error (line %d): '%s' is not a matrix.\n",
                        s->line, target);
                errors++;
            }

            TypeKind rk = infer_expr_type(t, ft, ms->row, s->line, &errors);
            TypeKind ck = infer_expr_type(t, ft, ms->col, s->line, &errors);
            if (rk != TYPE_INT) {
                fprintf(stderr,
                        "Error (line %d): row index for matrix '%s' must be int.\n",
                        s->line, target);
                errors++;
            }
            if (ck != TYPE_INT) {
                fprintf(stderr,
                        "Error (line %d): column index for matrix '%s' must be int.\n",
                        s->line, target);
                errors++;
            }

            TypeKind vk = infer_expr_type(t, ft, ms->value, s->line, &errors);
                        if (sym->type != TYPE_DYNAMIC &&
                                !(((sym->type == TYPE_STRING || sym->type == TYPE_CHAR) && text_mesh_compatible(vk)) ||
                  type_compatible(sym->type, vk))) {
                fprintf(stderr,
                        "Error (line %d): matrix '%s' expects %s values, got %s.\n",
                    s->line, target, type_name(sym->type), type_name(vk));
                errors++;
            }
            break;
        }

        case STMT_COLLECTION_SET_INPUT: {
            const CollectionSetInputStmt *cs = &s->as.collection_set_input;
            int in_collection_loop = (collection_context != NULL);
            if (in_collection_loop && (cs->name != NULL || cs->index != NULL)) {
                fprintf(stderr,
                        "Error (line %d): inside 'for ... in <mesh>' use only 'set value to take user input ...'.\n",
                        s->line);
                errors++;
            }
            const char *target = cs->name ? cs->name : collection_context;
            if (!target) {
                fprintf(stderr,
                        "Error (line %d): 'set value ... to take user input' used outside a bound collection loop.\n",
                        s->line);
                errors++;
                if (cs->index) errors += check_expr(t, ft, cs->index, s->line);
                for (size_t k = 0; k < cs->message_count; k++)
                    errors += check_expr(t, ft, cs->message_parts[k], s->line);
                break;
            }

            Symbol *sym = symtab_lookup(t, target);
            if (!sym) {
                fprintf(stderr,
                        "Error (line %d): collection '%s' is undeclared.\n",
                        s->line, target);
                errors++;
                if (cs->index) errors += check_expr(t, ft, cs->index, s->line);
                for (size_t k = 0; k < cs->message_count; k++)
                    errors += check_expr(t, ft, cs->message_parts[k], s->line);
                break;
            }
            if (sym->kind != SYM_ARRAY && sym->kind != SYM_TABLE && sym->kind != SYM_MATRIX) {
                fprintf(stderr,
                        "Error (line %d): '%s' is not an array/table/matrix.\n",
                        s->line, target);
                errors++;
            }

            if (cs->index) {
                TypeKind ik = infer_expr_type(t, ft, cs->index, s->line, &errors);
                if (ik != TYPE_INT) {
                    fprintf(stderr,
                            "Error (line %d): collection index for '%s' must be int.\n",
                            s->line, target);
                    errors++;
                }
            } else if (!collection_index_context) {
                fprintf(stderr,
                        "Error (line %d): contextual 'set value to take user input' requires a bound loop index.\n",
                        s->line);
                errors++;
            }

            for (size_t k = 0; k < cs->message_count; k++)
                errors += check_expr(t, ft, cs->message_parts[k], s->line);
            break;
        }

        case STMT_COLLECTION_APPEND: {
            const CollectionAppendStmt *ca = &s->as.collection_append;
            int in_collection_loop = (collection_context != NULL);
            if (in_collection_loop && (ca->name != NULL || ca->index != NULL)) {
                fprintf(stderr,
                        "Error (line %d): inside 'for ... in <mesh>' use only 'append <value>'.\n",
                        s->line);
                errors++;
            }
            const char *target = ca->name ? ca->name : collection_context;
            if (!target) {
                fprintf(stderr,
                        "Error (line %d): 'append <value>' used outside a bound collection loop; use 'append <value> to <collection>'.\n",
                        s->line);
                errors++;
                errors += check_expr(t, ft, ca->value, s->line);
                if (ca->index) errors += check_expr(t, ft, ca->index, s->line);
                break;
            }

            Symbol *sym = symtab_lookup(t, target);
            if (!sym) {
                fprintf(stderr,
                        "Error (line %d): collection '%s' is undeclared.\n",
                        s->line, target);
                errors++;
                errors += check_expr(t, ft, ca->value, s->line);
                if (ca->index) errors += check_expr(t, ft, ca->index, s->line);
                break;
            }
            if (sym->kind != SYM_ARRAY && sym->kind != SYM_TABLE && sym->kind != SYM_MATRIX) {
                fprintf(stderr,
                        "Error (line %d): '%s' is not an array/table/matrix.\n",
                        s->line, target);
                errors++;
            }

            TypeKind vk = infer_expr_type(t, ft, ca->value, s->line, &errors);
                        if ((sym->kind == SYM_ARRAY || sym->kind == SYM_MATRIX) &&
                                sym->type != TYPE_DYNAMIC &&
                                !(((sym->type == TYPE_STRING || sym->type == TYPE_CHAR) && text_mesh_compatible(vk)) ||
                                    type_compatible(sym->type, vk))) {
                fprintf(stderr,
                                                "Error (line %d): collection '%s' expects %s values, got %s.\n",
                        s->line, target, type_name(sym->type), type_name(vk));
                errors++;
            }
            if (sym->kind == SYM_TABLE && vk == TYPE_DYNAMIC) {
                fprintf(stderr,
                        "Error (line %d): table dynamic values cannot be appended directly.\n",
                        s->line);
                errors++;
            }

            if (ca->index) {
                TypeKind ik = infer_expr_type(t, ft, ca->index, s->line, &errors);
                if (ik != TYPE_INT) {
                    fprintf(stderr,
                            "Error (line %d): append index for '%s' must be int.\n",
                            s->line, target);
                    errors++;
                }
            }
            break;
        }

        case STMT_COLLECTION_SORT: {
            const CollectionSortStmt *cs = &s->as.collection_sort;
            Symbol *sym = symtab_lookup(t, cs->name);
            if (!sym) {
                fprintf(stderr,
                        "Error (line %d): mesh '%s' is undeclared.\n",
                        s->line, cs->name);
                errors++;
                break;
            }
            if (sym->kind != SYM_ARRAY) {
                fprintf(stderr,
                        "Error (line %d): sort works only on typed meshes; '%s' is not a typed mesh.\n",
                        s->line, cs->name);
                errors++;
                break;
            }
            if (sym->type != TYPE_INT && sym->type != TYPE_FLOAT &&
                sym->type != TYPE_CHAR && sym->type != TYPE_STRING) {
                fprintf(stderr,
                        "Error (line %d): sort on mesh '%s' supports only int, float, char, and string meshes.\n",
                        s->line, cs->name);
                errors++;
            }
            break;
        }

        case STMT_PRINT: {
            const PrintStmt *p = &s->as.print;
            for (size_t j = 0; j < p->count; j++) {
                const Expr *arg = p->args[j];
                if (arg && arg->kind == EXPR_IDENT) {
                    Symbol *sym = symtab_lookup(t, arg->as.ident);
                    if (!sym) {
                        fprintf(stderr,
                                "Error (line %d): use of undeclared identifier '%s'.\n",
                                s->line, arg->as.ident);
                        errors++;
                        continue;
                    }
                    if (sym->kind == SYM_ARRAY || sym->kind == SYM_TABLE || sym->kind == SYM_MATRIX) {
                        /* Predefined print behavior for meshes/tables/matrices. */
                        continue;
                    }
                }
                errors += check_expr(t, ft, p->args[j], s->line);
            }
            break;
        }

        case STMT_SET: {
            const SetStmt *st = &s->as.set;
            Symbol *target = symtab_lookup(t, st->target);
            if (!target) {
                fprintf(stderr,
                        "Error (line %d): assignment to undeclared variable '%s'.\n",
                        s->line, st->target);
                errors++;
            } else if (target->kind != SYM_VAR) {
                fprintf(stderr,
                        "Error (line %d): '%s' is a collection; use 'set %s at <index> to <value>'.\n",
                        s->line, st->target, st->target);
                errors++;
            }

            TypeKind rhs = infer_expr_type(t, ft, st->value, s->line, &errors);
            if (target && target->kind == SYM_VAR && !type_compatible(target->type, rhs)) {
                fprintf(stderr,
                        "Error (line %d): assignment to '%s' has type %s, expected %s.\n",
                        s->line, st->target, type_name(rhs), type_name(target->type));
                errors++;
            }
            break;
        }

        case STMT_CONVERT: {
            const ConvertStmt *cv = &s->as.convert;
            Symbol *target = symtab_lookup(t, cv->target);

            if (!target) {
                fprintf(stderr,
                        "Error (line %d): convert target '%s' is undeclared.\n",
                        s->line, cv->target);
                errors++;
                break;
            }

            if (target->kind != SYM_VAR) {
                fprintf(stderr,
                        "Error (line %d): convert target '%s' must be a scalar variable.\n",
                        s->line, cv->target);
                errors++;
                break;
            }

            if (!convert_compatible(target->type, cv->to_type)) {
                fprintf(stderr,
                        "Error (line %d): cannot convert '%s' from %s to %s.\n",
                        s->line, cv->target, type_name(target->type), type_name(cv->to_type));
                errors++;
                break;
            }

            target->type = cv->to_type;
            break;
        }

        case STMT_SET_MATRIX_CTX: {
            const SetMatrixCtxStmt *sm = &s->as.set_matrix_ctx;
            Symbol *target = symtab_lookup(t, sm->target);
            if (!target) {
                fprintf(stderr,
                        "Error (line %d): assignment to undeclared variable '%s'.\n",
                        s->line, sm->target);
                errors++;
            } else if (target->kind != SYM_VAR) {
                fprintf(stderr,
                        "Error (line %d): '%s' is a collection; contextual matrix read can only assign to scalar variables.\n",
                        s->line, sm->target);
                errors++;
            }

            if (!matrix_context) {
                fprintf(stderr,
                        "Error (line %d): contextual matrix read 'set %s at <row>,<col>' is only valid inside a matrix loop.\n",
                        s->line, sm->target);
                errors++;
                errors += check_expr(t, ft, sm->row, s->line);
                errors += check_expr(t, ft, sm->col, s->line);
                break;
            }

            Symbol *mat = symtab_lookup(t, matrix_context);
            if (!mat || mat->kind != SYM_MATRIX) {
                fprintf(stderr,
                        "Error (line %d): matrix loop context '%s' is invalid.\n",
                        s->line, matrix_context ? matrix_context : "<none>");
                errors++;
                errors += check_expr(t, ft, sm->row, s->line);
                errors += check_expr(t, ft, sm->col, s->line);
                break;
            }

            TypeKind rk = infer_expr_type(t, ft, sm->row, s->line, &errors);
            TypeKind ck = infer_expr_type(t, ft, sm->col, s->line, &errors);
            if (rk != TYPE_INT) {
                fprintf(stderr,
                        "Error (line %d): row index for contextual matrix read must be int.\n",
                        s->line);
                errors++;
            }
            if (ck != TYPE_INT) {
                fprintf(stderr,
                        "Error (line %d): column index for contextual matrix read must be int.\n",
                        s->line);
                errors++;
            }

            if (mat->type == TYPE_DYNAMIC) {
                fprintf(stderr,
                        "Error (line %d): contextual reads from dynamic matrix '%s' are not supported in scalar assignments.\n",
                        s->line, matrix_context);
                errors++;
                break;
            }

            if (target && target->kind == SYM_VAR &&
                !type_compatible(target->type, mat->type)) {
                fprintf(stderr,
                        "Error (line %d): assignment to '%s' has type %s, expected %s.\n",
                        s->line, sm->target, type_name(mat->type), type_name(target->type));
                errors++;
            }
            break;
        }

        case STMT_INPUT: {
            const InputStmt *in = &s->as.input;
            Symbol *sym = symtab_lookup(t, in->target);
            if (!sym) {
                fprintf(stderr,
                        "Error (line %d): input target '%s' is undeclared.\n",
                        s->line, in->target);
                errors++;
            } else if (sym->kind != SYM_VAR) {
                fprintf(stderr,
                        "Error (line %d): input target '%s' must be a scalar variable.\n",
                        s->line, in->target);
                errors++;
            }
            for (size_t k = 0; k < in->message_count; k++)
                errors += check_expr(t, ft, in->message_parts[k], s->line);
            break;
        }

        case STMT_EMIT: {
            const EmitStmt *em = &s->as.emit;
            int call_err = 0;
            (void)check_call(t, ft, em->name, em->args, em->count, s->line, &call_err);
            errors += call_err;
            break;
        }

        case STMT_GIVEBACK: {
            const GivebackStmt *gb = &s->as.giveback;
            if (!in_function) {
                fprintf(stderr,
                        "Error (line %d): 'giveback' is only valid inside a function body.\n",
                        s->line);
                errors++;
                errors += check_expr(t, ft, gb->value, s->line);
            } else {
                int expr_err = 0;
                TypeKind actual = infer_expr_type(t, ft, gb->value, s->line, &expr_err);
                errors += expr_err;
                if (!type_compatible(current_return, actual)) {
                    fprintf(stderr,
                            "Error (line %d): giveback type mismatch, expected %s but got %s.\n",
                            s->line, type_name(current_return), type_name(actual));
                    errors++;
                }
            }
            break;
        }

        case STMT_FUNCDEF: {
            if (!allow_function_defs) {
                fprintf(stderr,
                        "Error (line %d): routine definitions are only allowed at top level.\n",
                        s->line);
                errors++;
            }
            break;
        }

        case STMT_IF: {
            const IfStmt *fi = &s->as.if_;
            errors += check_cond(t, ft, fi->cond, s->line);
            if (fi->then_block)
                errors += analyze_stmts(t, ft, fi->then_block->as.block.stmts,
                                        fi->then_block->as.block.count,
                                        in_function, current_return, 0,
                                        in_file_block,
                                        collection_context,
                                        collection_index_context,
                                        matrix_context);
            if (fi->else_block) {
                Stmt *eb = fi->else_block;
                errors += analyze_stmts(t, ft, (Stmt *const *)&eb, 1,
                                        in_function, current_return, 0,
                                        in_file_block,
                                        collection_context,
                                        collection_index_context,
                                        matrix_context);
            }
            break;
        }

        case STMT_BLOCK:
            errors += analyze_stmts(t, ft, s->as.block.stmts, s->as.block.count,
                                    in_function, current_return, allow_function_defs,
                                    in_file_block,
                                    collection_context,
                                    collection_index_context,
                                    matrix_context);
            break;

        case STMT_WHILE: {
            const WhileStmt *w = &s->as.while_;
            errors += check_cond(t, ft, w->cond, s->line);
            errors += analyze_stmts(t, ft, w->body->as.block.stmts,
                                    w->body->as.block.count,
                                    in_function, current_return, 0,
                                    in_file_block,
                                    collection_context,
                                    collection_index_context,
                                    matrix_context);
            break;
        }

        case STMT_FOR: {
            const ForStmt *f = &s->as.for_;

            errors += check_expr(t, ft, f->from, s->line);
            errors += check_expr(t, ft, f->to, s->line);
            if (f->step) errors += check_expr(t, ft, f->step, s->line);

            size_t saved_count = t->count;
            symtab_add(t, f->var, TYPE_INT, s->line);
            errors += analyze_stmts(t, ft, f->body->as.block.stmts,
                                    f->body->as.block.count,
                                    in_function, current_return, 0,
                                    in_file_block,
                                    collection_context,
                                    collection_index_context,
                                    matrix_context);
            for (size_t k = saved_count; k < t->count; k++)
                free(t->entries[k].name);
            t->count = saved_count;
            break;
        }

        case STMT_FOR_EACH: {
            const ForEachStmt *fe = &s->as.for_each;
            Symbol *col = symtab_lookup(t, fe->collection);
            if (!col) {
                fprintf(stderr,
                        "Error (line %d): collection '%s' is undeclared.\n",
                        s->line, fe->collection);
                errors++;
                break;
            }
            if (col->kind != SYM_ARRAY && col->kind != SYM_TABLE) {
                fprintf(stderr,
                        "Error (line %d): '%s' is not an array/table.\n",
                        s->line, fe->collection);
                errors++;
                break;
            }

            TypeKind fk = infer_expr_type(t, ft, fe->from, s->line, &errors);
            TypeKind tk = infer_expr_type(t, ft, fe->to, s->line, &errors);
            if (fk != TYPE_INT || tk != TYPE_INT) {
                fprintf(stderr,
                        "Error (line %d): loop bounds in 'for ... in %s' must be int.\n",
                        s->line, fe->collection);
                errors++;
            }
            if (fe->step) {
                TypeKind sk = infer_expr_type(t, ft, fe->step, s->line, &errors);
                if (sk != TYPE_INT) {
                    fprintf(stderr,
                            "Error (line %d): loop step in 'for ... in %s' must be int.\n",
                            s->line, fe->collection);
                    errors++;
                }
            }

            size_t saved_count = t->count;
            symtab_add(t, fe->index_var, TYPE_INT, s->line);
            errors += analyze_stmts(t, ft, fe->body->as.block.stmts,
                                    fe->body->as.block.count,
                                    in_function, current_return, 0,
                                    in_file_block,
                                    fe->collection,
                                    fe->index_var,
                                    matrix_context);
            for (size_t k = saved_count; k < t->count; k++)
                free(t->entries[k].name);
            t->count = saved_count;
            break;
        }

        case STMT_FOR_MATRIX: {
            const ForMatrixStmt *fm = &s->as.for_matrix;
            Symbol *mat = symtab_lookup(t, fm->matrix);
            if (!mat) {
                fprintf(stderr,
                        "Error (line %d): matrix '%s' is undeclared.\n",
                        s->line, fm->matrix);
                errors++;
                break;
            }
            if (mat->kind != SYM_MATRIX) {
                fprintf(stderr,
                        "Error (line %d): '%s' is not a matrix.\n",
                        s->line, fm->matrix);
                errors++;
                break;
            }

            if (fm->row_from && fm->row_to && fm->col_from && fm->col_to) {
                TypeKind rfk = infer_expr_type(t, ft, fm->row_from, s->line, &errors);
                TypeKind rtk = infer_expr_type(t, ft, fm->row_to, s->line, &errors);
                TypeKind cfk = infer_expr_type(t, ft, fm->col_from, s->line, &errors);
                TypeKind ctk = infer_expr_type(t, ft, fm->col_to, s->line, &errors);
                if (rfk != TYPE_INT || rtk != TYPE_INT || cfk != TYPE_INT || ctk != TYPE_INT) {
                    fprintf(stderr,
                            "Error (line %d): matrix loop bounds for '%s' must be int.\n",
                            s->line, fm->matrix);
                    errors++;
                }
            }

            size_t saved_count = t->count;
            symtab_add(t, fm->row_var, TYPE_INT, s->line);
            symtab_add(t, fm->col_var, TYPE_INT, s->line);
            errors += analyze_stmts(t, ft, fm->body->as.block.stmts,
                                    fm->body->as.block.count,
                                    in_function, current_return, 0,
                                    in_file_block,
                                    collection_context,
                                    collection_index_context,
                                    fm->matrix);
            for (size_t k = saved_count; k < t->count; k++)
                free(t->entries[k].name);
            t->count = saved_count;
            break;
        }

        case STMT_REPEAT: {
            const RepeatStmt *r = &s->as.repeat_;
            errors += analyze_stmts(t, ft, r->body->as.block.stmts,
                                    r->body->as.block.count,
                                    in_function, current_return, 0,
                                    in_file_block,
                                    collection_context,
                                    collection_index_context,
                                    matrix_context);
            errors += check_cond(t, ft, r->cond, s->line);
            break;
        }

        case STMT_ATTEMPT: {
            const AttemptStmt *a = &s->as.attempt_;
            errors += check_expr(t, ft, a->max, s->line);
            errors += check_cond(t, ft, a->cond, s->line);
            errors += analyze_stmts(t, ft, a->body->as.block.stmts,
                                    a->body->as.block.count,
                                    in_function, current_return, 0,
                                    in_file_block,
                                    collection_context,
                                    collection_index_context,
                                    matrix_context);
            if (a->failure)
                errors += analyze_stmts(t, ft, a->failure->as.block.stmts,
                                        a->failure->as.block.count,
                                        in_function, current_return, 0,
                                        in_file_block,
                                        collection_context,
                                        collection_index_context,
                                        matrix_context);
            break;
        }

        case STMT_FILE_OPEN: {
            const FileOpenStmt *fo = &s->as.file_open;
            TypeKind pk = infer_expr_type(t, ft, fo->path, s->line, &errors);
            if (pk != TYPE_STRING) {
                fprintf(stderr,
                        "Error (line %d): file path expression must be string.\n",
                        s->line);
                errors++;
            }
            errors += analyze_stmts(t, ft, fo->body->as.block.stmts,
                                    fo->body->as.block.count,
                                    in_function, current_return, 0,
                                    1,
                                    collection_context,
                                    collection_index_context,
                                    matrix_context);
            break;
        }

        case STMT_FILE_WRITE: {
            const FileWriteStmt *fw = &s->as.file_write;
            if (!in_file_block) {
                fprintf(stderr,
                        "Error (line %d): file write is only valid inside an open file block.\n",
                        s->line);
                errors++;
            }
            TypeKind vk = infer_expr_type(t, ft, fw->value, s->line, &errors);
            if (vk != TYPE_STRING) {
                fprintf(stderr,
                        "Error (line %d): write value must be string.\n",
                        s->line);
                errors++;
            }
            if (fw->has_line) {
                TypeKind lk = infer_expr_type(t, ft, fw->line, s->line, &errors);
                if (lk != TYPE_INT) {
                    fprintf(stderr,
                            "Error (line %d): line index must be int.\n",
                            s->line);
                    errors++;
                }
            }
            break;
        }

        case STMT_FILE_READ: {
            const FileReadStmt *fr = &s->as.file_read;
            if (!in_file_block) {
                fprintf(stderr,
                        "Error (line %d): file read is only valid inside an open file block.\n",
                        s->line);
                errors++;
            }
            Symbol *dst = symtab_lookup(t, fr->target);
            if (!dst) {
                fprintf(stderr,
                        "Error (line %d): read target '%s' is undeclared.\n",
                        s->line, fr->target);
                errors++;
            } else if (fr->mode == FILE_READ_LINE) {
                if (!(dst->kind == SYM_VAR && dst->type == TYPE_STRING)) {
                    fprintf(stderr,
                            "Error (line %d): line read target '%s' must be a string variable.\n",
                            s->line, fr->target);
                    errors++;
                }
            } else if (dst->kind != SYM_TABLE) {
                fprintf(stderr,
                        "Error (line %d): multi-line read target '%s' must be a table.\n",
                        s->line, fr->target);
                errors++;
            }

            if (fr->line) {
                TypeKind lk = infer_expr_type(t, ft, fr->line, s->line, &errors);
                if (lk != TYPE_INT) {
                    fprintf(stderr,
                            "Error (line %d): read line index must be int.\n",
                            s->line);
                    errors++;
                }
            }
            if (fr->count) {
                TypeKind ck = infer_expr_type(t, ft, fr->count, s->line, &errors);
                if (ck != TYPE_INT) {
                    fprintf(stderr,
                            "Error (line %d): read line count must be int.\n",
                            s->line);
                    errors++;
                }
            }
            if (fr->from_line) {
                TypeKind fk = infer_expr_type(t, ft, fr->from_line, s->line, &errors);
                if (fk != TYPE_INT) {
                    fprintf(stderr,
                            "Error (line %d): read start line must be int.\n",
                            s->line);
                    errors++;
                }
            }
            break;
        }

        case STMT_FILE_CLEAR: {
            const FileClearStmt *fc = &s->as.file_clear;
            if (!in_file_block) {
                fprintf(stderr,
                        "Error (line %d): file clear is only valid inside an open file block.\n",
                        s->line);
                errors++;
            }
            if (fc->line) {
                TypeKind lk = infer_expr_type(t, ft, fc->line, s->line, &errors);
                if (lk != TYPE_INT) {
                    fprintf(stderr,
                            "Error (line %d): clear line index must be int.\n",
                            s->line);
                    errors++;
                }
            }
            if (fc->from_line) {
                TypeKind fk = infer_expr_type(t, ft, fc->from_line, s->line, &errors);
                if (fk != TYPE_INT) {
                    fprintf(stderr,
                            "Error (line %d): clear start line must be int.\n",
                            s->line);
                    errors++;
                }
            }
            if (fc->to_line) {
                TypeKind tk = infer_expr_type(t, ft, fc->to_line, s->line, &errors);
                if (tk != TYPE_INT) {
                    fprintf(stderr,
                            "Error (line %d): clear end line must be int.\n",
                            s->line);
                    errors++;
                }
            }
            break;
        }

        case STMT_FILE_SET_TITLE: {
            const FileSetTitleStmt *ftitle = &s->as.file_set_title;
            if (!in_file_block) {
                fprintf(stderr,
                        "Error (line %d): set file title is only valid inside an open file block.\n",
                        s->line);
                errors++;
            }
            TypeKind tk = infer_expr_type(t, ft, ftitle->value, s->line, &errors);
            if (tk != TYPE_STRING) {
                fprintf(stderr,
                        "Error (line %d): file title value must be string.\n",
                        s->line);
                errors++;
            }
            break;
        }

        case STMT_FILE_CLOSE:
            if (!in_file_block) {
                fprintf(stderr,
                        "Error (line %d): close file is only valid inside an open file block.\n",
                        s->line);
                errors++;
            }
            break;

        }
    }

    return errors;
}

static void collect_funcdefs_from_stmt(FuncTable *ft, const Stmt *s, int *errors) {
    if (!s) return;
    switch (s->kind) {
    case STMT_FUNCDEF:
        if (functab_add(ft, &s->as.funcdef, s->line) != 0) (*errors)++;
        collect_funcdefs_from_stmt(ft, s->as.funcdef.body, errors);
        break;
    case STMT_BLOCK:
        for (size_t i = 0; i < s->as.block.count; i++)
            collect_funcdefs_from_stmt(ft, s->as.block.stmts[i], errors);
        break;
    case STMT_IF:
        collect_funcdefs_from_stmt(ft, s->as.if_.then_block, errors);
        collect_funcdefs_from_stmt(ft, s->as.if_.else_block, errors);
        break;
    case STMT_WHILE:
        collect_funcdefs_from_stmt(ft, s->as.while_.body, errors);
        break;
    case STMT_FOR:
        collect_funcdefs_from_stmt(ft, s->as.for_.body, errors);
        break;
    case STMT_FOR_EACH:
        collect_funcdefs_from_stmt(ft, s->as.for_each.body, errors);
        break;
    case STMT_FOR_MATRIX:
        collect_funcdefs_from_stmt(ft, s->as.for_matrix.body, errors);
        break;
    case STMT_REPEAT:
        collect_funcdefs_from_stmt(ft, s->as.repeat_.body, errors);
        break;
    case STMT_ATTEMPT:
        collect_funcdefs_from_stmt(ft, s->as.attempt_.body, errors);
        collect_funcdefs_from_stmt(ft, s->as.attempt_.failure, errors);
        break;
    case STMT_FILE_OPEN:
        collect_funcdefs_from_stmt(ft, s->as.file_open.body, errors);
        break;
    default:
        break;
    }
}

static int analyze_funcdef_bodies(SymTable *t, FuncTable *ft, const Stmt *s) {
    if (!s) return 0;
    int errors = 0;
    switch (s->kind) {
    case STMT_FUNCDEF: {
        const FuncDefStmt *fd = &s->as.funcdef;
        size_t saved_count = t->count;

        for (size_t p = 0; p < fd->param_count; p++) {
            if (symtab_add(t, fd->params[p].name, fd->params[p].type, s->line) != 0)
                errors++;
        }

        errors += analyze_stmts(t, ft,
                                fd->body->as.block.stmts,
                                fd->body->as.block.count,
                                1, fd->return_type, 0,
                                0,
                                NULL,
                                NULL,
                                NULL);

        for (size_t k = saved_count; k < t->count; k++)
            free(t->entries[k].name);
        t->count = saved_count;

        errors += analyze_funcdef_bodies(t, ft, fd->body);
        break;
    }
    case STMT_BLOCK:
        for (size_t i = 0; i < s->as.block.count; i++)
            errors += analyze_funcdef_bodies(t, ft, s->as.block.stmts[i]);
        break;
    case STMT_IF:
        errors += analyze_funcdef_bodies(t, ft, s->as.if_.then_block);
        errors += analyze_funcdef_bodies(t, ft, s->as.if_.else_block);
        break;
    case STMT_WHILE:
        errors += analyze_funcdef_bodies(t, ft, s->as.while_.body);
        break;
    case STMT_FOR:
        errors += analyze_funcdef_bodies(t, ft, s->as.for_.body);
        break;
    case STMT_FOR_EACH:
        errors += analyze_funcdef_bodies(t, ft, s->as.for_each.body);
        break;
    case STMT_FOR_MATRIX:
        errors += analyze_funcdef_bodies(t, ft, s->as.for_matrix.body);
        break;
    case STMT_REPEAT:
        errors += analyze_funcdef_bodies(t, ft, s->as.repeat_.body);
        break;
    case STMT_ATTEMPT:
        errors += analyze_funcdef_bodies(t, ft, s->as.attempt_.body);
        errors += analyze_funcdef_bodies(t, ft, s->as.attempt_.failure);
        break;
    case STMT_FILE_OPEN:
        errors += analyze_funcdef_bodies(t, ft, s->as.file_open.body);
        break;
    default:
        break;
    }
    return errors;
}

/* ── Full program analysis ──────────────────────────────────────── */

int symtab_analyze(SymTable *t, const Program *prog) {
    if (!t || !prog) return -1;

    int errors = 0;
    FuncTable ft;
    functab_init(&ft);

    for (size_t i = 0; i < prog->count; i++)
        collect_funcdefs_from_stmt(&ft, prog->stmts[i], &errors);

    errors += analyze_stmts(t, &ft,
                            prog->stmts, prog->count,
                            0, TYPE_INT, 1,
                            0,
                            NULL,
                            NULL,
                            NULL);

    for (size_t i = 0; i < prog->count; i++)
        errors += analyze_funcdef_bodies(t, &ft, prog->stmts[i]);

    functab_free(&ft);
    return errors ? -1 : 0;
}
