/*
 * lexico.y â€“ Bison parser for the Lexico language.
 *
 * Grammar (current features):
 *   program      â†’ stmt_list | Îµ
 *   stmt_list    â†’ stmt_list stmt | stmt
 *   stmt         â†’ decl_stmt ';' | print_stmt ';' | set_stmt ';' | if_stmt
 *   decl_stmt    â†’ CREATE type id_list SET VALUE val_list
 *   print_stmt   â†’ PRINT parg_list
 *   set_stmt     â†’ SET IDENT TO expr
 *   if_stmt      â†’ IF condition THEN block
 *                 | IF condition THEN block OTHERWISE block
 *   block        â†’ INDENT block_body DEDENT
 *   block_body   â†’ block_body stmt | stmt
 *   condition    â†’ cond_or
 *   cond_or      â†’ cond_or OR cond_and | cond_and
 *   cond_and     â†’ cond_and AND cond_not | cond_not
 *   cond_not     â†’ NOT cond_atom | cond_atom
 *   cond_atom    â†’ expr CMP_xx expr | '(' condition ')'
 *   expr/term/factor  (arithmetic, as before)
 */

/* GLR parsing: needed so that "attempt up to N times while" resolves
   correctly despite TIMES also being the multiplication operator. */
%glr-parser

%code requires {
typedef struct MatrixInit MatrixInit;
}

%{
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
%}

/* â”€â”€ Token value types â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

%union {
    int64_t    ival;
    double     dval;
    char       cval;
    char      *sval;       /* heap-allocated */

    TypeKind   typekind;
    Literal    lit;
    Expr      *expr;       /* expression tree */
    CondExpr  *condexpr;   /* condition tree */

    char     **idlist;
    Expr     **exprlist;
    MatrixInit *minit;
    Param     *plist;

    Stmt      *stmt;       /* also used for blocks */
    Program   *prog;
}

/* â”€â”€ Terminal tokens â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

%token CREATE DEFINE CLASS INCLUDE INHERITS PROFILE USING ROUTINE OVERRIDE RETURNS SET VALUE VALUES CONVERT PRINT APPEND SORT ASCENDANTLY DESCENDANTLY COUNT CHECK POSITION EXTRACT TO ASK TAKE USER INPUT WITH MESSAGE
%token SET_VALUE_TAKE_USER_INPUT
%token OPEN MAKE FILE_T WRITE READ LINE LINES CLEAR BEGIN_T CLOSE TITLE INTO
%token ARRAY TABLE MESH MATRIX ROWS COLS SIZE LENGTH SIZED AT EACH INDEX
%token ROW COLUMN
%token EMIT GIVEBACK OUTCOME OF ARGS IN
%token PLUS MINUS TIMES DIV MOD
%token IF THEN OTHERWISE WHEN
%token AND OR NOT
%token CMP_EQ CMP_NEQ CMP_GT CMP_LT CMP_GTE CMP_LTE
%token INDENT DEDENT
%token T_INT T_FLOAT T_CHAR T_STRING T_BOOL TRUE FALSE
%token WHILE DO FOR FROM STEP REPEAT UNTIL ATTEMPT UPTO ONFAILURE TIMES_WHILE

%token <ival>  INT_LIT
%token <dval>  FLOAT_LIT
%token <cval>  CHAR_LIT
%token <sval>  STRING_LIT
%token <sval>  IDENT

%precedence PREC_LOWEST
%left AT
%left ','
%left ')'

/* â”€â”€ Non-terminal types â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

%type <typekind>  type
%type <lit>       literal
%type <expr>      expr term factor
%type <expr>      append_value append_term append_factor
%type <condexpr>  condition cond_or cond_and cond_not cond_atom

%type <idlist>    id_list_multi
%type <exprlist>  val_list_multi mesh_init_values print_args call_args simple_call_args opt_call_args
%type <minit>     matrix_init_row matrix_init_rows
%type <plist>     param_list opt_param_clause
%type <expr>      call_arg_simple

%type <stmt>      stmt decl_stmt print_stmt set_stmt convert_stmt if_stmt block else_part
%type <stmt>      while_stmt for_stmt repeat_stmt attempt_stmt compound_stmt class_stmt
%type <stmt>      function_stmt emit_stmt giveback_stmt
%type <stmt>      collection_decl_stmt collection_set_stmt append_stmt sort_stmt
%type <stmt>      file_open_stmt file_stmt
%type <stmt>      opt_onfailure
%type <exprlist>  opt_message message_parts
%type <expr>      opt_step
%type <prog>      program stmt_list block_body
%type <expr>      call_expr

/* Ask Bison for better error messages */
%define parse.error verbose

%%

/* â”€â”€ Program â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

program
    : /* empty */       { lexico_parsed_program = ast_new_program(); }
    | stmt_list         { lexico_parsed_program = $1; }
    ;

stmt_list
    : stmt              {
                            $$ = ast_new_program();
                            ast_program_add($$, $1);
                        }
    | stmt_list stmt    {
                            ast_program_add($1, $2);
                            $$ = $1;
                        }
    ;

/* â”€â”€ Statements â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

stmt
    : decl_stmt    ';'  { $$ = $1; }
    | collection_decl_stmt ';' { $$ = $1; }
    | collection_set_stmt ';' %dprec 1 { $$ = $1; }
    | append_stmt ';'          { $$ = $1; }
    | sort_stmt ';'            { $$ = $1; }
    | print_stmt   ';'  { $$ = $1; }
    | set_stmt     ';' %dprec 2 { $$ = $1; }
    | convert_stmt ';'  { $$ = $1; }
    | emit_stmt    ';'  { $$ = $1; }
    | giveback_stmt ';' { $$ = $1; }
    | compound_stmt ';' { $$ = $1; }   /* shortcut: a plus 3; */
    | function_stmt     { $$ = $1; }
    | class_stmt        { $$ = $1; }
    | file_open_stmt    { $$ = $1; }
    | file_stmt    ';'  { $$ = $1; }
    | if_stmt           { $$ = $1; }
    | while_stmt        { $$ = $1; }
    | for_stmt          { $$ = $1; }
    | repeat_stmt       { $$ = $1; }
    | attempt_stmt      { $$ = $1; }
    ;

file_open_stmt
    : OPEN expr FILE_T DO block {
        $$ = ast_new_file_open(yylineno, $2, 0, $5);
    }
    | OPEN expr FILE_T MAKE DO block {
        $$ = ast_new_file_open(yylineno, $2, 1, $6);
    }
    ;

file_stmt
    : WRITE expr IN FILE_T {
        $$ = ast_new_file_write(yylineno, $2, NULL);
    }
    | WRITE expr IN LINE expr IN FILE_T {
        $$ = ast_new_file_write(yylineno, $2, $5);
    }
    | READ FILE_T INTO IDENT {
        $$ = ast_new_file_read(yylineno, FILE_READ_ALL, $4, NULL, NULL, NULL);
    }
    | READ LINE expr IN FILE_T INTO IDENT {
        $$ = ast_new_file_read(yylineno, FILE_READ_LINE, $7, NULL, NULL, $3);
    }
    | READ expr LINES IN FILE_T INTO IDENT {
        $$ = ast_new_file_read(yylineno, FILE_READ_FIRST, $7, $2, NULL, NULL);
    }
    | READ expr LINES FROM LINE expr IN FILE_T INTO IDENT {
        $$ = ast_new_file_read(yylineno, FILE_READ_RANGE, $10, $2, $6, NULL);
    }
    | CLEAR FILE_T {
        $$ = ast_new_file_clear(yylineno, FILE_CLEAR_ALL, NULL, NULL, NULL);
    }
    | CLEAR LINE expr IN FILE_T {
        $$ = ast_new_file_clear(yylineno, FILE_CLEAR_LINE, $3, NULL, NULL);
    }
    | CLEAR FROM LINE expr TO LINE expr IN FILE_T {
        $$ = ast_new_file_clear(yylineno, FILE_CLEAR_RANGE, NULL, $4, $7);
    }
    | SET FILE_T TITLE TO expr {
        $$ = ast_new_file_set_title(yylineno, $5);
    }
    | CLOSE FILE_T {
        $$ = ast_new_file_close(yylineno);
    }
    ;

collection_decl_stmt
    : CREATE ARRAY OF type IDENT WITH SIZE expr {
        $$ = ast_new_array_decl(yylineno, $4, $5, $8);
    }
    | CREATE TABLE IDENT WITH SIZE expr {
        $$ = ast_new_table_decl(yylineno, $3, $6);
    }
    | CREATE type MESH IDENT SIZED expr {
        $$ = ast_new_array_decl(yylineno, $2, $4, $6);
    }
    | CREATE type MESH IDENT SET VALUES mesh_init_values {
        Stmt *blk = ast_new_block(yylineno);
        size_t name_len = strlen($4);
        ast_block_add(blk,
                      ast_new_array_decl(yylineno,
                                         $2,
                                         $4,
                                         ast_expr_literal(ast_lit_int((int64_t)mesh_init_list_len))));

        for (size_t i = 0; i < mesh_init_list_len; i++) {
            char *target = static_cast<char *>(malloc(name_len + 1));
            if (!target) {
                fprintf(stderr, "Fatal: out of memory.\n");
                exit(EXIT_FAILURE);
            }
            memcpy(target, $4, name_len + 1);
            ast_block_add(blk,
                          ast_new_collection_set(yylineno,
                                                 target,
                                                 ast_expr_literal(ast_lit_int((int64_t)i)),
                                                 $7[i]));
        }
        free($7);
        $$ = blk;
    }
    | CREATE MESH IDENT SIZED expr {
        $$ = ast_new_table_decl(yylineno, $3, $5);
    }
    | CREATE MESH IDENT SET VALUES mesh_init_values {
        Stmt *blk = ast_new_block(yylineno);
        size_t name_len = strlen($3);
        ast_block_add(blk,
                      ast_new_table_decl(yylineno,
                                         $3,
                                         ast_expr_literal(ast_lit_int((int64_t)mesh_init_list_len))));

        for (size_t i = 0; i < mesh_init_list_len; i++) {
            char *target = static_cast<char *>(malloc(name_len + 1));
            if (!target) {
                fprintf(stderr, "Fatal: out of memory.\n");
                exit(EXIT_FAILURE);
            }
            memcpy(target, $3, name_len + 1);
            ast_block_add(blk,
                          ast_new_collection_set(yylineno,
                                                 target,
                                                 ast_expr_literal(ast_lit_int((int64_t)i)),
                                                 $6[i]));
        }
        free($6);
        $$ = blk;
    }
    | CREATE type MATRIX MESH IDENT ROWS expr COLS expr {
        $$ = ast_new_matrix_decl(yylineno, $2, 0, $5, $7, $9);
    }
    | CREATE type MATRIX MESH IDENT SET VALUES matrix_init_rows {
        MatrixInit *mi = $8;
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
                                          $2,
                                          0,
                                          $5,
                                          ast_expr_literal(ast_lit_int((int64_t)mi->row_count)),
                                          ast_expr_literal(ast_lit_int((int64_t)cols))));

        size_t idx = 0;
        size_t name_len = strlen($5);
        for (size_t r = 0; r < mi->row_count; r++) {
            for (size_t c = 0; c < cols; c++) {
                char *target = static_cast<char *>(malloc(name_len + 1));
                if (!target) {
                    fprintf(stderr, "Fatal: out of memory.\n");
                    exit(EXIT_FAILURE);
                }
                memcpy(target, $5, name_len + 1);
                ast_block_add(blk,
                              ast_new_matrix_set(yylineno,
                                                 target,
                                                 ast_expr_literal(ast_lit_int((int64_t)r)),
                                                 ast_expr_literal(ast_lit_int((int64_t)c)),
                                                 mi->values[idx++]));
            }
        }

        matrix_init_free(mi, 0);
        $$ = blk;
    }
    | CREATE type MATRIX MESH IDENT SET VALUE matrix_init_rows {
        MatrixInit *mi = $8;
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
                                          $2,
                                          0,
                                          $5,
                                          ast_expr_literal(ast_lit_int((int64_t)mi->row_count)),
                                          ast_expr_literal(ast_lit_int((int64_t)cols))));

        size_t idx = 0;
        size_t name_len = strlen($5);
        for (size_t r = 0; r < mi->row_count; r++) {
            for (size_t c = 0; c < cols; c++) {
                char *target = static_cast<char *>(malloc(name_len + 1));
                if (!target) {
                    fprintf(stderr, "Fatal: out of memory.\n");
                    exit(EXIT_FAILURE);
                }
                memcpy(target, $5, name_len + 1);
                ast_block_add(blk,
                              ast_new_matrix_set(yylineno,
                                                 target,
                                                 ast_expr_literal(ast_lit_int((int64_t)r)),
                                                 ast_expr_literal(ast_lit_int((int64_t)c)),
                                                 mi->values[idx++]));
            }
        }

        matrix_init_free(mi, 0);
        $$ = blk;
    }
    | CREATE MATRIX MESH IDENT ROWS expr COLS expr {
        $$ = ast_new_matrix_decl(yylineno, TYPE_INT, 1, $4, $6, $8);
    }
    | CREATE MATRIX MESH IDENT SET VALUES matrix_init_rows {
        MatrixInit *mi = $7;
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
                                          $4,
                                          ast_expr_literal(ast_lit_int((int64_t)mi->row_count)),
                                          ast_expr_literal(ast_lit_int((int64_t)cols))));

        size_t idx = 0;
        size_t name_len = strlen($4);
        for (size_t r = 0; r < mi->row_count; r++) {
            for (size_t c = 0; c < cols; c++) {
                char *target = static_cast<char *>(malloc(name_len + 1));
                if (!target) {
                    fprintf(stderr, "Fatal: out of memory.\n");
                    exit(EXIT_FAILURE);
                }
                memcpy(target, $4, name_len + 1);
                ast_block_add(blk,
                              ast_new_matrix_set(yylineno,
                                                 target,
                                                 ast_expr_literal(ast_lit_int((int64_t)r)),
                                                 ast_expr_literal(ast_lit_int((int64_t)c)),
                                                 mi->values[idx++]));
            }
        }

        matrix_init_free(mi, 0);
        $$ = blk;
    }
    | CREATE MATRIX MESH IDENT SET VALUE matrix_init_rows {
        MatrixInit *mi = $7;
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
                                          $4,
                                          ast_expr_literal(ast_lit_int((int64_t)mi->row_count)),
                                          ast_expr_literal(ast_lit_int((int64_t)cols))));

        size_t idx = 0;
        size_t name_len = strlen($4);
        for (size_t r = 0; r < mi->row_count; r++) {
            for (size_t c = 0; c < cols; c++) {
                char *target = static_cast<char *>(malloc(name_len + 1));
                if (!target) {
                    fprintf(stderr, "Fatal: out of memory.\n");
                    exit(EXIT_FAILURE);
                }
                memcpy(target, $4, name_len + 1);
                ast_block_add(blk,
                              ast_new_matrix_set(yylineno,
                                                 target,
                                                 ast_expr_literal(ast_lit_int((int64_t)r)),
                                                 ast_expr_literal(ast_lit_int((int64_t)c)),
                                                 mi->values[idx++]));
            }
        }

        matrix_init_free(mi, 0);
        $$ = blk;
    }
    | CREATE type MATRIX IDENT ROWS expr COLS expr {
        /* Backward-compatible form. */
        $$ = ast_new_matrix_decl(yylineno, $2, 0, $4, $6, $8);
    }
    ;

collection_set_stmt
    : SET IDENT AT expr TO expr {
        $$ = ast_new_collection_set(yylineno, $2, $4, $6);
    }
    | SET IDENT VALUE AT expr TO expr {
        $$ = ast_new_collection_set(yylineno, $2, $5, $7);
    }
    | SET IDENT AT expr ',' expr TO expr {
        $$ = ast_new_matrix_set(yylineno, $2, $4, $6, $8);
    }
    | SET IDENT VALUE AT expr ',' expr TO expr {
        $$ = ast_new_matrix_set(yylineno, $2, $5, $7, $9);
    }
    | SET expr TO IDENT AT expr ',' expr {
        $$ = ast_new_matrix_set(yylineno, $4, $6, $8, $2);
    }
    | SET expr AT expr ',' expr {
        $$ = ast_new_matrix_set(yylineno, NULL, $4, $6, $2);
    }
    | SET IDENT AT expr TO TAKE USER INPUT opt_message {
        $$ = ast_new_collection_set_input(yylineno, $2, $4, $9, prompt_list_len);
    }
    | SET IDENT VALUE AT expr TO TAKE USER INPUT opt_message {
        $$ = ast_new_collection_set_input(yylineno, $2, $5, $10, prompt_list_len);
    }
    | SET VALUE AT expr TO expr {
        $$ = ast_new_collection_set(yylineno, NULL, $4, $6);
    }
    | SET VALUE TO expr {
        $$ = ast_new_collection_set(yylineno, NULL, NULL, $4);
    }
    | SET VALUE AT expr TO TAKE USER INPUT opt_message {
        $$ = ast_new_collection_set_input(yylineno, NULL, $4, $9, prompt_list_len);
    }
    | SET VALUE TO TAKE USER INPUT opt_message {
        $$ = ast_new_collection_set_input(yylineno, NULL, NULL, $7, prompt_list_len);
    }
    ;

append_stmt
    : APPEND append_value TO IDENT {
        $$ = ast_new_collection_append(yylineno, $4, $2, NULL);
    }
    | APPEND append_value TO IDENT AT expr {
        $$ = ast_new_collection_append(yylineno, $4, $2, $6);
    }
    | APPEND append_value {
        $$ = ast_new_collection_append(yylineno, NULL, $2, NULL);
    }
    | APPEND append_value AT expr {
        $$ = ast_new_collection_append(yylineno, NULL, $2, $4);
    }
    ;

sort_stmt
    : SORT IDENT ASCENDANTLY {
        $$ = ast_new_collection_sort(yylineno, $2, 1);
    }
    | SORT IDENT DESCENDANTLY {
        $$ = ast_new_collection_sort(yylineno, $2, 0);
    }
    ;

append_value
    : append_value PLUS append_term   { $$ = ast_expr_binary(OP_ADD, $1, $3); }
    | append_value MINUS append_term  { $$ = ast_expr_binary(OP_SUB, $1, $3); }
    | append_term                     { $$ = $1; }
    ;

append_term
    : append_term TIMES append_factor { $$ = ast_expr_binary(OP_MUL, $1, $3); }
    | append_term DIV   append_factor { $$ = ast_expr_binary(OP_DIV, $1, $3); }
    | append_term MOD   append_factor { $$ = ast_expr_binary(OP_MOD, $1, $3); }
    | append_factor                   { $$ = $1; }
    ;

append_factor
    : call_expr                      { $$ = $1; }
    | SIZE OF IDENT                  { $$ = ast_expr_length($3); }
    | ROW LENGTH OF IDENT            { $$ = ast_expr_row_length($4); }
    | COLUMN LENGTH OF IDENT         { $$ = ast_expr_col_length($4); }
    | LENGTH OF append_factor        { $$ = ast_expr_strlen($3); }
    | EXTRACT append_value FROM IDENT { $$ = ast_expr_extract($2, $4); }
    | EXTRACT FROM append_value TO append_value IN IDENT { $$ = ast_expr_extract_range($3, $5, $7); }
    | COUNT append_value IN IDENT    { $$ = ast_expr_count($2, $4); }
    | CHECK append_value IN IDENT    { $$ = ast_expr_check($2, $4); }
    | POSITION OF append_value IN IDENT { $$ = ast_expr_position($3, $5); }
    | IDENT                          { $$ = ast_expr_ident($1); }
    | literal                        { $$ = ast_expr_literal($1); }
    | '(' append_value ')'           { $$ = $2; }
    | MINUS append_factor            { $$ = ast_expr_neg($2); }
    | PLUS  append_factor            { $$ = $2; }
    ;

function_stmt
    : DEFINE ROUTINE IDENT opt_param_clause RETURNS type DO block {
        $$ = ast_new_funcdef(yylineno, $3, $6, $4, param_list_len, $8);
    }
    ;

opt_param_clause
    : /* empty */ {
        $$ = NULL;
        param_list_len = 0;
    }
    | WITH ARGS IN TAKE param_list {
        $$ = $5;
    }
    ;

param_list
    : type IDENT {
        $$ = ast_param_list_new($1, $2, &param_list_len);
    }
    | param_list ',' type IDENT {
        $$ = ast_param_list_push($1, &param_list_len, $3, $4);
    }
    ;

emit_stmt
    : EMIT IDENT opt_call_args {
        $$ = ast_new_emit(yylineno, $2, $3, call_list_len);
    }
    ;

giveback_stmt
    : GIVEBACK expr {
        $$ = ast_new_giveback(yylineno, $2);
    }
    ;

opt_call_args
    : /* empty */ {
        $$ = NULL;
        call_list_len = 0;
    }
    | WITH ARGS IN TAKE simple_call_args %prec PREC_LOWEST {
        $$ = $5;
    }
    | WITH ARGS IN TAKE '(' call_args ')' {
        $$ = $6;
    }
    ;

simple_call_args
    : call_arg_simple {
        $$ = ast_expr_list_new($1, &call_list_len);
    }
    | simple_call_args ',' call_arg_simple {
        $$ = ast_expr_list_push($1, &call_list_len, $3);
    }
    ;

call_arg_simple
    : IDENT                 { $$ = ast_expr_ident($1); }
    | literal               { $$ = ast_expr_literal($1); }
    | call_expr             { $$ = $1; }
    | '(' expr ')'          { $$ = $2; }
    | MINUS call_arg_simple { $$ = ast_expr_neg($2); }
    | PLUS  call_arg_simple { $$ = $2; }
    ;

call_args
    : expr %prec PREC_LOWEST {
        $$ = ast_expr_list_new($1, &call_list_len);
    }
    | call_args ',' expr {
        $$ = ast_expr_list_push($1, &call_list_len, $3);
    }
    ;

call_expr
    : OUTCOME OF IDENT opt_call_args {
        $$ = ast_expr_call($3, $4, call_list_len);
    }
    ;

decl_stmt
    : CREATE IDENT IDENT USING IDENT {
        Stmt *inst = class_instantiate(yylineno, $2, $3, $5);
        if (!inst) YYERROR;
        $$ = inst;
    }
    | CREATE type IDENT SET VALUE expr {
        char **names = static_cast<char **>(malloc(sizeof(char *)));
        Expr **vals  = static_cast<Expr **>(malloc(sizeof(Expr *)));
        if (!names || !vals) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        names[0] = $3;
        vals[0]  = $6;
        $$ = ast_new_decl(yylineno, $2, names, vals, 1);
    }
    | CREATE type id_list_multi SET VALUE val_list_multi {
        if (id_list_len != val_list_len) {
            fprintf(stderr,
                    "Error (line %d): %zu variable(s) but %zu value(s) â€“ "
                    "counts must match.\n",
                    yylineno, id_list_len, val_list_len);
            for (size_t i = 0; i < id_list_len; i++) free($3[i]);
            free($3);
            for (size_t i = 0; i < val_list_len; i++) {
                ast_free_expr($6[i]);
            }
            free($6);
            YYERROR;
        }
        $$ = ast_new_decl(yylineno, $2, $3, $6, id_list_len);
    }
    | CREATE type IDENT {
        char **names = static_cast<char **>(malloc(sizeof(char *)));
        Expr **defs  = static_cast<Expr **>(malloc(sizeof(Expr *)));
        if (!names || !defs) {
            fprintf(stderr, "Fatal: out of memory.\n");
            exit(EXIT_FAILURE);
        }
        names[0] = $3;
        defs[0] = default_init_expr($2);
        $$ = ast_new_decl(yylineno, $2, names, defs, 1);
    }
    | CREATE type id_list_multi {
        Expr **defs = default_init_list($2, id_list_len);
        $$ = ast_new_decl(yylineno, $2, $3, defs, id_list_len);
    }
    | CREATE type IDENT SET_VALUE_TAKE_USER_INPUT opt_message {
        $$ = make_decl_then_input(yylineno, $2, $3, $5, prompt_list_len);
    }
    ;

class_stmt
        : DEFINE CLASS IDENT INCLUDE {
                if (!class_begin($3)) YYERROR;
            } INDENT class_body DEDENT {
                profile_end();
                g_current_class = NULL;
                $$ = ast_new_block(yylineno);
            }
        | DEFINE CLASS IDENT INHERITS IDENT INCLUDE {
                if (!class_begin_inherits($3, $5)) YYERROR;
            } INDENT class_body DEDENT {
                profile_end();
                g_current_class = NULL;
                $$ = ast_new_block(yylineno);
            }
        ;

class_body
        : class_member
        | class_body class_member
        ;

class_member
        : CREATE type IDENT ';' {
                if (!class_add_field($2, $3)) YYERROR;
            }
        | OVERRIDE CREATE type IDENT ';' {
                if (!class_override_field($3, $4)) YYERROR;
            }
        | PROFILE IDENT DO {
                if (!profile_begin($2)) YYERROR;
            } INDENT profile_body DEDENT {
                profile_end();
            }
        | ROUTINE IDENT opt_param_clause RETURNS type DO block {
                if (!class_add_routine($2, $3, param_list_len, $5, $7)) YYERROR;
            }
        | ROUTINE IDENT opt_param_clause DO block {
                if (!class_add_routine($2, $3, param_list_len, TYPE_INT, $5)) YYERROR;
            }
        | OVERRIDE ROUTINE IDENT opt_param_clause RETURNS type DO block {
                if (!class_override_routine($3, $4, param_list_len, $6, $8)) YYERROR;
            }
        | OVERRIDE ROUTINE IDENT opt_param_clause DO block {
                if (!class_override_routine($3, $4, param_list_len, TYPE_INT, $6)) YYERROR;
            }
        ;

profile_body
        : profile_line
        | profile_body profile_line
        ;

profile_line
        : SET IDENT TO expr ';' {
                if (!profile_add_set($2, $4)) YYERROR;
            }
        | SET IDENT TO TAKE USER INPUT opt_message ';' {
                if (!profile_add_input($2, $7, prompt_list_len)) YYERROR;
            }
        ;

opt_message
    : /* empty */                    { $$ = NULL; prompt_list_len = 0; }
    | WITH MESSAGE message_parts     { $$ = $3; }
    ;

message_parts
    : expr                         { $$ = ast_expr_list_new($1, &prompt_list_len); }
    | message_parts ',' expr       { $$ = ast_expr_list_push($1, &prompt_list_len, $3); }
    ;

print_stmt
    : PRINT print_args   {
        $$ = ast_new_print(yylineno, $2, print_list_len);
    }
    ;

/* â”€â”€ If statement â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

if_stmt
    : IF condition THEN block {
        $$ = ast_new_if(yylineno, $2, $4, NULL);
    }
    | IF condition THEN block else_part {
        $$ = ast_new_if(yylineno, $2, $4, $5);
    }
    ;

/* â”€â”€ else_part: otherwise block  |  otherwise when cond then block [...] â”€â”€ */

else_part
    : OTHERWISE block {
        /* plain otherwise: else_block is a STMT_BLOCK */
        $$ = $2;
    }
    | OTHERWISE WHEN condition THEN block {
        /* otherwise when: else_block is a STMT_IF (no further else) */
        $$ = ast_new_if(yylineno, $3, $5, NULL);
    }
    | OTHERWISE WHEN condition THEN block else_part {
        /* otherwise when ... else_part: else_block is a STMT_IF with its own else */
        $$ = ast_new_if(yylineno, $3, $5, $6);
    }
    ;

/* â”€â”€ Block (indented with '>' markers) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

block
    : INDENT block_body DEDENT {
        /* Convert the temporary Program into a STMT_BLOCK */
        Stmt *blk = ast_new_block(yylineno);
        for (size_t i = 0; i < $2->count; i++)
            ast_block_add(blk, $2->stmts[i]);
        /* Free only the Program shell, not the stmts (now owned by block) */
        free($2->stmts);
        free($2);
        $$ = blk;
    }
    ;

block_body
    : stmt              {
                            $$ = ast_new_program();
                            ast_program_add($$, $1);
                        }
    | block_body stmt   {
                            ast_program_add($1, $2);
                            $$ = $1;
                        }
    ;

/* â”€â”€ while loop â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

while_stmt
    : WHILE condition DO block {
        $$ = ast_new_while(yylineno, $2, $4);
    }
    ;

/* â”€â”€ for loop â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

for_stmt
    : FOR IDENT FROM expr TO expr opt_step DO block {
        $$ = ast_new_for(yylineno, $2, $4, $6, $7, $9);
    }
    | FOR IDENT IN IDENT FROM expr TO expr opt_step DO block {
        $$ = ast_new_for_each(yylineno,
                              $2,
                              $6,
                              $8,
                              $9,
                              $4,
                              $11);
    }
    | FOR IDENT IDENT IN IDENT IDENT FROM expr TO expr AND IDENT FROM expr TO expr DO block {
        if (strcmp($2, $6) != 0 || strcmp($3, $12) != 0) {
            fprintf(stderr,
                    "Error (line %d): matrix loop index names must match header pair '%s %s'.\n",
                    yylineno, $2, $3);
            YYERROR;
        }
        free($6);
        free($12);
        $$ = ast_new_for_matrix(yylineno, $2, $3, $5,
                                $8, $10, $14, $16,
                                $18);
    }
    ;

opt_step
    : /* empty */   { $$ = NULL; }
    | STEP expr     { $$ = $2; }
    ;

/* â”€â”€ repeat until loop â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

repeat_stmt
    : REPEAT block UNTIL condition THEN {
        $$ = ast_new_repeat(yylineno, $2, $4);
    }
    ;

/* â”€â”€ attempt loop â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

attempt_stmt
    : ATTEMPT UPTO expr WHILE condition DO block opt_onfailure {
        $$ = ast_new_attempt(yylineno, $3, $5, $7, $8);
    }
    | ATTEMPT UPTO expr TIMES_WHILE condition DO block opt_onfailure {
        $$ = ast_new_attempt(yylineno, $3, $5, $7, $8);
    }
    ;

opt_onfailure
    : /* empty */ { $$ = NULL; }
    | ONFAILURE block { $$ = $2; }
    ;

/* â”€â”€ Conditions (boolean expressions) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

condition
    : cond_or           { $$ = $1; }
    ;

cond_or
    : cond_or OR cond_and   { $$ = ast_cond_or($1, $3); }
    | cond_and              { $$ = $1; }
    ;

cond_and
    : cond_and AND cond_not { $$ = ast_cond_and($1, $3); }
    | cond_not              { $$ = $1; }
    ;

cond_not
    : NOT cond_atom         { $$ = ast_cond_not($2); }
    | cond_atom             { $$ = $1; }
    ;

cond_atom
    : expr CMP_EQ  expr    { $$ = ast_cond_cmp(CMP_EQ,  $1, $3); }
    | expr CMP_NEQ expr    { $$ = ast_cond_cmp(CMP_NEQ, $1, $3); }
    | expr CMP_GT  expr    { $$ = ast_cond_cmp(CMP_GT,  $1, $3); }
    | expr CMP_LT  expr    { $$ = ast_cond_cmp(CMP_LT,  $1, $3); }
    | expr CMP_GTE expr    { $$ = ast_cond_cmp(CMP_GTE, $1, $3); }
    | expr CMP_LTE expr    { $$ = ast_cond_cmp(CMP_LTE, $1, $3); }
    | '(' condition ')'    { $$ = $2; }
    ;

/* â”€â”€ Type keyword â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

type
    : T_INT     { $$ = TYPE_INT;    }
    | T_FLOAT   { $$ = TYPE_FLOAT;  }
    | T_CHAR    { $$ = TYPE_CHAR;   }
    | T_STRING  { $$ = TYPE_STRING; }
    | T_BOOL    { $$ = TYPE_BOOL;   }
    ;

id_list_multi
    : IDENT ',' IDENT      {
                                $$ = ast_ident_list_new($1, &id_list_len);
                                $$ = ast_ident_list_push($$, &id_list_len, $3);
                            }
    | id_list_multi ',' IDENT {
                                $$ = ast_ident_list_push($1, &id_list_len, $3);
                            }
    ;

val_list_multi
    : expr ',' expr        {
                                $$ = ast_expr_list_new($1, &val_list_len);
                                $$ = ast_expr_list_push($$, &val_list_len, $3);
                            }
    | val_list_multi ',' expr {
                                $$ = ast_expr_list_push($1, &val_list_len, $3);
                            }
    ;

mesh_init_values
    : expr {
        $$ = ast_expr_list_new($1, &mesh_init_list_len);
    }
    | mesh_init_values ',' expr {
        $$ = ast_expr_list_push($1, &mesh_init_list_len, $3);
    }
    ;

matrix_init_row
    : expr {
        MatrixInit *m = matrix_init_new();
        matrix_init_push_value(m, $1);
        matrix_init_add_row(m, 1);
        $$ = m;
    }
    | matrix_init_row ',' expr {
        matrix_init_push_value($1, $3);
        $1->row_sizes[$1->row_count - 1]++;
        $$ = $1;
    }
    ;

matrix_init_rows
    : matrix_init_row {
        $$ = $1;
    }
    | matrix_init_rows '/' matrix_init_row {
        $$ = matrix_init_merge($1, $3);
    }
    ;

/* â”€â”€ Print argument list (now full expressions) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

print_args
    : expr                    {
                                  $$ = ast_expr_list_new($1, &print_list_len);
                              }
    | print_args ',' expr     {
                                  $$ = ast_expr_list_push($1, &print_list_len, $3);
                              }
    ;

/* â”€â”€ Compound self-assignment shortcut: a plus 3;  â†’  set a to a plus 3; â”€â”€ */

compound_stmt
    : IDENT PLUS  expr  {
        char *n = $1;
        $$ = ast_new_set(yylineno, n, ast_expr_binary(OP_ADD, ast_expr_ident(n), $3));
    }
    | IDENT MINUS expr  {
        char *n = $1;
        $$ = ast_new_set(yylineno, n, ast_expr_binary(OP_SUB, ast_expr_ident(n), $3));
    }
    | IDENT TIMES expr  {
        char *n = $1;
        $$ = ast_new_set(yylineno, n, ast_expr_binary(OP_MUL, ast_expr_ident(n), $3));
    }
    | IDENT DIV   expr  {
        char *n = $1;
        $$ = ast_new_set(yylineno, n, ast_expr_binary(OP_DIV, ast_expr_ident(n), $3));
    }
    | IDENT MOD   expr  {
        char *n = $1;
        $$ = ast_new_set(yylineno, n, ast_expr_binary(OP_MOD, ast_expr_ident(n), $3));
    }
    ;

/* â”€â”€ Assignment statement â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

set_stmt
    : SET IDENT TO expr     {
        $$ = ast_new_set(yylineno, $2, $4);
    }
    | SET IDENT AT expr ',' expr {
        $$ = ast_new_set_matrix_ctx(yylineno, $2, $4, $6);
    }
    | SET IDENT TO TAKE USER INPUT opt_message {
        $$ = ast_new_input(yylineno, $2, $7, prompt_list_len);
    }
    ;

convert_stmt
    : CONVERT IDENT TO type {
        $$ = ast_new_convert(yylineno, $2, $4);
    }
    ;

/* â”€â”€ Expressions (standard precedence) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

expr
    : expr PLUS  term       { $$ = ast_expr_binary(OP_ADD, $1, $3); }
    | expr MINUS term       { $$ = ast_expr_binary(OP_SUB, $1, $3); }
    | term                  { $$ = $1; }
    ;

term
    : term TIMES factor     { $$ = ast_expr_binary(OP_MUL, $1, $3); }
    | term DIV   factor     { $$ = ast_expr_binary(OP_DIV, $1, $3); }
    | term MOD   factor     { $$ = ast_expr_binary(OP_MOD, $1, $3); }
    | factor                { $$ = $1; }
    ;

factor
    : call_expr             { $$ = $1; }
    | CONVERT expr TO type  { $$ = ast_expr_convert($2, $4); }
    | IDENT AT factor ',' factor %dprec 2 { $$ = ast_expr_matrix_index($1, $3, $5); }
    | SIZE OF IDENT         { $$ = ast_expr_length($3); }
    | ROW LENGTH OF IDENT   { $$ = ast_expr_row_length($4); }
    | COLUMN LENGTH OF IDENT { $$ = ast_expr_col_length($4); }
    | LENGTH OF factor      { $$ = ast_expr_strlen($3); }
    | EXTRACT expr FROM IDENT { $$ = ast_expr_extract($2, $4); }
    | EXTRACT FROM expr TO expr IN IDENT { $$ = ast_expr_extract_range($3, $5, $7); }
    | COUNT expr IN IDENT   { $$ = ast_expr_count($2, $4); }
    | CHECK expr IN IDENT   { $$ = ast_expr_check($2, $4); }
    | POSITION OF expr IN IDENT { $$ = ast_expr_position($3, $5); }
    | IDENT AT factor %dprec 1      { $$ = ast_expr_index($1, $3); }
    | IDENT %prec PREC_LOWEST { $$ = ast_expr_ident($1); }
    | literal               { $$ = ast_expr_literal($1); }
    | '(' expr ')'          { $$ = $2; }
    | MINUS factor          { $$ = ast_expr_neg($2); }  /* step minus 1 */
    | PLUS  factor          { $$ = $2; }                /* step plus 1  */
    ;

/* â”€â”€ Literals â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

literal
    : INT_LIT               { $$ = ast_lit_int($1);    }
    | FLOAT_LIT             { $$ = ast_lit_float($1);  }
    | CHAR_LIT              { $$ = ast_lit_char($1);   }
    | STRING_LIT            { $$ = ast_lit_string($1); }
    | TRUE                  { $$ = ast_lit_bool(1);    }
    | FALSE                 { $$ = ast_lit_bool(0);    }
    ;

%%

void yyerror(const char *msg) {
    snprintf(lexico_last_parse_error, sizeof(lexico_last_parse_error),
             "Parse error (line %d): %s", yylineno, msg ? msg : "syntax error");
    fprintf(stderr, "Parse error (line %d): %s\n", yylineno, msg);
}
