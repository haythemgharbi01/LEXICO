/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Skeleton implementation for Bison GLR parsers in C

   Copyright (C) 2002-2015, 2018-2021 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C GLR parser skeleton written by Paul Hilfinger.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "glr.c"

/* Pure parsers.  */
#define YYPURE 0






/* First part of user prologue.  */
#line 31 "src\\lexico.y"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.hpp"

/* Flex/Bison glue */
extern int  yylex(void);
extern int  yylineno;
extern FILE *yyin;
void yyerror(const char *msg);

/* Result of parsing â€“ set on success, NULL on failure */
Program *lexico_parsed_program = NULL;
char lexico_last_parse_error[512] = "";

/* Temporaries used to carry list lengths alongside the pointers. */
static size_t id_list_len    = 0;
static size_t val_list_len   = 0;
static size_t print_list_len = 0;
static size_t prompt_list_len = 0;
static size_t param_list_len = 0;
static size_t call_list_len  = 0;
static size_t mesh_init_list_len = 0;

typedef struct MatrixInit {
    Expr   **values;
    size_t   value_count;
    size_t   value_cap;
    size_t  *row_sizes;
    size_t   row_count;
    size_t   row_cap;
} MatrixInit;

static MatrixInit *matrix_init_new(void) {
    MatrixInit *m = static_cast<MatrixInit *>(malloc(sizeof(*m)));
    if (!m) {
        fprintf(stderr, "Fatal: out of memory.\n");
        exit(EXIT_FAILURE);
    }
    m->values = NULL;
    m->value_count = 0;
    m->value_cap = 0;
    m->row_sizes = NULL;
    m->row_count = 0;
    m->row_cap = 0;
    return m;
}

static void matrix_init_push_value(MatrixInit *m, Expr *e) {
    if (m->value_count == m->value_cap) {
        size_t next = m->value_cap ? m->value_cap * 2 : 8;
        Expr **nv = static_cast<Expr **>(realloc(m->values, next * sizeof(Expr *)));
        if (!nv) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        m->values = nv;
        m->value_cap = next;
    }
    m->values[m->value_count++] = e;
}

static void matrix_init_add_row(MatrixInit *m, size_t row_size) {
    if (m->row_count == m->row_cap) {
        size_t next = m->row_cap ? m->row_cap * 2 : 4;
        size_t *nr = static_cast<size_t *>(realloc(m->row_sizes, next * sizeof(size_t)));
        if (!nr) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        m->row_sizes = nr;
        m->row_cap = next;
    }
    m->row_sizes[m->row_count++] = row_size;
}

static void matrix_init_free(MatrixInit *m, int free_exprs) {
    if (!m) return;
    if (free_exprs) {
        for (size_t i = 0; i < m->value_count; i++)
            ast_free_expr(m->values[i]);
    }
    free(m->values);
    free(m->row_sizes);
    free(m);
}

static MatrixInit *matrix_init_merge(MatrixInit *left, MatrixInit *right) {
    if (!left || !right) return left ? left : right;
    for (size_t i = 0; i < right->value_count; i++)
        matrix_init_push_value(left, right->values[i]);
    for (size_t r = 0; r < right->row_count; r++)
        matrix_init_add_row(left, right->row_sizes[r]);
    free(right->values);
    free(right->row_sizes);
    free(right);
    return left;
}

static Expr *default_init_expr(TypeKind t) {
    switch (t) {
    case TYPE_INT:    return ast_expr_literal(ast_lit_int(0));
    case TYPE_FLOAT:  return ast_expr_literal(ast_lit_float(0.0));
    case TYPE_CHAR:   return ast_expr_literal(ast_lit_char('\0'));
    case TYPE_STRING: return ast_expr_literal(ast_lit_string(""));
    case TYPE_BOOL:   return ast_expr_literal(ast_lit_bool(0));
    }
    return ast_expr_literal(ast_lit_int(0));
}

static Expr **default_init_list(TypeKind t, size_t n) {
    Expr **vals = static_cast<Expr **>(malloc(n * sizeof(Expr *)));
    if (!vals && n) {
        fprintf(stderr, "Fatal: out of memory.\n");
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < n; i++)
        vals[i] = default_init_expr(t);
    return vals;
}

static Stmt *make_decl_then_input(int line, TypeKind t, char *name, Expr **message_parts, size_t message_count) {
    Stmt *blk = ast_new_block(line);

    char **names = static_cast<char **>(malloc(sizeof(char *)));
    Expr **vals  = static_cast<Expr **>(malloc(sizeof(Expr *)));
    if (!names || !vals) {
        fprintf(stderr, "Fatal: out of memory.\n");
        exit(EXIT_FAILURE);
    }

    names[0] = name; /* takes ownership */
    vals[0]  = default_init_expr(t);

    size_t nlen = strlen(name);
    char *input_target = static_cast<char *>(malloc(nlen + 1));
    if (!input_target) {
        fprintf(stderr, "Fatal: out of memory.\n");
        exit(EXIT_FAILURE);
    }
    memcpy(input_target, name, nlen + 1);

    ast_block_add(blk, ast_new_decl(line, t, names, vals, 1));
    ast_block_add(blk, ast_new_input(line, input_target, message_parts, message_count));
    return blk;
}

typedef struct {
    TypeKind type;
    char *name;
} ClassField;

typedef enum {
    PROFILE_SET_VALUE,
    PROFILE_SET_INPUT
} ProfileActionKind;

typedef struct {
    ProfileActionKind kind;
    char *field_name;
    Expr *value;
    Expr **message_parts;
    size_t message_count;
} ProfileAction;

typedef struct {
    char *name;
    ProfileAction *actions;
    size_t action_count;
    size_t action_cap;
} ClassProfile;

typedef struct {
    char *name;
    TypeKind return_type;
    Param *params;
    size_t param_count;
    Stmt *body;
} ClassRoutine;

typedef struct {
    char *name;
    ClassField *fields;
    size_t field_count;
    size_t field_cap;
    ClassProfile *profiles;
    size_t profile_count;
    size_t profile_cap;
    ClassRoutine *routines;
    size_t routine_count;
    size_t routine_cap;
} ClassTemplate;

static ClassTemplate *g_class_templates = NULL;
static size_t g_class_templates_count = 0;
static size_t g_class_templates_cap = 0;
static ClassTemplate *g_current_class = NULL;
static ClassProfile *g_current_profile = NULL;

static char *make_prefixed_name(const char *obj_name, const char *field_name);
static char *map_field_ref_name(const ClassTemplate *ct, const char *obj_name, const char *name);
static char *map_callable_ref_name(const ClassTemplate *ct, const char *obj_name, const char *name);
static Expr *clone_expr_for_object(const ClassTemplate *ct, const char *obj_name, const Expr *e);
static CondExpr *clone_cond_for_object(const ClassTemplate *ct, const char *obj_name, const CondExpr *c);
static Stmt *clone_stmt_for_object(const ClassTemplate *ct, const char *obj_name, const Stmt *s);
static CondExpr *clone_cond_raw(const CondExpr *c);
static Stmt *clone_stmt_raw(const Stmt *s);
static Param *clone_params(const Param *src, size_t n);
static int class_add_field(TypeKind type, char *name);
static int class_add_routine(char *name, Param *params, size_t param_count, TypeKind return_type, Stmt *body);

static char *dup_text(const char *s) {
    size_t n = strlen(s);
    char *d = static_cast<char *>(malloc(n + 1));
    if (!d) {
        fprintf(stderr, "Fatal: out of memory.\n");
        exit(EXIT_FAILURE);
    }
    memcpy(d, s, n + 1);
    return d;
}

static char *map_field_ref_name(const ClassTemplate *ct, const char *obj_name, const char *name) {
    for (size_t i = 0; i < ct->field_count; i++) {
        if (strcmp(ct->fields[i].name, name) == 0)
            return make_prefixed_name(obj_name, name);
    }
    return dup_text(name);
}

static char *map_callable_ref_name(const ClassTemplate *ct, const char *obj_name, const char *name) {
    for (size_t i = 0; i < ct->routine_count; i++) {
        if (strcmp(ct->routines[i].name, name) == 0)
            return make_prefixed_name(obj_name, name);
    }
    return dup_text(name);
}

static Expr *clone_expr_for_object(const ClassTemplate *ct, const char *obj_name, const Expr *e) {
    if (!e) return NULL;
    switch (e->kind) {
    case EXPR_LITERAL:
        return ast_expr_literal(e->as.lit.kind == LIT_STRING
                                    ? ast_lit_string(e->as.lit.as.s ? e->as.lit.as.s : "")
                                    : (e->as.lit.kind == LIT_INT ? ast_lit_int(e->as.lit.as.i)
                                       : e->as.lit.kind == LIT_FLOAT ? ast_lit_float(e->as.lit.as.d)
                                       : e->as.lit.kind == LIT_CHAR ? ast_lit_char(e->as.lit.as.c)
                                       : ast_lit_bool(e->as.lit.as.b)));
    case EXPR_IDENT: {
        char *m = map_field_ref_name(ct, obj_name, e->as.ident);
        Expr *out = ast_expr_ident(m);
        free(m);
        return out;
    }
    case EXPR_BINARY:
        return ast_expr_binary(e->as.bin.op,
                               clone_expr_for_object(ct, obj_name, e->as.bin.left),
                               clone_expr_for_object(ct, obj_name, e->as.bin.right));
    case EXPR_UNARY_NEG:
        return ast_expr_neg(clone_expr_for_object(ct, obj_name, e->as.operand));
    case EXPR_CALL: {
        size_t n = e->as.call.count;
        Expr **args = n ? static_cast<Expr **>(malloc(n * sizeof(Expr *))) : NULL;
        if (n && !args) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        for (size_t i = 0; i < n; i++) args[i] = clone_expr_for_object(ct, obj_name, e->as.call.args[i]);
        return ast_expr_call(map_callable_ref_name(ct, obj_name, e->as.call.name), args, n);
    }
    case EXPR_INDEX:
        return ast_expr_index(map_field_ref_name(ct, obj_name, e->as.index.name),
                              clone_expr_for_object(ct, obj_name, e->as.index.index));
    case EXPR_MATRIX_INDEX:
        return ast_expr_matrix_index(map_field_ref_name(ct, obj_name, e->as.matrix_index.name),
                                     clone_expr_for_object(ct, obj_name, e->as.matrix_index.row),
                                     clone_expr_for_object(ct, obj_name, e->as.matrix_index.col));
    case EXPR_LENGTH:
        return ast_expr_length(map_field_ref_name(ct, obj_name, e->as.length_name));
    case EXPR_ROW_LENGTH:
        return ast_expr_row_length(map_field_ref_name(ct, obj_name, e->as.row_length_name));
    case EXPR_COL_LENGTH:
        return ast_expr_col_length(map_field_ref_name(ct, obj_name, e->as.col_length_name));
    case EXPR_STRLEN:
        return ast_expr_strlen(clone_expr_for_object(ct, obj_name, e->as.strlen_value));
    case EXPR_EXTRACT:
        return ast_expr_extract(clone_expr_for_object(ct, obj_name, e->as.extract.value),
                                map_field_ref_name(ct, obj_name, e->as.extract.source_name));
    case EXPR_EXTRACT_RANGE:
        return ast_expr_extract_range(clone_expr_for_object(ct, obj_name, e->as.extract_range.from),
                                      clone_expr_for_object(ct, obj_name, e->as.extract_range.to),
                                      map_field_ref_name(ct, obj_name, e->as.extract_range.source_name));
    case EXPR_COUNT:
        return ast_expr_count(clone_expr_for_object(ct, obj_name, e->as.count.value),
                              map_field_ref_name(ct, obj_name, e->as.count.collection_name));
    case EXPR_CHECK:
        return ast_expr_check(clone_expr_for_object(ct, obj_name, e->as.check.value),
                              map_field_ref_name(ct, obj_name, e->as.check.collection_name));
    case EXPR_POSITION:
        return ast_expr_position(clone_expr_for_object(ct, obj_name, e->as.position.value),
                                 map_field_ref_name(ct, obj_name, e->as.position.collection_name));
    case EXPR_CONVERT:
        return ast_expr_convert(clone_expr_for_object(ct, obj_name, e->as.convert.value),
                                e->as.convert.to_type);
    }
    return NULL;
}

static Expr **clone_expr_list_for_object(const ClassTemplate *ct, const char *obj_name, Expr **src, size_t n) {
    Expr **dst = NULL;
    if (n) {
        dst = static_cast<Expr **>(malloc(n * sizeof(Expr *)));
        if (!dst) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
    }
    for (size_t i = 0; i < n; i++) dst[i] = clone_expr_for_object(ct, obj_name, src[i]);
    return dst;
}

static void free_profile_action(ProfileAction *a) {
    if (!a) return;
    free(a->field_name);
    ast_free_expr(a->value);
    for (size_t i = 0; i < a->message_count; i++) ast_free_expr(a->message_parts[i]);
    free(a->message_parts);
}

static void free_class_registry(void) {
    for (size_t i = 0; i < g_class_templates_count; i++) {
        ClassTemplate *ct = &g_class_templates[i];
        free(ct->name);
        for (size_t f = 0; f < ct->field_count; f++) free(ct->fields[f].name);
        free(ct->fields);
        for (size_t p = 0; p < ct->profile_count; p++) {
            ClassProfile *cp = &ct->profiles[p];
            free(cp->name);
            for (size_t a = 0; a < cp->action_count; a++) free_profile_action(&cp->actions[a]);
            free(cp->actions);
        }
        free(ct->profiles);
        for (size_t r = 0; r < ct->routine_count; r++) {
            ClassRoutine *cr = &ct->routines[r];
            free(cr->name);
            for (size_t p = 0; p < cr->param_count; p++) free(cr->params[p].name);
            free(cr->params);
            ast_free_stmt(cr->body);
        }
        free(ct->routines);
    }
    free(g_class_templates);
    g_class_templates = NULL;
    g_class_templates_count = 0;
    g_class_templates_cap = 0;
    g_current_class = NULL;
    g_current_profile = NULL;
}

void lexico_class_registry_reset(void) {
    free_class_registry();
}

static ClassTemplate *find_class_template(const char *name) {
    for (size_t i = 0; i < g_class_templates_count; i++) {
        if (strcmp(g_class_templates[i].name, name) == 0) return &g_class_templates[i];
    }
    return NULL;
}

static ClassField *find_class_field(ClassTemplate *ct, const char *name) {
    for (size_t i = 0; i < ct->field_count; i++) {
        if (strcmp(ct->fields[i].name, name) == 0) return &ct->fields[i];
    }
    return NULL;
}

static ClassProfile *find_class_profile(ClassTemplate *ct, const char *name) {
    for (size_t i = 0; i < ct->profile_count; i++) {
        if (strcmp(ct->profiles[i].name, name) == 0) return &ct->profiles[i];
    }
    return NULL;
}

static ClassRoutine *find_class_routine(ClassTemplate *ct, const char *name) {
    for (size_t i = 0; i < ct->routine_count; i++) {
        if (strcmp(ct->routines[i].name, name) == 0) return &ct->routines[i];
    }
    return NULL;
}

static int class_begin(char *name) {
    if (find_class_template(name)) {
        fprintf(stderr, "Error (line %d): class '%s' is already defined.\n", yylineno, name);
        free(name);
        g_current_class = NULL;
        return 0;
    }
    if (g_class_templates_count == g_class_templates_cap) {
        size_t next = g_class_templates_cap ? g_class_templates_cap * 2 : 8;
        ClassTemplate *nt = static_cast<ClassTemplate *>(realloc(g_class_templates, next * sizeof(ClassTemplate)));
        if (!nt) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        g_class_templates = nt;
        g_class_templates_cap = next;
    }
    ClassTemplate *ct = &g_class_templates[g_class_templates_count++];
    memset(ct, 0, sizeof(*ct));
    ct->name = name;
    g_current_class = ct;
    g_current_profile = NULL;
    return 1;
}

static int class_begin_inherits(char *name, char *parent_name) {
    if (!class_begin(name)) {
        free(parent_name);
        return 0;
    }

    ClassTemplate *parent = find_class_template(parent_name);
    if (!parent) {
        fprintf(stderr, "Error (line %d): parent class '%s' is not defined.\n", yylineno, parent_name);
        free(parent_name);
        g_current_class = NULL;
        return 0;
    }

    for (size_t i = 0; i < parent->field_count; i++) {
        if (!class_add_field(parent->fields[i].type, dup_text(parent->fields[i].name))) {
            free(parent_name);
            return 0;
        }
    }

    for (size_t i = 0; i < parent->routine_count; i++) {
        ClassRoutine *pr = &parent->routines[i];
        Param *params = clone_params(pr->params, pr->param_count);
        Stmt *body = clone_stmt_raw(pr->body);
        if (!class_add_routine(dup_text(pr->name), params, pr->param_count, pr->return_type, body)) {
            free(parent_name);
            return 0;
        }
    }

    free(parent_name);
    return 1;
}

static int class_override_field(TypeKind type, char *name) {
    if (!g_current_class) {
        free(name);
        return 0;
    }
    ClassField *f = find_class_field(g_current_class, name);
    if (!f) {
        fprintf(stderr, "Error (line %d): cannot override unknown field '%s' in class '%s'.\n",
                yylineno, name, g_current_class->name);
        free(name);
        return 0;
    }
    f->type = type;
    free(name);
    return 1;
}

static int class_override_routine(char *name, Param *params, size_t param_count, TypeKind return_type, Stmt *body) {
    if (!g_current_class) {
        free(name);
        for (size_t i = 0; i < param_count; i++) free(params[i].name);
        free(params);
        ast_free_stmt(body);
        return 0;
    }
    ClassRoutine *r = find_class_routine(g_current_class, name);
    if (!r) {
        fprintf(stderr, "Error (line %d): cannot override unknown routine '%s' in class '%s'.\n",
                yylineno, name, g_current_class->name);
        free(name);
        for (size_t i = 0; i < param_count; i++) free(params[i].name);
        free(params);
        ast_free_stmt(body);
        return 0;
    }

    free(r->name);
    for (size_t i = 0; i < r->param_count; i++) free(r->params[i].name);
    free(r->params);
    ast_free_stmt(r->body);

    r->name = name;
    r->params = params;
    r->param_count = param_count;
    r->return_type = return_type;
    r->body = body;
    return 1;
}

static int class_add_field(TypeKind type, char *name) {
    if (!g_current_class) {
        free(name);
        return 0;
    }
    if (find_class_field(g_current_class, name)) {
        fprintf(stderr, "Error (line %d): field '%s' is already defined in class '%s'.\n",
                yylineno, name, g_current_class->name);
        free(name);
        return 0;
    }
    if (g_current_class->field_count == g_current_class->field_cap) {
        size_t next = g_current_class->field_cap ? g_current_class->field_cap * 2 : 8;
        ClassField *nf = static_cast<ClassField *>(realloc(g_current_class->fields, next * sizeof(ClassField)));
        if (!nf) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        g_current_class->fields = nf;
        g_current_class->field_cap = next;
    }
    ClassField *f = &g_current_class->fields[g_current_class->field_count++];
    f->type = type;
    f->name = name;
    return 1;
}

static int profile_begin(char *name) {
    if (!g_current_class) {
        free(name);
        return 0;
    }
    if (find_class_profile(g_current_class, name)) {
        fprintf(stderr, "Error (line %d): profile '%s' is already defined in class '%s'.\n",
                yylineno, name, g_current_class->name);
        free(name);
        g_current_profile = NULL;
        return 0;
    }
    if (g_current_class->profile_count == g_current_class->profile_cap) {
        size_t next = g_current_class->profile_cap ? g_current_class->profile_cap * 2 : 4;
        ClassProfile *np = static_cast<ClassProfile *>(realloc(g_current_class->profiles, next * sizeof(ClassProfile)));
        if (!np) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        g_current_class->profiles = np;
        g_current_class->profile_cap = next;
    }
    ClassProfile *p = &g_current_class->profiles[g_current_class->profile_count++];
    memset(p, 0, sizeof(*p));
    p->name = name;
    g_current_profile = p;
    return 1;
}

static void profile_end(void) {
    g_current_profile = NULL;
}

static int profile_add_set(char *field_name, Expr *value) {
    if (!g_current_class || !g_current_profile) {
        free(field_name);
        ast_free_expr(value);
        return 0;
    }
    if (!find_class_field(g_current_class, field_name)) {
        fprintf(stderr, "Error (line %d): unknown field '%s' in class '%s'.\n",
                yylineno, field_name, g_current_class->name);
        free(field_name);
        ast_free_expr(value);
        return 0;
    }
    if (g_current_profile->action_count == g_current_profile->action_cap) {
        size_t next = g_current_profile->action_cap ? g_current_profile->action_cap * 2 : 8;
        ProfileAction *na = static_cast<ProfileAction *>(realloc(g_current_profile->actions, next * sizeof(ProfileAction)));
        if (!na) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        g_current_profile->actions = na;
        g_current_profile->action_cap = next;
    }
    ProfileAction *a = &g_current_profile->actions[g_current_profile->action_count++];
    memset(a, 0, sizeof(*a));
    a->kind = PROFILE_SET_VALUE;
    a->field_name = field_name;
    a->value = value;
    return 1;
}

static int profile_add_input(char *field_name, Expr **message_parts, size_t message_count) {
    if (!g_current_class || !g_current_profile) {
        free(field_name);
        for (size_t i = 0; i < message_count; i++) ast_free_expr(message_parts[i]);
        free(message_parts);
        return 0;
    }
    if (!find_class_field(g_current_class, field_name)) {
        fprintf(stderr, "Error (line %d): unknown field '%s' in class '%s'.\n",
                yylineno, field_name, g_current_class->name);
        free(field_name);
        for (size_t i = 0; i < message_count; i++) ast_free_expr(message_parts[i]);
        free(message_parts);
        return 0;
    }
    if (g_current_profile->action_count == g_current_profile->action_cap) {
        size_t next = g_current_profile->action_cap ? g_current_profile->action_cap * 2 : 8;
        ProfileAction *na = static_cast<ProfileAction *>(realloc(g_current_profile->actions, next * sizeof(ProfileAction)));
        if (!na) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        g_current_profile->actions = na;
        g_current_profile->action_cap = next;
    }
    ProfileAction *a = &g_current_profile->actions[g_current_profile->action_count++];
    memset(a, 0, sizeof(*a));
    a->kind = PROFILE_SET_INPUT;
    a->field_name = field_name;
    a->message_parts = message_parts;
    a->message_count = message_count;
    return 1;
}

static Param *clone_params(const Param *src, size_t n) {
    Param *out = n ? static_cast<Param *>(malloc(n * sizeof(Param))) : NULL;
    if (n && !out) {
        fprintf(stderr, "Fatal: out of memory.\n");
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < n; i++) {
        out[i].type = src[i].type;
        out[i].name = dup_text(src[i].name);
    }
    return out;
}

static CondExpr *clone_cond_raw(const CondExpr *c) {
    if (!c) return NULL;
    switch (c->kind) {
    case COND_CMP:
        return ast_cond_cmp(c->as.cmp.op,
                            ast_expr_clone(c->as.cmp.left),
                            ast_expr_clone(c->as.cmp.right));
    case COND_AND:
        return ast_cond_and(clone_cond_raw(c->as.logic.left),
                            clone_cond_raw(c->as.logic.right));
    case COND_OR:
        return ast_cond_or(clone_cond_raw(c->as.logic.left),
                           clone_cond_raw(c->as.logic.right));
    case COND_NOT:
        return ast_cond_not(clone_cond_raw(c->as.operand));
    }
    return NULL;
}

static Stmt *clone_stmt_raw(const Stmt *s) {
    if (!s) return NULL;
    switch (s->kind) {
    case STMT_DECL: {
        size_t n = s->as.decl.count;
        char **names = n ? static_cast<char **>(malloc(n * sizeof(char *))) : NULL;
        Expr **vals = n ? static_cast<Expr **>(malloc(n * sizeof(Expr *))) : NULL;
        if (n && (!names || !vals)) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        for (size_t i = 0; i < n; i++) {
            names[i] = dup_text(s->as.decl.names[i]);
            vals[i] = ast_expr_clone(s->as.decl.values[i]);
        }
        return ast_new_decl(s->line, s->as.decl.type, names, vals, n);
    }
    case STMT_PRINT: {
        size_t n = s->as.print.count;
        Expr **args = n ? static_cast<Expr **>(malloc(n * sizeof(Expr *))) : NULL;
        if (n && !args) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        for (size_t i = 0; i < n; i++) args[i] = ast_expr_clone(s->as.print.args[i]);
        return ast_new_print(s->line, args, n);
    }
    case STMT_SET:
        return ast_new_set(s->line, dup_text(s->as.set.target), ast_expr_clone(s->as.set.value));
    case STMT_INPUT: {
        size_t n = s->as.input.message_count;
        Expr **msgs = n ? static_cast<Expr **>(malloc(n * sizeof(Expr *))) : NULL;
        if (n && !msgs) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        for (size_t i = 0; i < n; i++) msgs[i] = ast_expr_clone(s->as.input.message_parts[i]);
        return ast_new_input(s->line, dup_text(s->as.input.target), msgs, n);
    }
    case STMT_EMIT: {
        size_t n = s->as.emit.count;
        Expr **args = n ? static_cast<Expr **>(malloc(n * sizeof(Expr *))) : NULL;
        if (n && !args) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        for (size_t i = 0; i < n; i++) args[i] = ast_expr_clone(s->as.emit.args[i]);
        return ast_new_emit(s->line, dup_text(s->as.emit.name), args, n);
    }
    case STMT_GIVEBACK:
        return ast_new_giveback(s->line, ast_expr_clone(s->as.giveback.value));
    case STMT_BLOCK: {
        Stmt *b = ast_new_block(s->line);
        for (size_t i = 0; i < s->as.block.count; i++)
            ast_block_add(b, clone_stmt_raw(s->as.block.stmts[i]));
        return b;
    }
    case STMT_IF:
        return ast_new_if(s->line,
                          clone_cond_raw(s->as.if_.cond),
                          clone_stmt_raw(s->as.if_.then_block),
                          clone_stmt_raw(s->as.if_.else_block));
    case STMT_WHILE:
        return ast_new_while(s->line,
                             clone_cond_raw(s->as.while_.cond),
                             clone_stmt_raw(s->as.while_.body));
    default:
        fprintf(stderr, "Error (line %d): inherited routine body contains unsupported statement kind.\n", s->line);
        return ast_new_block(s->line);
    }
}

static CondExpr *clone_cond_for_object(const ClassTemplate *ct, const char *obj_name, const CondExpr *c) {
    if (!c) return NULL;
    switch (c->kind) {
    case COND_CMP:
        return ast_cond_cmp(c->as.cmp.op,
                            clone_expr_for_object(ct, obj_name, c->as.cmp.left),
                            clone_expr_for_object(ct, obj_name, c->as.cmp.right));
    case COND_AND:
        return ast_cond_and(clone_cond_for_object(ct, obj_name, c->as.logic.left),
                            clone_cond_for_object(ct, obj_name, c->as.logic.right));
    case COND_OR:
        return ast_cond_or(clone_cond_for_object(ct, obj_name, c->as.logic.left),
                           clone_cond_for_object(ct, obj_name, c->as.logic.right));
    case COND_NOT:
        return ast_cond_not(clone_cond_for_object(ct, obj_name, c->as.operand));
    }
    return NULL;
}

static Stmt *clone_stmt_for_object(const ClassTemplate *ct, const char *obj_name, const Stmt *s) {
    if (!s) return NULL;
    switch (s->kind) {
    case STMT_DECL: {
        char **names = s->as.decl.count ? static_cast<char **>(malloc(s->as.decl.count * sizeof(char *))) : NULL;
        Expr **vals = s->as.decl.count ? static_cast<Expr **>(malloc(s->as.decl.count * sizeof(Expr *))) : NULL;
        if (s->as.decl.count && (!names || !vals)) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        for (size_t i = 0; i < s->as.decl.count; i++) {
            names[i] = map_field_ref_name(ct, obj_name, s->as.decl.names[i]);
            vals[i] = clone_expr_for_object(ct, obj_name, s->as.decl.values[i]);
        }
        return ast_new_decl(s->line, s->as.decl.type, names, vals, s->as.decl.count);
    }
    case STMT_PRINT: {
        Expr **args = s->as.print.count ? static_cast<Expr **>(malloc(s->as.print.count * sizeof(Expr *))) : NULL;
        if (s->as.print.count && !args) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        for (size_t i = 0; i < s->as.print.count; i++)
            args[i] = clone_expr_for_object(ct, obj_name, s->as.print.args[i]);
        return ast_new_print(s->line, args, s->as.print.count);
    }
    case STMT_SET:
        return ast_new_set(s->line,
                           map_field_ref_name(ct, obj_name, s->as.set.target),
                           clone_expr_for_object(ct, obj_name, s->as.set.value));
    case STMT_INPUT: {
        Expr **msgs = clone_expr_list_for_object(ct, obj_name,
                                                 s->as.input.message_parts,
                                                 s->as.input.message_count);
        return ast_new_input(s->line,
                             map_field_ref_name(ct, obj_name, s->as.input.target),
                             msgs,
                             s->as.input.message_count);
    }
    case STMT_EMIT: {
        Expr **args = s->as.emit.count ? static_cast<Expr **>(malloc(s->as.emit.count * sizeof(Expr *))) : NULL;
        if (s->as.emit.count && !args) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        for (size_t i = 0; i < s->as.emit.count; i++)
            args[i] = clone_expr_for_object(ct, obj_name, s->as.emit.args[i]);
        return ast_new_emit(s->line,
                            map_callable_ref_name(ct, obj_name, s->as.emit.name),
                            args,
                            s->as.emit.count);
    }
    case STMT_GIVEBACK:
        return ast_new_giveback(s->line, clone_expr_for_object(ct, obj_name, s->as.giveback.value));
    case STMT_BLOCK: {
        Stmt *b = ast_new_block(s->line);
        for (size_t i = 0; i < s->as.block.count; i++)
            ast_block_add(b, clone_stmt_for_object(ct, obj_name, s->as.block.stmts[i]));
        return b;
    }
    case STMT_IF:
        return ast_new_if(s->line,
                          clone_cond_for_object(ct, obj_name, s->as.if_.cond),
                          clone_stmt_for_object(ct, obj_name, s->as.if_.then_block),
                          clone_stmt_for_object(ct, obj_name, s->as.if_.else_block));
    case STMT_WHILE:
        return ast_new_while(s->line,
                             clone_cond_for_object(ct, obj_name, s->as.while_.cond),
                             clone_stmt_for_object(ct, obj_name, s->as.while_.body));
    default:
        fprintf(stderr, "Error (line %d): class routine body contains unsupported statement kind for object rewrite.\n", s->line);
        return ast_new_block(s->line);
    }
}

static int class_add_routine(char *name, Param *params, size_t param_count, TypeKind return_type, Stmt *body) {
    if (!g_current_class) {
        free(name);
        for (size_t i = 0; i < param_count; i++) free(params[i].name);
        free(params);
        ast_free_stmt(body);
        return 0;
    }
    if (find_class_routine(g_current_class, name)) {
        fprintf(stderr, "Error (line %d): routine '%s' is already defined in class '%s'.\n",
                yylineno, name, g_current_class->name);
        free(name);
        for (size_t i = 0; i < param_count; i++) free(params[i].name);
        free(params);
        ast_free_stmt(body);
        return 0;
    }
    if (g_current_class->routine_count == g_current_class->routine_cap) {
        size_t next = g_current_class->routine_cap ? g_current_class->routine_cap * 2 : 4;
        ClassRoutine *nr = static_cast<ClassRoutine *>(realloc(g_current_class->routines, next * sizeof(ClassRoutine)));
        if (!nr) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        g_current_class->routines = nr;
        g_current_class->routine_cap = next;
    }
    ClassRoutine *r = &g_current_class->routines[g_current_class->routine_count++];
    r->name = name;
    r->params = params;
    r->param_count = param_count;
    r->return_type = return_type;
    r->body = body;
    return 1;
}

static char *make_prefixed_name(const char *obj_name, const char *field_name) {
    size_t on = strlen(obj_name);
    size_t fn = strlen(field_name);
    char *out = static_cast<char *>(malloc(on + 1 + fn + 1));
    if (!out) {
        fprintf(stderr, "Fatal: out of memory.\n");
        exit(EXIT_FAILURE);
    }
    memcpy(out, obj_name, on);
    out[on] = '_';
    memcpy(out + on + 1, field_name, fn + 1);
    return out;
}

static Stmt *class_instantiate(int line, char *class_name, char *obj_name, char *profile_name) {
    ClassTemplate *ct = find_class_template(class_name);
    if (!ct) {
        fprintf(stderr, "Error (line %d): class '%s' is not defined.\n", line, class_name);
        free(class_name);
        free(obj_name);
        free(profile_name);
        return NULL;
    }

    ClassProfile *cp = find_class_profile(ct, profile_name);
    if (!cp) {
        fprintf(stderr, "Error (line %d): profile '%s' is not defined in class '%s'.\n",
                line, profile_name, class_name);
        free(class_name);
        free(obj_name);
        free(profile_name);
        return NULL;
    }

    Stmt *blk = ast_new_block(line);

    for (size_t i = 0; i < ct->field_count; i++) {
        char **names = static_cast<char **>(malloc(sizeof(char *)));
        Expr **vals = static_cast<Expr **>(malloc(sizeof(Expr *)));
        if (!names || !vals) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        names[0] = make_prefixed_name(obj_name, ct->fields[i].name);
        vals[0] = default_init_expr(ct->fields[i].type);
        ast_block_add(blk, ast_new_decl(line, ct->fields[i].type, names, vals, 1));
    }

    for (size_t i = 0; i < cp->action_count; i++) {
        ProfileAction *a = &cp->actions[i];
        char *target = make_prefixed_name(obj_name, a->field_name);
        if (a->kind == PROFILE_SET_VALUE) {
            ast_block_add(blk, ast_new_set(line, target, clone_expr_for_object(ct, obj_name, a->value)));
        } else {
            Expr **msgs = clone_expr_list_for_object(ct, obj_name, a->message_parts, a->message_count);
            ast_block_add(blk, ast_new_input(line, target, msgs, a->message_count));
        }
    }

    for (size_t i = 0; i < ct->routine_count; i++) {
        ClassRoutine *r = &ct->routines[i];
        char *name = make_prefixed_name(obj_name, r->name);
        Param *params = clone_params(r->params, r->param_count);
        Stmt *body = clone_stmt_for_object(ct, obj_name, r->body);
        ast_block_add(blk, ast_new_funcdef(line, name, r->return_type, params, r->param_count, body));
    }

    free(class_name);
    free(obj_name);
    free(profile_name);
    return blk;
}

#line 993 "build\\lexico.tab.cpp"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "lexico.tab.hpp"

/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_CREATE = 3,                     /* CREATE  */
  YYSYMBOL_DEFINE = 4,                     /* DEFINE  */
  YYSYMBOL_CLASS = 5,                      /* CLASS  */
  YYSYMBOL_INCLUDE = 6,                    /* INCLUDE  */
  YYSYMBOL_INHERITS = 7,                   /* INHERITS  */
  YYSYMBOL_PROFILE = 8,                    /* PROFILE  */
  YYSYMBOL_USING = 9,                      /* USING  */
  YYSYMBOL_ROUTINE = 10,                   /* ROUTINE  */
  YYSYMBOL_OVERRIDE = 11,                  /* OVERRIDE  */
  YYSYMBOL_RETURNS = 12,                   /* RETURNS  */
  YYSYMBOL_SET = 13,                       /* SET  */
  YYSYMBOL_VALUE = 14,                     /* VALUE  */
  YYSYMBOL_VALUES = 15,                    /* VALUES  */
  YYSYMBOL_CONVERT = 16,                   /* CONVERT  */
  YYSYMBOL_PRINT = 17,                     /* PRINT  */
  YYSYMBOL_APPEND = 18,                    /* APPEND  */
  YYSYMBOL_SORT = 19,                      /* SORT  */
  YYSYMBOL_ASCENDANTLY = 20,               /* ASCENDANTLY  */
  YYSYMBOL_DESCENDANTLY = 21,              /* DESCENDANTLY  */
  YYSYMBOL_COUNT = 22,                     /* COUNT  */
  YYSYMBOL_CHECK = 23,                     /* CHECK  */
  YYSYMBOL_POSITION = 24,                  /* POSITION  */
  YYSYMBOL_EXTRACT = 25,                   /* EXTRACT  */
  YYSYMBOL_TO = 26,                        /* TO  */
  YYSYMBOL_ASK = 27,                       /* ASK  */
  YYSYMBOL_TAKE = 28,                      /* TAKE  */
  YYSYMBOL_USER = 29,                      /* USER  */
  YYSYMBOL_INPUT = 30,                     /* INPUT  */
  YYSYMBOL_WITH = 31,                      /* WITH  */
  YYSYMBOL_MESSAGE = 32,                   /* MESSAGE  */
  YYSYMBOL_SET_VALUE_TAKE_USER_INPUT = 33, /* SET_VALUE_TAKE_USER_INPUT  */
  YYSYMBOL_OPEN = 34,                      /* OPEN  */
  YYSYMBOL_MAKE = 35,                      /* MAKE  */
  YYSYMBOL_FILE_T = 36,                    /* FILE_T  */
  YYSYMBOL_WRITE = 37,                     /* WRITE  */
  YYSYMBOL_READ = 38,                      /* READ  */
  YYSYMBOL_LINE = 39,                      /* LINE  */
  YYSYMBOL_LINES = 40,                     /* LINES  */
  YYSYMBOL_CLEAR = 41,                     /* CLEAR  */
  YYSYMBOL_BEGIN_T = 42,                   /* BEGIN_T  */
  YYSYMBOL_CLOSE = 43,                     /* CLOSE  */
  YYSYMBOL_TITLE = 44,                     /* TITLE  */
  YYSYMBOL_INTO = 45,                      /* INTO  */
  YYSYMBOL_ARRAY = 46,                     /* ARRAY  */
  YYSYMBOL_TABLE = 47,                     /* TABLE  */
  YYSYMBOL_MESH = 48,                      /* MESH  */
  YYSYMBOL_MATRIX = 49,                    /* MATRIX  */
  YYSYMBOL_ROWS = 50,                      /* ROWS  */
  YYSYMBOL_COLS = 51,                      /* COLS  */
  YYSYMBOL_SIZE = 52,                      /* SIZE  */
  YYSYMBOL_LENGTH = 53,                    /* LENGTH  */
  YYSYMBOL_SIZED = 54,                     /* SIZED  */
  YYSYMBOL_AT = 55,                        /* AT  */
  YYSYMBOL_EACH = 56,                      /* EACH  */
  YYSYMBOL_INDEX = 57,                     /* INDEX  */
  YYSYMBOL_ROW = 58,                       /* ROW  */
  YYSYMBOL_COLUMN = 59,                    /* COLUMN  */
  YYSYMBOL_EMIT = 60,                      /* EMIT  */
  YYSYMBOL_GIVEBACK = 61,                  /* GIVEBACK  */
  YYSYMBOL_OUTCOME = 62,                   /* OUTCOME  */
  YYSYMBOL_OF = 63,                        /* OF  */
  YYSYMBOL_ARGS = 64,                      /* ARGS  */
  YYSYMBOL_IN = 65,                        /* IN  */
  YYSYMBOL_PLUS = 66,                      /* PLUS  */
  YYSYMBOL_MINUS = 67,                     /* MINUS  */
  YYSYMBOL_TIMES = 68,                     /* TIMES  */
  YYSYMBOL_DIV = 69,                       /* DIV  */
  YYSYMBOL_MOD = 70,                       /* MOD  */
  YYSYMBOL_IF = 71,                        /* IF  */
  YYSYMBOL_THEN = 72,                      /* THEN  */
  YYSYMBOL_OTHERWISE = 73,                 /* OTHERWISE  */
  YYSYMBOL_WHEN = 74,                      /* WHEN  */
  YYSYMBOL_AND = 75,                       /* AND  */
  YYSYMBOL_OR = 76,                        /* OR  */
  YYSYMBOL_NOT = 77,                       /* NOT  */
  YYSYMBOL_CMP_EQ = 78,                    /* CMP_EQ  */
  YYSYMBOL_CMP_NEQ = 79,                   /* CMP_NEQ  */
  YYSYMBOL_CMP_GT = 80,                    /* CMP_GT  */
  YYSYMBOL_CMP_LT = 81,                    /* CMP_LT  */
  YYSYMBOL_CMP_GTE = 82,                   /* CMP_GTE  */
  YYSYMBOL_CMP_LTE = 83,                   /* CMP_LTE  */
  YYSYMBOL_INDENT = 84,                    /* INDENT  */
  YYSYMBOL_DEDENT = 85,                    /* DEDENT  */
  YYSYMBOL_T_INT = 86,                     /* T_INT  */
  YYSYMBOL_T_FLOAT = 87,                   /* T_FLOAT  */
  YYSYMBOL_T_CHAR = 88,                    /* T_CHAR  */
  YYSYMBOL_T_STRING = 89,                  /* T_STRING  */
  YYSYMBOL_T_BOOL = 90,                    /* T_BOOL  */
  YYSYMBOL_TRUE = 91,                      /* TRUE  */
  YYSYMBOL_FALSE = 92,                     /* FALSE  */
  YYSYMBOL_WHILE = 93,                     /* WHILE  */
  YYSYMBOL_DO = 94,                        /* DO  */
  YYSYMBOL_FOR = 95,                       /* FOR  */
  YYSYMBOL_FROM = 96,                      /* FROM  */
  YYSYMBOL_STEP = 97,                      /* STEP  */
  YYSYMBOL_REPEAT = 98,                    /* REPEAT  */
  YYSYMBOL_UNTIL = 99,                     /* UNTIL  */
  YYSYMBOL_ATTEMPT = 100,                  /* ATTEMPT  */
  YYSYMBOL_UPTO = 101,                     /* UPTO  */
  YYSYMBOL_ONFAILURE = 102,                /* ONFAILURE  */
  YYSYMBOL_TIMES_WHILE = 103,              /* TIMES_WHILE  */
  YYSYMBOL_INT_LIT = 104,                  /* INT_LIT  */
  YYSYMBOL_FLOAT_LIT = 105,                /* FLOAT_LIT  */
  YYSYMBOL_CHAR_LIT = 106,                 /* CHAR_LIT  */
  YYSYMBOL_STRING_LIT = 107,               /* STRING_LIT  */
  YYSYMBOL_IDENT = 108,                    /* IDENT  */
  YYSYMBOL_PREC_LOWEST = 109,              /* PREC_LOWEST  */
  YYSYMBOL_110_ = 110,                     /* ','  */
  YYSYMBOL_111_ = 111,                     /* ')'  */
  YYSYMBOL_112_ = 112,                     /* ';'  */
  YYSYMBOL_113_ = 113,                     /* '('  */
  YYSYMBOL_114_ = 114,                     /* '/'  */
  YYSYMBOL_YYACCEPT = 115,                 /* $accept  */
  YYSYMBOL_program = 116,                  /* program  */
  YYSYMBOL_stmt_list = 117,                /* stmt_list  */
  YYSYMBOL_stmt = 118,                     /* stmt  */
  YYSYMBOL_file_open_stmt = 119,           /* file_open_stmt  */
  YYSYMBOL_file_stmt = 120,                /* file_stmt  */
  YYSYMBOL_collection_decl_stmt = 121,     /* collection_decl_stmt  */
  YYSYMBOL_collection_set_stmt = 122,      /* collection_set_stmt  */
  YYSYMBOL_append_stmt = 123,              /* append_stmt  */
  YYSYMBOL_sort_stmt = 124,                /* sort_stmt  */
  YYSYMBOL_append_value = 125,             /* append_value  */
  YYSYMBOL_append_term = 126,              /* append_term  */
  YYSYMBOL_append_factor = 127,            /* append_factor  */
  YYSYMBOL_function_stmt = 128,            /* function_stmt  */
  YYSYMBOL_opt_param_clause = 129,         /* opt_param_clause  */
  YYSYMBOL_param_list = 130,               /* param_list  */
  YYSYMBOL_emit_stmt = 131,                /* emit_stmt  */
  YYSYMBOL_giveback_stmt = 132,            /* giveback_stmt  */
  YYSYMBOL_opt_call_args = 133,            /* opt_call_args  */
  YYSYMBOL_simple_call_args = 134,         /* simple_call_args  */
  YYSYMBOL_call_arg_simple = 135,          /* call_arg_simple  */
  YYSYMBOL_call_args = 136,                /* call_args  */
  YYSYMBOL_call_expr = 137,                /* call_expr  */
  YYSYMBOL_decl_stmt = 138,                /* decl_stmt  */
  YYSYMBOL_class_stmt = 139,               /* class_stmt  */
  YYSYMBOL_140_1 = 140,                    /* $@1  */
  YYSYMBOL_141_2 = 141,                    /* $@2  */
  YYSYMBOL_class_body = 142,               /* class_body  */
  YYSYMBOL_class_member = 143,             /* class_member  */
  YYSYMBOL_144_3 = 144,                    /* $@3  */
  YYSYMBOL_profile_body = 145,             /* profile_body  */
  YYSYMBOL_profile_line = 146,             /* profile_line  */
  YYSYMBOL_opt_message = 147,              /* opt_message  */
  YYSYMBOL_message_parts = 148,            /* message_parts  */
  YYSYMBOL_print_stmt = 149,               /* print_stmt  */
  YYSYMBOL_if_stmt = 150,                  /* if_stmt  */
  YYSYMBOL_else_part = 151,                /* else_part  */
  YYSYMBOL_block = 152,                    /* block  */
  YYSYMBOL_block_body = 153,               /* block_body  */
  YYSYMBOL_while_stmt = 154,               /* while_stmt  */
  YYSYMBOL_for_stmt = 155,                 /* for_stmt  */
  YYSYMBOL_opt_step = 156,                 /* opt_step  */
  YYSYMBOL_repeat_stmt = 157,              /* repeat_stmt  */
  YYSYMBOL_attempt_stmt = 158,             /* attempt_stmt  */
  YYSYMBOL_opt_onfailure = 159,            /* opt_onfailure  */
  YYSYMBOL_condition = 160,                /* condition  */
  YYSYMBOL_cond_or = 161,                  /* cond_or  */
  YYSYMBOL_cond_and = 162,                 /* cond_and  */
  YYSYMBOL_cond_not = 163,                 /* cond_not  */
  YYSYMBOL_cond_atom = 164,                /* cond_atom  */
  YYSYMBOL_type = 165,                     /* type  */
  YYSYMBOL_id_list_multi = 166,            /* id_list_multi  */
  YYSYMBOL_val_list_multi = 167,           /* val_list_multi  */
  YYSYMBOL_mesh_init_values = 168,         /* mesh_init_values  */
  YYSYMBOL_matrix_init_row = 169,          /* matrix_init_row  */
  YYSYMBOL_matrix_init_rows = 170,         /* matrix_init_rows  */
  YYSYMBOL_print_args = 171,               /* print_args  */
  YYSYMBOL_compound_stmt = 172,            /* compound_stmt  */
  YYSYMBOL_set_stmt = 173,                 /* set_stmt  */
  YYSYMBOL_convert_stmt = 174,             /* convert_stmt  */
  YYSYMBOL_expr = 175,                     /* expr  */
  YYSYMBOL_term = 176,                     /* term  */
  YYSYMBOL_factor = 177,                   /* factor  */
  YYSYMBOL_literal = 178                   /* literal  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;


/* Default (constant) value used for initialization for null
   right-hand sides.  Unlike the standard yacc.c template, here we set
   the default value of $$ to a zeroed-out value.  Since the default
   value is undefined, this behavior is technically correct.  */
static YYSTYPE yyval_default;



#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif
#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YYFREE
# define YYFREE free
#endif
#ifndef YYMALLOC
# define YYMALLOC malloc
#endif
#ifndef YYREALLOC
# define YYREALLOC realloc
#endif

#ifdef __cplusplus
  typedef bool yybool;
# define yytrue true
# define yyfalse false
#else
  /* When we move to stdbool, get rid of the various casts to yybool.  */
  typedef signed char yybool;
# define yytrue 1
# define yyfalse 0
#endif

#ifndef YYSETJMP
# include <setjmp.h>
# define YYJMP_BUF jmp_buf
# define YYSETJMP(Env) setjmp (Env)
/* Pacify Clang and ICC.  */
# define YYLONGJMP(Env, Val)                    \
 do {                                           \
   longjmp (Env, Val);                          \
   YY_ASSERT (0);                               \
 } while (yyfalse)
#endif

#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* The _Noreturn keyword of C11.  */
#ifndef _Noreturn
# if (defined __cplusplus \
      && ((201103 <= __cplusplus && !(__GNUC__ == 4 && __GNUC_MINOR__ == 7)) \
          || (defined _MSC_VER && 1900 <= _MSC_VER)))
#  define _Noreturn [[noreturn]]
# elif ((!defined __cplusplus || defined __clang__) \
        && (201112 <= (defined __STDC_VERSION__ ? __STDC_VERSION__ : 0) \
            || (!defined __STRICT_ANSI__ \
                && (4 < __GNUC__ + (7 <= __GNUC_MINOR__) \
                    || (defined __apple_build_version__ \
                        ? 6000000 <= __apple_build_version__ \
                        : 3 < __clang_major__ + (5 <= __clang_minor__))))))
   /* _Noreturn works as-is.  */
# elif (2 < __GNUC__ + (8 <= __GNUC_MINOR__) || defined __clang__ \
        || 0x5110 <= __SUNPRO_C)
#  define _Noreturn __attribute__ ((__noreturn__))
# elif 1200 <= (defined _MSC_VER ? _MSC_VER : 0)
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  135
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1371

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  115
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  64
/* YYNRULES -- Number of rules.  */
#define YYNRULES  231
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  638
/* YYMAXRHS -- Maximum number of symbols on right-hand side of rule.  */
#define YYMAXRHS 18
/* YYMAXLEFT -- Maximum number of symbols to the left of a handle
   accessed by $0, $-1, etc., in any rule.  */
#define YYMAXLEFT 0

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   364

/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     113,   111,     2,     2,   110,     2,     2,   114,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,   112,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109
};

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,  1048,  1048,  1049,  1053,  1057,  1066,  1067,  1068,  1069,
    1070,  1071,  1072,  1073,  1074,  1075,  1076,  1077,  1078,  1079,
    1080,  1081,  1082,  1083,  1084,  1085,  1089,  1092,  1098,  1101,
    1104,  1107,  1110,  1113,  1116,  1119,  1122,  1125,  1128,  1134,
    1137,  1140,  1143,  1168,  1171,  1195,  1198,  1248,  1298,  1301,
    1351,  1401,  1408,  1411,  1414,  1417,  1420,  1423,  1426,  1429,
    1432,  1435,  1438,  1441,  1447,  1450,  1453,  1456,  1462,  1465,
    1471,  1472,  1473,  1477,  1478,  1479,  1480,  1484,  1485,  1486,
    1487,  1488,  1489,  1490,  1491,  1492,  1493,  1494,  1495,  1496,
    1497,  1498,  1502,  1508,  1512,  1518,  1521,  1527,  1533,  1539,
    1543,  1546,  1552,  1555,  1561,  1562,  1563,  1564,  1565,  1566,
    1570,  1573,  1579,  1585,  1590,  1601,  1617,  1628,  1632,  1638,
    1638,  1645,  1645,  1655,  1656,  1660,  1663,  1666,  1666,  1671,
    1674,  1677,  1680,  1686,  1687,  1691,  1694,  1700,  1701,  1705,
    1706,  1710,  1718,  1721,  1729,  1733,  1737,  1746,  1759,  1763,
    1772,  1780,  1783,  1792,  1808,  1809,  1815,  1823,  1826,  1832,
    1833,  1839,  1843,  1844,  1848,  1849,  1853,  1854,  1858,  1859,
    1860,  1861,  1862,  1863,  1864,  1870,  1871,  1872,  1873,  1874,
    1878,  1882,  1888,  1892,  1898,  1901,  1907,  1913,  1921,  1924,
    1932,  1935,  1943,  1947,  1951,  1955,  1959,  1968,  1971,  1974,
    1980,  1988,  1989,  1990,  1994,  1995,  1996,  1997,  2001,  2002,
    2003,  2004,  2005,  2006,  2007,  2008,  2009,  2010,  2011,  2012,
    2013,  2014,  2015,  2016,  2017,  2018,  2024,  2025,  2026,  2027,
    2028,  2029
};
#endif

#define YYPACT_NINF (-501)
#define YYTABLE_NINF (-221)

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     620,   173,   239,   650,   -62,  1126,  1258,   -43,  1126,  1126,
     707,    44,    31,   -34,  1126,   593,   593,    -8,    20,    58,
     326,   121,   620,  -501,  -501,     2,    49,    71,   103,   125,
    -501,   165,   186,   197,  -501,   200,  -501,  -501,  -501,  -501,
    -501,   212,   215,   223,   178,   157,   172,   255,  -501,  -501,
    -501,  -501,  -501,   244,   -18,   249,   258,   153,  1126,  1126,
    1126,   275,   764,   337,   298,   307,   322,   333,   348,  1126,
    1126,  -501,  -501,  -501,  -501,  -501,  -501,   118,  1126,  -501,
     191,   320,  -501,  -501,   376,   354,   315,   129,  1258,  1258,
     364,  1201,   378,   380,   396,   408,  1258,  1258,  -501,  1258,
     240,   331,  -501,  -501,  -501,   394,   114,   339,   419,  1126,
     288,  -501,  1126,   420,  -501,   454,   129,  1183,   593,   431,
     410,   460,  -501,  -501,   357,   395,   -12,   620,   407,  1126,
    1126,  1126,  1126,  1126,  1126,  -501,  -501,  -501,  -501,  -501,
    -501,  -501,  -501,  -501,  -501,  -501,  -501,  -501,  -501,   384,
     480,    34,   434,   509,   438,   -27,    -5,     1,   411,   517,
     821,  1126,   110,   366,   414,  1126,  1126,   217,   531,   450,
    1126,   496,   497,   453,  -501,  -501,   507,   878,  1126,   -26,
     456,  1126,  1126,  1126,  1126,  1126,  1126,   384,  1126,  1126,
     417,   435,  1258,  1258,   225,   457,  1258,   500,   503,  -501,
    -501,    -4,   459,  1126,  1258,  1258,  1258,  1258,  1258,  -501,
    -501,    -1,   146,   462,   443,   144,   449,  1126,   505,  -501,
    -501,   463,    86,    20,   593,   593,  1126,  1126,  1126,  1126,
    1126,  1126,    20,   465,  1126,   513,  -501,   534,   593,   233,
     129,   129,   129,   129,   129,   471,   528,   566,  1126,    32,
     476,   133,   477,   532,   572,   556,   481,   574,   482,  -501,
     483,   529,   580,   567,   129,   204,   384,   490,   491,   455,
     230,   492,  1126,  -501,  -501,   493,   494,   454,  1126,   575,
     129,    10,     3,  -501,   548,   126,   320,   320,  -501,  -501,
    -501,  -501,   498,   129,   499,   502,   458,   238,   504,  -501,
    -501,   506,   514,  -501,   551,   129,   331,   331,  -501,  -501,
    -501,   519,    20,  -501,  1126,  -501,   589,   592,   591,   595,
     248,   546,  -501,   562,   460,  -501,   129,   129,   129,   129,
     129,   129,  -501,   544,   250,   533,  -501,  -501,   571,   593,
     593,   613,  1126,  1126,   129,   406,  1126,  -501,   632,  1126,
      53,  1126,  1126,   616,  -501,  -501,  1126,  -501,   565,   644,
     588,   384,   626,   935,  -501,  -501,  -501,   554,  1126,  -501,
     129,  -501,  -501,  -501,    12,   635,   992,  1126,  1126,  1126,
    1126,  -501,  -501,   559,  1258,  -501,  -501,  -501,  1126,    20,
    -501,   461,   623,   624,  1126,  -501,   637,   643,   -20,  -501,
    1126,  1126,   569,  -501,   584,   585,   630,   129,   573,   129,
    1126,  1126,   296,  1126,   129,   436,  1126,   305,   129,  1126,
     577,   145,   334,  -501,   660,   596,   556,   663,   129,  -501,
     464,  1049,  1126,   556,   664,   129,   253,  -501,   168,   129,
    -501,   467,   129,  -501,   653,   586,   587,   474,  1126,   181,
     593,  -501,   256,   132,   600,    20,    20,  1126,  1126,   594,
     597,   129,   597,  1126,   573,  1126,  1126,   317,  1126,   604,
     129,  1126,  1126,   384,   599,   602,    39,    14,  -501,   621,
     384,    20,  -501,   689,   614,   692,   129,   264,  -501,   694,
    1126,  1126,   617,  -501,  -501,  -501,   690,   489,   386,   386,
    -501,  1126,   625,  -501,  -501,  -501,   655,  1126,  1126,   639,
    1126,   634,   634,   129,   129,  1126,  1126,   129,   597,   597,
    1126,   129,  1126,   129,   129,   629,   640,   517,   384,   631,
    -501,  -501,   334,   628,   636,  -501,   556,  -501,   710,  1126,
     556,   129,   129,  -501,   700,   711,  1126,  -501,  -501,   346,
     122,   386,    20,   132,   129,    20,   284,    20,  -501,  -501,
     129,   594,   129,   129,   638,  -501,     7,   641,   517,    40,
     384,  -501,  -501,   556,   129,  -501,   645,  -501,   122,  1126,
    -501,  -501,  -501,   562,   654,  -501,  1126,  -501,  -501,   667,
     384,    20,   649,     8,  -501,   656,  -501,  -501,   129,  -501,
      20,   312,   739,   668,  -501,  -501,   384,    20,  -501,  -501,
     659,   662,    13,  -501,    20,   674,  -501,   675,   746,  -501,
    -501,  -501,    20,  1126,  1106,  -501,   292,   747,    72,  1126,
     745,  -501,   235,   556,    20,   665,  -501,  -501
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       2,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     3,     4,    19,     0,     0,     0,     0,     0,
      17,     0,     0,     0,    18,     0,    21,    22,    23,    24,
      25,     0,     0,     0,     0,     0,     0,     0,   175,   176,
     177,   178,   179,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   230,   231,   226,   227,   228,   229,   221,     0,   208,
       0,   203,   207,   222,     0,   221,   141,   190,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    87,     0,
      66,    72,    76,    77,    88,     0,     0,     0,     0,     0,
       0,    34,     0,     0,    38,    99,    98,     0,     0,     0,
     161,   163,   165,   167,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     1,     5,    20,     7,     8,
       9,    10,    14,    15,     6,    11,    16,    12,    13,     0,
       0,     0,     0,     0,     0,     0,   116,   117,     0,    93,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   225,   224,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    91,
      90,     0,     0,     0,     0,     0,     0,     0,     0,    68,
      69,     0,     0,     0,     0,     0,     0,     0,     0,    97,
     166,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   148,     0,     0,     0,
     192,   193,   194,   195,   196,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   137,     0,     0,     0,   119,
       0,     0,     0,     0,    61,     0,     0,     0,     0,     0,
       0,     0,     0,   211,   214,     0,     0,    99,     0,     0,
     197,     0,   207,   223,     0,     0,   201,   202,   204,   205,
     206,   200,   220,   191,     0,     0,     0,     0,     0,    78,
      81,     0,     0,    89,    64,    67,    70,    71,    73,    74,
      75,     0,     0,    28,     0,    30,     0,     0,     0,     0,
       0,     0,   174,   142,   162,   164,   168,   169,   170,   171,
     172,   173,   150,     0,     0,     0,   147,   149,     0,     0,
       0,     0,     0,     0,    43,     0,     0,   113,     0,     0,
       0,     0,     0,     0,   118,   180,     0,   181,     0,     0,
       0,     0,     0,     0,   209,   217,   218,     0,     0,   215,
      37,   212,   213,   112,     0,     0,     0,     0,     0,     0,
       0,    84,    85,     0,     0,    82,    79,    80,     0,     0,
      26,     0,     0,     0,     0,    35,     0,     0,     0,   143,
       0,     0,     0,   156,     0,     0,     0,    40,    44,   184,
       0,     0,     0,     0,    41,     0,     0,     0,   114,     0,
     115,     0,     0,   121,     0,     0,   137,     0,    60,   219,
       0,     0,     0,   137,     0,    52,   198,   210,     0,    57,
      86,     0,    65,    27,     0,     0,     0,     0,     0,     0,
       0,   144,     0,   154,     0,     0,     0,     0,     0,   188,
      50,   186,    49,     0,    42,     0,     0,     0,     0,   138,
     139,     0,     0,     0,     0,     0,     0,     0,   123,     0,
       0,     0,    63,     0,     0,     0,    53,     0,   199,     0,
       0,     0,     0,    29,    31,    32,     0,     0,     0,     0,
     104,     0,   100,   102,   106,   105,     0,     0,     0,     0,
       0,   159,   159,    39,   185,     0,     0,    48,    47,    46,
       0,    51,     0,   183,   182,     0,     0,    93,     0,     0,
     120,   124,     0,    94,     0,    92,   137,   216,     0,     0,
     137,    54,    56,    83,     0,     0,     0,   109,   108,     0,
     110,     0,     0,   154,   155,     0,     0,     0,   157,   158,
     187,   189,    45,   140,     0,   127,     0,     0,    93,     0,
       0,    95,    62,   137,    55,    58,     0,    36,     0,     0,
     101,   107,   103,   145,     0,   151,     0,   160,   125,     0,
       0,     0,     0,     0,   122,     0,    59,    33,   111,   146,
       0,     0,     0,     0,   130,   126,     0,     0,    96,   152,
       0,     0,     0,   133,     0,     0,   132,     0,     0,   128,
     134,   129,     0,     0,     0,   131,     0,     0,     0,     0,
       0,   135,     0,   137,     0,     0,   153,   136
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -501,  -501,  -501,   -19,  -501,  -501,  -501,  -501,  -501,  -501,
     -76,   262,   -36,  -501,  -500,  -501,  -501,  -501,   501,  -501,
    -344,  -501,    -2,  -501,  -501,  -501,  -501,   247,  -459,  -501,
    -501,   169,  -417,  -501,  -501,  -501,   199,  -188,  -501,  -501,
    -501,   231,  -501,  -501,   271,    -6,  -501,   561,   568,   673,
    -148,  -501,  -501,   379,   278,  -374,  -501,  -501,  -501,  -501,
      -3,   293,   -37,    46
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,    21,    22,    23,    24,    25,    26,    27,    28,    29,
     100,   101,   102,    30,   262,   533,    31,    32,   219,   502,
     503,   549,    79,    33,    34,   358,   479,   477,   478,   589,
     612,   613,   354,   469,    35,    36,   399,   128,   237,    37,
      38,   509,    39,    40,   558,   119,   120,   121,   122,   123,
      54,   157,   420,   408,   459,   460,    86,    41,    42,    43,
     124,    81,    82,    83
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      80,   245,    87,   136,   103,   106,   107,   110,   254,   482,
     125,   116,   190,   191,   257,   194,   488,   473,   531,   590,
     606,   252,   474,   201,   475,   476,   611,   566,   255,  -207,
     154,   155,   174,   175,   311,   323,   376,   462,   431,   291,
     182,   183,   528,   473,   332,   345,    84,   247,   474,   529,
     475,   476,   104,   233,   450,   162,   163,   164,  -220,   167,
     199,   200,   204,   205,   127,   105,   415,   114,   593,  -207,
    -207,  -207,  -207,  -207,   115,   179,   182,   183,   182,   183,
     111,   253,   346,   112,   234,   283,   103,   103,   248,   103,
     156,   518,   519,   312,   103,   103,   235,   103,   619,   530,
     126,   591,   607,   416,   127,   256,   214,   303,   236,   216,
     531,   258,   221,   378,   137,   222,   296,   297,   364,   572,
     377,   135,   432,   575,   390,   594,   239,   240,   241,   242,
     243,   244,   176,   274,   104,   104,   266,   104,   182,   183,
     113,   282,   104,   104,   177,   104,   348,   288,   289,   290,
     211,   292,   182,   183,   547,   548,   596,   264,   265,   129,
     300,   138,   269,   270,   226,   227,   228,   229,   230,   231,
     308,   309,   310,   178,   280,   281,   182,   183,   285,   160,
     182,   183,   313,   139,   631,   314,   293,   349,   182,   183,
     103,   103,   182,   183,   103,   182,   183,   283,   182,   183,
     305,   443,   103,   103,   103,   103,   103,   582,   161,   317,
     451,   182,   183,   425,   320,   140,   635,   180,   337,    44,
      45,    46,    47,   326,   327,   328,   329,   330,   331,   508,
     363,   334,   338,   581,   182,   183,   380,   141,   104,   104,
     318,   149,   104,    68,    55,   344,   181,   498,   499,    56,
     104,   104,   104,   104,   104,   472,   368,   182,   183,    48,
      49,    50,    51,    52,   384,   150,   202,   511,   512,   370,
     182,   183,    71,    72,   396,   374,   401,   142,   491,   490,
     151,    53,   507,   182,   183,    73,    74,    75,    76,   500,
     539,   204,   205,   535,   501,   203,   182,   183,   143,   182,
     183,   182,   183,   152,   204,   205,   204,   205,   441,   144,
     586,   391,   145,   271,   182,   183,   182,   183,   629,   182,
     183,   298,   182,   183,   146,   525,   339,   147,   215,   634,
     182,   183,   534,   404,   405,   148,   340,   473,   165,   407,
     409,   437,   474,   412,   475,   476,   414,   463,   417,   418,
     182,   183,   153,   421,   182,   183,   468,   158,   182,   183,
     428,   169,   182,   183,   583,   430,   159,   585,   520,   587,
     170,   182,   183,   435,   436,   171,   438,   439,   182,   183,
     567,   168,   103,   182,   183,   442,   172,   610,   184,   185,
     186,   447,   130,   131,   132,   133,   134,   452,   453,   206,
     207,   208,   187,   604,   212,   182,   183,   461,   461,   188,
     409,   173,   609,   467,   209,   210,   470,   259,   260,   616,
     410,   411,   595,   182,   183,   189,   621,   192,   486,   487,
     104,   267,   182,   183,   625,   226,   227,   228,   229,   230,
     231,   195,   603,   196,   506,   497,   636,   504,    68,   197,
     465,   466,   498,   499,   513,   514,   579,   580,   615,   217,
     517,   198,   461,   461,   213,   521,   306,   307,   523,   524,
      48,    49,    50,    51,    52,   286,   287,    71,    72,   268,
     182,   183,   294,   204,   205,   218,   224,   541,   542,   232,
      73,    74,    75,    76,   500,   505,   504,   504,   550,   546,
     295,   204,   205,   223,   553,   554,   238,   556,   316,   182,
     183,   246,   560,   461,   319,   182,   183,   562,   250,   563,
     367,   182,   183,   383,   204,   205,   444,   182,   183,   484,
     182,   183,   492,   204,   205,   225,   574,     1,     2,   496,
     182,   183,   249,   578,   505,   505,   251,     3,   261,   504,
       4,     5,     6,     7,   545,   182,   183,   272,   273,   275,
     276,   277,   278,   301,   284,   299,   302,   304,     8,   321,
     315,     9,    10,   333,   322,    11,   598,    12,   335,   341,
     342,   343,   351,   601,   347,   350,   352,   353,   356,   355,
     357,   359,   361,   360,    13,    14,   362,   505,   365,   366,
     369,   371,   372,   379,   375,    15,   388,   381,   378,    58,
     382,   397,   385,   389,   386,    59,    60,    61,    62,   336,
     626,   628,   387,     1,     2,   392,   632,    16,   393,    17,
     394,   395,    18,     3,    19,   398,     4,     5,     6,     7,
     400,   402,    20,   403,   406,    64,    65,   413,   419,   422,
     423,    66,    67,   424,     8,    68,   426,     9,    10,    69,
      70,    11,   429,    12,    57,   433,    58,   440,   445,   446,
     117,   449,    59,    60,    61,    62,   448,   454,   455,   456,
      13,    14,   457,   458,    71,    72,    63,   471,   480,   493,
     481,    15,   483,   489,   494,   495,   510,    73,    74,    75,
      76,    85,    64,    65,   515,   532,   118,   526,    66,    67,
     527,   516,    68,    16,   522,    17,    69,    70,    18,   536,
      19,   538,   537,    58,   540,   543,   544,   552,    20,    59,
      60,    61,    62,   555,   565,   551,   557,   564,   570,   568,
     573,    71,    72,   108,   571,   576,   109,   577,   600,   592,
     588,   602,   611,   597,    73,    74,    75,    76,    77,    64,
      65,   605,   614,    78,   608,    66,    67,   617,   622,    68,
     618,   623,   624,    69,    70,   633,   630,   637,   373,   569,
      58,   620,   599,   559,   584,   324,    59,    60,    61,    62,
     220,     0,   464,   325,   561,     0,     0,     0,    71,    72,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    73,    74,    75,    76,    85,    64,    65,     0,     0,
      78,     0,    66,    67,     0,     0,    68,     0,     0,     0,
      69,    70,     0,     0,     0,     0,     0,    58,     0,     0,
       0,     0,     0,    59,    60,    61,    62,     0,     0,   263,
       0,     0,     0,     0,     0,    71,    72,     0,     0,     0,
     166,     0,     0,     0,     0,     0,     0,     0,    73,    74,
      75,    76,    85,    64,    65,     0,     0,    78,     0,    66,
      67,     0,     0,    68,     0,     0,     0,    69,    70,     0,
       0,     0,     0,     0,    58,     0,     0,     0,     0,     0,
      59,    60,    61,    62,     0,     0,   279,     0,     0,     0,
       0,     0,    71,    72,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    73,    74,    75,    76,    85,
      64,    65,     0,     0,    78,     0,    66,    67,     0,     0,
      68,     0,     0,     0,    69,    70,     0,     0,     0,     0,
       0,    58,     0,     0,     0,     0,     0,    59,    60,    61,
      62,     0,     0,   427,     0,     0,     0,     0,     0,    71,
      72,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    73,    74,    75,    76,    85,    64,    65,     0,
       0,    78,     0,    66,    67,     0,     0,    68,     0,     0,
       0,    69,    70,     0,     0,     0,     0,     0,    58,     0,
       0,     0,     0,     0,    59,    60,    61,    62,     0,     0,
     434,     0,     0,     0,     0,     0,    71,    72,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    73,
      74,    75,    76,    85,    64,    65,     0,     0,    78,     0,
      66,    67,     0,     0,    68,     0,     0,     0,    69,    70,
       0,     0,     0,     0,     0,    58,     0,     0,     0,     0,
       0,    59,    60,    61,    62,     0,     0,   485,     0,     0,
       0,     0,     0,    71,    72,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    73,    74,    75,    76,
      85,    64,    65,     0,     0,    78,     0,    66,    67,     0,
       0,    68,     0,     0,     0,    69,    70,     0,     0,     0,
       0,     0,    58,     0,     0,     0,     0,     0,    59,    60,
      61,    62,     0,     0,   627,     0,     0,     0,     0,     0,
      71,    72,    58,     0,     0,     0,     0,     0,    59,    60,
      61,    62,     0,    73,    74,    75,    76,    85,    64,    65,
       0,     0,    78,     0,    66,    67,     0,     0,    68,     0,
       0,     0,    69,    70,     0,     0,     0,     0,    64,    65,
       0,     0,     0,     0,    66,    67,     0,     0,    68,     0,
       0,     0,    69,    70,     0,     0,     0,    71,    72,    58,
       0,     0,     0,     0,     0,    59,    60,    61,    62,     0,
      73,    74,    75,    76,    85,     0,     0,    71,    72,    78,
       0,     0,     0,    88,    89,    90,    91,     0,     0,     0,
      73,    74,    75,    76,    85,    64,    65,     0,     0,    78,
       0,    66,    67,     0,     0,    68,     0,     0,     0,    69,
      70,     0,     0,    92,    93,     0,     0,     0,     0,    94,
      95,     0,     0,    68,     0,     0,     0,    96,    97,     0,
       0,     0,     0,     0,    71,    72,     0,     0,     0,     0,
      88,    89,    90,    91,     0,     0,     0,    73,    74,    75,
      76,    85,    71,    72,     0,     0,   118,   193,     0,     0,
       0,     0,     0,     0,     0,    73,    74,    75,    76,    98,
      92,    93,     0,     0,    99,     0,    94,    95,     0,     0,
      68,     0,     0,     0,    96,    97,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    71,
      72,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    73,    74,    75,    76,    98,     0,     0,     0,
       0,    99
};

static const yytype_int16 yycheck[] =
{
       3,   149,     5,    22,     6,     8,     9,    10,    13,   426,
      16,    14,    88,    89,    13,    91,   433,     3,   477,    12,
      12,    48,     8,    99,    10,    11,    13,   527,    33,    26,
      48,    49,    69,    70,    35,   223,    26,   411,    26,   187,
      66,    67,     3,     3,   232,    13,   108,    13,     8,    10,
      10,    11,     6,    65,    74,    58,    59,    60,    55,    62,
      96,    97,    66,    67,    84,   108,    13,    36,   568,    66,
      67,    68,    69,    70,   108,    78,    66,    67,    66,    67,
      36,   108,    50,    39,    96,   111,    88,    89,    54,    91,
     108,   465,   466,    94,    96,    97,   108,    99,    85,    85,
     108,    94,    94,    50,    84,   110,   109,   111,   127,   112,
     569,   110,   118,   110,   112,   118,   192,   193,   266,   536,
     110,     0,   110,   540,   312,    85,   129,   130,   131,   132,
     133,   134,    14,   170,    88,    89,    26,    91,    66,    67,
      96,   178,    96,    97,    26,    99,    13,   184,   185,   186,
      36,   188,    66,    67,   498,   499,   573,   160,   161,   101,
     196,   112,   165,   166,    78,    79,    80,    81,    82,    83,
     206,   207,   208,    55,   177,   178,    66,    67,   181,    26,
      66,    67,    36,   112,   112,    39,   189,    54,    66,    67,
     192,   193,    66,    67,   196,    66,    67,   111,    66,    67,
     203,   389,   204,   205,   206,   207,   208,   551,    55,    65,
     398,    66,    67,   361,   217,   112,   633,    26,   237,    46,
      47,    48,    49,   226,   227,   228,   229,   230,   231,    97,
      26,   234,   238,   111,    66,    67,   110,   112,   192,   193,
      96,    63,   196,    62,     5,   248,    55,    66,    67,    10,
     204,   205,   206,   207,   208,   110,    26,    66,    67,    86,
      87,    88,    89,    90,    26,   108,    26,   455,   456,   272,
      66,    67,    91,    92,    26,   278,    26,   112,   110,    26,
     108,   108,    26,    66,    67,   104,   105,   106,   107,   108,
      26,    66,    67,   481,   113,    55,    66,    67,   112,    66,
      67,    66,    67,    48,    66,    67,    66,    67,   384,   112,
      26,   314,   112,    96,    66,    67,    66,    67,    26,    66,
      67,    96,    66,    67,   112,   473,    93,   112,    40,    94,
      66,    67,   480,   339,   340,   112,   103,     3,    63,   342,
     343,   378,     8,   346,    10,    11,   349,    51,   351,   352,
      66,    67,   108,   356,    66,    67,    51,   108,    66,    67,
     363,    63,    66,    67,   552,   368,   108,   555,    51,   557,
      63,    66,    67,   376,   377,    53,   379,   380,    66,    67,
     528,    44,   384,    66,    67,   388,    53,    75,    68,    69,
      70,   394,    66,    67,    68,    69,    70,   400,   401,    68,
      69,    70,    26,   591,    65,    66,    67,   410,   411,    55,
     413,    63,   600,   416,    20,    21,   419,     6,     7,   607,
      14,    15,   570,    66,    67,   110,   614,    63,   431,   432,
     384,    65,    66,    67,   622,    78,    79,    80,    81,    82,
      83,    63,   590,    63,   450,   448,   634,   449,    62,    53,
      14,    15,    66,    67,   457,   458,   110,   111,   606,    39,
     463,    53,   465,   466,    45,   468,   204,   205,   471,   472,
      86,    87,    88,    89,    90,   182,   183,    91,    92,    65,
      66,    67,    65,    66,    67,    31,    76,   490,   491,    94,
     104,   105,   106,   107,   108,   449,   498,   499,   501,   113,
      65,    66,    67,    72,   507,   508,    99,   510,    65,    66,
      67,    31,   515,   516,    65,    66,    67,   520,     9,   522,
      65,    66,    67,    65,    66,    67,    65,    66,    67,    65,
      66,    67,    65,    66,    67,    75,   539,     3,     4,    65,
      66,    67,   108,   546,   498,   499,   108,    13,    31,   551,
      16,    17,    18,    19,    65,    66,    67,    26,   108,    63,
      63,   108,    55,    63,   108,   108,    63,   108,    34,    64,
     108,    37,    38,   108,   111,    41,   579,    43,    65,   108,
      52,    15,    50,   586,   108,   108,    14,    31,    14,   108,
     108,   108,    12,    64,    60,    61,    29,   551,   108,   108,
     108,   108,   108,    55,    29,    71,    55,   108,   110,    16,
     108,    65,   108,    94,   108,    22,    23,    24,    25,    85,
     623,   624,   108,     3,     4,    36,   629,    93,    36,    95,
      39,    36,    98,    13,   100,    73,    16,    17,    18,    19,
      96,   108,   108,    72,    31,    52,    53,    15,    32,    84,
       6,    58,    59,    65,    34,    62,    30,    37,    38,    66,
      67,    41,   108,    43,    14,    30,    16,   108,    45,    45,
      77,    28,    22,    23,    24,    25,    39,   108,    94,    94,
      60,    61,    52,   110,    91,    92,    36,   110,    28,    36,
      94,    71,    29,    29,   108,   108,    96,   104,   105,   106,
     107,   108,    52,    53,   110,    84,   113,   108,    58,    59,
     108,   114,    62,    93,   110,    95,    66,    67,    98,    30,
     100,    29,   108,    16,    30,   108,    36,    72,   108,    22,
      23,    24,    25,    94,    94,   110,   102,   108,   110,   108,
      30,    91,    92,    36,   108,    45,    39,    36,    94,   108,
     112,    84,    13,   108,   104,   105,   106,   107,   108,    52,
      53,   112,    94,   113,   108,    58,    59,   108,    94,    62,
     108,    96,    26,    66,    67,    30,    29,   112,   277,   532,
      16,   612,   583,   512,   553,   224,    22,    23,    24,    25,
     117,    -1,   413,   225,   516,    -1,    -1,    -1,    91,    92,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   104,   105,   106,   107,   108,    52,    53,    -1,    -1,
     113,    -1,    58,    59,    -1,    -1,    62,    -1,    -1,    -1,
      66,    67,    -1,    -1,    -1,    -1,    -1,    16,    -1,    -1,
      -1,    -1,    -1,    22,    23,    24,    25,    -1,    -1,    28,
      -1,    -1,    -1,    -1,    -1,    91,    92,    -1,    -1,    -1,
      96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   104,   105,
     106,   107,   108,    52,    53,    -1,    -1,   113,    -1,    58,
      59,    -1,    -1,    62,    -1,    -1,    -1,    66,    67,    -1,
      -1,    -1,    -1,    -1,    16,    -1,    -1,    -1,    -1,    -1,
      22,    23,    24,    25,    -1,    -1,    28,    -1,    -1,    -1,
      -1,    -1,    91,    92,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   104,   105,   106,   107,   108,
      52,    53,    -1,    -1,   113,    -1,    58,    59,    -1,    -1,
      62,    -1,    -1,    -1,    66,    67,    -1,    -1,    -1,    -1,
      -1,    16,    -1,    -1,    -1,    -1,    -1,    22,    23,    24,
      25,    -1,    -1,    28,    -1,    -1,    -1,    -1,    -1,    91,
      92,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   104,   105,   106,   107,   108,    52,    53,    -1,
      -1,   113,    -1,    58,    59,    -1,    -1,    62,    -1,    -1,
      -1,    66,    67,    -1,    -1,    -1,    -1,    -1,    16,    -1,
      -1,    -1,    -1,    -1,    22,    23,    24,    25,    -1,    -1,
      28,    -1,    -1,    -1,    -1,    -1,    91,    92,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   104,
     105,   106,   107,   108,    52,    53,    -1,    -1,   113,    -1,
      58,    59,    -1,    -1,    62,    -1,    -1,    -1,    66,    67,
      -1,    -1,    -1,    -1,    -1,    16,    -1,    -1,    -1,    -1,
      -1,    22,    23,    24,    25,    -1,    -1,    28,    -1,    -1,
      -1,    -1,    -1,    91,    92,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   104,   105,   106,   107,
     108,    52,    53,    -1,    -1,   113,    -1,    58,    59,    -1,
      -1,    62,    -1,    -1,    -1,    66,    67,    -1,    -1,    -1,
      -1,    -1,    16,    -1,    -1,    -1,    -1,    -1,    22,    23,
      24,    25,    -1,    -1,    28,    -1,    -1,    -1,    -1,    -1,
      91,    92,    16,    -1,    -1,    -1,    -1,    -1,    22,    23,
      24,    25,    -1,   104,   105,   106,   107,   108,    52,    53,
      -1,    -1,   113,    -1,    58,    59,    -1,    -1,    62,    -1,
      -1,    -1,    66,    67,    -1,    -1,    -1,    -1,    52,    53,
      -1,    -1,    -1,    -1,    58,    59,    -1,    -1,    62,    -1,
      -1,    -1,    66,    67,    -1,    -1,    -1,    91,    92,    16,
      -1,    -1,    -1,    -1,    -1,    22,    23,    24,    25,    -1,
     104,   105,   106,   107,   108,    -1,    -1,    91,    92,   113,
      -1,    -1,    -1,    22,    23,    24,    25,    -1,    -1,    -1,
     104,   105,   106,   107,   108,    52,    53,    -1,    -1,   113,
      -1,    58,    59,    -1,    -1,    62,    -1,    -1,    -1,    66,
      67,    -1,    -1,    52,    53,    -1,    -1,    -1,    -1,    58,
      59,    -1,    -1,    62,    -1,    -1,    -1,    66,    67,    -1,
      -1,    -1,    -1,    -1,    91,    92,    -1,    -1,    -1,    -1,
      22,    23,    24,    25,    -1,    -1,    -1,   104,   105,   106,
     107,   108,    91,    92,    -1,    -1,   113,    96,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   104,   105,   106,   107,   108,
      52,    53,    -1,    -1,   113,    -1,    58,    59,    -1,    -1,
      62,    -1,    -1,    -1,    66,    67,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,
      92,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   104,   105,   106,   107,   108,    -1,    -1,    -1,
      -1,   113
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     3,     4,    13,    16,    17,    18,    19,    34,    37,
      38,    41,    43,    60,    61,    71,    93,    95,    98,   100,
     108,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     128,   131,   132,   138,   139,   149,   150,   154,   155,   157,
     158,   172,   173,   174,    46,    47,    48,    49,    86,    87,
      88,    89,    90,   108,   165,     5,    10,    14,    16,    22,
      23,    24,    25,    36,    52,    53,    58,    59,    62,    66,
      67,    91,    92,   104,   105,   106,   107,   108,   113,   137,
     175,   176,   177,   178,   108,   108,   171,   175,    22,    23,
      24,    25,    52,    53,    58,    59,    66,    67,   108,   113,
     125,   126,   127,   137,   178,   108,   175,   175,    36,    39,
     175,    36,    39,    96,    36,   108,   175,    77,   113,   160,
     161,   162,   163,   164,   175,   160,   108,    84,   152,   101,
      66,    67,    68,    69,    70,     0,   118,   112,   112,   112,
     112,   112,   112,   112,   112,   112,   112,   112,   112,    63,
     108,   108,    48,   108,    48,    49,   108,   166,   108,   108,
      26,    55,   175,   175,   175,    63,    96,   175,    44,    63,
      63,    53,    53,    63,   177,   177,    14,    26,    55,   175,
      26,    55,    66,    67,    68,    69,    70,    26,    55,   110,
     125,   125,    63,    96,   125,    63,    63,    53,    53,   127,
     127,   125,    26,    55,    66,    67,    68,    69,    70,    20,
      21,    36,    65,    45,   175,    40,   175,    39,    31,   133,
     164,   160,   175,    72,    76,    75,    78,    79,    80,    81,
      82,    83,    94,    65,    96,   108,   118,   153,    99,   175,
     175,   175,   175,   175,   175,   165,    31,    13,    54,   108,
       9,   108,    48,   108,    13,    33,   110,    13,   110,     6,
       7,    31,   129,    28,   175,   175,    26,    65,    65,   175,
     175,    96,    26,   108,   177,    63,    63,   108,    55,    28,
     175,   175,   177,   111,   108,   175,   176,   176,   177,   177,
     177,   165,   177,   175,    65,    65,   125,   125,    96,   108,
     127,    63,    63,   111,   108,   175,   126,   126,   127,   127,
     127,    35,    94,    36,    39,   108,    65,    65,    96,    65,
     175,    64,   111,   152,   162,   163,   175,   175,   175,   175,
     175,   175,   152,   108,   175,    65,    85,   118,   160,    93,
     103,   108,    52,    15,   175,    13,    50,   108,    13,    54,
     108,    50,    14,    31,   147,   108,    14,   108,   140,   108,
      64,    12,    29,    26,   165,   108,   108,    65,    26,   108,
     175,   108,   108,   133,   175,    29,    26,   110,   110,    55,
     110,   108,   108,    65,    26,   108,   108,   108,    55,    94,
     152,   175,    36,    36,    39,    36,    26,    65,    73,   151,
      96,    26,   108,    72,   160,   160,    31,   175,   168,   175,
      14,    15,   175,    15,   175,    13,    50,   175,   175,    32,
     167,   175,    84,     6,    65,   165,    30,    28,   175,   108,
     175,    26,   110,    30,    28,   175,   175,   177,   175,   175,
     108,   125,   175,   152,    65,    45,    45,   175,    39,    28,
      74,   152,   175,   175,   108,    94,    94,    52,   110,   169,
     170,   175,   170,    51,   168,    14,    15,   175,    51,   148,
     175,   110,   110,     3,     8,    10,    11,   142,   143,   141,
      28,    94,   147,    29,    65,    28,   175,   175,   147,    29,
      26,   110,    65,    36,   108,   108,    65,   175,    66,    67,
     108,   113,   134,   135,   137,   178,   160,    26,    97,   156,
      96,   152,   152,   175,   175,   110,   114,   175,   170,   170,
      51,   175,   110,   175,   175,   165,   108,   108,     3,    10,
      85,   143,    84,   130,   165,   152,    30,   108,    29,    26,
      30,   175,   175,   108,    36,    65,   113,   135,   135,   136,
     175,   110,    72,   175,   175,    94,   175,   102,   159,   159,
     175,   169,   175,   175,   108,    94,   129,   165,   108,   142,
     110,   108,   147,    30,   175,   147,    45,    36,   175,   110,
     111,   111,   135,   152,   156,   152,    26,   152,   112,   144,
      12,    94,   108,   129,    85,   165,   147,   108,   175,   151,
      94,   175,    84,   165,   152,   112,    12,    94,   108,   152,
      75,    13,   145,   146,    94,   165,   152,   108,   108,    85,
     146,   152,    94,    96,    26,   152,   175,    28,   175,    26,
      29,   112,   175,    30,    94,   147,   152,   112
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_uint8 yyr1[] =
{
       0,   115,   116,   116,   117,   117,   118,   118,   118,   118,
     118,   118,   118,   118,   118,   118,   118,   118,   118,   118,
     118,   118,   118,   118,   118,   118,   119,   119,   120,   120,
     120,   120,   120,   120,   120,   120,   120,   120,   120,   121,
     121,   121,   121,   121,   121,   121,   121,   121,   121,   121,
     121,   121,   122,   122,   122,   122,   122,   122,   122,   122,
     122,   122,   122,   122,   123,   123,   123,   123,   124,   124,
     125,   125,   125,   126,   126,   126,   126,   127,   127,   127,
     127,   127,   127,   127,   127,   127,   127,   127,   127,   127,
     127,   127,   128,   129,   129,   130,   130,   131,   132,   133,
     133,   133,   134,   134,   135,   135,   135,   135,   135,   135,
     136,   136,   137,   138,   138,   138,   138,   138,   138,   140,
     139,   141,   139,   142,   142,   143,   143,   144,   143,   143,
     143,   143,   143,   145,   145,   146,   146,   147,   147,   148,
     148,   149,   150,   150,   151,   151,   151,   152,   153,   153,
     154,   155,   155,   155,   156,   156,   157,   158,   158,   159,
     159,   160,   161,   161,   162,   162,   163,   163,   164,   164,
     164,   164,   164,   164,   164,   165,   165,   165,   165,   165,
     166,   166,   167,   167,   168,   168,   169,   169,   170,   170,
     171,   171,   172,   172,   172,   172,   172,   173,   173,   173,
     174,   175,   175,   175,   176,   176,   176,   176,   177,   177,
     177,   177,   177,   177,   177,   177,   177,   177,   177,   177,
     177,   177,   177,   177,   177,   177,   178,   178,   178,   178,
     178,   178
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     0,     1,     1,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     1,     1,     1,
       2,     1,     1,     1,     1,     1,     5,     6,     4,     7,
       4,     7,     7,    10,     2,     5,     9,     5,     2,     8,
       6,     6,     7,     5,     6,     9,     8,     8,     8,     7,
       7,     8,     6,     7,     8,     9,     8,     6,     9,    10,
       6,     4,     9,     7,     4,     6,     2,     4,     3,     3,
       3,     3,     1,     3,     3,     3,     1,     1,     3,     4,
       4,     3,     4,     7,     4,     4,     5,     1,     1,     3,
       2,     2,     8,     0,     5,     2,     4,     3,     2,     0,
       5,     7,     1,     3,     1,     1,     1,     3,     2,     2,
       1,     3,     4,     5,     6,     6,     3,     3,     5,     0,
       8,     0,    10,     1,     2,     4,     5,     0,     7,     7,
       5,     8,     6,     1,     2,     5,     8,     0,     3,     1,
       3,     2,     4,     5,     2,     5,     6,     3,     1,     2,
       4,     9,    11,    18,     0,     2,     5,     8,     8,     0,
       2,     1,     3,     1,     3,     1,     2,     1,     3,     3,
       3,     3,     3,     3,     3,     1,     1,     1,     1,     1,
       3,     3,     3,     3,     1,     3,     1,     3,     1,     3,
       1,     3,     3,     3,     3,     3,     3,     4,     6,     7,
       4,     3,     3,     1,     3,     3,     3,     1,     1,     4,
       5,     3,     4,     4,     3,     4,     7,     4,     4,     5,
       3,     1,     1,     3,     2,     2,     1,     1,     1,     1,
       1,     1
};


/* YYDPREC[RULE-NUM] -- Dynamic precedence of rule #RULE-NUM (0 if none).  */
static const yytype_int8 yydprec[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     1,     0,
       0,     0,     2,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       2,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       1,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0
};

/* YYMERGER[RULE-NUM] -- Index of merging function for rule #RULE-NUM.  */
static const yytype_int8 yymerger[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0
};

/* YYIMMEDIATE[RULE-NUM] -- True iff rule #RULE-NUM is not to be deferred, as
   in the case of predicates.  */
static const yybool yyimmediate[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0
};

/* YYCONFLP[YYPACT[STATE-NUM]] -- Pointer into YYCONFL of start of
   list of conflicting reductions corresponding to action entry for
   state STATE-NUM in yytable.  0 means no conflicts.  The list in
   yyconfl is terminated by a rule number of 0.  */
static const yytype_int8 yyconflp[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     3,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     5,
       7,     9,    11,    13,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    15,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     1,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0
};

/* YYCONFL[I] -- lists of conflicting rule numbers, each terminated by
   0, pointed into by YYCONFLP.  */
static const short yyconfl[] =
{
       0,   221,     0,   220,     0,   220,     0,   220,     0,   220,
       0,   220,     0,   220,     0,   207,     0
};



YYSTYPE yylval;

int yynerrs;
int yychar;

enum { YYENOMEM = -2 };

typedef enum { yyok, yyaccept, yyabort, yyerr, yynomem } YYRESULTTAG;

#define YYCHK(YYE)                              \
  do {                                          \
    YYRESULTTAG yychk_flag = YYE;               \
    if (yychk_flag != yyok)                     \
      return yychk_flag;                        \
  } while (0)

/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYMAXDEPTH * sizeof (GLRStackItem)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif

/* Minimum number of free items on the stack allowed after an
   allocation.  This is to allow allocation and initialization
   to be completed by functions that call yyexpandGLRStack before the
   stack is expanded, thus insuring that all necessary pointers get
   properly redirected to new data.  */
#define YYHEADROOM 2

#ifndef YYSTACKEXPANDABLE
#  define YYSTACKEXPANDABLE 1
#endif

#if YYSTACKEXPANDABLE
# define YY_RESERVE_GLRSTACK(Yystack)                   \
  do {                                                  \
    if (Yystack->yyspaceLeft < YYHEADROOM)              \
      yyexpandGLRStack (Yystack);                       \
  } while (0)
#else
# define YY_RESERVE_GLRSTACK(Yystack)                   \
  do {                                                  \
    if (Yystack->yyspaceLeft < YYHEADROOM)              \
      yyMemoryExhausted (Yystack);                      \
  } while (0)
#endif

/** State numbers. */
typedef int yy_state_t;

/** Rule numbers. */
typedef int yyRuleNum;

/** Item references. */
typedef short yyItemNum;

typedef struct yyGLRState yyGLRState;
typedef struct yyGLRStateSet yyGLRStateSet;
typedef struct yySemanticOption yySemanticOption;
typedef union yyGLRStackItem yyGLRStackItem;
typedef struct yyGLRStack yyGLRStack;

struct yyGLRState
{
  /** Type tag: always true.  */
  yybool yyisState;
  /** Type tag for yysemantics.  If true, yyval applies, otherwise
   *  yyfirstVal applies.  */
  yybool yyresolved;
  /** Number of corresponding LALR(1) machine state.  */
  yy_state_t yylrState;
  /** Preceding state in this stack */
  yyGLRState* yypred;
  /** Source position of the last token produced by my symbol */
  YYPTRDIFF_T yyposn;
  union {
    /** First in a chain of alternative reductions producing the
     *  nonterminal corresponding to this state, threaded through
     *  yynext.  */
    yySemanticOption* yyfirstVal;
    /** Semantic value for this state.  */
    YYSTYPE yyval;
  } yysemantics;
};

struct yyGLRStateSet
{
  yyGLRState** yystates;
  /** During nondeterministic operation, yylookaheadNeeds tracks which
   *  stacks have actually needed the current lookahead.  During deterministic
   *  operation, yylookaheadNeeds[0] is not maintained since it would merely
   *  duplicate yychar != YYEMPTY.  */
  yybool* yylookaheadNeeds;
  YYPTRDIFF_T yysize;
  YYPTRDIFF_T yycapacity;
};

struct yySemanticOption
{
  /** Type tag: always false.  */
  yybool yyisState;
  /** Rule number for this reduction */
  yyRuleNum yyrule;
  /** The last RHS state in the list of states to be reduced.  */
  yyGLRState* yystate;
  /** The lookahead for this reduction.  */
  int yyrawchar;
  YYSTYPE yyval;
  /** Next sibling in chain of options.  To facilitate merging,
   *  options are chained in decreasing order by address.  */
  yySemanticOption* yynext;
};

/** Type of the items in the GLR stack.  The yyisState field
 *  indicates which item of the union is valid.  */
union yyGLRStackItem {
  yyGLRState yystate;
  yySemanticOption yyoption;
};

struct yyGLRStack {
  int yyerrState;


  YYJMP_BUF yyexception_buffer;
  yyGLRStackItem* yyitems;
  yyGLRStackItem* yynextFree;
  YYPTRDIFF_T yyspaceLeft;
  yyGLRState* yysplitPoint;
  yyGLRState* yylastDeleted;
  yyGLRStateSet yytops;
};

#if YYSTACKEXPANDABLE
static void yyexpandGLRStack (yyGLRStack* yystackp);
#endif

_Noreturn static void
yyFail (yyGLRStack* yystackp, const char* yymsg)
{
  if (yymsg != YY_NULLPTR)
    yyerror (yymsg);
  YYLONGJMP (yystackp->yyexception_buffer, 1);
}

_Noreturn static void
yyMemoryExhausted (yyGLRStack* yystackp)
{
  YYLONGJMP (yystackp->yyexception_buffer, 2);
}

/** Accessing symbol of state YYSTATE.  */
static inline yysymbol_kind_t
yy_accessing_symbol (yy_state_t yystate)
{
  return YY_CAST (yysymbol_kind_t, yystos[yystate]);
}

#if 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "CREATE", "DEFINE",
  "CLASS", "INCLUDE", "INHERITS", "PROFILE", "USING", "ROUTINE",
  "OVERRIDE", "RETURNS", "SET", "VALUE", "VALUES", "CONVERT", "PRINT",
  "APPEND", "SORT", "ASCENDANTLY", "DESCENDANTLY", "COUNT", "CHECK",
  "POSITION", "EXTRACT", "TO", "ASK", "TAKE", "USER", "INPUT", "WITH",
  "MESSAGE", "SET_VALUE_TAKE_USER_INPUT", "OPEN", "MAKE", "FILE_T",
  "WRITE", "READ", "LINE", "LINES", "CLEAR", "BEGIN_T", "CLOSE", "TITLE",
  "INTO", "ARRAY", "TABLE", "MESH", "MATRIX", "ROWS", "COLS", "SIZE",
  "LENGTH", "SIZED", "AT", "EACH", "INDEX", "ROW", "COLUMN", "EMIT",
  "GIVEBACK", "OUTCOME", "OF", "ARGS", "IN", "PLUS", "MINUS", "TIMES",
  "DIV", "MOD", "IF", "THEN", "OTHERWISE", "WHEN", "AND", "OR", "NOT",
  "CMP_EQ", "CMP_NEQ", "CMP_GT", "CMP_LT", "CMP_GTE", "CMP_LTE", "INDENT",
  "DEDENT", "T_INT", "T_FLOAT", "T_CHAR", "T_STRING", "T_BOOL", "TRUE",
  "FALSE", "WHILE", "DO", "FOR", "FROM", "STEP", "REPEAT", "UNTIL",
  "ATTEMPT", "UPTO", "ONFAILURE", "TIMES_WHILE", "INT_LIT", "FLOAT_LIT",
  "CHAR_LIT", "STRING_LIT", "IDENT", "PREC_LOWEST", "','", "')'", "';'",
  "'('", "'/'", "$accept", "program", "stmt_list", "stmt",
  "file_open_stmt", "file_stmt", "collection_decl_stmt",
  "collection_set_stmt", "append_stmt", "sort_stmt", "append_value",
  "append_term", "append_factor", "function_stmt", "opt_param_clause",
  "param_list", "emit_stmt", "giveback_stmt", "opt_call_args",
  "simple_call_args", "call_arg_simple", "call_args", "call_expr",
  "decl_stmt", "class_stmt", "$@1", "$@2", "class_body", "class_member",
  "$@3", "profile_body", "profile_line", "opt_message", "message_parts",
  "print_stmt", "if_stmt", "else_part", "block", "block_body",
  "while_stmt", "for_stmt", "opt_step", "repeat_stmt", "attempt_stmt",
  "opt_onfailure", "condition", "cond_or", "cond_and", "cond_not",
  "cond_atom", "type", "id_list_multi", "val_list_multi",
  "mesh_init_values", "matrix_init_row", "matrix_init_rows", "print_args",
  "compound_stmt", "set_stmt", "convert_stmt", "expr", "term", "factor",
  "literal", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

/** Left-hand-side symbol for rule #YYRULE.  */
static inline yysymbol_kind_t
yylhsNonterm (yyRuleNum yyrule)
{
  return YY_CAST (yysymbol_kind_t, yyr1[yyrule]);
}

#if YYDEBUG

# ifndef YYFPRINTF
#  define YYFPRINTF fprintf
# endif

# define YY_FPRINTF                             \
  YY_IGNORE_USELESS_CAST_BEGIN YY_FPRINTF_

# define YY_FPRINTF_(Args)                      \
  do {                                          \
    YYFPRINTF Args;                             \
    YY_IGNORE_USELESS_CAST_END                  \
  } while (0)

# define YY_DPRINTF                             \
  YY_IGNORE_USELESS_CAST_BEGIN YY_DPRINTF_

# define YY_DPRINTF_(Args)                      \
  do {                                          \
    if (yydebug)                                \
      YYFPRINTF Args;                           \
    YY_IGNORE_USELESS_CAST_END                  \
  } while (0)





/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                  \
  do {                                                                  \
    if (yydebug)                                                        \
      {                                                                 \
        YY_FPRINTF ((stderr, "%s ", Title));                            \
        yy_symbol_print (stderr, Kind, Value);        \
        YY_FPRINTF ((stderr, "\n"));                                    \
      }                                                                 \
  } while (0)

static inline void
yy_reduce_print (yybool yynormal, yyGLRStackItem* yyvsp, YYPTRDIFF_T yyk,
                 yyRuleNum yyrule);

# define YY_REDUCE_PRINT(Args)          \
  do {                                  \
    if (yydebug)                        \
      yy_reduce_print Args;             \
  } while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;

static void yypstack (yyGLRStack* yystackp, YYPTRDIFF_T yyk)
  YY_ATTRIBUTE_UNUSED;
static void yypdumpstack (yyGLRStack* yystackp)
  YY_ATTRIBUTE_UNUSED;

#else /* !YYDEBUG */

# define YY_DPRINTF(Args) do {} while (yyfalse)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_REDUCE_PRINT(Args)

#endif /* !YYDEBUG */

#ifndef yystrlen
# define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
#endif

#ifndef yystpcpy
# if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#  define yystpcpy stpcpy
# else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
# endif
#endif

#ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
#endif


/** Fill in YYVSP[YYLOW1 .. YYLOW0-1] from the chain of states starting
 *  at YYVSP[YYLOW0].yystate.yypred.  Leaves YYVSP[YYLOW1].yystate.yypred
 *  containing the pointer to the next state in the chain.  */
static void yyfillin (yyGLRStackItem *, int, int) YY_ATTRIBUTE_UNUSED;
static void
yyfillin (yyGLRStackItem *yyvsp, int yylow0, int yylow1)
{
  int i;
  yyGLRState *s = yyvsp[yylow0].yystate.yypred;
  for (i = yylow0-1; i >= yylow1; i -= 1)
    {
#if YYDEBUG
      yyvsp[i].yystate.yylrState = s->yylrState;
#endif
      yyvsp[i].yystate.yyresolved = s->yyresolved;
      if (s->yyresolved)
        yyvsp[i].yystate.yysemantics.yyval = s->yysemantics.yyval;
      else
        /* The effect of using yyval or yyloc (in an immediate rule) is
         * undefined.  */
        yyvsp[i].yystate.yysemantics.yyfirstVal = YY_NULLPTR;
      s = yyvsp[i].yystate.yypred = s->yypred;
    }
}


/** If yychar is empty, fetch the next token.  */
static inline yysymbol_kind_t
yygetToken (int *yycharp)
{
  yysymbol_kind_t yytoken;
  if (*yycharp == YYEMPTY)
    {
      YY_DPRINTF ((stderr, "Reading a token\n"));
      *yycharp = yylex ();
    }
  if (*yycharp <= YYEOF)
    {
      *yycharp = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YY_DPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (*yycharp);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }
  return yytoken;
}

/* Do nothing if YYNORMAL or if *YYLOW <= YYLOW1.  Otherwise, fill in
 * YYVSP[YYLOW1 .. *YYLOW-1] as in yyfillin and set *YYLOW = YYLOW1.
 * For convenience, always return YYLOW1.  */
static inline int yyfill (yyGLRStackItem *, int *, int, yybool)
     YY_ATTRIBUTE_UNUSED;
static inline int
yyfill (yyGLRStackItem *yyvsp, int *yylow, int yylow1, yybool yynormal)
{
  if (!yynormal && yylow1 < *yylow)
    {
      yyfillin (yyvsp, *yylow, yylow1);
      *yylow = yylow1;
    }
  return yylow1;
}

/** Perform user action for rule number YYN, with RHS length YYRHSLEN,
 *  and top stack item YYVSP.  YYLVALP points to place to put semantic
 *  value ($$), and yylocp points to place for location information
 *  (@$).  Returns yyok for normal return, yyaccept for YYACCEPT,
 *  yyerr for YYERROR, yyabort for YYABORT, yynomem for YYNOMEM.  */
static YYRESULTTAG
yyuserAction (yyRuleNum yyrule, int yyrhslen, yyGLRStackItem* yyvsp,
              yyGLRStack* yystackp, YYPTRDIFF_T yyk,
              YYSTYPE* yyvalp)
{
  const yybool yynormal YY_ATTRIBUTE_UNUSED = yystackp->yysplitPoint == YY_NULLPTR;
  int yylow = 1;
  YY_USE (yyvalp);
  YY_USE (yyk);
  YY_USE (yyrhslen);
# undef yyerrok
# define yyerrok (yystackp->yyerrState = 0)
# undef YYACCEPT
# define YYACCEPT return yyaccept
# undef YYABORT
# define YYABORT return yyabort
# undef YYNOMEM
# define YYNOMEM return yynomem
# undef YYERROR
# define YYERROR return yyerrok, yyerr
# undef YYRECOVERING
# define YYRECOVERING() (yystackp->yyerrState != 0)
# undef yyclearin
# define yyclearin (yychar = YYEMPTY)
# undef YYFILL
# define YYFILL(N) yyfill (yyvsp, &yylow, (N), yynormal)
# undef YYBACKUP
# define YYBACKUP(Token, Value)                                              \
  return yyerror (YY_("syntax error: cannot back up")),     \
         yyerrok, yyerr

  if (yyrhslen == 0)
    *yyvalp = yyval_default;
  else
    *yyvalp = yyvsp[YYFILL (1-yyrhslen)].yystate.yysemantics.yyval;
  /* If yyk == -1, we are running a deferred action on a temporary
     stack.  In that case, YY_REDUCE_PRINT must not play with YYFILL,
     so pretend the stack is "normal". */
  YY_REDUCE_PRINT ((yynormal || yyk == -1, yyvsp, yyk, yyrule));
  switch (yyrule)
    {
  case 2: /* program: %empty  */
#line 1048 "src\\lexico.y"
                        { lexico_parsed_program = ast_new_program(); }
#line 2905 "build\\lexico.tab.cpp"
    break;

  case 3: /* program: stmt_list  */
#line 1049 "src\\lexico.y"
                        { lexico_parsed_program = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.prog); }
#line 2911 "build\\lexico.tab.cpp"
    break;

  case 4: /* stmt_list: stmt  */
#line 1053 "src\\lexico.y"
                        {
                            ((*yyvalp).prog) = ast_new_program();
                            ast_program_add(((*yyvalp).prog), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt));
                        }
#line 2920 "build\\lexico.tab.cpp"
    break;

  case 5: /* stmt_list: stmt_list stmt  */
#line 1057 "src\\lexico.y"
                        {
                            ast_program_add((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.prog), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt));
                            ((*yyvalp).prog) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.prog);
                        }
#line 2929 "build\\lexico.tab.cpp"
    break;

  case 6: /* stmt: decl_stmt ';'  */
#line 1066 "src\\lexico.y"
                        { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.stmt); }
#line 2935 "build\\lexico.tab.cpp"
    break;

  case 7: /* stmt: collection_decl_stmt ';'  */
#line 1067 "src\\lexico.y"
                               { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.stmt); }
#line 2941 "build\\lexico.tab.cpp"
    break;

  case 8: /* stmt: collection_set_stmt ';'  */
#line 1068 "src\\lexico.y"
                                       { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.stmt); }
#line 2947 "build\\lexico.tab.cpp"
    break;

  case 9: /* stmt: append_stmt ';'  */
#line 1069 "src\\lexico.y"
                               { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.stmt); }
#line 2953 "build\\lexico.tab.cpp"
    break;

  case 10: /* stmt: sort_stmt ';'  */
#line 1070 "src\\lexico.y"
                               { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.stmt); }
#line 2959 "build\\lexico.tab.cpp"
    break;

  case 11: /* stmt: print_stmt ';'  */
#line 1071 "src\\lexico.y"
                        { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.stmt); }
#line 2965 "build\\lexico.tab.cpp"
    break;

  case 12: /* stmt: set_stmt ';'  */
#line 1072 "src\\lexico.y"
                                { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.stmt); }
#line 2971 "build\\lexico.tab.cpp"
    break;

  case 13: /* stmt: convert_stmt ';'  */
#line 1073 "src\\lexico.y"
                        { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.stmt); }
#line 2977 "build\\lexico.tab.cpp"
    break;

  case 14: /* stmt: emit_stmt ';'  */
#line 1074 "src\\lexico.y"
                        { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.stmt); }
#line 2983 "build\\lexico.tab.cpp"
    break;

  case 15: /* stmt: giveback_stmt ';'  */
#line 1075 "src\\lexico.y"
                        { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.stmt); }
#line 2989 "build\\lexico.tab.cpp"
    break;

  case 16: /* stmt: compound_stmt ';'  */
#line 1076 "src\\lexico.y"
                        { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.stmt); }
#line 2995 "build\\lexico.tab.cpp"
    break;

  case 17: /* stmt: function_stmt  */
#line 1077 "src\\lexico.y"
                        { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt); }
#line 3001 "build\\lexico.tab.cpp"
    break;

  case 18: /* stmt: class_stmt  */
#line 1078 "src\\lexico.y"
                        { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt); }
#line 3007 "build\\lexico.tab.cpp"
    break;

  case 19: /* stmt: file_open_stmt  */
#line 1079 "src\\lexico.y"
                        { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt); }
#line 3013 "build\\lexico.tab.cpp"
    break;

  case 20: /* stmt: file_stmt ';'  */
#line 1080 "src\\lexico.y"
                        { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.stmt); }
#line 3019 "build\\lexico.tab.cpp"
    break;

  case 21: /* stmt: if_stmt  */
#line 1081 "src\\lexico.y"
                        { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt); }
#line 3025 "build\\lexico.tab.cpp"
    break;

  case 22: /* stmt: while_stmt  */
#line 1082 "src\\lexico.y"
                        { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt); }
#line 3031 "build\\lexico.tab.cpp"
    break;

  case 23: /* stmt: for_stmt  */
#line 1083 "src\\lexico.y"
                        { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt); }
#line 3037 "build\\lexico.tab.cpp"
    break;

  case 24: /* stmt: repeat_stmt  */
#line 1084 "src\\lexico.y"
                        { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt); }
#line 3043 "build\\lexico.tab.cpp"
    break;

  case 25: /* stmt: attempt_stmt  */
#line 1085 "src\\lexico.y"
                        { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt); }
#line 3049 "build\\lexico.tab.cpp"
    break;

  case 26: /* file_open_stmt: OPEN expr FILE_T DO block  */
#line 1089 "src\\lexico.y"
                                {
        ((*yyvalp).stmt) = ast_new_file_open(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.expr), 0, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt));
    }
#line 3057 "build\\lexico.tab.cpp"
    break;

  case 27: /* file_open_stmt: OPEN expr FILE_T MAKE DO block  */
#line 1092 "src\\lexico.y"
                                     {
        ((*yyvalp).stmt) = ast_new_file_open(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.expr), 1, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt));
    }
#line 3065 "build\\lexico.tab.cpp"
    break;

  case 28: /* file_stmt: WRITE expr IN FILE_T  */
#line 1098 "src\\lexico.y"
                           {
        ((*yyvalp).stmt) = ast_new_file_write(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), NULL);
    }
#line 3073 "build\\lexico.tab.cpp"
    break;

  case 29: /* file_stmt: WRITE expr IN LINE expr IN FILE_T  */
#line 1101 "src\\lexico.y"
                                        {
        ((*yyvalp).stmt) = ast_new_file_write(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr));
    }
#line 3081 "build\\lexico.tab.cpp"
    break;

  case 30: /* file_stmt: READ FILE_T INTO IDENT  */
#line 1104 "src\\lexico.y"
                             {
        ((*yyvalp).stmt) = ast_new_file_read(yylineno, FILE_READ_ALL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval), NULL, NULL, NULL);
    }
#line 3089 "build\\lexico.tab.cpp"
    break;

  case 31: /* file_stmt: READ LINE expr IN FILE_T INTO IDENT  */
#line 1107 "src\\lexico.y"
                                          {
        ((*yyvalp).stmt) = ast_new_file_read(yylineno, FILE_READ_LINE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval), NULL, NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.expr));
    }
#line 3097 "build\\lexico.tab.cpp"
    break;

  case 32: /* file_stmt: READ expr LINES IN FILE_T INTO IDENT  */
#line 1110 "src\\lexico.y"
                                           {
        ((*yyvalp).stmt) = ast_new_file_read(yylineno, FILE_READ_FIRST, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.expr), NULL, NULL);
    }
#line 3105 "build\\lexico.tab.cpp"
    break;

  case 33: /* file_stmt: READ expr LINES FROM LINE expr IN FILE_T INTO IDENT  */
#line 1113 "src\\lexico.y"
                                                          {
        ((*yyvalp).stmt) = ast_new_file_read(yylineno, FILE_READ_RANGE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.expr), NULL);
    }
#line 3113 "build\\lexico.tab.cpp"
    break;

  case 34: /* file_stmt: CLEAR FILE_T  */
#line 1116 "src\\lexico.y"
                   {
        ((*yyvalp).stmt) = ast_new_file_clear(yylineno, FILE_CLEAR_ALL, NULL, NULL, NULL);
    }
#line 3121 "build\\lexico.tab.cpp"
    break;

  case 35: /* file_stmt: CLEAR LINE expr IN FILE_T  */
#line 1119 "src\\lexico.y"
                                {
        ((*yyvalp).stmt) = ast_new_file_clear(yylineno, FILE_CLEAR_LINE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), NULL, NULL);
    }
#line 3129 "build\\lexico.tab.cpp"
    break;

  case 36: /* file_stmt: CLEAR FROM LINE expr TO LINE expr IN FILE_T  */
#line 1122 "src\\lexico.y"
                                                  {
        ((*yyvalp).stmt) = ast_new_file_clear(yylineno, FILE_CLEAR_RANGE, NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr));
    }
#line 3137 "build\\lexico.tab.cpp"
    break;

  case 37: /* file_stmt: SET FILE_T TITLE TO expr  */
#line 1125 "src\\lexico.y"
                               {
        ((*yyvalp).stmt) = ast_new_file_set_title(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3145 "build\\lexico.tab.cpp"
    break;

  case 38: /* file_stmt: CLOSE FILE_T  */
#line 1128 "src\\lexico.y"
                   {
        ((*yyvalp).stmt) = ast_new_file_close(yylineno);
    }
#line 3153 "build\\lexico.tab.cpp"
    break;

  case 39: /* collection_decl_stmt: CREATE ARRAY OF type IDENT WITH SIZE expr  */
#line 1134 "src\\lexico.y"
                                                {
        ((*yyvalp).stmt) = ast_new_array_decl(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.typekind), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3161 "build\\lexico.tab.cpp"
    break;

  case 40: /* collection_decl_stmt: CREATE TABLE IDENT WITH SIZE expr  */
#line 1137 "src\\lexico.y"
                                        {
        ((*yyvalp).stmt) = ast_new_table_decl(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3169 "build\\lexico.tab.cpp"
    break;

  case 41: /* collection_decl_stmt: CREATE type MESH IDENT SIZED expr  */
#line 1140 "src\\lexico.y"
                                        {
        ((*yyvalp).stmt) = ast_new_array_decl(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.typekind), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3177 "build\\lexico.tab.cpp"
    break;

  case 42: /* collection_decl_stmt: CREATE type MESH IDENT SET VALUES mesh_init_values  */
#line 1143 "src\\lexico.y"
                                                         {
        Stmt *blk = ast_new_block(yylineno);
        size_t name_len = strlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval));
        ast_block_add(blk,
                      ast_new_array_decl(yylineno,
                                         (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.typekind),
                                         (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval),
                                         ast_expr_literal(ast_lit_int((int64_t)mesh_init_list_len))));

        for (size_t i = 0; i < mesh_init_list_len; i++) {
            char *target = static_cast<char *>(malloc(name_len + 1));
            if (!target) {
                fprintf(stderr, "Fatal: out of memory.\n");
                exit(EXIT_FAILURE);
            }
            memcpy(target, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval), name_len + 1);
            ast_block_add(blk,
                          ast_new_collection_set(yylineno,
                                                 target,
                                                 ast_expr_literal(ast_lit_int((int64_t)i)),
                                                 (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.exprlist)[i]));
        }
        free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.exprlist));
        ((*yyvalp).stmt) = blk;
    }
#line 3207 "build\\lexico.tab.cpp"
    break;

  case 43: /* collection_decl_stmt: CREATE MESH IDENT SIZED expr  */
#line 1168 "src\\lexico.y"
                                   {
        ((*yyvalp).stmt) = ast_new_table_decl(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3215 "build\\lexico.tab.cpp"
    break;

  case 44: /* collection_decl_stmt: CREATE MESH IDENT SET VALUES mesh_init_values  */
#line 1171 "src\\lexico.y"
                                                    {
        Stmt *blk = ast_new_block(yylineno);
        size_t name_len = strlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval));
        ast_block_add(blk,
                      ast_new_table_decl(yylineno,
                                         (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval),
                                         ast_expr_literal(ast_lit_int((int64_t)mesh_init_list_len))));

        for (size_t i = 0; i < mesh_init_list_len; i++) {
            char *target = static_cast<char *>(malloc(name_len + 1));
            if (!target) {
                fprintf(stderr, "Fatal: out of memory.\n");
                exit(EXIT_FAILURE);
            }
            memcpy(target, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval), name_len + 1);
            ast_block_add(blk,
                          ast_new_collection_set(yylineno,
                                                 target,
                                                 ast_expr_literal(ast_lit_int((int64_t)i)),
                                                 (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.exprlist)[i]));
        }
        free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.exprlist));
        ((*yyvalp).stmt) = blk;
    }
#line 3244 "build\\lexico.tab.cpp"
    break;

  case 45: /* collection_decl_stmt: CREATE type MATRIX MESH IDENT ROWS expr COLS expr  */
#line 1195 "src\\lexico.y"
                                                        {
        ((*yyvalp).stmt) = ast_new_matrix_decl(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.typekind), 0, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3252 "build\\lexico.tab.cpp"
    break;

  case 46: /* collection_decl_stmt: CREATE type MATRIX MESH IDENT SET VALUES matrix_init_rows  */
#line 1198 "src\\lexico.y"
                                                                {
        MatrixInit *mi = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.minit);
        if (!mi || mi->row_count == 0 || mi->row_sizes[0] == 0) {
            fprintf(stderr, "Error (line %d): matrix initializer cannot be empty.\n", yylineno);
            if (mi) matrix_init_free(mi, 1);
            YYERROR;
        }

        size_t cols = mi->row_sizes[0];
        for (size_t r = 1; r < mi->row_count; r++) {
            if (mi->row_sizes[r] != cols) {
                fprintf(stderr,
                        "Error (line %d): matrix initializer rows must have equal lengths (row 0 has %zu, row %zu has %zu).\n",
                        yylineno, cols, r, mi->row_sizes[r]);
                matrix_init_free(mi, 1);
                YYERROR;
            }
        }

        Stmt *blk = ast_new_block(yylineno);
        ast_block_add(blk,
                      ast_new_matrix_decl(yylineno,
                                          (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.typekind),
                                          0,
                                          (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval),
                                          ast_expr_literal(ast_lit_int((int64_t)mi->row_count)),
                                          ast_expr_literal(ast_lit_int((int64_t)cols))));

        size_t idx = 0;
        size_t name_len = strlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval));
        for (size_t r = 0; r < mi->row_count; r++) {
            for (size_t c = 0; c < cols; c++) {
                char *target = static_cast<char *>(malloc(name_len + 1));
                if (!target) {
                    fprintf(stderr, "Fatal: out of memory.\n");
                    exit(EXIT_FAILURE);
                }
                memcpy(target, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval), name_len + 1);
                ast_block_add(blk,
                              ast_new_matrix_set(yylineno,
                                                 target,
                                                 ast_expr_literal(ast_lit_int((int64_t)r)),
                                                 ast_expr_literal(ast_lit_int((int64_t)c)),
                                                 mi->values[idx++]));
            }
        }

        matrix_init_free(mi, 0);
        ((*yyvalp).stmt) = blk;
    }
#line 3307 "build\\lexico.tab.cpp"
    break;

  case 47: /* collection_decl_stmt: CREATE type MATRIX MESH IDENT SET VALUE matrix_init_rows  */
#line 1248 "src\\lexico.y"
                                                               {
        MatrixInit *mi = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.minit);
        if (!mi || mi->row_count == 0 || mi->row_sizes[0] == 0) {
            fprintf(stderr, "Error (line %d): matrix initializer cannot be empty.\n", yylineno);
            if (mi) matrix_init_free(mi, 1);
            YYERROR;
        }

        size_t cols = mi->row_sizes[0];
        for (size_t r = 1; r < mi->row_count; r++) {
            if (mi->row_sizes[r] != cols) {
                fprintf(stderr,
                        "Error (line %d): matrix initializer rows must have equal lengths (row 0 has %zu, row %zu has %zu).\n",
                        yylineno, cols, r, mi->row_sizes[r]);
                matrix_init_free(mi, 1);
                YYERROR;
            }
        }

        Stmt *blk = ast_new_block(yylineno);
        ast_block_add(blk,
                      ast_new_matrix_decl(yylineno,
                                          (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.typekind),
                                          0,
                                          (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval),
                                          ast_expr_literal(ast_lit_int((int64_t)mi->row_count)),
                                          ast_expr_literal(ast_lit_int((int64_t)cols))));

        size_t idx = 0;
        size_t name_len = strlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval));
        for (size_t r = 0; r < mi->row_count; r++) {
            for (size_t c = 0; c < cols; c++) {
                char *target = static_cast<char *>(malloc(name_len + 1));
                if (!target) {
                    fprintf(stderr, "Fatal: out of memory.\n");
                    exit(EXIT_FAILURE);
                }
                memcpy(target, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval), name_len + 1);
                ast_block_add(blk,
                              ast_new_matrix_set(yylineno,
                                                 target,
                                                 ast_expr_literal(ast_lit_int((int64_t)r)),
                                                 ast_expr_literal(ast_lit_int((int64_t)c)),
                                                 mi->values[idx++]));
            }
        }

        matrix_init_free(mi, 0);
        ((*yyvalp).stmt) = blk;
    }
#line 3362 "build\\lexico.tab.cpp"
    break;

  case 48: /* collection_decl_stmt: CREATE MATRIX MESH IDENT ROWS expr COLS expr  */
#line 1298 "src\\lexico.y"
                                                   {
        ((*yyvalp).stmt) = ast_new_matrix_decl(yylineno, TYPE_INT, 1, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3370 "build\\lexico.tab.cpp"
    break;

  case 49: /* collection_decl_stmt: CREATE MATRIX MESH IDENT SET VALUES matrix_init_rows  */
#line 1301 "src\\lexico.y"
                                                           {
        MatrixInit *mi = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.minit);
        if (!mi || mi->row_count == 0 || mi->row_sizes[0] == 0) {
            fprintf(stderr, "Error (line %d): matrix initializer cannot be empty.\n", yylineno);
            if (mi) matrix_init_free(mi, 1);
            YYERROR;
        }

        size_t cols = mi->row_sizes[0];
        for (size_t r = 1; r < mi->row_count; r++) {
            if (mi->row_sizes[r] != cols) {
                fprintf(stderr,
                        "Error (line %d): matrix initializer rows must have equal lengths (row 0 has %zu, row %zu has %zu).\n",
                        yylineno, cols, r, mi->row_sizes[r]);
                matrix_init_free(mi, 1);
                YYERROR;
            }
        }

        Stmt *blk = ast_new_block(yylineno);
        ast_block_add(blk,
                      ast_new_matrix_decl(yylineno,
                                          TYPE_INT,
                                          1,
                                          (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval),
                                          ast_expr_literal(ast_lit_int((int64_t)mi->row_count)),
                                          ast_expr_literal(ast_lit_int((int64_t)cols))));

        size_t idx = 0;
        size_t name_len = strlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval));
        for (size_t r = 0; r < mi->row_count; r++) {
            for (size_t c = 0; c < cols; c++) {
                char *target = static_cast<char *>(malloc(name_len + 1));
                if (!target) {
                    fprintf(stderr, "Fatal: out of memory.\n");
                    exit(EXIT_FAILURE);
                }
                memcpy(target, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval), name_len + 1);
                ast_block_add(blk,
                              ast_new_matrix_set(yylineno,
                                                 target,
                                                 ast_expr_literal(ast_lit_int((int64_t)r)),
                                                 ast_expr_literal(ast_lit_int((int64_t)c)),
                                                 mi->values[idx++]));
            }
        }

        matrix_init_free(mi, 0);
        ((*yyvalp).stmt) = blk;
    }
#line 3425 "build\\lexico.tab.cpp"
    break;

  case 50: /* collection_decl_stmt: CREATE MATRIX MESH IDENT SET VALUE matrix_init_rows  */
#line 1351 "src\\lexico.y"
                                                          {
        MatrixInit *mi = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.minit);
        if (!mi || mi->row_count == 0 || mi->row_sizes[0] == 0) {
            fprintf(stderr, "Error (line %d): matrix initializer cannot be empty.\n", yylineno);
            if (mi) matrix_init_free(mi, 1);
            YYERROR;
        }

        size_t cols = mi->row_sizes[0];
        for (size_t r = 1; r < mi->row_count; r++) {
            if (mi->row_sizes[r] != cols) {
                fprintf(stderr,
                        "Error (line %d): matrix initializer rows must have equal lengths (row 0 has %zu, row %zu has %zu).\n",
                        yylineno, cols, r, mi->row_sizes[r]);
                matrix_init_free(mi, 1);
                YYERROR;
            }
        }

        Stmt *blk = ast_new_block(yylineno);
        ast_block_add(blk,
                      ast_new_matrix_decl(yylineno,
                                          TYPE_INT,
                                          1,
                                          (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval),
                                          ast_expr_literal(ast_lit_int((int64_t)mi->row_count)),
                                          ast_expr_literal(ast_lit_int((int64_t)cols))));

        size_t idx = 0;
        size_t name_len = strlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval));
        for (size_t r = 0; r < mi->row_count; r++) {
            for (size_t c = 0; c < cols; c++) {
                char *target = static_cast<char *>(malloc(name_len + 1));
                if (!target) {
                    fprintf(stderr, "Fatal: out of memory.\n");
                    exit(EXIT_FAILURE);
                }
                memcpy(target, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval), name_len + 1);
                ast_block_add(blk,
                              ast_new_matrix_set(yylineno,
                                                 target,
                                                 ast_expr_literal(ast_lit_int((int64_t)r)),
                                                 ast_expr_literal(ast_lit_int((int64_t)c)),
                                                 mi->values[idx++]));
            }
        }

        matrix_init_free(mi, 0);
        ((*yyvalp).stmt) = blk;
    }
#line 3480 "build\\lexico.tab.cpp"
    break;

  case 51: /* collection_decl_stmt: CREATE type MATRIX IDENT ROWS expr COLS expr  */
#line 1401 "src\\lexico.y"
                                                   {
        /* Backward-compatible form. */
        ((*yyvalp).stmt) = ast_new_matrix_decl(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.typekind), 0, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3489 "build\\lexico.tab.cpp"
    break;

  case 52: /* collection_set_stmt: SET IDENT AT expr TO expr  */
#line 1408 "src\\lexico.y"
                                {
        ((*yyvalp).stmt) = ast_new_collection_set(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3497 "build\\lexico.tab.cpp"
    break;

  case 53: /* collection_set_stmt: SET IDENT VALUE AT expr TO expr  */
#line 1411 "src\\lexico.y"
                                      {
        ((*yyvalp).stmt) = ast_new_collection_set(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3505 "build\\lexico.tab.cpp"
    break;

  case 54: /* collection_set_stmt: SET IDENT AT expr ',' expr TO expr  */
#line 1414 "src\\lexico.y"
                                         {
        ((*yyvalp).stmt) = ast_new_matrix_set(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3513 "build\\lexico.tab.cpp"
    break;

  case 55: /* collection_set_stmt: SET IDENT VALUE AT expr ',' expr TO expr  */
#line 1417 "src\\lexico.y"
                                               {
        ((*yyvalp).stmt) = ast_new_matrix_set(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3521 "build\\lexico.tab.cpp"
    break;

  case 56: /* collection_set_stmt: SET expr TO IDENT AT expr ',' expr  */
#line 1420 "src\\lexico.y"
                                         {
        ((*yyvalp).stmt) = ast_new_matrix_set(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.expr));
    }
#line 3529 "build\\lexico.tab.cpp"
    break;

  case 57: /* collection_set_stmt: SET expr AT expr ',' expr  */
#line 1423 "src\\lexico.y"
                                {
        ((*yyvalp).stmt) = ast_new_matrix_set(yylineno, NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.expr));
    }
#line 3537 "build\\lexico.tab.cpp"
    break;

  case 58: /* collection_set_stmt: SET IDENT AT expr TO TAKE USER INPUT opt_message  */
#line 1426 "src\\lexico.y"
                                                       {
        ((*yyvalp).stmt) = ast_new_collection_set_input(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.exprlist), prompt_list_len);
    }
#line 3545 "build\\lexico.tab.cpp"
    break;

  case 59: /* collection_set_stmt: SET IDENT VALUE AT expr TO TAKE USER INPUT opt_message  */
#line 1429 "src\\lexico.y"
                                                             {
        ((*yyvalp).stmt) = ast_new_collection_set_input(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.exprlist), prompt_list_len);
    }
#line 3553 "build\\lexico.tab.cpp"
    break;

  case 60: /* collection_set_stmt: SET VALUE AT expr TO expr  */
#line 1432 "src\\lexico.y"
                                {
        ((*yyvalp).stmt) = ast_new_collection_set(yylineno, NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3561 "build\\lexico.tab.cpp"
    break;

  case 61: /* collection_set_stmt: SET VALUE TO expr  */
#line 1435 "src\\lexico.y"
                        {
        ((*yyvalp).stmt) = ast_new_collection_set(yylineno, NULL, NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3569 "build\\lexico.tab.cpp"
    break;

  case 62: /* collection_set_stmt: SET VALUE AT expr TO TAKE USER INPUT opt_message  */
#line 1438 "src\\lexico.y"
                                                       {
        ((*yyvalp).stmt) = ast_new_collection_set_input(yylineno, NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.exprlist), prompt_list_len);
    }
#line 3577 "build\\lexico.tab.cpp"
    break;

  case 63: /* collection_set_stmt: SET VALUE TO TAKE USER INPUT opt_message  */
#line 1441 "src\\lexico.y"
                                               {
        ((*yyvalp).stmt) = ast_new_collection_set_input(yylineno, NULL, NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.exprlist), prompt_list_len);
    }
#line 3585 "build\\lexico.tab.cpp"
    break;

  case 64: /* append_stmt: APPEND append_value TO IDENT  */
#line 1447 "src\\lexico.y"
                                   {
        ((*yyvalp).stmt) = ast_new_collection_append(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), NULL);
    }
#line 3593 "build\\lexico.tab.cpp"
    break;

  case 65: /* append_stmt: APPEND append_value TO IDENT AT expr  */
#line 1450 "src\\lexico.y"
                                           {
        ((*yyvalp).stmt) = ast_new_collection_append(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3601 "build\\lexico.tab.cpp"
    break;

  case 66: /* append_stmt: APPEND append_value  */
#line 1453 "src\\lexico.y"
                          {
        ((*yyvalp).stmt) = ast_new_collection_append(yylineno, NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr), NULL);
    }
#line 3609 "build\\lexico.tab.cpp"
    break;

  case 67: /* append_stmt: APPEND append_value AT expr  */
#line 1456 "src\\lexico.y"
                                  {
        ((*yyvalp).stmt) = ast_new_collection_append(yylineno, NULL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3617 "build\\lexico.tab.cpp"
    break;

  case 68: /* sort_stmt: SORT IDENT ASCENDANTLY  */
#line 1462 "src\\lexico.y"
                             {
        ((*yyvalp).stmt) = ast_new_collection_sort(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.sval), 1);
    }
#line 3625 "build\\lexico.tab.cpp"
    break;

  case 69: /* sort_stmt: SORT IDENT DESCENDANTLY  */
#line 1465 "src\\lexico.y"
                              {
        ((*yyvalp).stmt) = ast_new_collection_sort(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.sval), 0);
    }
#line 3633 "build\\lexico.tab.cpp"
    break;

  case 70: /* append_value: append_value PLUS append_term  */
#line 1471 "src\\lexico.y"
                                      { ((*yyvalp).expr) = ast_expr_binary(OP_ADD, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 3639 "build\\lexico.tab.cpp"
    break;

  case 71: /* append_value: append_value MINUS append_term  */
#line 1472 "src\\lexico.y"
                                      { ((*yyvalp).expr) = ast_expr_binary(OP_SUB, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 3645 "build\\lexico.tab.cpp"
    break;

  case 72: /* append_value: append_term  */
#line 1473 "src\\lexico.y"
                                      { ((*yyvalp).expr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr); }
#line 3651 "build\\lexico.tab.cpp"
    break;

  case 73: /* append_term: append_term TIMES append_factor  */
#line 1477 "src\\lexico.y"
                                      { ((*yyvalp).expr) = ast_expr_binary(OP_MUL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 3657 "build\\lexico.tab.cpp"
    break;

  case 74: /* append_term: append_term DIV append_factor  */
#line 1478 "src\\lexico.y"
                                      { ((*yyvalp).expr) = ast_expr_binary(OP_DIV, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 3663 "build\\lexico.tab.cpp"
    break;

  case 75: /* append_term: append_term MOD append_factor  */
#line 1479 "src\\lexico.y"
                                      { ((*yyvalp).expr) = ast_expr_binary(OP_MOD, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 3669 "build\\lexico.tab.cpp"
    break;

  case 76: /* append_term: append_factor  */
#line 1480 "src\\lexico.y"
                                      { ((*yyvalp).expr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr); }
#line 3675 "build\\lexico.tab.cpp"
    break;

  case 77: /* append_factor: call_expr  */
#line 1484 "src\\lexico.y"
                                     { ((*yyvalp).expr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr); }
#line 3681 "build\\lexico.tab.cpp"
    break;

  case 78: /* append_factor: SIZE OF IDENT  */
#line 1485 "src\\lexico.y"
                                     { ((*yyvalp).expr) = ast_expr_length((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 3687 "build\\lexico.tab.cpp"
    break;

  case 79: /* append_factor: ROW LENGTH OF IDENT  */
#line 1486 "src\\lexico.y"
                                     { ((*yyvalp).expr) = ast_expr_row_length((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 3693 "build\\lexico.tab.cpp"
    break;

  case 80: /* append_factor: COLUMN LENGTH OF IDENT  */
#line 1487 "src\\lexico.y"
                                     { ((*yyvalp).expr) = ast_expr_col_length((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 3699 "build\\lexico.tab.cpp"
    break;

  case 81: /* append_factor: LENGTH OF append_factor  */
#line 1488 "src\\lexico.y"
                                     { ((*yyvalp).expr) = ast_expr_strlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 3705 "build\\lexico.tab.cpp"
    break;

  case 82: /* append_factor: EXTRACT append_value FROM IDENT  */
#line 1489 "src\\lexico.y"
                                      { ((*yyvalp).expr) = ast_expr_extract((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 3711 "build\\lexico.tab.cpp"
    break;

  case 83: /* append_factor: EXTRACT FROM append_value TO append_value IN IDENT  */
#line 1490 "src\\lexico.y"
                                                         { ((*yyvalp).expr) = ast_expr_extract_range((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 3717 "build\\lexico.tab.cpp"
    break;

  case 84: /* append_factor: COUNT append_value IN IDENT  */
#line 1491 "src\\lexico.y"
                                     { ((*yyvalp).expr) = ast_expr_count((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 3723 "build\\lexico.tab.cpp"
    break;

  case 85: /* append_factor: CHECK append_value IN IDENT  */
#line 1492 "src\\lexico.y"
                                     { ((*yyvalp).expr) = ast_expr_check((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 3729 "build\\lexico.tab.cpp"
    break;

  case 86: /* append_factor: POSITION OF append_value IN IDENT  */
#line 1493 "src\\lexico.y"
                                        { ((*yyvalp).expr) = ast_expr_position((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 3735 "build\\lexico.tab.cpp"
    break;

  case 87: /* append_factor: IDENT  */
#line 1494 "src\\lexico.y"
                                     { ((*yyvalp).expr) = ast_expr_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 3741 "build\\lexico.tab.cpp"
    break;

  case 88: /* append_factor: literal  */
#line 1495 "src\\lexico.y"
                                     { ((*yyvalp).expr) = ast_expr_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.lit)); }
#line 3747 "build\\lexico.tab.cpp"
    break;

  case 89: /* append_factor: '(' append_value ')'  */
#line 1496 "src\\lexico.y"
                                     { ((*yyvalp).expr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.expr); }
#line 3753 "build\\lexico.tab.cpp"
    break;

  case 90: /* append_factor: MINUS append_factor  */
#line 1497 "src\\lexico.y"
                                     { ((*yyvalp).expr) = ast_expr_neg((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 3759 "build\\lexico.tab.cpp"
    break;

  case 91: /* append_factor: PLUS append_factor  */
#line 1498 "src\\lexico.y"
                                     { ((*yyvalp).expr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr); }
#line 3765 "build\\lexico.tab.cpp"
    break;

  case 92: /* function_stmt: DEFINE ROUTINE IDENT opt_param_clause RETURNS type DO block  */
#line 1502 "src\\lexico.y"
                                                                  {
        ((*yyvalp).stmt) = ast_new_funcdef(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.typekind), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.plist), param_list_len, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt));
    }
#line 3773 "build\\lexico.tab.cpp"
    break;

  case 93: /* opt_param_clause: %empty  */
#line 1508 "src\\lexico.y"
                  {
        ((*yyvalp).plist) = NULL;
        param_list_len = 0;
    }
#line 3782 "build\\lexico.tab.cpp"
    break;

  case 94: /* opt_param_clause: WITH ARGS IN TAKE param_list  */
#line 1512 "src\\lexico.y"
                                   {
        ((*yyvalp).plist) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.plist);
    }
#line 3790 "build\\lexico.tab.cpp"
    break;

  case 95: /* param_list: type IDENT  */
#line 1518 "src\\lexico.y"
                 {
        ((*yyvalp).plist) = ast_param_list_new((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.typekind), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval), &param_list_len);
    }
#line 3798 "build\\lexico.tab.cpp"
    break;

  case 96: /* param_list: param_list ',' type IDENT  */
#line 1521 "src\\lexico.y"
                                {
        ((*yyvalp).plist) = ast_param_list_push((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.plist), &param_list_len, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.typekind), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval));
    }
#line 3806 "build\\lexico.tab.cpp"
    break;

  case 97: /* emit_stmt: EMIT IDENT opt_call_args  */
#line 1527 "src\\lexico.y"
                               {
        ((*yyvalp).stmt) = ast_new_emit(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.exprlist), call_list_len);
    }
#line 3814 "build\\lexico.tab.cpp"
    break;

  case 98: /* giveback_stmt: GIVEBACK expr  */
#line 1533 "src\\lexico.y"
                    {
        ((*yyvalp).stmt) = ast_new_giveback(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3822 "build\\lexico.tab.cpp"
    break;

  case 99: /* opt_call_args: %empty  */
#line 1539 "src\\lexico.y"
                  {
        ((*yyvalp).exprlist) = NULL;
        call_list_len = 0;
    }
#line 3831 "build\\lexico.tab.cpp"
    break;

  case 100: /* opt_call_args: WITH ARGS IN TAKE simple_call_args  */
#line 1543 "src\\lexico.y"
                                                           {
        ((*yyvalp).exprlist) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.exprlist);
    }
#line 3839 "build\\lexico.tab.cpp"
    break;

  case 101: /* opt_call_args: WITH ARGS IN TAKE '(' call_args ')'  */
#line 1546 "src\\lexico.y"
                                          {
        ((*yyvalp).exprlist) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.exprlist);
    }
#line 3847 "build\\lexico.tab.cpp"
    break;

  case 102: /* simple_call_args: call_arg_simple  */
#line 1552 "src\\lexico.y"
                      {
        ((*yyvalp).exprlist) = ast_expr_list_new((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr), &call_list_len);
    }
#line 3855 "build\\lexico.tab.cpp"
    break;

  case 103: /* simple_call_args: simple_call_args ',' call_arg_simple  */
#line 1555 "src\\lexico.y"
                                           {
        ((*yyvalp).exprlist) = ast_expr_list_push((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.exprlist), &call_list_len, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3863 "build\\lexico.tab.cpp"
    break;

  case 104: /* call_arg_simple: IDENT  */
#line 1561 "src\\lexico.y"
                            { ((*yyvalp).expr) = ast_expr_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 3869 "build\\lexico.tab.cpp"
    break;

  case 105: /* call_arg_simple: literal  */
#line 1562 "src\\lexico.y"
                            { ((*yyvalp).expr) = ast_expr_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.lit)); }
#line 3875 "build\\lexico.tab.cpp"
    break;

  case 106: /* call_arg_simple: call_expr  */
#line 1563 "src\\lexico.y"
                            { ((*yyvalp).expr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr); }
#line 3881 "build\\lexico.tab.cpp"
    break;

  case 107: /* call_arg_simple: '(' expr ')'  */
#line 1564 "src\\lexico.y"
                            { ((*yyvalp).expr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.expr); }
#line 3887 "build\\lexico.tab.cpp"
    break;

  case 108: /* call_arg_simple: MINUS call_arg_simple  */
#line 1565 "src\\lexico.y"
                            { ((*yyvalp).expr) = ast_expr_neg((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 3893 "build\\lexico.tab.cpp"
    break;

  case 109: /* call_arg_simple: PLUS call_arg_simple  */
#line 1566 "src\\lexico.y"
                            { ((*yyvalp).expr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr); }
#line 3899 "build\\lexico.tab.cpp"
    break;

  case 110: /* call_args: expr  */
#line 1570 "src\\lexico.y"
                             {
        ((*yyvalp).exprlist) = ast_expr_list_new((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr), &call_list_len);
    }
#line 3907 "build\\lexico.tab.cpp"
    break;

  case 111: /* call_args: call_args ',' expr  */
#line 1573 "src\\lexico.y"
                         {
        ((*yyvalp).exprlist) = ast_expr_list_push((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.exprlist), &call_list_len, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 3915 "build\\lexico.tab.cpp"
    break;

  case 112: /* call_expr: OUTCOME OF IDENT opt_call_args  */
#line 1579 "src\\lexico.y"
                                     {
        ((*yyvalp).expr) = ast_expr_call((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.exprlist), call_list_len);
    }
#line 3923 "build\\lexico.tab.cpp"
    break;

  case 113: /* decl_stmt: CREATE IDENT IDENT USING IDENT  */
#line 1585 "src\\lexico.y"
                                     {
        Stmt *inst = class_instantiate(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval));
        if (!inst) YYERROR;
        ((*yyvalp).stmt) = inst;
    }
#line 3933 "build\\lexico.tab.cpp"
    break;

  case 114: /* decl_stmt: CREATE type IDENT SET VALUE expr  */
#line 1590 "src\\lexico.y"
                                       {
        char **names = static_cast<char **>(malloc(sizeof(char *)));
        Expr **vals  = static_cast<Expr **>(malloc(sizeof(Expr *)));
        if (!names || !vals) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        names[0] = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval);
        vals[0]  = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr);
        ((*yyvalp).stmt) = ast_new_decl(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.typekind), names, vals, 1);
    }
#line 3949 "build\\lexico.tab.cpp"
    break;

  case 115: /* decl_stmt: CREATE type id_list_multi SET VALUE val_list_multi  */
#line 1601 "src\\lexico.y"
                                                         {
        if (id_list_len != val_list_len) {
            fprintf(stderr,
                    "Error (line %d): %zu variable(s) but %zu value(s) â€“ "
                    "counts must match.\n",
                    yylineno, id_list_len, val_list_len);
            for (size_t i = 0; i < id_list_len; i++) free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.idlist)[i]);
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.idlist));
            for (size_t i = 0; i < val_list_len; i++) {
                ast_free_expr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.exprlist)[i]);
            }
            free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.exprlist));
            YYERROR;
        }
        ((*yyvalp).stmt) = ast_new_decl(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.typekind), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.idlist), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.exprlist), id_list_len);
    }
#line 3970 "build\\lexico.tab.cpp"
    break;

  case 116: /* decl_stmt: CREATE type IDENT  */
#line 1617 "src\\lexico.y"
                        {
        char **names = static_cast<char **>(malloc(sizeof(char *)));
        Expr **defs  = static_cast<Expr **>(malloc(sizeof(Expr *)));
        if (!names || !defs) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        names[0] = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval);
        defs[0] = default_init_expr((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.typekind));
        ((*yyvalp).stmt) = ast_new_decl(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.typekind), names, defs, 1);
    }
#line 3986 "build\\lexico.tab.cpp"
    break;

  case 117: /* decl_stmt: CREATE type id_list_multi  */
#line 1628 "src\\lexico.y"
                                {
        Expr **defs = default_init_list((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.typekind), id_list_len);
        ((*yyvalp).stmt) = ast_new_decl(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.typekind), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.idlist), defs, id_list_len);
    }
#line 3995 "build\\lexico.tab.cpp"
    break;

  case 118: /* decl_stmt: CREATE type IDENT SET_VALUE_TAKE_USER_INPUT opt_message  */
#line 1632 "src\\lexico.y"
                                                              {
        ((*yyvalp).stmt) = make_decl_then_input(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.typekind), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.exprlist), prompt_list_len);
    }
#line 4003 "build\\lexico.tab.cpp"
    break;

  case 119: /* $@1: %empty  */
#line 1638 "src\\lexico.y"
                                     {
                if (!class_begin((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.sval))) YYERROR;
            }
#line 4011 "build\\lexico.tab.cpp"
    break;

  case 120: /* class_stmt: DEFINE CLASS IDENT INCLUDE $@1 INDENT class_body DEDENT  */
#line 1640 "src\\lexico.y"
                                       {
                profile_end();
                g_current_class = NULL;
                ((*yyvalp).stmt) = ast_new_block(yylineno);
            }
#line 4021 "build\\lexico.tab.cpp"
    break;

  case 121: /* $@2: %empty  */
#line 1645 "src\\lexico.y"
                                                    {
                if (!class_begin_inherits((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.sval))) YYERROR;
            }
#line 4029 "build\\lexico.tab.cpp"
    break;

  case 122: /* class_stmt: DEFINE CLASS IDENT INHERITS IDENT INCLUDE $@2 INDENT class_body DEDENT  */
#line 1647 "src\\lexico.y"
                                       {
                profile_end();
                g_current_class = NULL;
                ((*yyvalp).stmt) = ast_new_block(yylineno);
            }
#line 4039 "build\\lexico.tab.cpp"
    break;

  case 125: /* class_member: CREATE type IDENT ';'  */
#line 1660 "src\\lexico.y"
                                {
                if (!class_add_field((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.typekind), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.sval))) YYERROR;
            }
#line 4047 "build\\lexico.tab.cpp"
    break;

  case 126: /* class_member: OVERRIDE CREATE type IDENT ';'  */
#line 1663 "src\\lexico.y"
                                         {
                if (!class_override_field((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.typekind), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.sval))) YYERROR;
            }
#line 4055 "build\\lexico.tab.cpp"
    break;

  case 127: /* $@3: %empty  */
#line 1666 "src\\lexico.y"
                           {
                if (!profile_begin((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.sval))) YYERROR;
            }
#line 4063 "build\\lexico.tab.cpp"
    break;

  case 128: /* class_member: PROFILE IDENT DO $@3 INDENT profile_body DEDENT  */
#line 1668 "src\\lexico.y"
                                         {
                profile_end();
            }
#line 4071 "build\\lexico.tab.cpp"
    break;

  case 129: /* class_member: ROUTINE IDENT opt_param_clause RETURNS type DO block  */
#line 1671 "src\\lexico.y"
                                                               {
                if (!class_add_routine((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.plist), param_list_len, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.typekind), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt))) YYERROR;
            }
#line 4079 "build\\lexico.tab.cpp"
    break;

  case 130: /* class_member: ROUTINE IDENT opt_param_clause DO block  */
#line 1674 "src\\lexico.y"
                                                  {
                if (!class_add_routine((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.plist), param_list_len, TYPE_INT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt))) YYERROR;
            }
#line 4087 "build\\lexico.tab.cpp"
    break;

  case 131: /* class_member: OVERRIDE ROUTINE IDENT opt_param_clause RETURNS type DO block  */
#line 1677 "src\\lexico.y"
                                                                        {
                if (!class_override_routine((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.plist), param_list_len, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.typekind), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt))) YYERROR;
            }
#line 4095 "build\\lexico.tab.cpp"
    break;

  case 132: /* class_member: OVERRIDE ROUTINE IDENT opt_param_clause DO block  */
#line 1680 "src\\lexico.y"
                                                           {
                if (!class_override_routine((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.plist), param_list_len, TYPE_INT, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt))) YYERROR;
            }
#line 4103 "build\\lexico.tab.cpp"
    break;

  case 135: /* profile_line: SET IDENT TO expr ';'  */
#line 1691 "src\\lexico.y"
                                {
                if (!profile_add_set((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.expr))) YYERROR;
            }
#line 4111 "build\\lexico.tab.cpp"
    break;

  case 136: /* profile_line: SET IDENT TO TAKE USER INPUT opt_message ';'  */
#line 1694 "src\\lexico.y"
                                                       {
                if (!profile_add_input((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.exprlist), prompt_list_len)) YYERROR;
            }
#line 4119 "build\\lexico.tab.cpp"
    break;

  case 137: /* opt_message: %empty  */
#line 1700 "src\\lexico.y"
                                     { ((*yyvalp).exprlist) = NULL; prompt_list_len = 0; }
#line 4125 "build\\lexico.tab.cpp"
    break;

  case 138: /* opt_message: WITH MESSAGE message_parts  */
#line 1701 "src\\lexico.y"
                                     { ((*yyvalp).exprlist) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.exprlist); }
#line 4131 "build\\lexico.tab.cpp"
    break;

  case 139: /* message_parts: expr  */
#line 1705 "src\\lexico.y"
                                   { ((*yyvalp).exprlist) = ast_expr_list_new((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr), &prompt_list_len); }
#line 4137 "build\\lexico.tab.cpp"
    break;

  case 140: /* message_parts: message_parts ',' expr  */
#line 1706 "src\\lexico.y"
                                   { ((*yyvalp).exprlist) = ast_expr_list_push((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.exprlist), &prompt_list_len, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 4143 "build\\lexico.tab.cpp"
    break;

  case 141: /* print_stmt: PRINT print_args  */
#line 1710 "src\\lexico.y"
                         {
        ((*yyvalp).stmt) = ast_new_print(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.exprlist), print_list_len);
    }
#line 4151 "build\\lexico.tab.cpp"
    break;

  case 142: /* if_stmt: IF condition THEN block  */
#line 1718 "src\\lexico.y"
                              {
        ((*yyvalp).stmt) = ast_new_if(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.condexpr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt), NULL);
    }
#line 4159 "build\\lexico.tab.cpp"
    break;

  case 143: /* if_stmt: IF condition THEN block else_part  */
#line 1721 "src\\lexico.y"
                                        {
        ((*yyvalp).stmt) = ast_new_if(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.condexpr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.stmt), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt));
    }
#line 4167 "build\\lexico.tab.cpp"
    break;

  case 144: /* else_part: OTHERWISE block  */
#line 1729 "src\\lexico.y"
                      {
        /* plain otherwise: else_block is a STMT_BLOCK */
        ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt);
    }
#line 4176 "build\\lexico.tab.cpp"
    break;

  case 145: /* else_part: OTHERWISE WHEN condition THEN block  */
#line 1733 "src\\lexico.y"
                                          {
        /* otherwise when: else_block is a STMT_IF (no further else) */
        ((*yyvalp).stmt) = ast_new_if(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.condexpr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt), NULL);
    }
#line 4185 "build\\lexico.tab.cpp"
    break;

  case 146: /* else_part: OTHERWISE WHEN condition THEN block else_part  */
#line 1737 "src\\lexico.y"
                                                    {
        /* otherwise when ... else_part: else_block is a STMT_IF with its own else */
        ((*yyvalp).stmt) = ast_new_if(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.condexpr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.stmt), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt));
    }
#line 4194 "build\\lexico.tab.cpp"
    break;

  case 147: /* block: INDENT block_body DEDENT  */
#line 1746 "src\\lexico.y"
                               {
        /* Convert the temporary Program into a STMT_BLOCK */
        Stmt *blk = ast_new_block(yylineno);
        for (size_t i = 0; i < (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.prog)->count; i++)
            ast_block_add(blk, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.prog)->stmts[i]);
        /* Free only the Program shell, not the stmts (now owned by block) */
        free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.prog)->stmts);
        free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.prog));
        ((*yyvalp).stmt) = blk;
    }
#line 4209 "build\\lexico.tab.cpp"
    break;

  case 148: /* block_body: stmt  */
#line 1759 "src\\lexico.y"
                        {
                            ((*yyvalp).prog) = ast_new_program();
                            ast_program_add(((*yyvalp).prog), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt));
                        }
#line 4218 "build\\lexico.tab.cpp"
    break;

  case 149: /* block_body: block_body stmt  */
#line 1763 "src\\lexico.y"
                        {
                            ast_program_add((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.prog), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt));
                            ((*yyvalp).prog) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.prog);
                        }
#line 4227 "build\\lexico.tab.cpp"
    break;

  case 150: /* while_stmt: WHILE condition DO block  */
#line 1772 "src\\lexico.y"
                               {
        ((*yyvalp).stmt) = ast_new_while(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.condexpr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt));
    }
#line 4235 "build\\lexico.tab.cpp"
    break;

  case 151: /* for_stmt: FOR IDENT FROM expr TO expr opt_step DO block  */
#line 1780 "src\\lexico.y"
                                                    {
        ((*yyvalp).stmt) = ast_new_for(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt));
    }
#line 4243 "build\\lexico.tab.cpp"
    break;

  case 152: /* for_stmt: FOR IDENT IN IDENT FROM expr TO expr opt_step DO block  */
#line 1783 "src\\lexico.y"
                                                             {
        ((*yyvalp).stmt) = ast_new_for_each(yylineno,
                              (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-9)].yystate.yysemantics.yyval.sval),
                              (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.expr),
                              (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.expr),
                              (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr),
                              (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-7)].yystate.yysemantics.yyval.sval),
                              (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt));
    }
#line 4257 "build\\lexico.tab.cpp"
    break;

  case 153: /* for_stmt: FOR IDENT IDENT IN IDENT IDENT FROM expr TO expr AND IDENT FROM expr TO expr DO block  */
#line 1792 "src\\lexico.y"
                                                                                            {
        if (strcmp((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-16)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-12)].yystate.yysemantics.yyval.sval)) != 0 || strcmp((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-15)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.sval)) != 0) {
            fprintf(stderr,
                    "Error (line %d): matrix loop index names must match header pair '%s %s'.\n",
                    yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-16)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-15)].yystate.yysemantics.yyval.sval));
            YYERROR;
        }
        free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-12)].yystate.yysemantics.yyval.sval));
        free((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-6)].yystate.yysemantics.yyval.sval));
        ((*yyvalp).stmt) = ast_new_for_matrix(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-16)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-15)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-13)].yystate.yysemantics.yyval.sval),
                                (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-10)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-8)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr),
                                (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt));
    }
#line 4275 "build\\lexico.tab.cpp"
    break;

  case 154: /* opt_step: %empty  */
#line 1808 "src\\lexico.y"
                    { ((*yyvalp).expr) = NULL; }
#line 4281 "build\\lexico.tab.cpp"
    break;

  case 155: /* opt_step: STEP expr  */
#line 1809 "src\\lexico.y"
                    { ((*yyvalp).expr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr); }
#line 4287 "build\\lexico.tab.cpp"
    break;

  case 156: /* repeat_stmt: REPEAT block UNTIL condition THEN  */
#line 1815 "src\\lexico.y"
                                        {
        ((*yyvalp).stmt) = ast_new_repeat(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.stmt), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.condexpr));
    }
#line 4295 "build\\lexico.tab.cpp"
    break;

  case 157: /* attempt_stmt: ATTEMPT UPTO expr WHILE condition DO block opt_onfailure  */
#line 1823 "src\\lexico.y"
                                                               {
        ((*yyvalp).stmt) = ast_new_attempt(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.condexpr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.stmt), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt));
    }
#line 4303 "build\\lexico.tab.cpp"
    break;

  case 158: /* attempt_stmt: ATTEMPT UPTO expr TIMES_WHILE condition DO block opt_onfailure  */
#line 1826 "src\\lexico.y"
                                                                     {
        ((*yyvalp).stmt) = ast_new_attempt(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-3)].yystate.yysemantics.yyval.condexpr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.stmt), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt));
    }
#line 4311 "build\\lexico.tab.cpp"
    break;

  case 159: /* opt_onfailure: %empty  */
#line 1832 "src\\lexico.y"
                  { ((*yyvalp).stmt) = NULL; }
#line 4317 "build\\lexico.tab.cpp"
    break;

  case 160: /* opt_onfailure: ONFAILURE block  */
#line 1833 "src\\lexico.y"
                      { ((*yyvalp).stmt) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.stmt); }
#line 4323 "build\\lexico.tab.cpp"
    break;

  case 161: /* condition: cond_or  */
#line 1839 "src\\lexico.y"
                        { ((*yyvalp).condexpr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.condexpr); }
#line 4329 "build\\lexico.tab.cpp"
    break;

  case 162: /* cond_or: cond_or OR cond_and  */
#line 1843 "src\\lexico.y"
                            { ((*yyvalp).condexpr) = ast_cond_or((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.condexpr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.condexpr)); }
#line 4335 "build\\lexico.tab.cpp"
    break;

  case 163: /* cond_or: cond_and  */
#line 1844 "src\\lexico.y"
                            { ((*yyvalp).condexpr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.condexpr); }
#line 4341 "build\\lexico.tab.cpp"
    break;

  case 164: /* cond_and: cond_and AND cond_not  */
#line 1848 "src\\lexico.y"
                            { ((*yyvalp).condexpr) = ast_cond_and((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.condexpr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.condexpr)); }
#line 4347 "build\\lexico.tab.cpp"
    break;

  case 165: /* cond_and: cond_not  */
#line 1849 "src\\lexico.y"
                            { ((*yyvalp).condexpr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.condexpr); }
#line 4353 "build\\lexico.tab.cpp"
    break;

  case 166: /* cond_not: NOT cond_atom  */
#line 1853 "src\\lexico.y"
                            { ((*yyvalp).condexpr) = ast_cond_not((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.condexpr)); }
#line 4359 "build\\lexico.tab.cpp"
    break;

  case 167: /* cond_not: cond_atom  */
#line 1854 "src\\lexico.y"
                            { ((*yyvalp).condexpr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.condexpr); }
#line 4365 "build\\lexico.tab.cpp"
    break;

  case 168: /* cond_atom: expr CMP_EQ expr  */
#line 1858 "src\\lexico.y"
                           { ((*yyvalp).condexpr) = ast_cond_cmp(CMP_EQ,  (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 4371 "build\\lexico.tab.cpp"
    break;

  case 169: /* cond_atom: expr CMP_NEQ expr  */
#line 1859 "src\\lexico.y"
                           { ((*yyvalp).condexpr) = ast_cond_cmp(CMP_NEQ, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 4377 "build\\lexico.tab.cpp"
    break;

  case 170: /* cond_atom: expr CMP_GT expr  */
#line 1860 "src\\lexico.y"
                           { ((*yyvalp).condexpr) = ast_cond_cmp(CMP_GT,  (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 4383 "build\\lexico.tab.cpp"
    break;

  case 171: /* cond_atom: expr CMP_LT expr  */
#line 1861 "src\\lexico.y"
                           { ((*yyvalp).condexpr) = ast_cond_cmp(CMP_LT,  (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 4389 "build\\lexico.tab.cpp"
    break;

  case 172: /* cond_atom: expr CMP_GTE expr  */
#line 1862 "src\\lexico.y"
                           { ((*yyvalp).condexpr) = ast_cond_cmp(CMP_GTE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 4395 "build\\lexico.tab.cpp"
    break;

  case 173: /* cond_atom: expr CMP_LTE expr  */
#line 1863 "src\\lexico.y"
                           { ((*yyvalp).condexpr) = ast_cond_cmp(CMP_LTE, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 4401 "build\\lexico.tab.cpp"
    break;

  case 174: /* cond_atom: '(' condition ')'  */
#line 1864 "src\\lexico.y"
                           { ((*yyvalp).condexpr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.condexpr); }
#line 4407 "build\\lexico.tab.cpp"
    break;

  case 175: /* type: T_INT  */
#line 1870 "src\\lexico.y"
                { ((*yyvalp).typekind) = TYPE_INT;    }
#line 4413 "build\\lexico.tab.cpp"
    break;

  case 176: /* type: T_FLOAT  */
#line 1871 "src\\lexico.y"
                { ((*yyvalp).typekind) = TYPE_FLOAT;  }
#line 4419 "build\\lexico.tab.cpp"
    break;

  case 177: /* type: T_CHAR  */
#line 1872 "src\\lexico.y"
                { ((*yyvalp).typekind) = TYPE_CHAR;   }
#line 4425 "build\\lexico.tab.cpp"
    break;

  case 178: /* type: T_STRING  */
#line 1873 "src\\lexico.y"
                { ((*yyvalp).typekind) = TYPE_STRING; }
#line 4431 "build\\lexico.tab.cpp"
    break;

  case 179: /* type: T_BOOL  */
#line 1874 "src\\lexico.y"
                { ((*yyvalp).typekind) = TYPE_BOOL;   }
#line 4437 "build\\lexico.tab.cpp"
    break;

  case 180: /* id_list_multi: IDENT ',' IDENT  */
#line 1878 "src\\lexico.y"
                           {
                                ((*yyvalp).idlist) = ast_ident_list_new((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.sval), &id_list_len);
                                ((*yyvalp).idlist) = ast_ident_list_push(((*yyvalp).idlist), &id_list_len, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval));
                            }
#line 4446 "build\\lexico.tab.cpp"
    break;

  case 181: /* id_list_multi: id_list_multi ',' IDENT  */
#line 1882 "src\\lexico.y"
                              {
                                ((*yyvalp).idlist) = ast_ident_list_push((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.idlist), &id_list_len, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval));
                            }
#line 4454 "build\\lexico.tab.cpp"
    break;

  case 182: /* val_list_multi: expr ',' expr  */
#line 1888 "src\\lexico.y"
                           {
                                ((*yyvalp).exprlist) = ast_expr_list_new((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), &val_list_len);
                                ((*yyvalp).exprlist) = ast_expr_list_push(((*yyvalp).exprlist), &val_list_len, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
                            }
#line 4463 "build\\lexico.tab.cpp"
    break;

  case 183: /* val_list_multi: val_list_multi ',' expr  */
#line 1892 "src\\lexico.y"
                              {
                                ((*yyvalp).exprlist) = ast_expr_list_push((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.exprlist), &val_list_len, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
                            }
#line 4471 "build\\lexico.tab.cpp"
    break;

  case 184: /* mesh_init_values: expr  */
#line 1898 "src\\lexico.y"
           {
        ((*yyvalp).exprlist) = ast_expr_list_new((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr), &mesh_init_list_len);
    }
#line 4479 "build\\lexico.tab.cpp"
    break;

  case 185: /* mesh_init_values: mesh_init_values ',' expr  */
#line 1901 "src\\lexico.y"
                                {
        ((*yyvalp).exprlist) = ast_expr_list_push((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.exprlist), &mesh_init_list_len, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 4487 "build\\lexico.tab.cpp"
    break;

  case 186: /* matrix_init_row: expr  */
#line 1907 "src\\lexico.y"
           {
        MatrixInit *m = matrix_init_new();
        matrix_init_push_value(m, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
        matrix_init_add_row(m, 1);
        ((*yyvalp).minit) = m;
    }
#line 4498 "build\\lexico.tab.cpp"
    break;

  case 187: /* matrix_init_row: matrix_init_row ',' expr  */
#line 1913 "src\\lexico.y"
                               {
        matrix_init_push_value((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.minit), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
        (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.minit)->row_sizes[(YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.minit)->row_count - 1]++;
        ((*yyvalp).minit) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.minit);
    }
#line 4508 "build\\lexico.tab.cpp"
    break;

  case 188: /* matrix_init_rows: matrix_init_row  */
#line 1921 "src\\lexico.y"
                      {
        ((*yyvalp).minit) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.minit);
    }
#line 4516 "build\\lexico.tab.cpp"
    break;

  case 189: /* matrix_init_rows: matrix_init_rows '/' matrix_init_row  */
#line 1924 "src\\lexico.y"
                                           {
        ((*yyvalp).minit) = matrix_init_merge((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.minit), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.minit));
    }
#line 4524 "build\\lexico.tab.cpp"
    break;

  case 190: /* print_args: expr  */
#line 1932 "src\\lexico.y"
                              {
                                  ((*yyvalp).exprlist) = ast_expr_list_new((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr), &print_list_len);
                              }
#line 4532 "build\\lexico.tab.cpp"
    break;

  case 191: /* print_args: print_args ',' expr  */
#line 1935 "src\\lexico.y"
                              {
                                  ((*yyvalp).exprlist) = ast_expr_list_push((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.exprlist), &print_list_len, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
                              }
#line 4540 "build\\lexico.tab.cpp"
    break;

  case 192: /* compound_stmt: IDENT PLUS expr  */
#line 1943 "src\\lexico.y"
                        {
        char *n = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.sval);
        ((*yyvalp).stmt) = ast_new_set(yylineno, n, ast_expr_binary(OP_ADD, ast_expr_ident(n), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)));
    }
#line 4549 "build\\lexico.tab.cpp"
    break;

  case 193: /* compound_stmt: IDENT MINUS expr  */
#line 1947 "src\\lexico.y"
                        {
        char *n = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.sval);
        ((*yyvalp).stmt) = ast_new_set(yylineno, n, ast_expr_binary(OP_SUB, ast_expr_ident(n), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)));
    }
#line 4558 "build\\lexico.tab.cpp"
    break;

  case 194: /* compound_stmt: IDENT TIMES expr  */
#line 1951 "src\\lexico.y"
                        {
        char *n = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.sval);
        ((*yyvalp).stmt) = ast_new_set(yylineno, n, ast_expr_binary(OP_MUL, ast_expr_ident(n), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)));
    }
#line 4567 "build\\lexico.tab.cpp"
    break;

  case 195: /* compound_stmt: IDENT DIV expr  */
#line 1955 "src\\lexico.y"
                        {
        char *n = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.sval);
        ((*yyvalp).stmt) = ast_new_set(yylineno, n, ast_expr_binary(OP_DIV, ast_expr_ident(n), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)));
    }
#line 4576 "build\\lexico.tab.cpp"
    break;

  case 196: /* compound_stmt: IDENT MOD expr  */
#line 1959 "src\\lexico.y"
                        {
        char *n = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.sval);
        ((*yyvalp).stmt) = ast_new_set(yylineno, n, ast_expr_binary(OP_MOD, ast_expr_ident(n), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)));
    }
#line 4585 "build\\lexico.tab.cpp"
    break;

  case 197: /* set_stmt: SET IDENT TO expr  */
#line 1968 "src\\lexico.y"
                            {
        ((*yyvalp).stmt) = ast_new_set(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 4593 "build\\lexico.tab.cpp"
    break;

  case 198: /* set_stmt: SET IDENT AT expr ',' expr  */
#line 1971 "src\\lexico.y"
                                 {
        ((*yyvalp).stmt) = ast_new_set_matrix_ctx(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr));
    }
#line 4601 "build\\lexico.tab.cpp"
    break;

  case 199: /* set_stmt: SET IDENT TO TAKE USER INPUT opt_message  */
#line 1974 "src\\lexico.y"
                                               {
        ((*yyvalp).stmt) = ast_new_input(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-5)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.exprlist), prompt_list_len);
    }
#line 4609 "build\\lexico.tab.cpp"
    break;

  case 200: /* convert_stmt: CONVERT IDENT TO type  */
#line 1980 "src\\lexico.y"
                            {
        ((*yyvalp).stmt) = ast_new_convert(yylineno, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.typekind));
    }
#line 4617 "build\\lexico.tab.cpp"
    break;

  case 201: /* expr: expr PLUS term  */
#line 1988 "src\\lexico.y"
                            { ((*yyvalp).expr) = ast_expr_binary(OP_ADD, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 4623 "build\\lexico.tab.cpp"
    break;

  case 202: /* expr: expr MINUS term  */
#line 1989 "src\\lexico.y"
                            { ((*yyvalp).expr) = ast_expr_binary(OP_SUB, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 4629 "build\\lexico.tab.cpp"
    break;

  case 203: /* expr: term  */
#line 1990 "src\\lexico.y"
                            { ((*yyvalp).expr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr); }
#line 4635 "build\\lexico.tab.cpp"
    break;

  case 204: /* term: term TIMES factor  */
#line 1994 "src\\lexico.y"
                            { ((*yyvalp).expr) = ast_expr_binary(OP_MUL, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 4641 "build\\lexico.tab.cpp"
    break;

  case 205: /* term: term DIV factor  */
#line 1995 "src\\lexico.y"
                            { ((*yyvalp).expr) = ast_expr_binary(OP_DIV, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 4647 "build\\lexico.tab.cpp"
    break;

  case 206: /* term: term MOD factor  */
#line 1996 "src\\lexico.y"
                            { ((*yyvalp).expr) = ast_expr_binary(OP_MOD, (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 4653 "build\\lexico.tab.cpp"
    break;

  case 207: /* term: factor  */
#line 1997 "src\\lexico.y"
                            { ((*yyvalp).expr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr); }
#line 4659 "build\\lexico.tab.cpp"
    break;

  case 208: /* factor: call_expr  */
#line 2001 "src\\lexico.y"
                            { ((*yyvalp).expr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr); }
#line 4665 "build\\lexico.tab.cpp"
    break;

  case 209: /* factor: CONVERT expr TO type  */
#line 2002 "src\\lexico.y"
                            { ((*yyvalp).expr) = ast_expr_convert((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.typekind)); }
#line 4671 "build\\lexico.tab.cpp"
    break;

  case 210: /* factor: IDENT AT factor ',' factor  */
#line 2003 "src\\lexico.y"
                                          { ((*yyvalp).expr) = ast_expr_matrix_index((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 4677 "build\\lexico.tab.cpp"
    break;

  case 211: /* factor: SIZE OF IDENT  */
#line 2004 "src\\lexico.y"
                            { ((*yyvalp).expr) = ast_expr_length((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 4683 "build\\lexico.tab.cpp"
    break;

  case 212: /* factor: ROW LENGTH OF IDENT  */
#line 2005 "src\\lexico.y"
                            { ((*yyvalp).expr) = ast_expr_row_length((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 4689 "build\\lexico.tab.cpp"
    break;

  case 213: /* factor: COLUMN LENGTH OF IDENT  */
#line 2006 "src\\lexico.y"
                             { ((*yyvalp).expr) = ast_expr_col_length((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 4695 "build\\lexico.tab.cpp"
    break;

  case 214: /* factor: LENGTH OF factor  */
#line 2007 "src\\lexico.y"
                            { ((*yyvalp).expr) = ast_expr_strlen((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 4701 "build\\lexico.tab.cpp"
    break;

  case 215: /* factor: EXTRACT expr FROM IDENT  */
#line 2008 "src\\lexico.y"
                              { ((*yyvalp).expr) = ast_expr_extract((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 4707 "build\\lexico.tab.cpp"
    break;

  case 216: /* factor: EXTRACT FROM expr TO expr IN IDENT  */
#line 2009 "src\\lexico.y"
                                         { ((*yyvalp).expr) = ast_expr_extract_range((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-4)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 4713 "build\\lexico.tab.cpp"
    break;

  case 217: /* factor: COUNT expr IN IDENT  */
#line 2010 "src\\lexico.y"
                            { ((*yyvalp).expr) = ast_expr_count((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 4719 "build\\lexico.tab.cpp"
    break;

  case 218: /* factor: CHECK expr IN IDENT  */
#line 2011 "src\\lexico.y"
                            { ((*yyvalp).expr) = ast_expr_check((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 4725 "build\\lexico.tab.cpp"
    break;

  case 219: /* factor: POSITION OF expr IN IDENT  */
#line 2012 "src\\lexico.y"
                                { ((*yyvalp).expr) = ast_expr_position((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.expr), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 4731 "build\\lexico.tab.cpp"
    break;

  case 220: /* factor: IDENT AT factor  */
#line 2013 "src\\lexico.y"
                                    { ((*yyvalp).expr) = ast_expr_index((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-2)].yystate.yysemantics.yyval.sval), (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 4737 "build\\lexico.tab.cpp"
    break;

  case 221: /* factor: IDENT  */
#line 2014 "src\\lexico.y"
                              { ((*yyvalp).expr) = ast_expr_ident((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 4743 "build\\lexico.tab.cpp"
    break;

  case 222: /* factor: literal  */
#line 2015 "src\\lexico.y"
                            { ((*yyvalp).expr) = ast_expr_literal((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.lit)); }
#line 4749 "build\\lexico.tab.cpp"
    break;

  case 223: /* factor: '(' expr ')'  */
#line 2016 "src\\lexico.y"
                            { ((*yyvalp).expr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (-1)].yystate.yysemantics.yyval.expr); }
#line 4755 "build\\lexico.tab.cpp"
    break;

  case 224: /* factor: MINUS factor  */
#line 2017 "src\\lexico.y"
                            { ((*yyvalp).expr) = ast_expr_neg((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr)); }
#line 4761 "build\\lexico.tab.cpp"
    break;

  case 225: /* factor: PLUS factor  */
#line 2018 "src\\lexico.y"
                            { ((*yyvalp).expr) = (YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.expr); }
#line 4767 "build\\lexico.tab.cpp"
    break;

  case 226: /* literal: INT_LIT  */
#line 2024 "src\\lexico.y"
                            { ((*yyvalp).lit) = ast_lit_int((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.ival));    }
#line 4773 "build\\lexico.tab.cpp"
    break;

  case 227: /* literal: FLOAT_LIT  */
#line 2025 "src\\lexico.y"
                            { ((*yyvalp).lit) = ast_lit_float((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.dval));  }
#line 4779 "build\\lexico.tab.cpp"
    break;

  case 228: /* literal: CHAR_LIT  */
#line 2026 "src\\lexico.y"
                            { ((*yyvalp).lit) = ast_lit_char((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.cval));   }
#line 4785 "build\\lexico.tab.cpp"
    break;

  case 229: /* literal: STRING_LIT  */
#line 2027 "src\\lexico.y"
                            { ((*yyvalp).lit) = ast_lit_string((YY_CAST (yyGLRStackItem const *, yyvsp)[YYFILL (0)].yystate.yysemantics.yyval.sval)); }
#line 4791 "build\\lexico.tab.cpp"
    break;

  case 230: /* literal: TRUE  */
#line 2028 "src\\lexico.y"
                            { ((*yyvalp).lit) = ast_lit_bool(1);    }
#line 4797 "build\\lexico.tab.cpp"
    break;

  case 231: /* literal: FALSE  */
#line 2029 "src\\lexico.y"
                            { ((*yyvalp).lit) = ast_lit_bool(0);    }
#line 4803 "build\\lexico.tab.cpp"
    break;


#line 4807 "build\\lexico.tab.cpp"

      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yylhsNonterm (yyrule), yyvalp, yylocp);

  return yyok;
# undef yyerrok
# undef YYABORT
# undef YYACCEPT
# undef YYNOMEM
# undef YYERROR
# undef YYBACKUP
# undef yyclearin
# undef YYRECOVERING
}


static void
yyuserMerge (int yyn, YYSTYPE* yy0, YYSTYPE* yy1)
{
  YY_USE (yy0);
  YY_USE (yy1);

  switch (yyn)
    {

      default: break;
    }
}

                              /* Bison grammar-table manipulation.  */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}

/** Number of symbols composing the right hand side of rule #RULE.  */
static inline int
yyrhsLength (yyRuleNum yyrule)
{
  return yyr2[yyrule];
}

static void
yydestroyGLRState (char const *yymsg, yyGLRState *yys)
{
  if (yys->yyresolved)
    yydestruct (yymsg, yy_accessing_symbol (yys->yylrState),
                &yys->yysemantics.yyval);
  else
    {
#if YYDEBUG
      if (yydebug)
        {
          if (yys->yysemantics.yyfirstVal)
            YY_FPRINTF ((stderr, "%s unresolved", yymsg));
          else
            YY_FPRINTF ((stderr, "%s incomplete", yymsg));
          YY_SYMBOL_PRINT ("", yy_accessing_symbol (yys->yylrState), YY_NULLPTR, &yys->yyloc);
        }
#endif

      if (yys->yysemantics.yyfirstVal)
        {
          yySemanticOption *yyoption = yys->yysemantics.yyfirstVal;
          yyGLRState *yyrh;
          int yyn;
          for (yyrh = yyoption->yystate, yyn = yyrhsLength (yyoption->yyrule);
               yyn > 0;
               yyrh = yyrh->yypred, yyn -= 1)
            yydestroyGLRState (yymsg, yyrh);
        }
    }
}

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

/** True iff LR state YYSTATE has only a default reduction (regardless
 *  of token).  */
static inline yybool
yyisDefaultedState (yy_state_t yystate)
{
  return yypact_value_is_default (yypact[yystate]);
}

/** The default reduction for YYSTATE, assuming it has one.  */
static inline yyRuleNum
yydefaultAction (yy_state_t yystate)
{
  return yydefact[yystate];
}

#define yytable_value_is_error(Yyn) \
  0

/** The action to take in YYSTATE on seeing YYTOKEN.
 *  Result R means
 *    R < 0:  Reduce on rule -R.
 *    R = 0:  Error.
 *    R > 0:  Shift to state R.
 *  Set *YYCONFLICTS to a pointer into yyconfl to a 0-terminated list
 *  of conflicting reductions.
 */
static inline int
yygetLRActions (yy_state_t yystate, yysymbol_kind_t yytoken, const short** yyconflicts)
{
  int yyindex = yypact[yystate] + yytoken;
  if (yytoken == YYSYMBOL_YYerror)
    {
      // This is the error token.
      *yyconflicts = yyconfl;
      return 0;
    }
  else if (yyisDefaultedState (yystate)
           || yyindex < 0 || YYLAST < yyindex || yycheck[yyindex] != yytoken)
    {
      *yyconflicts = yyconfl;
      return -yydefact[yystate];
    }
  else if (! yytable_value_is_error (yytable[yyindex]))
    {
      *yyconflicts = yyconfl + yyconflp[yyindex];
      return yytable[yyindex];
    }
  else
    {
      *yyconflicts = yyconfl + yyconflp[yyindex];
      return 0;
    }
}

/** Compute post-reduction state.
 * \param yystate   the current state
 * \param yysym     the nonterminal to push on the stack
 */
static inline yy_state_t
yyLRgotoState (yy_state_t yystate, yysymbol_kind_t yysym)
{
  int yyr = yypgoto[yysym - YYNTOKENS] + yystate;
  if (0 <= yyr && yyr <= YYLAST && yycheck[yyr] == yystate)
    return yytable[yyr];
  else
    return yydefgoto[yysym - YYNTOKENS];
}

static inline yybool
yyisShiftAction (int yyaction)
{
  return 0 < yyaction;
}

static inline yybool
yyisErrorAction (int yyaction)
{
  return yyaction == 0;
}

                                /* GLRStates */

/** Return a fresh GLRStackItem in YYSTACKP.  The item is an LR state
 *  if YYISSTATE, and otherwise a semantic option.  Callers should call
 *  YY_RESERVE_GLRSTACK afterwards to make sure there is sufficient
 *  headroom.  */

static inline yyGLRStackItem*
yynewGLRStackItem (yyGLRStack* yystackp, yybool yyisState)
{
  yyGLRStackItem* yynewItem = yystackp->yynextFree;
  yystackp->yyspaceLeft -= 1;
  yystackp->yynextFree += 1;
  yynewItem->yystate.yyisState = yyisState;
  return yynewItem;
}

/** Add a new semantic action that will execute the action for rule
 *  YYRULE on the semantic values in YYRHS to the list of
 *  alternative actions for YYSTATE.  Assumes that YYRHS comes from
 *  stack #YYK of *YYSTACKP. */
static void
yyaddDeferredAction (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yyGLRState* yystate,
                     yyGLRState* yyrhs, yyRuleNum yyrule)
{
  yySemanticOption* yynewOption =
    &yynewGLRStackItem (yystackp, yyfalse)->yyoption;
  YY_ASSERT (!yynewOption->yyisState);
  yynewOption->yystate = yyrhs;
  yynewOption->yyrule = yyrule;
  if (yystackp->yytops.yylookaheadNeeds[yyk])
    {
      yynewOption->yyrawchar = yychar;
      yynewOption->yyval = yylval;
    }
  else
    yynewOption->yyrawchar = YYEMPTY;
  yynewOption->yynext = yystate->yysemantics.yyfirstVal;
  yystate->yysemantics.yyfirstVal = yynewOption;

  YY_RESERVE_GLRSTACK (yystackp);
}

                                /* GLRStacks */

/** Initialize YYSET to a singleton set containing an empty stack.  */
static yybool
yyinitStateSet (yyGLRStateSet* yyset)
{
  yyset->yysize = 1;
  yyset->yycapacity = 16;
  yyset->yystates
    = YY_CAST (yyGLRState**,
               YYMALLOC (YY_CAST (YYSIZE_T, yyset->yycapacity)
                         * sizeof yyset->yystates[0]));
  if (! yyset->yystates)
    return yyfalse;
  yyset->yystates[0] = YY_NULLPTR;
  yyset->yylookaheadNeeds
    = YY_CAST (yybool*,
               YYMALLOC (YY_CAST (YYSIZE_T, yyset->yycapacity)
                         * sizeof yyset->yylookaheadNeeds[0]));
  if (! yyset->yylookaheadNeeds)
    {
      YYFREE (yyset->yystates);
      return yyfalse;
    }
  memset (yyset->yylookaheadNeeds,
          0,
          YY_CAST (YYSIZE_T, yyset->yycapacity) * sizeof yyset->yylookaheadNeeds[0]);
  return yytrue;
}

static void yyfreeStateSet (yyGLRStateSet* yyset)
{
  YYFREE (yyset->yystates);
  YYFREE (yyset->yylookaheadNeeds);
}

/** Initialize *YYSTACKP to a single empty stack, with total maximum
 *  capacity for all stacks of YYSIZE.  */
static yybool
yyinitGLRStack (yyGLRStack* yystackp, YYPTRDIFF_T yysize)
{
  yystackp->yyerrState = 0;
  yynerrs = 0;
  yystackp->yyspaceLeft = yysize;
  yystackp->yyitems
    = YY_CAST (yyGLRStackItem*,
               YYMALLOC (YY_CAST (YYSIZE_T, yysize)
                         * sizeof yystackp->yynextFree[0]));
  if (!yystackp->yyitems)
    return yyfalse;
  yystackp->yynextFree = yystackp->yyitems;
  yystackp->yysplitPoint = YY_NULLPTR;
  yystackp->yylastDeleted = YY_NULLPTR;
  return yyinitStateSet (&yystackp->yytops);
}


#if YYSTACKEXPANDABLE
# define YYRELOC(YYFROMITEMS, YYTOITEMS, YYX, YYTYPE)                   \
  &((YYTOITEMS)                                                         \
    - ((YYFROMITEMS) - YY_REINTERPRET_CAST (yyGLRStackItem*, (YYX))))->YYTYPE

/** If *YYSTACKP is expandable, extend it.  WARNING: Pointers into the
    stack from outside should be considered invalid after this call.
    We always expand when there are 1 or fewer items left AFTER an
    allocation, so that we can avoid having external pointers exist
    across an allocation.  */
static void
yyexpandGLRStack (yyGLRStack* yystackp)
{
  yyGLRStackItem* yynewItems;
  yyGLRStackItem* yyp0, *yyp1;
  YYPTRDIFF_T yynewSize;
  YYPTRDIFF_T yyn;
  YYPTRDIFF_T yysize = yystackp->yynextFree - yystackp->yyitems;
  if (YYMAXDEPTH - YYHEADROOM < yysize)
    yyMemoryExhausted (yystackp);
  yynewSize = 2*yysize;
  if (YYMAXDEPTH < yynewSize)
    yynewSize = YYMAXDEPTH;
  yynewItems
    = YY_CAST (yyGLRStackItem*,
               YYMALLOC (YY_CAST (YYSIZE_T, yynewSize)
                         * sizeof yynewItems[0]));
  if (! yynewItems)
    yyMemoryExhausted (yystackp);
  for (yyp0 = yystackp->yyitems, yyp1 = yynewItems, yyn = yysize;
       0 < yyn;
       yyn -= 1, yyp0 += 1, yyp1 += 1)
    {
      *yyp1 = *yyp0;
      if (*YY_REINTERPRET_CAST (yybool *, yyp0))
        {
          yyGLRState* yys0 = &yyp0->yystate;
          yyGLRState* yys1 = &yyp1->yystate;
          if (yys0->yypred != YY_NULLPTR)
            yys1->yypred =
              YYRELOC (yyp0, yyp1, yys0->yypred, yystate);
          if (! yys0->yyresolved && yys0->yysemantics.yyfirstVal != YY_NULLPTR)
            yys1->yysemantics.yyfirstVal =
              YYRELOC (yyp0, yyp1, yys0->yysemantics.yyfirstVal, yyoption);
        }
      else
        {
          yySemanticOption* yyv0 = &yyp0->yyoption;
          yySemanticOption* yyv1 = &yyp1->yyoption;
          if (yyv0->yystate != YY_NULLPTR)
            yyv1->yystate = YYRELOC (yyp0, yyp1, yyv0->yystate, yystate);
          if (yyv0->yynext != YY_NULLPTR)
            yyv1->yynext = YYRELOC (yyp0, yyp1, yyv0->yynext, yyoption);
        }
    }
  if (yystackp->yysplitPoint != YY_NULLPTR)
    yystackp->yysplitPoint = YYRELOC (yystackp->yyitems, yynewItems,
                                      yystackp->yysplitPoint, yystate);

  for (yyn = 0; yyn < yystackp->yytops.yysize; yyn += 1)
    if (yystackp->yytops.yystates[yyn] != YY_NULLPTR)
      yystackp->yytops.yystates[yyn] =
        YYRELOC (yystackp->yyitems, yynewItems,
                 yystackp->yytops.yystates[yyn], yystate);
  YYFREE (yystackp->yyitems);
  yystackp->yyitems = yynewItems;
  yystackp->yynextFree = yynewItems + yysize;
  yystackp->yyspaceLeft = yynewSize - yysize;
}
#endif

static void
yyfreeGLRStack (yyGLRStack* yystackp)
{
  YYFREE (yystackp->yyitems);
  yyfreeStateSet (&yystackp->yytops);
}

/** Assuming that YYS is a GLRState somewhere on *YYSTACKP, update the
 *  splitpoint of *YYSTACKP, if needed, so that it is at least as deep as
 *  YYS.  */
static inline void
yyupdateSplit (yyGLRStack* yystackp, yyGLRState* yys)
{
  if (yystackp->yysplitPoint != YY_NULLPTR && yystackp->yysplitPoint > yys)
    yystackp->yysplitPoint = yys;
}

/** Invalidate stack #YYK in *YYSTACKP.  */
static inline void
yymarkStackDeleted (yyGLRStack* yystackp, YYPTRDIFF_T yyk)
{
  if (yystackp->yytops.yystates[yyk] != YY_NULLPTR)
    yystackp->yylastDeleted = yystackp->yytops.yystates[yyk];
  yystackp->yytops.yystates[yyk] = YY_NULLPTR;
}

/** Undelete the last stack in *YYSTACKP that was marked as deleted.  Can
    only be done once after a deletion, and only when all other stacks have
    been deleted.  */
static void
yyundeleteLastStack (yyGLRStack* yystackp)
{
  if (yystackp->yylastDeleted == YY_NULLPTR || yystackp->yytops.yysize != 0)
    return;
  yystackp->yytops.yystates[0] = yystackp->yylastDeleted;
  yystackp->yytops.yysize = 1;
  YY_DPRINTF ((stderr, "Restoring last deleted stack as stack #0.\n"));
  yystackp->yylastDeleted = YY_NULLPTR;
}

static inline void
yyremoveDeletes (yyGLRStack* yystackp)
{
  YYPTRDIFF_T yyi, yyj;
  yyi = yyj = 0;
  while (yyj < yystackp->yytops.yysize)
    {
      if (yystackp->yytops.yystates[yyi] == YY_NULLPTR)
        {
          if (yyi == yyj)
            YY_DPRINTF ((stderr, "Removing dead stacks.\n"));
          yystackp->yytops.yysize -= 1;
        }
      else
        {
          yystackp->yytops.yystates[yyj] = yystackp->yytops.yystates[yyi];
          /* In the current implementation, it's unnecessary to copy
             yystackp->yytops.yylookaheadNeeds[yyi] since, after
             yyremoveDeletes returns, the parser immediately either enters
             deterministic operation or shifts a token.  However, it doesn't
             hurt, and the code might evolve to need it.  */
          yystackp->yytops.yylookaheadNeeds[yyj] =
            yystackp->yytops.yylookaheadNeeds[yyi];
          if (yyj != yyi)
            YY_DPRINTF ((stderr, "Rename stack %ld -> %ld.\n",
                        YY_CAST (long, yyi), YY_CAST (long, yyj)));
          yyj += 1;
        }
      yyi += 1;
    }
}

/** Shift to a new state on stack #YYK of *YYSTACKP, corresponding to LR
 * state YYLRSTATE, at input position YYPOSN, with (resolved) semantic
 * value *YYVALP and source location *YYLOCP.  */
static inline void
yyglrShift (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yy_state_t yylrState,
            YYPTRDIFF_T yyposn,
            YYSTYPE* yyvalp)
{
  yyGLRState* yynewState = &yynewGLRStackItem (yystackp, yytrue)->yystate;

  yynewState->yylrState = yylrState;
  yynewState->yyposn = yyposn;
  yynewState->yyresolved = yytrue;
  yynewState->yypred = yystackp->yytops.yystates[yyk];
  yynewState->yysemantics.yyval = *yyvalp;
  yystackp->yytops.yystates[yyk] = yynewState;

  YY_RESERVE_GLRSTACK (yystackp);
}

/** Shift stack #YYK of *YYSTACKP, to a new state corresponding to LR
 *  state YYLRSTATE, at input position YYPOSN, with the (unresolved)
 *  semantic value of YYRHS under the action for YYRULE.  */
static inline void
yyglrShiftDefer (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yy_state_t yylrState,
                 YYPTRDIFF_T yyposn, yyGLRState* yyrhs, yyRuleNum yyrule)
{
  yyGLRState* yynewState = &yynewGLRStackItem (yystackp, yytrue)->yystate;
  YY_ASSERT (yynewState->yyisState);

  yynewState->yylrState = yylrState;
  yynewState->yyposn = yyposn;
  yynewState->yyresolved = yyfalse;
  yynewState->yypred = yystackp->yytops.yystates[yyk];
  yynewState->yysemantics.yyfirstVal = YY_NULLPTR;
  yystackp->yytops.yystates[yyk] = yynewState;

  /* Invokes YY_RESERVE_GLRSTACK.  */
  yyaddDeferredAction (yystackp, yyk, yynewState, yyrhs, yyrule);
}

#if YYDEBUG

/*----------------------------------------------------------------------.
| Report that stack #YYK of *YYSTACKP is going to be reduced by YYRULE. |
`----------------------------------------------------------------------*/

static inline void
yy_reduce_print (yybool yynormal, yyGLRStackItem* yyvsp, YYPTRDIFF_T yyk,
                 yyRuleNum yyrule)
{
  int yynrhs = yyrhsLength (yyrule);
  int yyi;
  YY_FPRINTF ((stderr, "Reducing stack %ld by rule %d (line %d):\n",
               YY_CAST (long, yyk), yyrule - 1, yyrline[yyrule]));
  if (! yynormal)
    yyfillin (yyvsp, 1, -yynrhs);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YY_FPRINTF ((stderr, "   $%d = ", yyi + 1));
      yy_symbol_print (stderr,
                       yy_accessing_symbol (yyvsp[yyi - yynrhs + 1].yystate.yylrState),
                       &yyvsp[yyi - yynrhs + 1].yystate.yysemantics.yyval                       );
      if (!yyvsp[yyi - yynrhs + 1].yystate.yyresolved)
        YY_FPRINTF ((stderr, " (unresolved)"));
      YY_FPRINTF ((stderr, "\n"));
    }
}
#endif

/** Pop the symbols consumed by reduction #YYRULE from the top of stack
 *  #YYK of *YYSTACKP, and perform the appropriate semantic action on their
 *  semantic values.  Assumes that all ambiguities in semantic values
 *  have been previously resolved.  Set *YYVALP to the resulting value,
 *  and *YYLOCP to the computed location (if any).  Return value is as
 *  for userAction.  */
static inline YYRESULTTAG
yydoAction (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yyRuleNum yyrule,
            YYSTYPE* yyvalp)
{
  int yynrhs = yyrhsLength (yyrule);

  if (yystackp->yysplitPoint == YY_NULLPTR)
    {
      /* Standard special case: single stack.  */
      yyGLRStackItem* yyrhs
        = YY_REINTERPRET_CAST (yyGLRStackItem*, yystackp->yytops.yystates[yyk]);
      YY_ASSERT (yyk == 0);
      yystackp->yynextFree -= yynrhs;
      yystackp->yyspaceLeft += yynrhs;
      yystackp->yytops.yystates[0] = & yystackp->yynextFree[-1].yystate;
      return yyuserAction (yyrule, yynrhs, yyrhs, yystackp, yyk,
                           yyvalp);
    }
  else
    {
      yyGLRStackItem yyrhsVals[YYMAXRHS + YYMAXLEFT + 1];
      yyGLRState* yys = yyrhsVals[YYMAXRHS + YYMAXLEFT].yystate.yypred
        = yystackp->yytops.yystates[yyk];
      int yyi;
      for (yyi = 0; yyi < yynrhs; yyi += 1)
        {
          yys = yys->yypred;
          YY_ASSERT (yys);
        }
      yyupdateSplit (yystackp, yys);
      yystackp->yytops.yystates[yyk] = yys;
      return yyuserAction (yyrule, yynrhs, yyrhsVals + YYMAXRHS + YYMAXLEFT - 1,
                           yystackp, yyk, yyvalp);
    }
}

/** Pop items off stack #YYK of *YYSTACKP according to grammar rule YYRULE,
 *  and push back on the resulting nonterminal symbol.  Perform the
 *  semantic action associated with YYRULE and store its value with the
 *  newly pushed state, if YYFORCEEVAL or if *YYSTACKP is currently
 *  unambiguous.  Otherwise, store the deferred semantic action with
 *  the new state.  If the new state would have an identical input
 *  position, LR state, and predecessor to an existing state on the stack,
 *  it is identified with that existing state, eliminating stack #YYK from
 *  *YYSTACKP.  In this case, the semantic value is
 *  added to the options for the existing state's semantic value.
 */
static inline YYRESULTTAG
yyglrReduce (yyGLRStack* yystackp, YYPTRDIFF_T yyk, yyRuleNum yyrule,
             yybool yyforceEval)
{
  YYPTRDIFF_T yyposn = yystackp->yytops.yystates[yyk]->yyposn;

  if (yyforceEval || yystackp->yysplitPoint == YY_NULLPTR)
    {
      YYSTYPE yyval;

      YYRESULTTAG yyflag = yydoAction (yystackp, yyk, yyrule, &yyval);
      if (yyflag == yyerr && yystackp->yysplitPoint != YY_NULLPTR)
        YY_DPRINTF ((stderr,
                     "Parse on stack %ld rejected by rule %d (line %d).\n",
                     YY_CAST (long, yyk), yyrule - 1, yyrline[yyrule]));
      if (yyflag != yyok)
        return yyflag;
      yyglrShift (yystackp, yyk,
                  yyLRgotoState (yystackp->yytops.yystates[yyk]->yylrState,
                                 yylhsNonterm (yyrule)),
                  yyposn, &yyval);
    }
  else
    {
      YYPTRDIFF_T yyi;
      int yyn;
      yyGLRState* yys, *yys0 = yystackp->yytops.yystates[yyk];
      yy_state_t yynewLRState;

      for (yys = yystackp->yytops.yystates[yyk], yyn = yyrhsLength (yyrule);
           0 < yyn; yyn -= 1)
        {
          yys = yys->yypred;
          YY_ASSERT (yys);
        }
      yyupdateSplit (yystackp, yys);
      yynewLRState = yyLRgotoState (yys->yylrState, yylhsNonterm (yyrule));
      YY_DPRINTF ((stderr,
                   "Reduced stack %ld by rule %d (line %d); action deferred.  "
                   "Now in state %d.\n",
                   YY_CAST (long, yyk), yyrule - 1, yyrline[yyrule],
                   yynewLRState));
      for (yyi = 0; yyi < yystackp->yytops.yysize; yyi += 1)
        if (yyi != yyk && yystackp->yytops.yystates[yyi] != YY_NULLPTR)
          {
            yyGLRState *yysplit = yystackp->yysplitPoint;
            yyGLRState *yyp = yystackp->yytops.yystates[yyi];
            while (yyp != yys && yyp != yysplit && yyp->yyposn >= yyposn)
              {
                if (yyp->yylrState == yynewLRState && yyp->yypred == yys)
                  {
                    yyaddDeferredAction (yystackp, yyk, yyp, yys0, yyrule);
                    yymarkStackDeleted (yystackp, yyk);
                    YY_DPRINTF ((stderr, "Merging stack %ld into stack %ld.\n",
                                 YY_CAST (long, yyk), YY_CAST (long, yyi)));
                    return yyok;
                  }
                yyp = yyp->yypred;
              }
          }
      yystackp->yytops.yystates[yyk] = yys;
      yyglrShiftDefer (yystackp, yyk, yynewLRState, yyposn, yys0, yyrule);
    }
  return yyok;
}

static YYPTRDIFF_T
yysplitStack (yyGLRStack* yystackp, YYPTRDIFF_T yyk)
{
  if (yystackp->yysplitPoint == YY_NULLPTR)
    {
      YY_ASSERT (yyk == 0);
      yystackp->yysplitPoint = yystackp->yytops.yystates[yyk];
    }
  if (yystackp->yytops.yycapacity <= yystackp->yytops.yysize)
    {
      YYPTRDIFF_T state_size = YYSIZEOF (yystackp->yytops.yystates[0]);
      YYPTRDIFF_T half_max_capacity = YYSIZE_MAXIMUM / 2 / state_size;
      if (half_max_capacity < yystackp->yytops.yycapacity)
        yyMemoryExhausted (yystackp);
      yystackp->yytops.yycapacity *= 2;

      {
        yyGLRState** yynewStates
          = YY_CAST (yyGLRState**,
                     YYREALLOC (yystackp->yytops.yystates,
                                (YY_CAST (YYSIZE_T, yystackp->yytops.yycapacity)
                                 * sizeof yynewStates[0])));
        if (yynewStates == YY_NULLPTR)
          yyMemoryExhausted (yystackp);
        yystackp->yytops.yystates = yynewStates;
      }

      {
        yybool* yynewLookaheadNeeds
          = YY_CAST (yybool*,
                     YYREALLOC (yystackp->yytops.yylookaheadNeeds,
                                (YY_CAST (YYSIZE_T, yystackp->yytops.yycapacity)
                                 * sizeof yynewLookaheadNeeds[0])));
        if (yynewLookaheadNeeds == YY_NULLPTR)
          yyMemoryExhausted (yystackp);
        yystackp->yytops.yylookaheadNeeds = yynewLookaheadNeeds;
      }
    }
  yystackp->yytops.yystates[yystackp->yytops.yysize]
    = yystackp->yytops.yystates[yyk];
  yystackp->yytops.yylookaheadNeeds[yystackp->yytops.yysize]
    = yystackp->yytops.yylookaheadNeeds[yyk];
  yystackp->yytops.yysize += 1;
  return yystackp->yytops.yysize - 1;
}

/** True iff YYY0 and YYY1 represent identical options at the top level.
 *  That is, they represent the same rule applied to RHS symbols
 *  that produce the same terminal symbols.  */
static yybool
yyidenticalOptions (yySemanticOption* yyy0, yySemanticOption* yyy1)
{
  if (yyy0->yyrule == yyy1->yyrule)
    {
      yyGLRState *yys0, *yys1;
      int yyn;
      for (yys0 = yyy0->yystate, yys1 = yyy1->yystate,
           yyn = yyrhsLength (yyy0->yyrule);
           yyn > 0;
           yys0 = yys0->yypred, yys1 = yys1->yypred, yyn -= 1)
        if (yys0->yyposn != yys1->yyposn)
          return yyfalse;
      return yytrue;
    }
  else
    return yyfalse;
}

/** Assuming identicalOptions (YYY0,YYY1), destructively merge the
 *  alternative semantic values for the RHS-symbols of YYY1 and YYY0.  */
static void
yymergeOptionSets (yySemanticOption* yyy0, yySemanticOption* yyy1)
{
  yyGLRState *yys0, *yys1;
  int yyn;
  for (yys0 = yyy0->yystate, yys1 = yyy1->yystate,
       yyn = yyrhsLength (yyy0->yyrule);
       0 < yyn;
       yys0 = yys0->yypred, yys1 = yys1->yypred, yyn -= 1)
    {
      if (yys0 == yys1)
        break;
      else if (yys0->yyresolved)
        {
          yys1->yyresolved = yytrue;
          yys1->yysemantics.yyval = yys0->yysemantics.yyval;
        }
      else if (yys1->yyresolved)
        {
          yys0->yyresolved = yytrue;
          yys0->yysemantics.yyval = yys1->yysemantics.yyval;
        }
      else
        {
          yySemanticOption** yyz0p = &yys0->yysemantics.yyfirstVal;
          yySemanticOption* yyz1 = yys1->yysemantics.yyfirstVal;
          while (yytrue)
            {
              if (yyz1 == *yyz0p || yyz1 == YY_NULLPTR)
                break;
              else if (*yyz0p == YY_NULLPTR)
                {
                  *yyz0p = yyz1;
                  break;
                }
              else if (*yyz0p < yyz1)
                {
                  yySemanticOption* yyz = *yyz0p;
                  *yyz0p = yyz1;
                  yyz1 = yyz1->yynext;
                  (*yyz0p)->yynext = yyz;
                }
              yyz0p = &(*yyz0p)->yynext;
            }
          yys1->yysemantics.yyfirstVal = yys0->yysemantics.yyfirstVal;
        }
    }
}

/** Y0 and Y1 represent two possible actions to take in a given
 *  parsing state; return 0 if no combination is possible,
 *  1 if user-mergeable, 2 if Y0 is preferred, 3 if Y1 is preferred.  */
static int
yypreference (yySemanticOption* y0, yySemanticOption* y1)
{
  yyRuleNum r0 = y0->yyrule, r1 = y1->yyrule;
  int p0 = yydprec[r0], p1 = yydprec[r1];

  if (p0 == p1)
    {
      if (yymerger[r0] == 0 || yymerger[r0] != yymerger[r1])
        return 0;
      else
        return 1;
    }
  if (p0 == 0 || p1 == 0)
    return 0;
  if (p0 < p1)
    return 3;
  if (p1 < p0)
    return 2;
  return 0;
}

static YYRESULTTAG
yyresolveValue (yyGLRState* yys, yyGLRStack* yystackp);


/** Resolve the previous YYN states starting at and including state YYS
 *  on *YYSTACKP. If result != yyok, some states may have been left
 *  unresolved possibly with empty semantic option chains.  Regardless
 *  of whether result = yyok, each state has been left with consistent
 *  data so that yydestroyGLRState can be invoked if necessary.  */
static YYRESULTTAG
yyresolveStates (yyGLRState* yys, int yyn,
                 yyGLRStack* yystackp)
{
  if (0 < yyn)
    {
      YY_ASSERT (yys->yypred);
      YYCHK (yyresolveStates (yys->yypred, yyn-1, yystackp));
      if (! yys->yyresolved)
        YYCHK (yyresolveValue (yys, yystackp));
    }
  return yyok;
}

/** Resolve the states for the RHS of YYOPT on *YYSTACKP, perform its
 *  user action, and return the semantic value and location in *YYVALP
 *  and *YYLOCP.  Regardless of whether result = yyok, all RHS states
 *  have been destroyed (assuming the user action destroys all RHS
 *  semantic values if invoked).  */
static YYRESULTTAG
yyresolveAction (yySemanticOption* yyopt, yyGLRStack* yystackp,
                 YYSTYPE* yyvalp)
{
  yyGLRStackItem yyrhsVals[YYMAXRHS + YYMAXLEFT + 1];
  int yynrhs = yyrhsLength (yyopt->yyrule);
  YYRESULTTAG yyflag =
    yyresolveStates (yyopt->yystate, yynrhs, yystackp);
  if (yyflag != yyok)
    {
      yyGLRState *yys;
      for (yys = yyopt->yystate; yynrhs > 0; yys = yys->yypred, yynrhs -= 1)
        yydestroyGLRState ("Cleanup: popping", yys);
      return yyflag;
    }

  yyrhsVals[YYMAXRHS + YYMAXLEFT].yystate.yypred = yyopt->yystate;
  {
    int yychar_current = yychar;
    YYSTYPE yylval_current = yylval;
    yychar = yyopt->yyrawchar;
    yylval = yyopt->yyval;
    yyflag = yyuserAction (yyopt->yyrule, yynrhs,
                           yyrhsVals + YYMAXRHS + YYMAXLEFT - 1,
                           yystackp, -1, yyvalp);
    yychar = yychar_current;
    yylval = yylval_current;
  }
  return yyflag;
}

#if YYDEBUG
static void
yyreportTree (yySemanticOption* yyx, int yyindent)
{
  int yynrhs = yyrhsLength (yyx->yyrule);
  int yyi;
  yyGLRState* yys;
  yyGLRState* yystates[1 + YYMAXRHS];
  yyGLRState yyleftmost_state;

  for (yyi = yynrhs, yys = yyx->yystate; 0 < yyi; yyi -= 1, yys = yys->yypred)
    yystates[yyi] = yys;
  if (yys == YY_NULLPTR)
    {
      yyleftmost_state.yyposn = 0;
      yystates[0] = &yyleftmost_state;
    }
  else
    yystates[0] = yys;

  if (yyx->yystate->yyposn < yys->yyposn + 1)
    YY_FPRINTF ((stderr, "%*s%s -> <Rule %d, empty>\n",
                 yyindent, "", yysymbol_name (yylhsNonterm (yyx->yyrule)),
                 yyx->yyrule - 1));
  else
    YY_FPRINTF ((stderr, "%*s%s -> <Rule %d, tokens %ld .. %ld>\n",
                 yyindent, "", yysymbol_name (yylhsNonterm (yyx->yyrule)),
                 yyx->yyrule - 1, YY_CAST (long, yys->yyposn + 1),
                 YY_CAST (long, yyx->yystate->yyposn)));
  for (yyi = 1; yyi <= yynrhs; yyi += 1)
    {
      if (yystates[yyi]->yyresolved)
        {
          if (yystates[yyi-1]->yyposn+1 > yystates[yyi]->yyposn)
            YY_FPRINTF ((stderr, "%*s%s <empty>\n", yyindent+2, "",
                         yysymbol_name (yy_accessing_symbol (yystates[yyi]->yylrState))));
          else
            YY_FPRINTF ((stderr, "%*s%s <tokens %ld .. %ld>\n", yyindent+2, "",
                         yysymbol_name (yy_accessing_symbol (yystates[yyi]->yylrState)),
                         YY_CAST (long, yystates[yyi-1]->yyposn + 1),
                         YY_CAST (long, yystates[yyi]->yyposn)));
        }
      else
        yyreportTree (yystates[yyi]->yysemantics.yyfirstVal, yyindent+2);
    }
}
#endif

static YYRESULTTAG
yyreportAmbiguity (yySemanticOption* yyx0,
                   yySemanticOption* yyx1)
{
  YY_USE (yyx0);
  YY_USE (yyx1);

#if YYDEBUG
  YY_FPRINTF ((stderr, "Ambiguity detected.\n"));
  YY_FPRINTF ((stderr, "Option 1,\n"));
  yyreportTree (yyx0, 2);
  YY_FPRINTF ((stderr, "\nOption 2,\n"));
  yyreportTree (yyx1, 2);
  YY_FPRINTF ((stderr, "\n"));
#endif

  yyerror (YY_("syntax is ambiguous"));
  return yyabort;
}

/** Resolve the ambiguity represented in state YYS in *YYSTACKP,
 *  perform the indicated actions, and set the semantic value of YYS.
 *  If result != yyok, the chain of semantic options in YYS has been
 *  cleared instead or it has been left unmodified except that
 *  redundant options may have been removed.  Regardless of whether
 *  result = yyok, YYS has been left with consistent data so that
 *  yydestroyGLRState can be invoked if necessary.  */
static YYRESULTTAG
yyresolveValue (yyGLRState* yys, yyGLRStack* yystackp)
{
  yySemanticOption* yyoptionList = yys->yysemantics.yyfirstVal;
  yySemanticOption* yybest = yyoptionList;
  yySemanticOption** yypp;
  yybool yymerge = yyfalse;
  YYSTYPE yyval;
  YYRESULTTAG yyflag;

  for (yypp = &yyoptionList->yynext; *yypp != YY_NULLPTR; )
    {
      yySemanticOption* yyp = *yypp;

      if (yyidenticalOptions (yybest, yyp))
        {
          yymergeOptionSets (yybest, yyp);
          *yypp = yyp->yynext;
        }
      else
        {
          switch (yypreference (yybest, yyp))
            {
            case 0:
              return yyreportAmbiguity (yybest, yyp);
              break;
            case 1:
              yymerge = yytrue;
              break;
            case 2:
              break;
            case 3:
              yybest = yyp;
              yymerge = yyfalse;
              break;
            default:
              /* This cannot happen so it is not worth a YY_ASSERT (yyfalse),
                 but some compilers complain if the default case is
                 omitted.  */
              break;
            }
          yypp = &yyp->yynext;
        }
    }

  if (yymerge)
    {
      yySemanticOption* yyp;
      int yyprec = yydprec[yybest->yyrule];
      yyflag = yyresolveAction (yybest, yystackp, &yyval);
      if (yyflag == yyok)
        for (yyp = yybest->yynext; yyp != YY_NULLPTR; yyp = yyp->yynext)
          {
            if (yyprec == yydprec[yyp->yyrule])
              {
                YYSTYPE yyval_other;
                yyflag = yyresolveAction (yyp, yystackp, &yyval_other);
                if (yyflag != yyok)
                  {
                    yydestruct ("Cleanup: discarding incompletely merged value for",
                                yy_accessing_symbol (yys->yylrState),
                                &yyval);
                    break;
                  }
                yyuserMerge (yymerger[yyp->yyrule], &yyval, &yyval_other);
              }
          }
    }
  else
    yyflag = yyresolveAction (yybest, yystackp, &yyval);

  if (yyflag == yyok)
    {
      yys->yyresolved = yytrue;
      yys->yysemantics.yyval = yyval;
    }
  else
    yys->yysemantics.yyfirstVal = YY_NULLPTR;
  return yyflag;
}

static YYRESULTTAG
yyresolveStack (yyGLRStack* yystackp)
{
  if (yystackp->yysplitPoint != YY_NULLPTR)
    {
      yyGLRState* yys;
      int yyn;

      for (yyn = 0, yys = yystackp->yytops.yystates[0];
           yys != yystackp->yysplitPoint;
           yys = yys->yypred, yyn += 1)
        continue;
      YYCHK (yyresolveStates (yystackp->yytops.yystates[0], yyn, yystackp
                             ));
    }
  return yyok;
}

/** Called when returning to deterministic operation to clean up the extra
 * stacks. */
static void
yycompressStack (yyGLRStack* yystackp)
{
  /* yyr is the state after the split point.  */
  yyGLRState *yyr;

  if (yystackp->yytops.yysize != 1 || yystackp->yysplitPoint == YY_NULLPTR)
    return;

  {
    yyGLRState *yyp, *yyq;
    for (yyp = yystackp->yytops.yystates[0], yyq = yyp->yypred, yyr = YY_NULLPTR;
         yyp != yystackp->yysplitPoint;
         yyr = yyp, yyp = yyq, yyq = yyp->yypred)
      yyp->yypred = yyr;
  }

  yystackp->yyspaceLeft += yystackp->yynextFree - yystackp->yyitems;
  yystackp->yynextFree = YY_REINTERPRET_CAST (yyGLRStackItem*, yystackp->yysplitPoint) + 1;
  yystackp->yyspaceLeft -= yystackp->yynextFree - yystackp->yyitems;
  yystackp->yysplitPoint = YY_NULLPTR;
  yystackp->yylastDeleted = YY_NULLPTR;

  while (yyr != YY_NULLPTR)
    {
      yystackp->yynextFree->yystate = *yyr;
      yyr = yyr->yypred;
      yystackp->yynextFree->yystate.yypred = &yystackp->yynextFree[-1].yystate;
      yystackp->yytops.yystates[0] = &yystackp->yynextFree->yystate;
      yystackp->yynextFree += 1;
      yystackp->yyspaceLeft -= 1;
    }
}

static YYRESULTTAG
yyprocessOneStack (yyGLRStack* yystackp, YYPTRDIFF_T yyk,
                   YYPTRDIFF_T yyposn)
{
  while (yystackp->yytops.yystates[yyk] != YY_NULLPTR)
    {
      yy_state_t yystate = yystackp->yytops.yystates[yyk]->yylrState;
      YY_DPRINTF ((stderr, "Stack %ld Entering state %d\n",
                   YY_CAST (long, yyk), yystate));

      YY_ASSERT (yystate != YYFINAL);

      if (yyisDefaultedState (yystate))
        {
          YYRESULTTAG yyflag;
          yyRuleNum yyrule = yydefaultAction (yystate);
          if (yyrule == 0)
            {
              YY_DPRINTF ((stderr, "Stack %ld dies.\n", YY_CAST (long, yyk)));
              yymarkStackDeleted (yystackp, yyk);
              return yyok;
            }
          yyflag = yyglrReduce (yystackp, yyk, yyrule, yyimmediate[yyrule]);
          if (yyflag == yyerr)
            {
              YY_DPRINTF ((stderr,
                           "Stack %ld dies "
                           "(predicate failure or explicit user error).\n",
                           YY_CAST (long, yyk)));
              yymarkStackDeleted (yystackp, yyk);
              return yyok;
            }
          if (yyflag != yyok)
            return yyflag;
        }
      else
        {
          yysymbol_kind_t yytoken = yygetToken (&yychar);
          const short* yyconflicts;
          const int yyaction = yygetLRActions (yystate, yytoken, &yyconflicts);
          yystackp->yytops.yylookaheadNeeds[yyk] = yytrue;

          for (/* nothing */; *yyconflicts; yyconflicts += 1)
            {
              YYRESULTTAG yyflag;
              YYPTRDIFF_T yynewStack = yysplitStack (yystackp, yyk);
              YY_DPRINTF ((stderr, "Splitting off stack %ld from %ld.\n",
                           YY_CAST (long, yynewStack), YY_CAST (long, yyk)));
              yyflag = yyglrReduce (yystackp, yynewStack,
                                    *yyconflicts,
                                    yyimmediate[*yyconflicts]);
              if (yyflag == yyok)
                YYCHK (yyprocessOneStack (yystackp, yynewStack,
                                          yyposn));
              else if (yyflag == yyerr)
                {
                  YY_DPRINTF ((stderr, "Stack %ld dies.\n", YY_CAST (long, yynewStack)));
                  yymarkStackDeleted (yystackp, yynewStack);
                }
              else
                return yyflag;
            }

          if (yyisShiftAction (yyaction))
            break;
          else if (yyisErrorAction (yyaction))
            {
              YY_DPRINTF ((stderr, "Stack %ld dies.\n", YY_CAST (long, yyk)));
              yymarkStackDeleted (yystackp, yyk);
              break;
            }
          else
            {
              YYRESULTTAG yyflag = yyglrReduce (yystackp, yyk, -yyaction,
                                                yyimmediate[-yyaction]);
              if (yyflag == yyerr)
                {
                  YY_DPRINTF ((stderr,
                               "Stack %ld dies "
                               "(predicate failure or explicit user error).\n",
                               YY_CAST (long, yyk)));
                  yymarkStackDeleted (yystackp, yyk);
                  break;
                }
              else if (yyflag != yyok)
                return yyflag;
            }
        }
    }
  return yyok;
}

/* Put in YYARG at most YYARGN of the expected tokens given the
   current YYSTACKP, and return the number of tokens stored in YYARG.  If
   YYARG is null, return the number of expected tokens (guaranteed to
   be less than YYNTOKENS).  */
static int
yypcontext_expected_tokens (const yyGLRStack* yystackp,
                            yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  int yyn = yypact[yystackp->yytops.yystates[0]->yylrState];
  if (!yypact_value_is_default (yyn))
    {
      /* Start YYX at -YYN if negative to avoid negative indexes in
         YYCHECK.  In other words, skip the first -YYN actions for
         this state because they are default actions.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;
      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yyx;
      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
        if (yycheck[yyx + yyn] == yyx && yyx != YYSYMBOL_YYerror
            && !yytable_value_is_error (yytable[yyx + yyn]))
          {
            if (!yyarg)
              ++yycount;
            else if (yycount == yyargn)
              return 0;
            else
              yyarg[yycount++] = YY_CAST (yysymbol_kind_t, yyx);
          }
    }
  if (yyarg && yycount == 0 && 0 < yyargn)
    yyarg[0] = YYSYMBOL_YYEMPTY;
  return yycount;
}

static int
yy_syntax_error_arguments (const yyGLRStack* yystackp,
                           yysymbol_kind_t yyarg[], int yyargn)
{
  yysymbol_kind_t yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* Actual size of YYARG. */
  int yycount = 0;
  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYSYMBOL_YYEMPTY)
    {
      int yyn;
      if (yyarg)
        yyarg[yycount] = yytoken;
      ++yycount;
      yyn = yypcontext_expected_tokens (yystackp,
                                        yyarg ? yyarg + 1 : yyarg, yyargn - 1);
      if (yyn == YYENOMEM)
        return YYENOMEM;
      else
        yycount += yyn;
    }
  return yycount;
}



static void
yyreportSyntaxError (yyGLRStack* yystackp)
{
  if (yystackp->yyerrState != 0)
    return;
  {
  yybool yysize_overflow = yyfalse;
  char* yymsg = YY_NULLPTR;
  enum { YYARGS_MAX = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  yysymbol_kind_t yyarg[YYARGS_MAX];
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* Actual size of YYARG. */
  int yycount
    = yy_syntax_error_arguments (yystackp, yyarg, YYARGS_MAX);
  if (yycount == YYENOMEM)
    yyMemoryExhausted (yystackp);

  switch (yycount)
    {
#define YYCASE_(N, S)                   \
      case N:                           \
        yyformat = S;                   \
      break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
    }

  /* Compute error message size.  Don't count the "%s"s, but reserve
     room for the terminator.  */
  yysize = yystrlen (yyformat) - 2 * yycount + 1;
  {
    int yyi;
    for (yyi = 0; yyi < yycount; ++yyi)
      {
        YYPTRDIFF_T yysz
          = yytnamerr (YY_NULLPTR, yytname[yyarg[yyi]]);
        if (YYSIZE_MAXIMUM - yysize < yysz)
          yysize_overflow = yytrue;
        else
          yysize += yysz;
      }
  }

  if (!yysize_overflow)
    yymsg = YY_CAST (char *, YYMALLOC (YY_CAST (YYSIZE_T, yysize)));

  if (yymsg)
    {
      char *yyp = yymsg;
      int yyi = 0;
      while ((*yyp = *yyformat))
        {
          if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
            {
              yyp += yytnamerr (yyp, yytname[yyarg[yyi++]]);
              yyformat += 2;
            }
          else
            {
              ++yyp;
              ++yyformat;
            }
        }
      yyerror (yymsg);
      YYFREE (yymsg);
    }
  else
    {
      yyerror (YY_("syntax error"));
      yyMemoryExhausted (yystackp);
    }
  }
  yynerrs += 1;
}

/* Recover from a syntax error on *YYSTACKP, assuming that *YYSTACKP->YYTOKENP,
   yylval, and yylloc are the syntactic category, semantic value, and location
   of the lookahead.  */
static void
yyrecoverSyntaxError (yyGLRStack* yystackp)
{
  if (yystackp->yyerrState == 3)
    /* We just shifted the error token and (perhaps) took some
       reductions.  Skip tokens until we can proceed.  */
    while (yytrue)
      {
        yysymbol_kind_t yytoken;
        int yyj;
        if (yychar == YYEOF)
          yyFail (yystackp, YY_NULLPTR);
        if (yychar != YYEMPTY)
          {
            yytoken = YYTRANSLATE (yychar);
            yydestruct ("Error: discarding",
                        yytoken, &yylval);
            yychar = YYEMPTY;
          }
        yytoken = yygetToken (&yychar);
        yyj = yypact[yystackp->yytops.yystates[0]->yylrState];
        if (yypact_value_is_default (yyj))
          return;
        yyj += yytoken;
        if (yyj < 0 || YYLAST < yyj || yycheck[yyj] != yytoken)
          {
            if (yydefact[yystackp->yytops.yystates[0]->yylrState] != 0)
              return;
          }
        else if (! yytable_value_is_error (yytable[yyj]))
          return;
      }

  /* Reduce to one stack.  */
  {
    YYPTRDIFF_T yyk;
    for (yyk = 0; yyk < yystackp->yytops.yysize; yyk += 1)
      if (yystackp->yytops.yystates[yyk] != YY_NULLPTR)
        break;
    if (yyk >= yystackp->yytops.yysize)
      yyFail (yystackp, YY_NULLPTR);
    for (yyk += 1; yyk < yystackp->yytops.yysize; yyk += 1)
      yymarkStackDeleted (yystackp, yyk);
    yyremoveDeletes (yystackp);
    yycompressStack (yystackp);
  }

  /* Pop stack until we find a state that shifts the error token.  */
  yystackp->yyerrState = 3;
  while (yystackp->yytops.yystates[0] != YY_NULLPTR)
    {
      yyGLRState *yys = yystackp->yytops.yystates[0];
      int yyj = yypact[yys->yylrState];
      if (! yypact_value_is_default (yyj))
        {
          yyj += YYSYMBOL_YYerror;
          if (0 <= yyj && yyj <= YYLAST && yycheck[yyj] == YYSYMBOL_YYerror
              && yyisShiftAction (yytable[yyj]))
            {
              /* Shift the error token.  */
              int yyaction = yytable[yyj];
              YY_SYMBOL_PRINT ("Shifting", yy_accessing_symbol (yyaction),
                               &yylval, &yyerrloc);
              yyglrShift (yystackp, 0, yyaction,
                          yys->yyposn, &yylval);
              yys = yystackp->yytops.yystates[0];
              break;
            }
        }
      if (yys->yypred != YY_NULLPTR)
        yydestroyGLRState ("Error: popping", yys);
      yystackp->yytops.yystates[0] = yys->yypred;
      yystackp->yynextFree -= 1;
      yystackp->yyspaceLeft += 1;
    }
  if (yystackp->yytops.yystates[0] == YY_NULLPTR)
    yyFail (yystackp, YY_NULLPTR);
}

#define YYCHK1(YYE)                             \
  do {                                          \
    switch (YYE) {                              \
    case yyok:     break;                       \
    case yyabort:  goto yyabortlab;             \
    case yyaccept: goto yyacceptlab;            \
    case yyerr:    goto yyuser_error;           \
    case yynomem:  goto yyexhaustedlab;         \
    default:       goto yybuglab;               \
    }                                           \
  } while (0)

/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
  int yyresult;
  yyGLRStack yystack;
  yyGLRStack* const yystackp = &yystack;
  YYPTRDIFF_T yyposn;

  YY_DPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY;
  yylval = yyval_default;

  if (! yyinitGLRStack (yystackp, YYINITDEPTH))
    goto yyexhaustedlab;
  switch (YYSETJMP (yystack.yyexception_buffer))
    {
    case 0: break;
    case 1: goto yyabortlab;
    case 2: goto yyexhaustedlab;
    default: goto yybuglab;
    }
  yyglrShift (&yystack, 0, 0, 0, &yylval);
  yyposn = 0;

  while (yytrue)
    {
      /* For efficiency, we have two loops, the first of which is
         specialized to deterministic operation (single stack, no
         potential ambiguity).  */
      /* Standard mode. */
      while (yytrue)
        {
          yy_state_t yystate = yystack.yytops.yystates[0]->yylrState;
          YY_DPRINTF ((stderr, "Entering state %d\n", yystate));
          if (yystate == YYFINAL)
            goto yyacceptlab;
          if (yyisDefaultedState (yystate))
            {
              yyRuleNum yyrule = yydefaultAction (yystate);
              if (yyrule == 0)
                {
                  yyreportSyntaxError (&yystack);
                  goto yyuser_error;
                }
              YYCHK1 (yyglrReduce (&yystack, 0, yyrule, yytrue));
            }
          else
            {
              yysymbol_kind_t yytoken = yygetToken (&yychar);
              const short* yyconflicts;
              int yyaction = yygetLRActions (yystate, yytoken, &yyconflicts);
              if (*yyconflicts)
                /* Enter nondeterministic mode.  */
                break;
              if (yyisShiftAction (yyaction))
                {
                  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
                  yychar = YYEMPTY;
                  yyposn += 1;
                  yyglrShift (&yystack, 0, yyaction, yyposn, &yylval);
                  if (0 < yystack.yyerrState)
                    yystack.yyerrState -= 1;
                }
              else if (yyisErrorAction (yyaction))
                {
                  /* Issue an error message unless the scanner already
                     did. */
                  if (yychar != YYerror)
                    yyreportSyntaxError (&yystack);
                  goto yyuser_error;
                }
              else
                YYCHK1 (yyglrReduce (&yystack, 0, -yyaction, yytrue));
            }
        }

      /* Nondeterministic mode. */
      while (yytrue)
        {
          yysymbol_kind_t yytoken_to_shift;
          YYPTRDIFF_T yys;

          for (yys = 0; yys < yystack.yytops.yysize; yys += 1)
            yystackp->yytops.yylookaheadNeeds[yys] = yychar != YYEMPTY;

          /* yyprocessOneStack returns one of three things:

              - An error flag.  If the caller is yyprocessOneStack, it
                immediately returns as well.  When the caller is finally
                yyparse, it jumps to an error label via YYCHK1.

              - yyok, but yyprocessOneStack has invoked yymarkStackDeleted
                (&yystack, yys), which sets the top state of yys to NULL.  Thus,
                yyparse's following invocation of yyremoveDeletes will remove
                the stack.

              - yyok, when ready to shift a token.

             Except in the first case, yyparse will invoke yyremoveDeletes and
             then shift the next token onto all remaining stacks.  This
             synchronization of the shift (that is, after all preceding
             reductions on all stacks) helps prevent double destructor calls
             on yylval in the event of memory exhaustion.  */

          for (yys = 0; yys < yystack.yytops.yysize; yys += 1)
            YYCHK1 (yyprocessOneStack (&yystack, yys, yyposn));
          yyremoveDeletes (&yystack);
          if (yystack.yytops.yysize == 0)
            {
              yyundeleteLastStack (&yystack);
              if (yystack.yytops.yysize == 0)
                yyFail (&yystack, YY_("syntax error"));
              YYCHK1 (yyresolveStack (&yystack));
              YY_DPRINTF ((stderr, "Returning to deterministic operation.\n"));
              yyreportSyntaxError (&yystack);
              goto yyuser_error;
            }

          /* If any yyglrShift call fails, it will fail after shifting.  Thus,
             a copy of yylval will already be on stack 0 in the event of a
             failure in the following loop.  Thus, yychar is set to YYEMPTY
             before the loop to make sure the user destructor for yylval isn't
             called twice.  */
          yytoken_to_shift = YYTRANSLATE (yychar);
          yychar = YYEMPTY;
          yyposn += 1;
          for (yys = 0; yys < yystack.yytops.yysize; yys += 1)
            {
              yy_state_t yystate = yystack.yytops.yystates[yys]->yylrState;
              const short* yyconflicts;
              int yyaction = yygetLRActions (yystate, yytoken_to_shift,
                              &yyconflicts);
              /* Note that yyconflicts were handled by yyprocessOneStack.  */
              YY_DPRINTF ((stderr, "On stack %ld, ", YY_CAST (long, yys)));
              YY_SYMBOL_PRINT ("shifting", yytoken_to_shift, &yylval, &yylloc);
              yyglrShift (&yystack, yys, yyaction, yyposn,
                          &yylval);
              YY_DPRINTF ((stderr, "Stack %ld now in state %d\n",
                           YY_CAST (long, yys),
                           yystack.yytops.yystates[yys]->yylrState));
            }

          if (yystack.yytops.yysize == 1)
            {
              YYCHK1 (yyresolveStack (&yystack));
              YY_DPRINTF ((stderr, "Returning to deterministic operation.\n"));
              yycompressStack (&yystack);
              break;
            }
        }
      continue;
    yyuser_error:
      yyrecoverSyntaxError (&yystack);
      yyposn = yystack.yytops.yystates[0]->yyposn;
    }

 yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;

 yybuglab:
  YY_ASSERT (yyfalse);
  goto yyabortlab;

 yyabortlab:
  yyresult = 1;
  goto yyreturnlab;

 yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;

 yyreturnlab:
  if (yychar != YYEMPTY)
    yydestruct ("Cleanup: discarding lookahead",
                YYTRANSLATE (yychar), &yylval);

  /* If the stack is well-formed, pop the stack until it is empty,
     destroying its entries as we go.  But free the stack regardless
     of whether it is well-formed.  */
  if (yystack.yyitems)
    {
      yyGLRState** yystates = yystack.yytops.yystates;
      if (yystates)
        {
          YYPTRDIFF_T yysize = yystack.yytops.yysize;
          YYPTRDIFF_T yyk;
          for (yyk = 0; yyk < yysize; yyk += 1)
            if (yystates[yyk])
              {
                while (yystates[yyk])
                  {
                    yyGLRState *yys = yystates[yyk];
                    if (yys->yypred != YY_NULLPTR)
                      yydestroyGLRState ("Cleanup: popping", yys);
                    yystates[yyk] = yys->yypred;
                    yystack.yynextFree -= 1;
                    yystack.yyspaceLeft += 1;
                  }
                break;
              }
        }
      yyfreeGLRStack (&yystack);
    }

  return yyresult;
}

/* DEBUGGING ONLY */
#if YYDEBUG
/* Print *YYS and its predecessors. */
static void
yy_yypstack (yyGLRState* yys)
{
  if (yys->yypred)
    {
      yy_yypstack (yys->yypred);
      YY_FPRINTF ((stderr, " -> "));
    }
  YY_FPRINTF ((stderr, "%d@%ld", yys->yylrState, YY_CAST (long, yys->yyposn)));
}

/* Print YYS (possibly NULL) and its predecessors. */
static void
yypstates (yyGLRState* yys)
{
  if (yys == YY_NULLPTR)
    YY_FPRINTF ((stderr, "<null>"));
  else
    yy_yypstack (yys);
  YY_FPRINTF ((stderr, "\n"));
}

/* Print the stack #YYK.  */
static void
yypstack (yyGLRStack* yystackp, YYPTRDIFF_T yyk)
{
  yypstates (yystackp->yytops.yystates[yyk]);
}

/* Print all the stacks.  */
static void
yypdumpstack (yyGLRStack* yystackp)
{
#define YYINDEX(YYX)                                                    \
  YY_CAST (long,                                                        \
           ((YYX)                                                       \
            ? YY_REINTERPRET_CAST (yyGLRStackItem*, (YYX)) - yystackp->yyitems \
            : -1))

  yyGLRStackItem* yyp;
  for (yyp = yystackp->yyitems; yyp < yystackp->yynextFree; yyp += 1)
    {
      YY_FPRINTF ((stderr, "%3ld. ",
                   YY_CAST (long, yyp - yystackp->yyitems)));
      if (*YY_REINTERPRET_CAST (yybool *, yyp))
        {
          YY_ASSERT (yyp->yystate.yyisState);
          YY_ASSERT (yyp->yyoption.yyisState);
          YY_FPRINTF ((stderr, "Res: %d, LR State: %d, posn: %ld, pred: %ld",
                       yyp->yystate.yyresolved, yyp->yystate.yylrState,
                       YY_CAST (long, yyp->yystate.yyposn),
                       YYINDEX (yyp->yystate.yypred)));
          if (! yyp->yystate.yyresolved)
            YY_FPRINTF ((stderr, ", firstVal: %ld",
                         YYINDEX (yyp->yystate.yysemantics.yyfirstVal)));
        }
      else
        {
          YY_ASSERT (!yyp->yystate.yyisState);
          YY_ASSERT (!yyp->yyoption.yyisState);
          YY_FPRINTF ((stderr, "Option. rule: %d, state: %ld, next: %ld",
                       yyp->yyoption.yyrule - 1,
                       YYINDEX (yyp->yyoption.yystate),
                       YYINDEX (yyp->yyoption.yynext)));
        }
      YY_FPRINTF ((stderr, "\n"));
    }

  YY_FPRINTF ((stderr, "Tops:"));
  {
    YYPTRDIFF_T yyi;
    for (yyi = 0; yyi < yystackp->yytops.yysize; yyi += 1)
      YY_FPRINTF ((stderr, "%ld: %ld; ", YY_CAST (long, yyi),
                   YYINDEX (yystackp->yytops.yystates[yyi])));
    YY_FPRINTF ((stderr, "\n"));
  }
#undef YYINDEX
}
#endif

#undef yylval
#undef yychar
#undef yynerrs




#line 2032 "src\\lexico.y"


void yyerror(const char *msg) {
    snprintf(lexico_last_parse_error, sizeof(lexico_last_parse_error),
             "Parse error (line %d): %s", yylineno, msg ? msg : "syntax error");
    fprintf(stderr, "Parse error (line %d): %s\n", yylineno, msg);
}
