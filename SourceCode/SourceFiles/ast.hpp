/*
 * ast.hpp – Abstract Syntax Tree definitions for the Lexico language.
 *
 * Memory contract:
 *   - Every char* stored in the AST is heap-allocated and owned by the node.
 *   - ast_free_program() releases the entire tree including all strings.
 *   - Callers must NOT free individual nodes; always free via the root Program.
 */

#ifndef LEXICO_AST_H
#define LEXICO_AST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations needed for mutual references */
typedef struct Expr Expr;
typedef struct Stmt Stmt;

/* ── Type system ────────────────────────────────────────────────── */

typedef enum {
    TYPE_INT,
    TYPE_FLOAT,   /* compiled as double */
    TYPE_CHAR,
    TYPE_STRING,
    TYPE_BOOL
} TypeKind;

typedef struct {
    TypeKind type;
    char    *name;   /* heap-allocated, owned */
} Param;

/* ── Literals ───────────────────────────────────────────────────── */

typedef enum {
    LIT_INT,
    LIT_FLOAT,
    LIT_CHAR,
    LIT_STRING,
    LIT_BOOL
} LiteralKind;

typedef struct {
    LiteralKind kind;
    union {
        int64_t  i;
        double   d;
        char     c;
        char    *s;   /* heap-allocated, owned */
        int      b;   /* 0/1 */
    } as;
} Literal;

/* ── Expressions ────────────────────────────────────────────────── */

typedef enum {
    EXPR_LITERAL,
    EXPR_IDENT,
    EXPR_BINARY,
    EXPR_UNARY_NEG,    /* unary minus: -expr */
    EXPR_CALL,         /* outcome of f ... */
    EXPR_INDEX,        /* arr/table/string at idx */
    EXPR_MATRIX_INDEX, /* matrix at row,col */
    EXPR_LENGTH,       /* length of arr/table */
    EXPR_ROW_LENGTH,   /* row length of matrix */
    EXPR_COL_LENGTH,   /* column length of matrix */
    EXPR_STRLEN,       /* length of string expression */
    EXPR_EXTRACT,      /* extract <string> from <stringname> */
    EXPR_EXTRACT_RANGE,/* extract from <number> to <number> in <stringname> */
    EXPR_COUNT,        /* count <value> in <collection> */
    EXPR_CHECK,        /* check <value> in <collection> */
    EXPR_POSITION,     /* position of <value> in <collection> */
    EXPR_CONVERT       /* convert <expr> to <type> */
} ExprKind;

typedef enum {
    OP_ADD,   /* plus   */
    OP_SUB,   /* minus  */
    OP_MUL,   /* times  */
    OP_DIV,   /* divided by */
    OP_MOD    /* mod    */
} BinOp;

/* ── Condition expressions (for if) ─────────────────────────────── */

/*
 * CmpOp values come from the Bison-generated token enum (CMP_EQ, CMP_NEQ, …).
 * We use a plain int typedef here to avoid redeclaring those enumerators.
 */
typedef int CmpOp;

typedef enum {
    COND_CMP,      /* expr CmpOp expr          */
    COND_AND,      /* cond and cond             */
    COND_OR,       /* cond or cond              */
    COND_NOT       /* not cond                  */
} CondKind;

typedef struct CondExpr CondExpr;
struct CondExpr {
    CondKind kind;
    union {
        struct { Expr *left; CmpOp op; Expr *right; } cmp;   /* COND_CMP */
        struct { CondExpr *left; CondExpr *right; }   logic;  /* COND_AND / COND_OR */
        CondExpr *operand;                                    /* COND_NOT */
    } as;
};

struct Expr {
    ExprKind kind;
    union {
        Literal  lit;          /* EXPR_LITERAL  */
        char    *ident;        /* EXPR_IDENT – heap-owned */
        struct { BinOp op; Expr *left; Expr *right; } bin; /* EXPR_BINARY */
        Expr    *operand;      /* EXPR_UNARY_NEG */
        struct { char *name; Expr **args; size_t count; } call; /* EXPR_CALL */
        struct { char *name; Expr *index; } index; /* EXPR_INDEX */
        struct { char *name; Expr *row; Expr *col; } matrix_index; /* EXPR_MATRIX_INDEX */
        char *length_name;     /* EXPR_LENGTH */
        char *row_length_name; /* EXPR_ROW_LENGTH */
        char *col_length_name; /* EXPR_COL_LENGTH */
        Expr *strlen_value;    /* EXPR_STRLEN */
        struct { Expr *value; char *source_name; } extract; /* EXPR_EXTRACT */
        struct { Expr *from; Expr *to; char *source_name; } extract_range; /* EXPR_EXTRACT_RANGE */
        struct { Expr *value; char *collection_name; } count; /* EXPR_COUNT */
        struct { Expr *value; char *collection_name; } check; /* EXPR_CHECK */
        struct { Expr *value; char *collection_name; } position; /* EXPR_POSITION */
        struct { Expr *value; TypeKind to_type; } convert; /* EXPR_CONVERT */
    } as;
};

/* ── Statements ─────────────────────────────────────────────────── */

typedef enum {
    STMT_DECL,
    STMT_ARRAY_DECL,
    STMT_TABLE_DECL,
    STMT_MATRIX_DECL,
    STMT_COLLECTION_SET,
    STMT_MATRIX_SET,
    STMT_COLLECTION_SET_INPUT,
    STMT_COLLECTION_APPEND,
    STMT_COLLECTION_SORT,
    STMT_PRINT,
    STMT_SET,
    STMT_CONVERT,
    STMT_SET_MATRIX_CTX,
    STMT_INPUT,
    STMT_EMIT,
    STMT_GIVEBACK,
    STMT_FUNCDEF,
    STMT_IF,
    STMT_BLOCK,    /* list of statements (used as if/loop body) */
    STMT_WHILE,
    STMT_FOR,
    STMT_FOR_EACH,
    STMT_FOR_MATRIX,
    STMT_REPEAT,
    STMT_ATTEMPT,
    STMT_FILE_OPEN,
    STMT_FILE_WRITE,
    STMT_FILE_READ,
    STMT_FILE_CLEAR,
    STMT_FILE_SET_TITLE,
    STMT_FILE_CLOSE
} StmtKind;

typedef enum {
    FILE_READ_ALL,
    FILE_READ_LINE,
    FILE_READ_FIRST,
    FILE_READ_RANGE
} FileReadMode;

typedef enum {
    FILE_CLEAR_ALL,
    FILE_CLEAR_LINE,
    FILE_CLEAR_RANGE
} FileClearMode;

typedef struct {
    TypeKind elem_type;
    char    *name;
    Expr    *size;
} ArrayDeclStmt;

typedef struct {
    char *name;
    Expr *size;
} TableDeclStmt;

typedef struct {
    TypeKind elem_type;
    int      is_dynamic; /* 1 => create mesh matrix ..., 0 => create <type> mesh matrix ... */
    char    *name;
    Expr    *rows;
    Expr    *cols;
} MatrixDeclStmt;

typedef struct {
    char *name;         /* NULL for context form: set value at ... */
    Expr *index;
    Expr *value;
} CollectionSetStmt;

typedef struct {
    char *name;
    Expr *row;
    Expr *col;
    Expr *value;
} MatrixSetStmt;

typedef struct {
    char *name;           /* NULL for context form: set value at ... */
    Expr *index;
    Expr **message_parts; /* optional prompt expression parts */
    size_t message_count;
} CollectionSetInputStmt;

typedef struct {
    char *name;   /* array/table name */
    Expr *value;
    Expr *index;  /* NULL => append at end */
} CollectionAppendStmt;

typedef struct {
    char *name;     /* typed mesh name */
    int   ascending; /* 1 => ascendantly, 0 => descendantly */
} CollectionSortStmt;

typedef struct {
    TypeKind  type;
    char    **names;    /* array of heap-allocated names */
    Expr    **values;   /* parallel array of init expressions */
    size_t    count;    /* length of both arrays */
} DeclStmt;

typedef struct {
    Expr   **args;      /* array of expression pointers */
    size_t   count;
} PrintStmt;

typedef struct {
    char *target;       /* variable name – heap-owned */
    Expr *value;        /* RHS expression – heap-owned tree */
} SetStmt;

typedef struct {
    char    *target;    /* variable name – heap-owned */
    TypeKind to_type;   /* destination scalar type */
} ConvertStmt;

typedef struct {
    char *target;       /* scalar variable name – heap-owned */
    Expr *row;          /* contextual matrix row expression */
    Expr *col;          /* contextual matrix col expression */
} SetMatrixCtxStmt;

typedef struct {
    char *target;       /* variable name – heap-owned */
    Expr **message_parts; /* optional prompt expression parts */
    size_t message_count;
} InputStmt;

typedef struct {
    char   *name;       /* function name – heap-owned */
    Expr  **args;       /* argument expressions */
    size_t  count;
} EmitStmt;

typedef struct {
    Expr *value;        /* returned expression */
} GivebackStmt;

typedef struct {
    char    *name;        /* function name – heap-owned */
    TypeKind return_type;
    Param   *params;      /* parameter list */
    size_t   param_count;
    Stmt    *body;        /* STMT_BLOCK */
} FuncDefStmt;

typedef struct {
    Stmt  **stmts;      /* array of statement pointers */
    size_t  count;
    size_t  cap;
} BlockStmt;

typedef struct {
    CondExpr *cond;
    Stmt     *body;    /* STMT_BLOCK */
} WhileStmt;

typedef struct {
    char *var;         /* loop variable name, heap-owned */
    Expr *from;        /* start expression */
    Expr *to;          /* end expression */
    Expr *step;        /* step expression, NULL = implicit +1 */
    Stmt *body;        /* STMT_BLOCK */
} ForStmt;

typedef struct {
    char *index_var;   /* index variable name */
    Expr *from;        /* start index */
    Expr *to;          /* end index (inclusive) */
    Expr *step;        /* step expression, NULL = implicit +1 */
    char *collection;  /* collection variable name */
    Stmt *body;        /* STMT_BLOCK */
} ForEachStmt;

typedef struct {
    char *row_var;     /* implicit row loop variable */
    char *col_var;     /* implicit col loop variable */
    char *matrix;      /* matrix variable name */
    Expr *row_from;    /* NULL => default 0 */
    Expr *row_to;      /* NULL => default rows-1 */
    Expr *col_from;    /* NULL => default 0 */
    Expr *col_to;      /* NULL => default cols-1 */
    Stmt *body;        /* STMT_BLOCK */
} ForMatrixStmt;

typedef struct {
    Stmt     *body;    /* STMT_BLOCK (runs at least once) */
    CondExpr *cond;    /* until condition – exit when true */
} RepeatStmt;

typedef struct {
    Expr     *max;     /* max attempts expression */
    CondExpr *cond;    /* while condition */
    Stmt     *body;    /* STMT_BLOCK */
    Stmt     *failure; /* STMT_BLOCK or NULL (on failure block) */
} AttemptStmt;

typedef struct {
    Expr *path;
    int   make_if_missing;
    Stmt *body;
} FileOpenStmt;

typedef struct {
    int   has_line;
    Expr *line;
    Expr *value;
} FileWriteStmt;

typedef struct {
    FileReadMode mode;
    Expr *count;
    Expr *from_line;
    Expr *line;
    char *target;
} FileReadStmt;

typedef struct {
    FileClearMode mode;
    Expr *line;
    Expr *from_line;
    Expr *to_line;
} FileClearStmt;

typedef struct {
    Expr *value;
} FileSetTitleStmt;

typedef struct {
    CondExpr  *cond;       /* condition – heap-owned tree */
    Stmt      *then_block; /* STMT_BLOCK – the '>' body */
    Stmt      *else_block; /* STMT_BLOCK or NULL (otherwise) */
} IfStmt;

struct Stmt {
    StmtKind kind;
    int      line;      /* source line for diagnostics */
    union {
        DeclStmt    decl;
        ArrayDeclStmt array_decl;
        TableDeclStmt table_decl;
        MatrixDeclStmt matrix_decl;
        CollectionSetStmt collection_set;
        MatrixSetStmt matrix_set;
        CollectionSetInputStmt collection_set_input;
        CollectionAppendStmt collection_append;
        CollectionSortStmt collection_sort;
        PrintStmt   print;
        SetStmt     set;
        ConvertStmt convert;
        SetMatrixCtxStmt set_matrix_ctx;
        InputStmt   input;
        EmitStmt    emit;
        GivebackStmt giveback;
        FuncDefStmt funcdef;
        IfStmt      if_;
        BlockStmt   block;
        WhileStmt   while_;
        ForStmt     for_;
        ForEachStmt for_each;
        ForMatrixStmt for_matrix;
        RepeatStmt  repeat_;
        AttemptStmt attempt_;
        FileOpenStmt file_open;
        FileWriteStmt file_write;
        FileReadStmt file_read;
        FileClearStmt file_clear;
        FileSetTitleStmt file_set_title;
    } as;
};

/* ── Program (root) ─────────────────────────────────────────────── */

typedef struct {
    Stmt  **stmts;      /* array of pointers (each heap-allocated) */
    size_t  count;
    size_t  cap;
} Program;

/* ── Constructor helpers ────────────────────────────────────────── */

Program  *ast_new_program(void);
void      ast_program_add(Program *p, Stmt *s);

Stmt     *ast_new_decl(int line, TypeKind type,
                       char **names, Expr **values, size_t count);
Stmt     *ast_new_array_decl(int line, TypeKind elem_type, char *name, Expr *size);
Stmt     *ast_new_table_decl(int line, char *name, Expr *size);
Stmt     *ast_new_matrix_decl(int line, TypeKind elem_type, int is_dynamic, char *name, Expr *rows, Expr *cols);
Stmt     *ast_new_collection_set(int line, char *name, Expr *index, Expr *value);
Stmt     *ast_new_matrix_set(int line, char *name, Expr *row, Expr *col, Expr *value);
Stmt     *ast_new_collection_set_input(int line, char *name, Expr *index,
                                       Expr **message_parts, size_t message_count);
Stmt     *ast_new_collection_append(int line, char *name, Expr *value, Expr *index);
Stmt     *ast_new_collection_sort(int line, char *name, int ascending);
Stmt     *ast_new_print(int line, Expr **args, size_t count);
Stmt     *ast_new_set  (int line, char *target, Expr *value);
Stmt     *ast_new_convert(int line, char *target, TypeKind to_type);
Stmt     *ast_new_set_matrix_ctx(int line, char *target, Expr *row, Expr *col);
Stmt     *ast_new_input(int line, char *target, Expr **message_parts, size_t message_count);
Stmt     *ast_new_emit (int line, char *name, Expr **args, size_t count);
Stmt     *ast_new_giveback(int line, Expr *value);
Stmt     *ast_new_funcdef(int line, char *name, TypeKind return_type,
                          Param *params, size_t param_count, Stmt *body);
Stmt     *ast_new_if   (int line, CondExpr *cond,
                        Stmt *then_block, Stmt *else_block);
Stmt     *ast_new_block(int line);
void      ast_block_add(Stmt *block, Stmt *s);

Stmt     *ast_new_while  (int line, CondExpr *cond, Stmt *body);
Stmt     *ast_new_for    (int line, char *var, Expr *from, Expr *to,
                          Expr *step, Stmt *body);
Stmt     *ast_new_for_each(int line, char *index_var, Expr *from, Expr *to,
                           Expr *step, char *collection, Stmt *body);
Stmt     *ast_new_for_matrix(int line, char *row_var, char *col_var,
                             char *matrix,
                             Expr *row_from, Expr *row_to,
                             Expr *col_from, Expr *col_to,
                             Stmt *body);
Stmt     *ast_new_repeat (int line, Stmt *body, CondExpr *cond);
Stmt     *ast_new_attempt(int line, Expr *max, CondExpr *cond,
                          Stmt *body, Stmt *failure);
Stmt     *ast_new_file_open(int line, Expr *path, int make_if_missing, Stmt *body);
Stmt     *ast_new_file_write(int line, Expr *value, Expr *line_expr);
Stmt     *ast_new_file_read(int line, FileReadMode mode, char *target,
                           Expr *count, Expr *from_line, Expr *line_expr);
Stmt     *ast_new_file_clear(int line, FileClearMode mode, Expr *line_expr,
                            Expr *from_line, Expr *to_line);
Stmt     *ast_new_file_set_title(int line, Expr *value);
Stmt     *ast_new_file_close(int line);

/* ── Condition constructors ─────────────────────────────────────── */
CondExpr *ast_cond_cmp (CmpOp op, Expr *left, Expr *right);
CondExpr *ast_cond_and (CondExpr *left, CondExpr *right);
CondExpr *ast_cond_or  (CondExpr *left, CondExpr *right);
CondExpr *ast_cond_not (CondExpr *operand);
void      ast_free_cond(CondExpr *c);

/* ── Expression constructors ────────────────────────────────────── */
Expr     *ast_expr_literal (Literal lit);
Expr     *ast_expr_ident   (const char *name);  /* copies name */
Expr     *ast_expr_binary  (BinOp op, Expr *left, Expr *right);
Expr     *ast_expr_neg     (Expr *operand);
Expr     *ast_expr_call    (char *name, Expr **args, size_t count);
Expr     *ast_expr_index   (char *name, Expr *index);
Expr     *ast_expr_matrix_index(char *name, Expr *row, Expr *col);
Expr     *ast_expr_length  (char *name);
Expr     *ast_expr_row_length(char *name);
Expr     *ast_expr_col_length(char *name);
Expr     *ast_expr_strlen  (Expr *value);
Expr     *ast_expr_extract (Expr *value, char *source_name);
Expr     *ast_expr_extract_range(Expr *from, Expr *to, char *source_name);
Expr     *ast_expr_count   (Expr *value, char *collection_name);
Expr     *ast_expr_check   (Expr *value, char *collection_name);
Expr     *ast_expr_position(Expr *value, char *collection_name);
Expr     *ast_expr_convert (Expr *value, TypeKind to_type);
Expr     *ast_expr_clone(const Expr *e);
void      ast_free_expr    (Expr *e);
void      ast_free_stmt    (Stmt *s);

/* List-builder helpers used during parsing */
char    **ast_ident_list_new (const char *first, size_t *out_count);
char    **ast_ident_list_push(char **list, size_t *count, const char *name);

Expr    **ast_expr_list_new (Expr *first, size_t *out_count);
Expr    **ast_expr_list_push(Expr **list, size_t *count, Expr *e);

Param    *ast_param_list_new (TypeKind type, const char *name, size_t *out_count);
Param    *ast_param_list_push(Param *list, size_t *count,
                              TypeKind type, const char *name);

Literal   ast_lit_int   (int64_t v);
Literal   ast_lit_float (double  v);
Literal   ast_lit_char  (char    v);
Literal   ast_lit_string(const char *s);   /* copies the string */
Literal   ast_lit_bool  (int     v);

/* ── Cleanup ────────────────────────────────────────────────────── */

void ast_free_program(Program *p);

#ifdef __cplusplus
}
#endif

#endif /* LEXICO_AST_H */
