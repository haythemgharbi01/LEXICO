/*
 * ast.cpp – AST construction and destruction for Lexico.
 *
 * Every allocation is checked; OOM causes an immediate clean abort.
 */

#include "ast.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Safe allocators ────────────────────────────────────────────── */

static void oom(void) {
    fprintf(stderr, "Fatal: out of memory.\n");
    exit(EXIT_FAILURE);
}

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p && n) oom();
    return p;
}

static void *xrealloc(void *ptr, size_t n) {
    void *p = realloc(ptr, n);
    if (!p && n) oom();
    return p;
}

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *d = static_cast<char *>(xmalloc(len + 1));
    memcpy(d, s, len + 1);
    return d;
}

/* ── Program ────────────────────────────────────────────────────── */

Program *ast_new_program(void) {
    Program *p = static_cast<Program *>(xmalloc(sizeof *p));
    p->stmts = NULL;
    p->count = 0;
    p->cap   = 0;
    return p;
}

void ast_program_add(Program *p, Stmt *s) {
    if (p->count == p->cap) {
        p->cap = p->cap ? p->cap * 2 : 8;
        p->stmts = static_cast<Stmt **>(xrealloc(p->stmts, p->cap * sizeof(Stmt *)));
    }
    p->stmts[p->count++] = s;
}

/* ── Statements ─────────────────────────────────────────────────── */

Stmt *ast_new_decl(int line, TypeKind type,
                   char **names, Expr **values, size_t count) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind   = STMT_DECL;
    s->line   = line;
    s->as.decl.type   = type;
    s->as.decl.names  = names;
    s->as.decl.values = values;
    s->as.decl.count  = count;
    return s;
}

Stmt *ast_new_array_decl(int line, TypeKind elem_type, char *name, Expr *size) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_ARRAY_DECL;
    s->line = line;
    s->as.array_decl.elem_type = elem_type;
    s->as.array_decl.name = name;
    s->as.array_decl.size = size;
    return s;
}

Stmt *ast_new_table_decl(int line, char *name, Expr *size) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_TABLE_DECL;
    s->line = line;
    s->as.table_decl.name = name;
    s->as.table_decl.size = size;
    return s;
}

Stmt *ast_new_matrix_decl(int line, TypeKind elem_type, int is_dynamic, char *name, Expr *rows, Expr *cols) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_MATRIX_DECL;
    s->line = line;
    s->as.matrix_decl.elem_type = elem_type;
    s->as.matrix_decl.is_dynamic = is_dynamic;
    s->as.matrix_decl.name = name;
    s->as.matrix_decl.rows = rows;
    s->as.matrix_decl.cols = cols;
    return s;
}

Stmt *ast_new_collection_set(int line, char *name, Expr *index, Expr *value) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_COLLECTION_SET;
    s->line = line;
    s->as.collection_set.name = name;
    s->as.collection_set.index = index;
    s->as.collection_set.value = value;
    return s;
}

Stmt *ast_new_matrix_set(int line, char *name, Expr *row, Expr *col, Expr *value) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_MATRIX_SET;
    s->line = line;
    s->as.matrix_set.name = name;
    s->as.matrix_set.row = row;
    s->as.matrix_set.col = col;
    s->as.matrix_set.value = value;
    return s;
}

Stmt *ast_new_collection_set_input(int line, char *name, Expr *index,
                                   Expr **message_parts, size_t message_count) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_COLLECTION_SET_INPUT;
    s->line = line;
    s->as.collection_set_input.name = name;
    s->as.collection_set_input.index = index;
    s->as.collection_set_input.message_parts = message_parts;
    s->as.collection_set_input.message_count = message_count;
    return s;
}

Stmt *ast_new_collection_append(int line, char *name, Expr *value, Expr *index) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_COLLECTION_APPEND;
    s->line = line;
    s->as.collection_append.name = name;
    s->as.collection_append.value = value;
    s->as.collection_append.index = index;
    return s;
}

Stmt *ast_new_collection_sort(int line, char *name, int ascending) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_COLLECTION_SORT;
    s->line = line;
    s->as.collection_sort.name = name;
    s->as.collection_sort.ascending = ascending ? 1 : 0;
    return s;
}

Stmt *ast_new_print(int line, Expr **args, size_t count) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind    = STMT_PRINT;
    s->line    = line;
    s->as.print.args  = args;
    s->as.print.count = count;
    return s;
}

Stmt *ast_new_set(int line, char *target, Expr *value) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind       = STMT_SET;
    s->line       = line;
    s->as.set.target = target;   /* takes ownership */
    s->as.set.value  = value;    /* takes ownership */
    return s;
}

Stmt *ast_new_convert(int line, char *target, TypeKind to_type) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_CONVERT;
    s->line = line;
    s->as.convert.target = target;
    s->as.convert.to_type = to_type;
    return s;
}

Stmt *ast_new_set_matrix_ctx(int line, char *target, Expr *row, Expr *col) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_SET_MATRIX_CTX;
    s->line = line;
    s->as.set_matrix_ctx.target = target;
    s->as.set_matrix_ctx.row = row;
    s->as.set_matrix_ctx.col = col;
    return s;
}

Stmt *ast_new_input(int line, char *target, Expr **message_parts, size_t message_count) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_INPUT;
    s->line = line;
    s->as.input.target  = target;   /* takes ownership */
    s->as.input.message_parts = message_parts;
    s->as.input.message_count = message_count;
    return s;
}

Stmt *ast_new_emit(int line, char *name, Expr **args, size_t count) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_EMIT;
    s->line = line;
    s->as.emit.name  = name;
    s->as.emit.args  = args;
    s->as.emit.count = count;
    return s;
}

Stmt *ast_new_giveback(int line, Expr *value) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_GIVEBACK;
    s->line = line;
    s->as.giveback.value = value;
    return s;
}

Stmt *ast_new_funcdef(int line, char *name, TypeKind return_type,
                      Param *params, size_t param_count, Stmt *body) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_FUNCDEF;
    s->line = line;
    s->as.funcdef.name = name;
    s->as.funcdef.return_type = return_type;
    s->as.funcdef.params = params;
    s->as.funcdef.param_count = param_count;
    s->as.funcdef.body = body;
    return s;
}

Stmt *ast_new_if(int line, CondExpr *cond,
                 Stmt *then_block, Stmt *else_block) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_IF;
    s->line = line;
    s->as.if_.cond       = cond;
    s->as.if_.then_block = then_block;
    s->as.if_.else_block = else_block;
    return s;
}

Stmt *ast_new_block(int line) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_BLOCK;
    s->line = line;
    s->as.block.stmts = NULL;
    s->as.block.count = 0;
    s->as.block.cap   = 0;
    return s;
}

void ast_block_add(Stmt *block, Stmt *child) {
    BlockStmt *b = &block->as.block;
    if (b->count == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 8;
        b->stmts = static_cast<Stmt **>(xrealloc(b->stmts, b->cap * sizeof(Stmt *)));
    }
    b->stmts[b->count++] = child;
}

Stmt *ast_new_while(int line, CondExpr *cond, Stmt *body) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_WHILE;
    s->line = line;
    s->as.while_.cond = cond;
    s->as.while_.body = body;
    return s;
}

Stmt *ast_new_for(int line, char *var, Expr *from, Expr *to,
                  Expr *step, Stmt *body) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_FOR;
    s->line = line;
    s->as.for_.var  = var;   /* takes ownership */
    s->as.for_.from = from;
    s->as.for_.to   = to;
    s->as.for_.step = step;  /* may be NULL */
    s->as.for_.body = body;
    return s;
}

Stmt *ast_new_for_each(int line, char *index_var, Expr *from, Expr *to,
                       Expr *step, char *collection, Stmt *body) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_FOR_EACH;
    s->line = line;
    s->as.for_each.index_var = index_var;
    s->as.for_each.from = from;
    s->as.for_each.to = to;
    s->as.for_each.step = step;
    s->as.for_each.collection = collection;
    s->as.for_each.body = body;
    return s;
}

Stmt *ast_new_for_matrix(int line, char *row_var, char *col_var,
                         char *matrix,
                         Expr *row_from, Expr *row_to,
                         Expr *col_from, Expr *col_to,
                         Stmt *body) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_FOR_MATRIX;
    s->line = line;
    s->as.for_matrix.row_var = row_var;
    s->as.for_matrix.col_var = col_var;
    s->as.for_matrix.matrix = matrix;
    s->as.for_matrix.row_from = row_from;
    s->as.for_matrix.row_to = row_to;
    s->as.for_matrix.col_from = col_from;
    s->as.for_matrix.col_to = col_to;
    s->as.for_matrix.body = body;
    return s;
}

Stmt *ast_new_repeat(int line, Stmt *body, CondExpr *cond) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_REPEAT;
    s->line = line;
    s->as.repeat_.body = body;
    s->as.repeat_.cond = cond;
    return s;
}

Stmt *ast_new_attempt(int line, Expr *max, CondExpr *cond,
                      Stmt *body, Stmt *failure) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_ATTEMPT;
    s->line = line;
    s->as.attempt_.max     = max;
    s->as.attempt_.cond    = cond;
    s->as.attempt_.body    = body;
    s->as.attempt_.failure = failure;
    return s;
}

Stmt *ast_new_file_open(int line, Expr *path, int make_if_missing, Stmt *body) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_FILE_OPEN;
    s->line = line;
    s->as.file_open.path = path;
    s->as.file_open.make_if_missing = make_if_missing ? 1 : 0;
    s->as.file_open.body = body;
    return s;
}

Stmt *ast_new_file_write(int line, Expr *value, Expr *line_expr) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_FILE_WRITE;
    s->line = line;
    s->as.file_write.has_line = (line_expr != NULL);
    s->as.file_write.line = line_expr;
    s->as.file_write.value = value;
    return s;
}

Stmt *ast_new_file_read(int line, FileReadMode mode, char *target,
                        Expr *count, Expr *from_line, Expr *line_expr) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_FILE_READ;
    s->line = line;
    s->as.file_read.mode = mode;
    s->as.file_read.target = target;
    s->as.file_read.count = count;
    s->as.file_read.from_line = from_line;
    s->as.file_read.line = line_expr;
    return s;
}

Stmt *ast_new_file_clear(int line, FileClearMode mode, Expr *line_expr,
                         Expr *from_line, Expr *to_line) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_FILE_CLEAR;
    s->line = line;
    s->as.file_clear.mode = mode;
    s->as.file_clear.line = line_expr;
    s->as.file_clear.from_line = from_line;
    s->as.file_clear.to_line = to_line;
    return s;
}

Stmt *ast_new_file_set_title(int line, Expr *value) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_FILE_SET_TITLE;
    s->line = line;
    s->as.file_set_title.value = value;
    return s;
}

Stmt *ast_new_file_close(int line) {
    Stmt *s = static_cast<Stmt *>(xmalloc(sizeof *s));
    s->kind = STMT_FILE_CLOSE;
    s->line = line;
    return s;
}

/* ── Condition constructors ─────────────────────────────────────── */

CondExpr *ast_cond_cmp(CmpOp op, Expr *left, Expr *right) {
    CondExpr *c = static_cast<CondExpr *>(xmalloc(sizeof *c));
    c->kind        = COND_CMP;
    c->as.cmp.op   = op;
    c->as.cmp.left = left;
    c->as.cmp.right = right;
    return c;
}

CondExpr *ast_cond_and(CondExpr *left, CondExpr *right) {
    CondExpr *c = static_cast<CondExpr *>(xmalloc(sizeof *c));
    c->kind          = COND_AND;
    c->as.logic.left = left;
    c->as.logic.right = right;
    return c;
}

CondExpr *ast_cond_or(CondExpr *left, CondExpr *right) {
    CondExpr *c = static_cast<CondExpr *>(xmalloc(sizeof *c));
    c->kind          = COND_OR;
    c->as.logic.left = left;
    c->as.logic.right = right;
    return c;
}

CondExpr *ast_cond_not(CondExpr *operand) {
    CondExpr *c = static_cast<CondExpr *>(xmalloc(sizeof *c));
    c->kind       = COND_NOT;
    c->as.operand = operand;
    return c;
}

void ast_free_cond(CondExpr *c) {
    if (!c) return;
    switch (c->kind) {
    case COND_CMP:
        ast_free_expr(c->as.cmp.left);
        ast_free_expr(c->as.cmp.right);
        break;
    case COND_AND:
    case COND_OR:
        ast_free_cond(c->as.logic.left);
        ast_free_cond(c->as.logic.right);
        break;
    case COND_NOT:
        ast_free_cond(c->as.operand);
        break;
    }
    free(c);
}

/* ── Expression constructors ────────────────────────────────────── */

Expr *ast_expr_literal(Literal lit) {
    Expr *e = static_cast<Expr *>(xmalloc(sizeof *e));
    e->kind   = EXPR_LITERAL;
    e->as.lit = lit;
    return e;
}

Expr *ast_expr_ident(const char *name) {
    Expr *e = static_cast<Expr *>(xmalloc(sizeof *e));
    e->kind     = EXPR_IDENT;
    e->as.ident = xstrdup(name);
    return e;
}

Expr *ast_expr_binary(BinOp op, Expr *left, Expr *right) {
    Expr *e = static_cast<Expr *>(xmalloc(sizeof *e));
    e->kind         = EXPR_BINARY;
    e->as.bin.op    = op;
    e->as.bin.left  = left;
    e->as.bin.right = right;
    return e;
}

Expr *ast_expr_neg(Expr *operand) {
    Expr *e = static_cast<Expr *>(xmalloc(sizeof *e));
    e->kind       = EXPR_UNARY_NEG;
    e->as.operand = operand;
    return e;
}

Expr *ast_expr_call(char *name, Expr **args, size_t count) {
    Expr *e = static_cast<Expr *>(xmalloc(sizeof *e));
    e->kind = EXPR_CALL;
    e->as.call.name  = name;
    e->as.call.args  = args;
    e->as.call.count = count;
    return e;
}

Expr *ast_expr_index(char *name, Expr *index) {
    Expr *e = static_cast<Expr *>(xmalloc(sizeof *e));
    e->kind = EXPR_INDEX;
    e->as.index.name = name;
    e->as.index.index = index;
    return e;
}

Expr *ast_expr_matrix_index(char *name, Expr *row, Expr *col) {
    Expr *e = static_cast<Expr *>(xmalloc(sizeof *e));
    e->kind = EXPR_MATRIX_INDEX;
    e->as.matrix_index.name = name;
    e->as.matrix_index.row = row;
    e->as.matrix_index.col = col;
    return e;
}

Expr *ast_expr_length(char *name) {
    Expr *e = static_cast<Expr *>(xmalloc(sizeof *e));
    e->kind = EXPR_LENGTH;
    e->as.length_name = name;
    return e;
}

Expr *ast_expr_row_length(char *name) {
    Expr *e = static_cast<Expr *>(xmalloc(sizeof *e));
    e->kind = EXPR_ROW_LENGTH;
    e->as.row_length_name = name;
    return e;
}

Expr *ast_expr_col_length(char *name) {
    Expr *e = static_cast<Expr *>(xmalloc(sizeof *e));
    e->kind = EXPR_COL_LENGTH;
    e->as.col_length_name = name;
    return e;
}

Expr *ast_expr_strlen(Expr *value) {
    Expr *e = static_cast<Expr *>(xmalloc(sizeof *e));
    e->kind = EXPR_STRLEN;
    e->as.strlen_value = value;
    return e;
}

Expr *ast_expr_extract(Expr *value, char *source_name) {
    Expr *e = static_cast<Expr *>(xmalloc(sizeof *e));
    e->kind = EXPR_EXTRACT;
    e->as.extract.value = value;
    e->as.extract.source_name = source_name;
    return e;
}

Expr *ast_expr_extract_range(Expr *from, Expr *to, char *source_name) {
    Expr *e = static_cast<Expr *>(xmalloc(sizeof *e));
    e->kind = EXPR_EXTRACT_RANGE;
    e->as.extract_range.from = from;
    e->as.extract_range.to = to;
    e->as.extract_range.source_name = source_name;
    return e;
}

Expr *ast_expr_count(Expr *value, char *collection_name) {
    Expr *e = static_cast<Expr *>(xmalloc(sizeof *e));
    e->kind = EXPR_COUNT;
    e->as.count.value = value;
    e->as.count.collection_name = collection_name;
    return e;
}

Expr *ast_expr_check(Expr *value, char *collection_name) {
    Expr *e = static_cast<Expr *>(xmalloc(sizeof *e));
    e->kind = EXPR_CHECK;
    e->as.check.value = value;
    e->as.check.collection_name = collection_name;
    return e;
}

Expr *ast_expr_position(Expr *value, char *collection_name) {
    Expr *e = static_cast<Expr *>(xmalloc(sizeof *e));
    e->kind = EXPR_POSITION;
    e->as.position.value = value;
    e->as.position.collection_name = collection_name;
    return e;
}

Expr *ast_expr_convert(Expr *value, TypeKind to_type) {
    Expr *e = static_cast<Expr *>(xmalloc(sizeof *e));
    e->kind = EXPR_CONVERT;
    e->as.convert.value = value;
    e->as.convert.to_type = to_type;
    return e;
}

Expr *ast_expr_clone(const Expr *e) {
    if (!e) return NULL;

    switch (e->kind) {
    case EXPR_LITERAL:
        switch (e->as.lit.kind) {
        case LIT_INT:    return ast_expr_literal(ast_lit_int(e->as.lit.as.i));
        case LIT_FLOAT:  return ast_expr_literal(ast_lit_float(e->as.lit.as.d));
        case LIT_CHAR:   return ast_expr_literal(ast_lit_char(e->as.lit.as.c));
        case LIT_STRING: return ast_expr_literal(ast_lit_string(e->as.lit.as.s ? e->as.lit.as.s : ""));
        case LIT_BOOL:   return ast_expr_literal(ast_lit_bool(e->as.lit.as.b));
        }
        break;
    case EXPR_IDENT:
        return ast_expr_ident(e->as.ident);
    case EXPR_BINARY:
        return ast_expr_binary(e->as.bin.op,
                               ast_expr_clone(e->as.bin.left),
                               ast_expr_clone(e->as.bin.right));
    case EXPR_UNARY_NEG:
        return ast_expr_neg(ast_expr_clone(e->as.operand));
    case EXPR_CALL: {
        size_t n = e->as.call.count;
        Expr **args = n ? static_cast<Expr **>(xmalloc(n * sizeof(Expr *))) : NULL;
        for (size_t i = 0; i < n; i++) args[i] = ast_expr_clone(e->as.call.args[i]);
        return ast_expr_call(xstrdup(e->as.call.name), args, n);
    }
    case EXPR_INDEX:
        return ast_expr_index(xstrdup(e->as.index.name), ast_expr_clone(e->as.index.index));
    case EXPR_MATRIX_INDEX:
        return ast_expr_matrix_index(xstrdup(e->as.matrix_index.name),
                                     ast_expr_clone(e->as.matrix_index.row),
                                     ast_expr_clone(e->as.matrix_index.col));
    case EXPR_LENGTH:
        return ast_expr_length(xstrdup(e->as.length_name));
    case EXPR_ROW_LENGTH:
        return ast_expr_row_length(xstrdup(e->as.row_length_name));
    case EXPR_COL_LENGTH:
        return ast_expr_col_length(xstrdup(e->as.col_length_name));
    case EXPR_STRLEN:
        return ast_expr_strlen(ast_expr_clone(e->as.strlen_value));
    case EXPR_EXTRACT:
        return ast_expr_extract(ast_expr_clone(e->as.extract.value), xstrdup(e->as.extract.source_name));
    case EXPR_EXTRACT_RANGE:
        return ast_expr_extract_range(ast_expr_clone(e->as.extract_range.from),
                                      ast_expr_clone(e->as.extract_range.to),
                                      xstrdup(e->as.extract_range.source_name));
    case EXPR_COUNT:
        return ast_expr_count(ast_expr_clone(e->as.count.value), xstrdup(e->as.count.collection_name));
    case EXPR_CHECK:
        return ast_expr_check(ast_expr_clone(e->as.check.value), xstrdup(e->as.check.collection_name));
    case EXPR_POSITION:
        return ast_expr_position(ast_expr_clone(e->as.position.value), xstrdup(e->as.position.collection_name));
    case EXPR_CONVERT:
        return ast_expr_convert(ast_expr_clone(e->as.convert.value), e->as.convert.to_type);
    }

    return NULL;
}

void ast_free_expr(Expr *e) {
    if (!e) return;
    switch (e->kind) {
    case EXPR_LITERAL:
        if (e->as.lit.kind == LIT_STRING) free(e->as.lit.as.s);
        break;
    case EXPR_IDENT:
        free(e->as.ident);
        break;
    case EXPR_BINARY:
        ast_free_expr(e->as.bin.left);
        ast_free_expr(e->as.bin.right);
        break;
    case EXPR_UNARY_NEG:
        ast_free_expr(e->as.operand);
        break;
    case EXPR_CALL:
        free(e->as.call.name);
        for (size_t i = 0; i < e->as.call.count; i++)
            ast_free_expr(e->as.call.args[i]);
        free(e->as.call.args);
        break;
    case EXPR_INDEX:
        free(e->as.index.name);
        ast_free_expr(e->as.index.index);
        break;
    case EXPR_MATRIX_INDEX:
        free(e->as.matrix_index.name);
        ast_free_expr(e->as.matrix_index.row);
        ast_free_expr(e->as.matrix_index.col);
        break;
    case EXPR_LENGTH:
        free(e->as.length_name);
        break;
    case EXPR_ROW_LENGTH:
        free(e->as.row_length_name);
        break;
    case EXPR_COL_LENGTH:
        free(e->as.col_length_name);
        break;
    case EXPR_STRLEN:
        ast_free_expr(e->as.strlen_value);
        break;
    case EXPR_EXTRACT:
        ast_free_expr(e->as.extract.value);
        free(e->as.extract.source_name);
        break;
    case EXPR_EXTRACT_RANGE:
        ast_free_expr(e->as.extract_range.from);
        ast_free_expr(e->as.extract_range.to);
        free(e->as.extract_range.source_name);
        break;
    case EXPR_COUNT:
        ast_free_expr(e->as.count.value);
        free(e->as.count.collection_name);
        break;
    case EXPR_CHECK:
        ast_free_expr(e->as.check.value);
        free(e->as.check.collection_name);
        break;
    case EXPR_POSITION:
        ast_free_expr(e->as.position.value);
        free(e->as.position.collection_name);
        break;
    case EXPR_CONVERT:
        ast_free_expr(e->as.convert.value);
        break;
    }
    free(e);
}

/* ── List builders ──────────────────────────────────────────────── */

char **ast_ident_list_new(const char *first, size_t *out_count) {
    char **list = static_cast<char **>(xmalloc(sizeof(char *)));
    list[0] = xstrdup(first);
    *out_count = 1;
    return list;
}

char **ast_ident_list_push(char **list, size_t *count, const char *name) {
    list = static_cast<char **>(xrealloc(list, (*count + 1) * sizeof(char *)));
    list[*count] = xstrdup(name);
    (*count)++;
    return list;
}

Expr **ast_expr_list_new(Expr *first, size_t *out_count) {
    Expr **list = static_cast<Expr **>(xmalloc(sizeof(Expr *)));
    list[0] = first;
    *out_count = 1;
    return list;
}

Expr **ast_expr_list_push(Expr **list, size_t *count, Expr *e) {
    list = static_cast<Expr **>(xrealloc(list, (*count + 1) * sizeof(Expr *)));
    list[*count] = e;
    (*count)++;
    return list;
}

Param *ast_param_list_new(TypeKind type, const char *name, size_t *out_count) {
    Param *list = static_cast<Param *>(xmalloc(sizeof(Param)));
    list[0].type = type;
    list[0].name = xstrdup(name);
    *out_count = 1;
    return list;
}

Param *ast_param_list_push(Param *list, size_t *count,
                           TypeKind type, const char *name) {
    list = static_cast<Param *>(xrealloc(list, (*count + 1) * sizeof(Param)));
    list[*count].type = type;
    list[*count].name = xstrdup(name);
    (*count)++;
    return list;
}



/* ── Literal constructors ───────────────────────────────────────── */

Literal ast_lit_int(int64_t v) {
    Literal lit = {};
    lit.kind = LIT_INT;
    lit.as.i = v;
    return lit;
}

Literal ast_lit_float(double v) {
    Literal lit = {};
    lit.kind = LIT_FLOAT;
    lit.as.d = v;
    return lit;
}

Literal ast_lit_char(char v) {
    Literal lit = {};
    lit.kind = LIT_CHAR;
    lit.as.c = v;
    return lit;
}

Literal ast_lit_string(const char *s) {
    Literal lit = {};
    lit.kind = LIT_STRING;
    lit.as.s = xstrdup(s);
    return lit;
}

Literal ast_lit_bool(int v) {
    Literal lit = {};
    lit.kind = LIT_BOOL;
    lit.as.b = v ? 1 : 0;
    return lit;
}

/* ── Cleanup ────────────────────────────────────────────────────── */

static void free_stmt(Stmt *s) {
    if (!s) return;
    switch (s->kind) {
    case STMT_DECL: {
        DeclStmt *d = &s->as.decl;
        for (size_t i = 0; i < d->count; i++) {
            free(d->names[i]);
            ast_free_expr(d->values[i]);
        }
        free(d->names);
        free(d->values);
        break;
    }
    case STMT_ARRAY_DECL: {
        free(s->as.array_decl.name);
        ast_free_expr(s->as.array_decl.size);
        break;
    }
    case STMT_TABLE_DECL: {
        free(s->as.table_decl.name);
        ast_free_expr(s->as.table_decl.size);
        break;
    }
    case STMT_MATRIX_DECL: {
        free(s->as.matrix_decl.name);
        ast_free_expr(s->as.matrix_decl.rows);
        ast_free_expr(s->as.matrix_decl.cols);
        break;
    }
    case STMT_COLLECTION_SET: {
        CollectionSetStmt *cs = &s->as.collection_set;
        free(cs->name);
        ast_free_expr(cs->index);
        ast_free_expr(cs->value);
        break;
    }
    case STMT_MATRIX_SET: {
        MatrixSetStmt *ms = &s->as.matrix_set;
        free(ms->name);
        ast_free_expr(ms->row);
        ast_free_expr(ms->col);
        ast_free_expr(ms->value);
        break;
    }
    case STMT_COLLECTION_SET_INPUT: {
        CollectionSetInputStmt *cs = &s->as.collection_set_input;
        free(cs->name);
        ast_free_expr(cs->index);
        for (size_t i = 0; i < cs->message_count; i++)
            ast_free_expr(cs->message_parts[i]);
        free(cs->message_parts);
        break;
    }
    case STMT_COLLECTION_APPEND: {
        CollectionAppendStmt *ca = &s->as.collection_append;
        free(ca->name);
        ast_free_expr(ca->value);
        if (ca->index) ast_free_expr(ca->index);
        break;
    }
    case STMT_COLLECTION_SORT: {
        CollectionSortStmt *cs = &s->as.collection_sort;
        free(cs->name);
        break;
    }
    case STMT_PRINT: {
        PrintStmt *p = &s->as.print;
        for (size_t i = 0; i < p->count; i++)
            ast_free_expr(p->args[i]);
        free(p->args);
        break;
    }
    case STMT_SET: {
        SetStmt *st = &s->as.set;
        free(st->target);
        ast_free_expr(st->value);
        break;
    }
    case STMT_CONVERT: {
        free(s->as.convert.target);
        break;
    }
    case STMT_SET_MATRIX_CTX: {
        SetMatrixCtxStmt *sm = &s->as.set_matrix_ctx;
        free(sm->target);
        ast_free_expr(sm->row);
        ast_free_expr(sm->col);
        break;
    }
    case STMT_INPUT: {
        InputStmt *in = &s->as.input;
        free(in->target);
        for (size_t i = 0; i < in->message_count; i++)
            ast_free_expr(in->message_parts[i]);
        free(in->message_parts);
        break;
    }
    case STMT_EMIT: {
        EmitStmt *em = &s->as.emit;
        free(em->name);
        for (size_t i = 0; i < em->count; i++)
            ast_free_expr(em->args[i]);
        free(em->args);
        break;
    }
    case STMT_GIVEBACK: {
        ast_free_expr(s->as.giveback.value);
        break;
    }
    case STMT_FUNCDEF: {
        FuncDefStmt *fd = &s->as.funcdef;
        free(fd->name);
        for (size_t i = 0; i < fd->param_count; i++)
            free(fd->params[i].name);
        free(fd->params);
        free_stmt(fd->body);
        break;
    }
    case STMT_IF: {
        IfStmt *fi = &s->as.if_;
        ast_free_cond(fi->cond);
        free_stmt(fi->then_block);
        free_stmt(fi->else_block);
        break;
    }
    case STMT_BLOCK: {
        BlockStmt *blk = &s->as.block;
        for (size_t i = 0; i < blk->count; i++)
            free_stmt(blk->stmts[i]);
        free(blk->stmts);
        break;
    }
    case STMT_WHILE: {
        ast_free_cond(s->as.while_.cond);
        free_stmt(s->as.while_.body);
        break;
    }
    case STMT_FOR: {
        free(s->as.for_.var);
        ast_free_expr(s->as.for_.from);
        ast_free_expr(s->as.for_.to);
        if (s->as.for_.step) ast_free_expr(s->as.for_.step);
        free_stmt(s->as.for_.body);
        break;
    }
    case STMT_FOR_EACH: {
        free(s->as.for_each.index_var);
        ast_free_expr(s->as.for_each.from);
        ast_free_expr(s->as.for_each.to);
        if (s->as.for_each.step) ast_free_expr(s->as.for_each.step);
        free(s->as.for_each.collection);
        free_stmt(s->as.for_each.body);
        break;
    }
    case STMT_FOR_MATRIX: {
        free(s->as.for_matrix.row_var);
        free(s->as.for_matrix.col_var);
        free(s->as.for_matrix.matrix);
        if (s->as.for_matrix.row_from) ast_free_expr(s->as.for_matrix.row_from);
        if (s->as.for_matrix.row_to) ast_free_expr(s->as.for_matrix.row_to);
        if (s->as.for_matrix.col_from) ast_free_expr(s->as.for_matrix.col_from);
        if (s->as.for_matrix.col_to) ast_free_expr(s->as.for_matrix.col_to);
        free_stmt(s->as.for_matrix.body);
        break;
    }
    case STMT_REPEAT: {
        free_stmt(s->as.repeat_.body);
        ast_free_cond(s->as.repeat_.cond);
        break;
    }
    case STMT_ATTEMPT: {
        ast_free_expr(s->as.attempt_.max);
        ast_free_cond(s->as.attempt_.cond);
        free_stmt(s->as.attempt_.body);
        if (s->as.attempt_.failure) free_stmt(s->as.attempt_.failure);
        break;
    }
    case STMT_FILE_OPEN: {
        ast_free_expr(s->as.file_open.path);
        free_stmt(s->as.file_open.body);
        break;
    }
    case STMT_FILE_WRITE: {
        if (s->as.file_write.line) ast_free_expr(s->as.file_write.line);
        ast_free_expr(s->as.file_write.value);
        break;
    }
    case STMT_FILE_READ: {
        free(s->as.file_read.target);
        if (s->as.file_read.count) ast_free_expr(s->as.file_read.count);
        if (s->as.file_read.from_line) ast_free_expr(s->as.file_read.from_line);
        if (s->as.file_read.line) ast_free_expr(s->as.file_read.line);
        break;
    }
    case STMT_FILE_CLEAR: {
        if (s->as.file_clear.line) ast_free_expr(s->as.file_clear.line);
        if (s->as.file_clear.from_line) ast_free_expr(s->as.file_clear.from_line);
        if (s->as.file_clear.to_line) ast_free_expr(s->as.file_clear.to_line);
        break;
    }
    case STMT_FILE_SET_TITLE: {
        ast_free_expr(s->as.file_set_title.value);
        break;
    }
    case STMT_FILE_CLOSE:
        break;
    }
    free(s);
}

void ast_free_stmt(Stmt *s) {
    free_stmt(s);
}

void ast_free_program(Program *p) {
    if (!p) return;
    for (size_t i = 0; i < p->count; i++)
        free_stmt(p->stmts[i]);
    free(p->stmts);
    free(p);
}

