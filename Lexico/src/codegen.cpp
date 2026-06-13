#include "codegen.hpp"
#include "lexico.tab.hpp"

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LX_TAG_EMPTY  = 0,
    LX_TAG_INT    = 1,
    LX_TAG_FLOAT  = 2,
    LX_TAG_CHAR   = 3,
    LX_TAG_STRING = 4,
    LX_TAG_BOOL   = 5
} LxTag;

#define TYPE_DYNAMIC ((TypeKind)1000)

typedef struct {
    LxTag tag;
    int64_t i;
    double d;
    char c;
    char *s;
} LxCell;

typedef struct {
    int is_table;
    int is_matrix;
    LxTag elem_tag;
    size_t rows;
    size_t cols;
    size_t len;
    size_t cap;
    void *data;
} LxCollection;

static LxCollection **g_collections = NULL;
static size_t g_collections_n = 0;
static size_t g_collections_cap = 0;
static int g_sort_descending = 0;

typedef struct {
    char *path;
} LxFile;

static LxFile **g_files = NULL;
static size_t g_files_n = 0;
static size_t g_files_cap = 0;

static LLVMContextRef g_codegen_ctx = NULL;

#define LLVMInt64Type() LLVMInt64TypeInContext(g_codegen_ctx)
#define LLVMDoubleType() LLVMDoubleTypeInContext(g_codegen_ctx)
#define LLVMInt8Type() LLVMInt8TypeInContext(g_codegen_ctx)
#define LLVMInt1Type() LLVMInt1TypeInContext(g_codegen_ctx)
#define LLVMInt32Type() LLVMInt32TypeInContext(g_codegen_ctx)
#define LLVMVoidType() LLVMVoidTypeInContext(g_codegen_ctx)
#define LLVMAppendBasicBlock(Fn, Name) LLVMAppendBasicBlockInContext(g_codegen_ctx, Fn, Name)

static void rt_oom(void) {
    fprintf(stderr, "Fatal: runtime out of memory.\n");
    exit(EXIT_FAILURE);
}

static void *rt_xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p && n) rt_oom();
    return p;
}

static void *rt_xrealloc(void *p, size_t n) {
    void *q = realloc(p, n);
    if (!q && n) rt_oom();
    return q;
}

static char *rt_xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *d = (char *)rt_xmalloc(n + 1);
    memcpy(d, s, n + 1);
    return d;
}

static char rt_value_to_char(LxTag tag, int64_t i, double d, int64_t cch, const char *s) {
    switch (tag) {
    case LX_TAG_CHAR:   return (char)cch;
    case LX_TAG_INT:    return (char)i;
    case LX_TAG_FLOAT:  return (char)((int64_t)d);
    case LX_TAG_STRING: return (s && s[0]) ? s[0] : '\0';
    case LX_TAG_BOOL:   return (i != 0) ? 1 : 0;
    default:            return '\0';
    }
}

static char *rt_value_to_string(LxTag tag, int64_t i, double d, int64_t cch, const char *s) {
    char buf[128];
    switch (tag) {
    case LX_TAG_STRING:
        return rt_xstrdup(s ? s : "");
    case LX_TAG_CHAR: {
        char tmp[2] = { (char)cch, '\0' };
        return rt_xstrdup(tmp);
    }
    case LX_TAG_INT:
        snprintf(buf, sizeof buf, "%lld", (long long)i);
        return rt_xstrdup(buf);
    case LX_TAG_FLOAT:
        snprintf(buf, sizeof buf, "%.15g", d);
        return rt_xstrdup(buf);
    case LX_TAG_BOOL:
        return rt_xstrdup((i != 0) ? "true" : "false");
    default:
        return rt_xstrdup("");
    }
}

static const char *lx_i64_to_string(int64_t v) {
    char buf[64];
    snprintf(buf, sizeof buf, "%lld", (long long)v);
    return rt_xstrdup(buf);
}

static const char *lx_f64_to_string(double v) {
    char buf[64];
    snprintf(buf, sizeof buf, "%.15g", v);
    return rt_xstrdup(buf);
}

static const char *lx_char_to_string(int64_t c) {
    char tmp[2];
    tmp[0] = (char)c;
    tmp[1] = '\0';
    return rt_xstrdup(tmp);
}

static const char *lx_bool_to_string(int64_t b) {
    return rt_xstrdup(b ? "true" : "false");
}

static int64_t lx_string_to_i64(const char *s) {
    char *end = NULL;
    long long v;
    if (!s) {
        fprintf(stderr, "Runtime error: cannot convert null string to int.\n");
        exit(EXIT_FAILURE);
    }
    errno = 0;
    v = strtoll(s, &end, 10);
    if (errno != 0 || end == s) {
        fprintf(stderr, "Runtime error: cannot convert '%s' to int.\n", s);
        exit(EXIT_FAILURE);
    }
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
    if (*end != '\0') {
        fprintf(stderr, "Runtime error: cannot convert '%s' to int.\n", s);
        exit(EXIT_FAILURE);
    }
    return (int64_t)v;
}

static double lx_string_to_f64(const char *s) {
    char *end = NULL;
    double v;
    if (!s) {
        fprintf(stderr, "Runtime error: cannot convert null string to float.\n");
        exit(EXIT_FAILURE);
    }
    errno = 0;
    v = strtod(s, &end);
    if (errno != 0 || end == s) {
        fprintf(stderr, "Runtime error: cannot convert '%s' to float.\n", s);
        exit(EXIT_FAILURE);
    }
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
    if (*end != '\0') {
        fprintf(stderr, "Runtime error: cannot convert '%s' to float.\n", s);
        exit(EXIT_FAILURE);
    }
    return v;
}

static int64_t lx_string_to_char(const char *s) {
    if (!s || s[0] == '\0' || s[1] != '\0') {
        fprintf(stderr, "Runtime error: cannot convert string to char (need exactly one character).\n");
        exit(EXIT_FAILURE);
    }
    return (unsigned char)s[0];
}

static void rt_bounds_check(size_t idx, size_t len) {
    if (idx >= len) {
        fprintf(stderr, "Runtime error: index %zu out of bounds [0..%zu).\n", idx, len);
        exit(EXIT_FAILURE);
    }
}

static int64_t rt_register_collection(LxCollection *c) {
    if (g_collections_n == g_collections_cap) {
        size_t next = g_collections_cap ? g_collections_cap * 2 : 16;
        if (next < g_collections_cap) rt_oom();
        g_collections = (LxCollection **)rt_xrealloc(g_collections, next * sizeof(*g_collections));
        g_collections_cap = next;
    }
    g_collections[g_collections_n] = c;
    return (int64_t)(g_collections_n++);
}

static LxCollection *rt_get_collection(int64_t handle) {
    if (handle < 0 || (size_t)handle >= g_collections_n || !g_collections[handle]) {
        fprintf(stderr, "Runtime error: invalid collection handle %lld.\n", (long long)handle);
        exit(EXIT_FAILURE);
    }
    return g_collections[handle];
}

static int rt_cmp_int64(const void *a, const void *b) {
    const int64_t av = *(const int64_t *)a;
    const int64_t bv = *(const int64_t *)b;
    int cmp = (av > bv) - (av < bv);
    return g_sort_descending ? -cmp : cmp;
}

static int rt_cmp_char(const void *a, const void *b) {
    const unsigned char av = (unsigned char)*(const char *)a;
    const unsigned char bv = (unsigned char)*(const char *)b;
    int cmp = (av > bv) - (av < bv);
    return g_sort_descending ? -cmp : cmp;
}

static double rt_frac_part(double x) {
    double ip = 0.0;
    return modf(fabs(x), &ip);
}

static int rt_cmp_float_frac(const void *a, const void *b) {
    const double av = *(const double *)a;
    const double bv = *(const double *)b;
    const double af = rt_frac_part(av);
    const double bf = rt_frac_part(bv);
    int cmp = (af > bf) - (af < bf);
    if (cmp == 0) cmp = (av > bv) - (av < bv);
    return g_sort_descending ? -cmp : cmp;
}

static int rt_cmp_str_first(const void *a, const void *b) {
    const char *as = *(char * const *)a;
    const char *bs = *(char * const *)b;
    const unsigned char ac = (as && as[0]) ? (unsigned char)as[0] : 0;
    const unsigned char bc = (bs && bs[0]) ? (unsigned char)bs[0] : 0;
    int cmp = (ac > bc) - (ac < bc);
    if (cmp == 0) {
        const char *aa = as ? as : "";
        const char *bb = bs ? bs : "";
        cmp = strcmp(aa, bb);
        if (cmp > 0) cmp = 1;
        else if (cmp < 0) cmp = -1;
    }
    return g_sort_descending ? -cmp : cmp;
}

int64_t lx_array_new(int64_t elem_tag, int64_t size) {
    if (size < 0) {
        fprintf(stderr, "Runtime error: array size cannot be negative.\n");
        exit(EXIT_FAILURE);
    }
    LxCollection *c = (LxCollection *)rt_xmalloc(sizeof(*c));
    c->is_table = 0;
    c->is_matrix = 0;
    c->elem_tag = (LxTag)elem_tag;
    c->rows = 0;
    c->cols = 0;
    c->len = (size_t)size;
    c->cap = (size_t)size;

    switch (c->elem_tag) {
    case LX_TAG_INT:    c->data = rt_xmalloc(c->cap * sizeof(int64_t)); memset(c->data, 0, c->cap * sizeof(int64_t)); break;
    case LX_TAG_FLOAT:  c->data = rt_xmalloc(c->cap * sizeof(double));  memset(c->data, 0, c->cap * sizeof(double)); break;
    case LX_TAG_CHAR:   c->data = rt_xmalloc(c->cap * sizeof(char));    memset(c->data, 0, c->cap * sizeof(char)); break;
    case LX_TAG_STRING: {
        c->data = rt_xmalloc(c->cap * sizeof(char *));
        char **arr = (char **)c->data;
        for (size_t i = 0; i < c->cap; i++) arr[i] = rt_xstrdup("");
        break;
    }
    case LX_TAG_BOOL:
        c->data = rt_xmalloc(c->cap * sizeof(int64_t));
        memset(c->data, 0, c->cap * sizeof(int64_t));
        break;
    default:
        fprintf(stderr, "Runtime error: invalid array element tag.\n");
        exit(EXIT_FAILURE);
    }

    return rt_register_collection(c);
}

int64_t lx_table_new(int64_t size) {
    if (size < 0) {
        fprintf(stderr, "Runtime error: table size cannot be negative.\n");
        exit(EXIT_FAILURE);
    }
    LxCollection *c = (LxCollection *)rt_xmalloc(sizeof(*c));
    c->is_table = 1;
    c->is_matrix = 0;
    c->elem_tag = LX_TAG_EMPTY;
    c->rows = 0;
    c->cols = 0;
    c->len = (size_t)size;
    c->cap = (size_t)size;
    c->data = rt_xmalloc(c->cap * sizeof(LxCell));
    LxCell *cells = (LxCell *)c->data;
    for (size_t i = 0; i < c->cap; i++) {
        cells[i].tag = LX_TAG_EMPTY;
        cells[i].i = 0;
        cells[i].d = 0.0;
        cells[i].c = 0;
        cells[i].s = NULL;
    }
    return rt_register_collection(c);
}

static size_t rt_matrix_offset(LxCollection *c, int64_t row, int64_t col) {
    if (!c->is_matrix) {
        fprintf(stderr, "Runtime error: matrix operation on non-matrix collection.\n");
        exit(EXIT_FAILURE);
    }
    if (row < 0 || col < 0) {
        fprintf(stderr, "Runtime error: negative matrix index.\n");
        exit(EXIT_FAILURE);
    }
    size_t r = (size_t)row;
    size_t cl = (size_t)col;
    size_t eff_rows = (c->cols == 0) ? 0 : ((c->len + c->cols - 1) / c->cols);
    if (r >= eff_rows || cl >= c->cols) {
        fprintf(stderr, "Runtime error: matrix index (%zu,%zu) out of bounds [%zu x %zu].\n", r, cl, eff_rows, c->cols);
        exit(EXIT_FAILURE);
    }
    size_t idx = r * c->cols + cl;
    if (idx >= c->len) {
        fprintf(stderr, "Runtime error: matrix index (%zu,%zu) points to an empty appended slot.\n", r, cl);
        exit(EXIT_FAILURE);
    }
    return idx;
}

int64_t lx_matrix_new(int64_t elem_tag, int64_t rows, int64_t cols) {
    if (rows < 0 || cols < 0) {
        fprintf(stderr, "Runtime error: matrix dimensions cannot be negative.\n");
        exit(EXIT_FAILURE);
    }

    size_t r = (size_t)rows;
    size_t c = (size_t)cols;
    if (r != 0 && c > ((size_t)-1) / r) {
        fprintf(stderr, "Runtime error: matrix dimensions overflow.\n");
        exit(EXIT_FAILURE);
    }
    size_t n = r * c;

    LxCollection *m = (LxCollection *)rt_xmalloc(sizeof(*m));
    m->is_matrix = 1;
    m->rows = r;
    m->cols = c;
    m->len = n;
    m->cap = n;

    if ((LxTag)elem_tag == LX_TAG_EMPTY) {
        m->is_table = 1;
        m->elem_tag = LX_TAG_EMPTY;
        m->data = rt_xmalloc(n * sizeof(LxCell));
        LxCell *cells = (LxCell *)m->data;
        for (size_t i = 0; i < n; i++) {
            cells[i].tag = LX_TAG_EMPTY;
            cells[i].i = 0;
            cells[i].d = 0.0;
            cells[i].c = 0;
            cells[i].s = NULL;
        }
        return rt_register_collection(m);
    }

    m->is_table = 0;
    m->elem_tag = (LxTag)elem_tag;
    switch (m->elem_tag) {
    case LX_TAG_INT:
        m->data = rt_xmalloc(n * sizeof(int64_t));
        memset(m->data, 0, n * sizeof(int64_t));
        break;
    case LX_TAG_FLOAT:
        m->data = rt_xmalloc(n * sizeof(double));
        memset(m->data, 0, n * sizeof(double));
        break;
    case LX_TAG_CHAR:
        m->data = rt_xmalloc(n * sizeof(char));
        memset(m->data, 0, n * sizeof(char));
        break;
    case LX_TAG_STRING: {
        m->data = rt_xmalloc(n * sizeof(char *));
        char **arr = (char **)m->data;
        for (size_t i = 0; i < n; i++) arr[i] = rt_xstrdup("");
        break;
    }
    case LX_TAG_BOOL:
        m->data = rt_xmalloc(n * sizeof(int64_t));
        memset(m->data, 0, n * sizeof(int64_t));
        break;
    default:
        fprintf(stderr, "Runtime error: invalid matrix element tag.\n");
        exit(EXIT_FAILURE);
    }

    return rt_register_collection(m);
}

void lx_matrix_set(int64_t handle, int64_t row, int64_t col,
                   int64_t tag, int64_t i, double d, int64_t cch, const char *s) {
    LxCollection *m = rt_get_collection(handle);
    size_t idx = rt_matrix_offset(m, row, col);

    if (!m->is_table) {
        if (m->elem_tag == LX_TAG_STRING) {
            char **arr = (char **)m->data;
            free(arr[idx]);
            arr[idx] = rt_value_to_string((LxTag)tag, i, d, cch, s);
            return;
        }
        if (m->elem_tag == LX_TAG_CHAR) {
            ((char *)m->data)[idx] = rt_value_to_char((LxTag)tag, i, d, cch, s);
            return;
        }
        if ((LxTag)tag != m->elem_tag) {
            fprintf(stderr, "Runtime error: type mismatch on matrix write.\n");
            exit(EXIT_FAILURE);
        }
        switch (m->elem_tag) {
        case LX_TAG_INT:    ((int64_t *)m->data)[idx] = i; break;
        case LX_TAG_FLOAT:  ((double  *)m->data)[idx] = d; break;
        case LX_TAG_CHAR:   ((char    *)m->data)[idx] = (char)cch; break;
        case LX_TAG_BOOL:   ((int64_t *)m->data)[idx] = (i != 0) ? 1 : 0; break;
        case LX_TAG_STRING: break;
        default: break;
        }
        return;
    }

    LxCell *cells = (LxCell *)m->data;
    if (cells[idx].tag == LX_TAG_STRING) {
        free(cells[idx].s);
        cells[idx].s = NULL;
    }
    cells[idx].tag = (LxTag)tag;
    cells[idx].i = i;
    cells[idx].d = d;
    cells[idx].c = (char)cch;
    cells[idx].s = ((LxTag)tag == LX_TAG_STRING) ? rt_xstrdup(s ? s : "") : NULL;
}

int64_t lx_matrix_get_i(int64_t handle, int64_t row, int64_t col) {
    LxCollection *m = rt_get_collection(handle);
    size_t idx = rt_matrix_offset(m, row, col);
    if (!m->is_table) {
        if (m->elem_tag != LX_TAG_INT && m->elem_tag != LX_TAG_BOOL) {
            fprintf(stderr, "Runtime error: matrix get int type mismatch.\n");
            exit(EXIT_FAILURE);
        }
        return ((int64_t *)m->data)[idx];
    }
    LxCell *cell = &((LxCell *)m->data)[idx];
    if (cell->tag != LX_TAG_INT && cell->tag != LX_TAG_BOOL) {
        fprintf(stderr, "Runtime error: dynamic matrix cell is not int.\n");
        exit(EXIT_FAILURE);
    }
    return cell->i;
}

double lx_matrix_get_d(int64_t handle, int64_t row, int64_t col) {
    LxCollection *m = rt_get_collection(handle);
    size_t idx = rt_matrix_offset(m, row, col);
    if (!m->is_table) {
        if (m->elem_tag != LX_TAG_FLOAT) {
            fprintf(stderr, "Runtime error: matrix get float type mismatch.\n");
            exit(EXIT_FAILURE);
        }
        return ((double *)m->data)[idx];
    }
    LxCell *cell = &((LxCell *)m->data)[idx];
    if (cell->tag != LX_TAG_FLOAT) {
        fprintf(stderr, "Runtime error: dynamic matrix cell is not float.\n");
        exit(EXIT_FAILURE);
    }
    return cell->d;
}

int64_t lx_matrix_get_c(int64_t handle, int64_t row, int64_t col) {
    LxCollection *m = rt_get_collection(handle);
    size_t idx = rt_matrix_offset(m, row, col);
    if (!m->is_table) {
        if (m->elem_tag != LX_TAG_CHAR) {
            fprintf(stderr, "Runtime error: matrix get char type mismatch.\n");
            exit(EXIT_FAILURE);
        }
        return (int64_t)((char *)m->data)[idx];
    }
    LxCell *cell = &((LxCell *)m->data)[idx];
    if (cell->tag != LX_TAG_CHAR) {
        fprintf(stderr, "Runtime error: dynamic matrix cell is not char.\n");
        exit(EXIT_FAILURE);
    }
    return (int64_t)cell->c;
}

const char *lx_matrix_get_s(int64_t handle, int64_t row, int64_t col) {
    LxCollection *m = rt_get_collection(handle);
    size_t idx = rt_matrix_offset(m, row, col);
    if (!m->is_table) {
        if (m->elem_tag != LX_TAG_STRING) {
            fprintf(stderr, "Runtime error: matrix get string type mismatch.\n");
            exit(EXIT_FAILURE);
        }
        return ((char **)m->data)[idx];
    }
    LxCell *cell = &((LxCell *)m->data)[idx];
    if (cell->tag != LX_TAG_STRING) {
        fprintf(stderr, "Runtime error: dynamic matrix cell is not string.\n");
        exit(EXIT_FAILURE);
    }
    return cell->s ? cell->s : "";
}

void lx_print_matrix_ascii(int64_t handle, const char *name) {
    LxCollection *m = rt_get_collection(handle);
    if (!m->is_matrix) {
        fprintf(stderr, "Runtime error: lx_print_matrix_ascii requires a matrix handle.\n");
        exit(EXIT_FAILURE);
    }

    size_t eff_rows = (m->cols == 0) ? 0 : ((m->len + m->cols - 1) / m->cols);
    printf("%s[%zux%zu]\n", name ? name : "matrix", eff_rows, m->cols);
    for (size_t r = 0; r < eff_rows; r++) {
        for (size_t c = 0; c < m->cols; c++) {
            size_t idx = r * m->cols + c;
            if (idx >= m->len) {
                printf("<empty>");
                if (c + 1 < m->cols) printf("\t");
                continue;
            }
            if (!m->is_table) {
                switch (m->elem_tag) {
                case LX_TAG_INT:    printf("%lld", (long long)((int64_t *)m->data)[idx]); break;
                case LX_TAG_FLOAT:  printf("%.2f", ((double *)m->data)[idx]); break;
                case LX_TAG_CHAR:   printf("%c", ((char *)m->data)[idx]); break;
                case LX_TAG_STRING: printf("%s", ((char **)m->data)[idx] ? ((char **)m->data)[idx] : ""); break;
                case LX_TAG_BOOL:   printf("%s", ((int64_t *)m->data)[idx] ? "true" : "false"); break;
                default:            printf("?"); break;
                }
            } else {
                LxCell *cell = &((LxCell *)m->data)[idx];
                switch (cell->tag) {
                case LX_TAG_INT:    printf("%lld", (long long)cell->i); break;
                case LX_TAG_FLOAT:  printf("%.2f", cell->d); break;
                case LX_TAG_CHAR:   printf("%c", cell->c); break;
                case LX_TAG_STRING: printf("%s", cell->s ? cell->s : ""); break;
                case LX_TAG_BOOL:   printf("%s", cell->i ? "true" : "false"); break;
                default:            printf("<empty>"); break;
                }
            }
            if (c + 1 < m->cols) printf("\t");
        }
        printf("\n");
    }
}

int64_t lx_len(int64_t handle) {
    LxCollection *c = rt_get_collection(handle);
    return (int64_t)c->len;
}

int64_t lx_rows(int64_t handle) {
    LxCollection *c = rt_get_collection(handle);
    if (!c->is_matrix) {
        fprintf(stderr, "Runtime error: rows of requires a matrix handle.\n");
        exit(EXIT_FAILURE);
    }
    if (c->cols == 0) return 0;
    return (int64_t)((c->len + c->cols - 1) / c->cols);
}

int64_t lx_cols(int64_t handle) {
    LxCollection *c = rt_get_collection(handle);
    if (!c->is_matrix) {
        fprintf(stderr, "Runtime error: cols of requires a matrix handle.\n");
        exit(EXIT_FAILURE);
    }
    return (int64_t)c->cols;
}

int64_t lx_strlen(const char *s) {
    if (!s) return 0;
    return (int64_t)strlen(s);
}

int64_t lx_str_at(const char *s, int64_t index) {
    if (!s) {
        fprintf(stderr, "Runtime error: string is null.\n");
        exit(EXIT_FAILURE);
    }
    if (index < 0) {
        fprintf(stderr, "Runtime error: negative string index.\n");
        exit(EXIT_FAILURE);
    }
    size_t idx = (size_t)index;
    size_t len = strlen(s);
    if (idx >= len) {
        fprintf(stderr, "Runtime error: string index %zu out of bounds [0..%zu).\n", idx, len);
        exit(EXIT_FAILURE);
    }
    return (int64_t)(unsigned char)s[idx];
}

const char *lx_str_concat(const char *a, const char *b) {
    const char *lhs = a ? a : "";
    const char *rhs = b ? b : "";
    size_t la = strlen(lhs);
    size_t lb = strlen(rhs);
    char *out = (char *)rt_xmalloc(la + lb + 1);
    memcpy(out, lhs, la);
    memcpy(out + la, rhs, lb + 1);
    return out;
}

const char *lx_str_minus(const char *a, const char *b) {
    const char *lhs = a ? a : "";
    const char *rhs = b ? b : "";
    size_t la = strlen(lhs);
    size_t lb = strlen(rhs);

    if (lb == 0 || la == 0 || lb > la) {
        return rt_xstrdup(lhs);
    }

    const char *hit = strstr(lhs, rhs);
    if (!hit) {
        return rt_xstrdup(lhs);
    }

    size_t prefix = (size_t)(hit - lhs);
    size_t out_len = la - lb;
    char *out = (char *)rt_xmalloc(out_len + 1);
    memcpy(out, lhs, prefix);
    memcpy(out + prefix, hit + lb, la - prefix - lb);
    out[out_len] = '\0';
    return out;
}

const char *lx_extract_from(const char *needle, const char *source) {
    const char *n = needle ? needle : "";
    const char *s = source ? source : "";
    if (n[0] == '\0') return rt_xstrdup("");
    if (!strstr(s, n)) return rt_xstrdup("");
    return rt_xstrdup(n);
}

const char *lx_extract_range(int64_t from, int64_t to, const char *source) {
    const char *s = source ? source : "";
    size_t len = strlen(s);
    if (from < 0 || to < 0 || from > to) return rt_xstrdup("");
    if ((uint64_t)to >= (uint64_t)len) return rt_xstrdup("");

    size_t start = (size_t)from;
    size_t n = (size_t)(to - from + 1);
    char *out = (char *)rt_xmalloc(n + 1);
    memcpy(out, s + start, n);
    out[n] = '\0';
    return out;
}

int64_t lx_str_contains(const char *source, const char *needle) {
    const char *s = source ? source : "";
    const char *n = needle ? needle : "";
    if (n[0] == '\0') return 1;
    return strstr(s, n) ? 1 : 0;
}

int64_t lx_str_contains_char(const char *source, int64_t ch) {
    const char *s = source ? source : "";
    char c = (char)(unsigned char)ch;
    return strchr(s, c) ? 1 : 0;
}

int64_t lx_str_position(const char *source, const char *needle) {
    const char *s = source ? source : "";
    const char *n = needle ? needle : "";
    if (n[0] == '\0') return 0;
    const char *hit = strstr(s, n);
    if (!hit) return -1;
    return (int64_t)(hit - s);
}

int64_t lx_str_position_char(const char *source, int64_t ch) {
    const char *s = source ? source : "";
    char c = (char)(unsigned char)ch;
    const char *hit = strchr(s, c);
    if (!hit) return -1;
    return (int64_t)(hit - s);
}

void lx_sort(int64_t handle, int64_t ascending) {
    LxCollection *c = rt_get_collection(handle);
    if (c->is_table) {
        fprintf(stderr, "Runtime error: sort works only on typed meshes.\n");
        exit(EXIT_FAILURE);
    }
    if (c->len <= 1) return;

    g_sort_descending = ascending ? 0 : 1;

    switch (c->elem_tag) {
    case LX_TAG_INT:
        qsort(c->data, c->len, sizeof(int64_t), rt_cmp_int64);
        break;
    case LX_TAG_FLOAT:
        qsort(c->data, c->len, sizeof(double), rt_cmp_float_frac);
        break;
    case LX_TAG_CHAR:
        qsort(c->data, c->len, sizeof(char), rt_cmp_char);
        break;
    case LX_TAG_STRING:
        qsort(c->data, c->len, sizeof(char *), rt_cmp_str_first);
        break;
    default:
        fprintf(stderr, "Runtime error: unsupported mesh type for sort.\n");
        exit(EXIT_FAILURE);
    }
}

void lx_set(int64_t handle, int64_t index,
            int64_t tag, int64_t i, double d, int64_t cch, const char *s) {
    LxCollection *c = rt_get_collection(handle);
    if (index < 0) {
        fprintf(stderr, "Runtime error: negative index.\n");
        exit(EXIT_FAILURE);
    }
    size_t idx = (size_t)index;
    rt_bounds_check(idx, c->len);

    if (!c->is_table) {
        if (c->elem_tag == LX_TAG_STRING) {
            char **arr = (char **)c->data;
            free(arr[idx]);
            arr[idx] = rt_value_to_string((LxTag)tag, i, d, cch, s);
            return;
        }
        if (c->elem_tag == LX_TAG_CHAR) {
            ((char *)c->data)[idx] = rt_value_to_char((LxTag)tag, i, d, cch, s);
            return;
        }
        if ((LxTag)tag != c->elem_tag) {
            fprintf(stderr, "Runtime error: type mismatch on array write.\n");
            exit(EXIT_FAILURE);
        }
        switch (c->elem_tag) {
        case LX_TAG_INT:    ((int64_t *)c->data)[idx] = i; break;
        case LX_TAG_FLOAT:  ((double  *)c->data)[idx] = d; break;
        case LX_TAG_CHAR:   ((char    *)c->data)[idx] = (char)cch; break;
        case LX_TAG_BOOL:   ((int64_t *)c->data)[idx] = (i != 0) ? 1 : 0; break;
        case LX_TAG_STRING: {
            char **arr = (char **)c->data;
            free(arr[idx]);
            arr[idx] = rt_xstrdup(s ? s : "");
            break;
        }
        default: break;
        }
        return;
    }

    LxCell *cells = (LxCell *)c->data;
    if (cells[idx].tag == LX_TAG_STRING) {
        free(cells[idx].s);
        cells[idx].s = NULL;
    }
    cells[idx].tag = (LxTag)tag;
    cells[idx].i = i;
    cells[idx].d = d;
    cells[idx].c = (char)cch;
    cells[idx].s = ((LxTag)tag == LX_TAG_STRING) ? rt_xstrdup(s ? s : "") : NULL;
}

static void rt_ensure_capacity(LxCollection *c, size_t need) {
    if (need <= c->cap) return;

    size_t next = c->cap ? c->cap : 4;
    while (next < need) {
        size_t doubled = next * 2;
        if (doubled < next) rt_oom();
        next = doubled;
    }

    if (!c->is_table) {
        switch (c->elem_tag) {
        case LX_TAG_INT:
            c->data = rt_xrealloc(c->data, next * sizeof(int64_t));
            break;
        case LX_TAG_FLOAT:
            c->data = rt_xrealloc(c->data, next * sizeof(double));
            break;
        case LX_TAG_CHAR:
            c->data = rt_xrealloc(c->data, next * sizeof(char));
            break;
        case LX_TAG_STRING:
            c->data = rt_xrealloc(c->data, next * sizeof(char *));
            break;
        case LX_TAG_BOOL:
            c->data = rt_xrealloc(c->data, next * sizeof(int64_t));
            break;
        default:
            fprintf(stderr, "Runtime error: invalid array element tag.\n");
            exit(EXIT_FAILURE);
        }
    } else {
        c->data = rt_xrealloc(c->data, next * sizeof(LxCell));
    }

    c->cap = next;
}

void lx_append(int64_t handle, int64_t index,
               int64_t tag, int64_t i, double d, int64_t cch, const char *s) {
    LxCollection *c = rt_get_collection(handle);

    if (index < -1) {
        fprintf(stderr, "Runtime error: append index cannot be less than -1.\n");
        exit(EXIT_FAILURE);
    }

    size_t pos = (index < 0) ? c->len : (size_t)index;
    if (pos > c->len) {
        fprintf(stderr, "Runtime error: append index %zu out of bounds [0..%zu].\n", pos, c->len);
        exit(EXIT_FAILURE);
    }

    if (!c->is_table) {
        rt_ensure_capacity(c, c->len + 1);

        if (c->elem_tag == LX_TAG_STRING) {
            char **arr = (char **)c->data;
            if (pos < c->len) memmove(&arr[pos + 1], &arr[pos], (c->len - pos) * sizeof(*arr));
            arr[pos] = rt_value_to_string((LxTag)tag, i, d, cch, s);
            c->len++;
            return;
        }

        if (c->elem_tag == LX_TAG_CHAR) {
            char *arr = (char *)c->data;
            if (pos < c->len) memmove(&arr[pos + 1], &arr[pos], (c->len - pos) * sizeof(*arr));
            arr[pos] = rt_value_to_char((LxTag)tag, i, d, cch, s);
            c->len++;
            return;
        }

        if ((LxTag)tag != c->elem_tag) {
            fprintf(stderr, "Runtime error: type mismatch on array append.\n");
            exit(EXIT_FAILURE);
        }

        switch (c->elem_tag) {
        case LX_TAG_INT: {
            int64_t *arr = (int64_t *)c->data;
            if (pos < c->len) memmove(&arr[pos + 1], &arr[pos], (c->len - pos) * sizeof(*arr));
            arr[pos] = i;
            break;
        }
        case LX_TAG_FLOAT: {
            double *arr = (double *)c->data;
            if (pos < c->len) memmove(&arr[pos + 1], &arr[pos], (c->len - pos) * sizeof(*arr));
            arr[pos] = d;
            break;
        }
        case LX_TAG_CHAR: {
            char *arr = (char *)c->data;
            if (pos < c->len) memmove(&arr[pos + 1], &arr[pos], (c->len - pos) * sizeof(*arr));
            arr[pos] = (char)cch;
            break;
        }
        case LX_TAG_STRING: {
            char **arr = (char **)c->data;
            if (pos < c->len) memmove(&arr[pos + 1], &arr[pos], (c->len - pos) * sizeof(*arr));
            arr[pos] = rt_xstrdup(s ? s : "");
            break;
        }
        case LX_TAG_BOOL: {
            int64_t *arr = (int64_t *)c->data;
            if (pos < c->len) memmove(&arr[pos + 1], &arr[pos], (c->len - pos) * sizeof(*arr));
            arr[pos] = (i != 0) ? 1 : 0;
            break;
        }
        default:
            fprintf(stderr, "Runtime error: invalid array element tag.\n");
            exit(EXIT_FAILURE);
        }

        c->len++;
        return;
    }

    LxTag t = (LxTag)tag;
    if (t != LX_TAG_INT && t != LX_TAG_FLOAT && t != LX_TAG_CHAR && t != LX_TAG_STRING && t != LX_TAG_BOOL) {
        fprintf(stderr, "Runtime error: invalid table append value tag.\n");
        exit(EXIT_FAILURE);
    }

    LxCell *cells = (LxCell *)c->data;

    if (index >= 0) {
        for (size_t k = pos; k < c->len; k++) {
            if (cells[k].tag == LX_TAG_EMPTY) {
                cells[k].tag = t;
                cells[k].i = i;
                cells[k].d = d;
                cells[k].c = (char)cch;
                cells[k].s = (t == LX_TAG_STRING) ? rt_xstrdup(s ? s : "") : NULL;
                return;
            }
        }
    }

    rt_ensure_capacity(c, c->len + 1);
    cells = (LxCell *)c->data;
    cells[c->len].tag = t;
    cells[c->len].i = i;
    cells[c->len].d = d;
    cells[c->len].c = (char)cch;
    cells[c->len].s = (t == LX_TAG_STRING) ? rt_xstrdup(s ? s : "") : NULL;
    c->len++;
}

int64_t lx_count(int64_t handle,
                 int64_t tag, int64_t i, double d, int64_t cch, const char *s) {
    LxCollection *c = rt_get_collection(handle);
    LxTag t = (LxTag)tag;
    int64_t matches = 0;

    if (!c->is_table) {
        if (c->elem_tag == LX_TAG_STRING) {
            char **arr = (char **)c->data;
            char *needle = rt_value_to_string(t, i, d, cch, s);
            for (size_t k = 0; k < c->len; k++) if (strcmp(arr[k], needle) == 0) matches++;
            free(needle);
            return matches;
        }

        if (c->elem_tag == LX_TAG_CHAR) {
            char *arr = (char *)c->data;
            char want = rt_value_to_char(t, i, d, cch, s);
            for (size_t k = 0; k < c->len; k++) if (arr[k] == want) matches++;
            return matches;
        }

        if (t != c->elem_tag) return 0;
        switch (c->elem_tag) {
        case LX_TAG_INT: {
            int64_t *arr = (int64_t *)c->data;
            for (size_t k = 0; k < c->len; k++) if (arr[k] == i) matches++;
            break;
        }
        case LX_TAG_FLOAT: {
            double *arr = (double *)c->data;
            for (size_t k = 0; k < c->len; k++) if (arr[k] == d) matches++;
            break;
        }
        case LX_TAG_CHAR: {
            char *arr = (char *)c->data;
            char ch = (char)cch;
            for (size_t k = 0; k < c->len; k++) if (arr[k] == ch) matches++;
            break;
        }
        case LX_TAG_STRING: {
            char **arr = (char **)c->data;
            const char *needle = s ? s : "";
            for (size_t k = 0; k < c->len; k++) if (strcmp(arr[k], needle) == 0) matches++;
            break;
        }
        case LX_TAG_BOOL: {
            int64_t *arr = (int64_t *)c->data;
            int want = (i != 0) ? 1 : 0;
            for (size_t k = 0; k < c->len; k++) if (((arr[k] != 0) ? 1 : 0) == want) matches++;
            break;
        }
        default:
            break;
        }
        return matches;
    }

    LxCell *cells = (LxCell *)c->data;
    const char *needle = s ? s : "";
    for (size_t k = 0; k < c->len; k++) {
        if (cells[k].tag != t) continue;
        switch (t) {
        case LX_TAG_INT:
            if (cells[k].i == i) matches++;
            break;
        case LX_TAG_FLOAT:
            if (cells[k].d == d) matches++;
            break;
        case LX_TAG_CHAR:
            if (cells[k].c == (char)cch) matches++;
            break;
        case LX_TAG_STRING:
            if (cells[k].s && strcmp(cells[k].s, needle) == 0) matches++;
            break;
        case LX_TAG_BOOL:
            if (((cells[k].i != 0) ? 1 : 0) == ((i != 0) ? 1 : 0)) matches++;
            break;
        default:
            break;
        }
    }
    return matches;
}

int64_t lx_position(int64_t handle,
                    int64_t tag, int64_t i, double d, int64_t cch, const char *s) {
    LxCollection *c = rt_get_collection(handle);
    LxTag t = (LxTag)tag;

    if (!c->is_table) {
        if (c->elem_tag == LX_TAG_STRING) {
            char **arr = (char **)c->data;
            char *needle = rt_value_to_string(t, i, d, cch, s);
            for (size_t k = 0; k < c->len; k++) {
                if (strcmp(arr[k], needle) == 0) {
                    free(needle);
                    return (int64_t)k;
                }
            }
            free(needle);
            return -1;
        }

        if (c->elem_tag == LX_TAG_CHAR) {
            char *arr = (char *)c->data;
            char want = rt_value_to_char(t, i, d, cch, s);
            for (size_t k = 0; k < c->len; k++) if (arr[k] == want) return (int64_t)k;
            return -1;
        }

        if (t != c->elem_tag) return -1;
        switch (c->elem_tag) {
        case LX_TAG_INT: {
            int64_t *arr = (int64_t *)c->data;
            for (size_t k = 0; k < c->len; k++) if (arr[k] == i) return (int64_t)k;
            break;
        }
        case LX_TAG_FLOAT: {
            double *arr = (double *)c->data;
            for (size_t k = 0; k < c->len; k++) if (arr[k] == d) return (int64_t)k;
            break;
        }
        case LX_TAG_CHAR: {
            char *arr = (char *)c->data;
            char ch = (char)cch;
            for (size_t k = 0; k < c->len; k++) if (arr[k] == ch) return (int64_t)k;
            break;
        }
        case LX_TAG_STRING: {
            char **arr = (char **)c->data;
            const char *needle = s ? s : "";
            for (size_t k = 0; k < c->len; k++) if (strcmp(arr[k], needle) == 0) return (int64_t)k;
            break;
        }
        case LX_TAG_BOOL: {
            int64_t *arr = (int64_t *)c->data;
            int want = (i != 0) ? 1 : 0;
            for (size_t k = 0; k < c->len; k++) if (((arr[k] != 0) ? 1 : 0) == want) return (int64_t)k;
            break;
        }
        default:
            break;
        }
        return -1;
    }

    LxCell *cells = (LxCell *)c->data;
    const char *needle = s ? s : "";
    for (size_t k = 0; k < c->len; k++) {
        if (cells[k].tag != t) continue;
        switch (t) {
        case LX_TAG_INT:
            if (cells[k].i == i) return (int64_t)k;
            break;
        case LX_TAG_FLOAT:
            if (cells[k].d == d) return (int64_t)k;
            break;
        case LX_TAG_CHAR:
            if (cells[k].c == (char)cch) return (int64_t)k;
            break;
        case LX_TAG_STRING:
            if (cells[k].s && strcmp(cells[k].s, needle) == 0) return (int64_t)k;
            break;
        case LX_TAG_BOOL:
            if (((cells[k].i != 0) ? 1 : 0) == ((i != 0) ? 1 : 0)) return (int64_t)k;
            break;
        default:
            break;
        }
    }
    return -1;
}

static int64_t rt_register_file(LxFile *f) {
    if (g_files_n == g_files_cap) {
        size_t next = g_files_cap ? g_files_cap * 2 : 8;
        if (next < g_files_cap) rt_oom();
        g_files = (LxFile **)rt_xrealloc(g_files, next * sizeof(*g_files));
        g_files_cap = next;
    }
    g_files[g_files_n] = f;
    return (int64_t)(g_files_n++);
}

static LxFile *rt_get_file(int64_t handle) {
    if (handle < 0 || (size_t)handle >= g_files_n || !g_files[handle]) {
        fprintf(stderr, "Runtime error: invalid file handle %lld.\n", (long long)handle);
        exit(EXIT_FAILURE);
    }
    return g_files[handle];
}

static char *rt_trim_line_end(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
    return s;
}

static char **rt_file_load_lines(const char *path, size_t *out_count) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        *out_count = 0;
        return NULL;
    }

    char **lines = NULL;
    size_t count = 0;
    size_t cap = 0;
    char buf[4096];
    while (fgets(buf, sizeof buf, fp)) {
        rt_trim_line_end(buf);
        if (count == cap) {
            size_t next = cap ? cap * 2 : 16;
            lines = (char **)rt_xrealloc(lines, next * sizeof(char *));
            cap = next;
        }
        lines[count++] = rt_xstrdup(buf);
    }
    fclose(fp);
    *out_count = count;
    return lines;
}

static void rt_file_free_lines(char **lines, size_t count) {
    for (size_t i = 0; i < count; i++) free(lines[i]);
    free(lines);
}

static void rt_file_store_lines(const char *path, char **lines, size_t count) {
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "Runtime error: cannot write file '%s'.\n", path);
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < count; i++) {
        fputs(lines[i] ? lines[i] : "", fp);
        fputc('\n', fp);
    }
    fclose(fp);
}

int64_t lx_file_open(const char *path, int64_t make_if_missing) {
    if (!path) {
        fprintf(stderr, "Runtime error: file path is null.\n");
        exit(EXIT_FAILURE);
    }
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        if (!make_if_missing) {
            fprintf(stderr, "Runtime error: cannot open file '%s'.\n", path);
            exit(EXIT_FAILURE);
        }
        fp = fopen(path, "wb");
        if (!fp) {
            fprintf(stderr, "Runtime error: cannot create file '%s'.\n", path);
            exit(EXIT_FAILURE);
        }
    }
    fclose(fp);

    LxFile *f = (LxFile *)rt_xmalloc(sizeof(*f));
    f->path = rt_xstrdup(path);
    return rt_register_file(f);
}

void lx_file_close(int64_t handle) {
    if (handle < 0 || (size_t)handle >= g_files_n) {
        fprintf(stderr, "Runtime error: invalid file handle %lld.\n", (long long)handle);
        exit(EXIT_FAILURE);
    }
    LxFile *f = g_files[handle];
    if (!f) return;
    free(f->path);
    free(f);
    g_files[handle] = NULL;
}

void lx_file_write(int64_t handle, const char *value) {
    LxFile *f = rt_get_file(handle);
    FILE *fp = fopen(f->path, "ab");
    if (!fp) {
        fprintf(stderr, "Runtime error: cannot append file '%s'.\n", f->path);
        exit(EXIT_FAILURE);
    }
    fputs(value ? value : "", fp);
    fputc('\n', fp);
    fclose(fp);
}

void lx_file_write_line(int64_t handle, int64_t line, const char *value) {
    LxFile *f = rt_get_file(handle);
    if (line < 0) {
        fprintf(stderr, "Runtime error: line index cannot be negative.\n");
        exit(EXIT_FAILURE);
    }

    size_t count = 0;
    char **lines = rt_file_load_lines(f->path, &count);
    size_t idx = (size_t)line;
    if (idx >= count) {
        lines = (char **)rt_xrealloc(lines, (idx + 1) * sizeof(char *));
        for (size_t i = count; i <= idx; i++) lines[i] = rt_xstrdup("");
        count = idx + 1;
    }
    free(lines[idx]);
    lines[idx] = rt_xstrdup(value ? value : "");
    rt_file_store_lines(f->path, lines, count);
    rt_file_free_lines(lines, count);
}

const char *lx_file_read_line(int64_t handle, int64_t line) {
    LxFile *f = rt_get_file(handle);
    if (line < 0) {
        fprintf(stderr, "Runtime error: line index cannot be negative.\n");
        exit(EXIT_FAILURE);
    }
    size_t count = 0;
    char **lines = rt_file_load_lines(f->path, &count);
    static char empty[1] = {0};
    const char *ret = empty;
    if ((size_t)line < count) ret = lines[line];
    char *out = rt_xstrdup(ret);
    rt_file_free_lines(lines, count);
    return out;
}

int64_t lx_file_read_all(int64_t handle) {
    LxFile *f = rt_get_file(handle);
    size_t count = 0;
    char **lines = rt_file_load_lines(f->path, &count);
    int64_t table = lx_table_new(0);
    for (size_t i = 0; i < count; i++) {
        lx_append(table, -1, LX_TAG_STRING, 0, 0.0, 0, lines[i]);
    }
    rt_file_free_lines(lines, count);
    return table;
}

int64_t lx_file_read_first(int64_t handle, int64_t n) {
    if (n < 0) n = 0;
    LxFile *f = rt_get_file(handle);
    size_t count = 0;
    char **lines = rt_file_load_lines(f->path, &count);
    int64_t table = lx_table_new(0);
    size_t lim = (size_t)n;
    if (lim > count) lim = count;
    for (size_t i = 0; i < lim; i++) {
        lx_append(table, -1, LX_TAG_STRING, 0, 0.0, 0, lines[i]);
    }
    rt_file_free_lines(lines, count);
    return table;
}

int64_t lx_file_read_range(int64_t handle, int64_t n, int64_t from_line) {
    if (n < 0) n = 0;
    if (from_line < 0) from_line = 0;
    LxFile *f = rt_get_file(handle);
    size_t count = 0;
    char **lines = rt_file_load_lines(f->path, &count);
    int64_t table = lx_table_new(0);
    size_t start = (size_t)from_line;
    size_t lim = (size_t)n;
    for (size_t i = 0; i < lim && (start + i) < count; i++) {
        lx_append(table, -1, LX_TAG_STRING, 0, 0.0, 0, lines[start + i]);
    }
    rt_file_free_lines(lines, count);
    return table;
}

void lx_file_clear(int64_t handle) {
    LxFile *f = rt_get_file(handle);
    FILE *fp = fopen(f->path, "wb");
    if (!fp) {
        fprintf(stderr, "Runtime error: cannot clear file '%s'.\n", f->path);
        exit(EXIT_FAILURE);
    }
    fclose(fp);
}

void lx_file_clear_line(int64_t handle, int64_t line) {
    if (line < 0) {
        fprintf(stderr, "Runtime error: clear line index cannot be negative.\n");
        exit(EXIT_FAILURE);
    }
    LxFile *f = rt_get_file(handle);
    size_t count = 0;
    char **lines = rt_file_load_lines(f->path, &count);
    if ((size_t)line < count) {
        free(lines[line]);
        for (size_t i = (size_t)line; i + 1 < count; i++) lines[i] = lines[i + 1];
        count--;
    }
    rt_file_store_lines(f->path, lines, count);
    rt_file_free_lines(lines, count);
}

void lx_file_clear_range(int64_t handle, int64_t from_line, int64_t to_line) {
    if (from_line < 0) from_line = 0;
    if (to_line < from_line) return;
    LxFile *f = rt_get_file(handle);
    size_t count = 0;
    char **lines = rt_file_load_lines(f->path, &count);
    size_t from = (size_t)from_line;
    size_t to = (size_t)to_line;
    if (from < count) {
        if (to >= count) to = count - 1;
        size_t remove_n = (to >= from) ? (to - from + 1) : 0;
        for (size_t i = from; i + remove_n < count; i++) lines[i] = lines[i + remove_n];
        count -= remove_n;
    }
    rt_file_store_lines(f->path, lines, count);
    rt_file_free_lines(lines, count);
}

void lx_file_set_title(int64_t handle, const char *title) {
    LxFile *f = rt_get_file(handle);
    if (!title || !title[0]) return;
    if (rename(f->path, title) != 0) {
        fprintf(stderr, "Runtime error: cannot rename '%s' to '%s'.\n", f->path, title);
        exit(EXIT_FAILURE);
    }
    free(f->path);
    f->path = rt_xstrdup(title);
}

int64_t lx_file_line_count(int64_t handle) {
    LxFile *f = rt_get_file(handle);
    size_t count = 0;
    char **lines = rt_file_load_lines(f->path, &count);
    rt_file_free_lines(lines, count);
    return (int64_t)count;
}

int64_t lx_get_tag(int64_t handle, int64_t index) {
    LxCollection *c = rt_get_collection(handle);
    if (index < 0) { fprintf(stderr, "Runtime error: negative index.\n"); exit(EXIT_FAILURE); }
    size_t idx = (size_t)index;
    rt_bounds_check(idx, c->len);
    if (!c->is_table) return (int64_t)c->elem_tag;
    return (int64_t)((LxCell *)c->data)[idx].tag;
}

int64_t lx_get_i(int64_t handle, int64_t index) {
    LxCollection *c = rt_get_collection(handle);
    if (index < 0) { fprintf(stderr, "Runtime error: negative index.\n"); exit(EXIT_FAILURE); }
    size_t idx = (size_t)index;
    rt_bounds_check(idx, c->len);
    if (!c->is_table) {
        if (c->elem_tag != LX_TAG_INT && c->elem_tag != LX_TAG_BOOL) { fprintf(stderr, "Runtime error: array get int type mismatch.\n"); exit(EXIT_FAILURE); }
        return ((int64_t *)c->data)[idx];
    }
    LxCell *cell = &((LxCell *)c->data)[idx];
    if (cell->tag != LX_TAG_INT && cell->tag != LX_TAG_BOOL) { fprintf(stderr, "Runtime error: table cell is not int.\n"); exit(EXIT_FAILURE); }
    return cell->i;
}

double lx_get_d(int64_t handle, int64_t index) {
    LxCollection *c = rt_get_collection(handle);
    if (index < 0) { fprintf(stderr, "Runtime error: negative index.\n"); exit(EXIT_FAILURE); }
    size_t idx = (size_t)index;
    rt_bounds_check(idx, c->len);
    if (!c->is_table) {
        if (c->elem_tag != LX_TAG_FLOAT) { fprintf(stderr, "Runtime error: array get float type mismatch.\n"); exit(EXIT_FAILURE); }
        return ((double *)c->data)[idx];
    }
    LxCell *cell = &((LxCell *)c->data)[idx];
    if (cell->tag != LX_TAG_FLOAT) { fprintf(stderr, "Runtime error: table cell is not float.\n"); exit(EXIT_FAILURE); }
    return cell->d;
}

int64_t lx_get_c(int64_t handle, int64_t index) {
    LxCollection *c = rt_get_collection(handle);
    if (index < 0) { fprintf(stderr, "Runtime error: negative index.\n"); exit(EXIT_FAILURE); }
    size_t idx = (size_t)index;
    rt_bounds_check(idx, c->len);
    if (!c->is_table) {
        if (c->elem_tag != LX_TAG_CHAR) { fprintf(stderr, "Runtime error: array get char type mismatch.\n"); exit(EXIT_FAILURE); }
        return (int64_t)((char *)c->data)[idx];
    }
    LxCell *cell = &((LxCell *)c->data)[idx];
    if (cell->tag != LX_TAG_CHAR) { fprintf(stderr, "Runtime error: table cell is not char.\n"); exit(EXIT_FAILURE); }
    return (int64_t)cell->c;
}

const char *lx_get_s(int64_t handle, int64_t index) {
    LxCollection *c = rt_get_collection(handle);
    if (index < 0) { fprintf(stderr, "Runtime error: negative index.\n"); exit(EXIT_FAILURE); }
    size_t idx = (size_t)index;
    rt_bounds_check(idx, c->len);
    if (!c->is_table) {
        if (c->elem_tag != LX_TAG_STRING) { fprintf(stderr, "Runtime error: array get string type mismatch.\n"); exit(EXIT_FAILURE); }
        return ((char **)c->data)[idx];
    }
    LxCell *cell = &((LxCell *)c->data)[idx];
    if (cell->tag != LX_TAG_STRING) { fprintf(stderr, "Runtime error: table cell is not string.\n"); exit(EXIT_FAILURE); }
    return cell->s ? cell->s : "";
}

void lx_cleanup(void) {
    for (size_t h = 0; h < g_collections_n; h++) {
        LxCollection *c = g_collections[h];
        if (!c) continue;
        if (!c->is_table) {
            if (c->elem_tag == LX_TAG_STRING) {
                char **arr = (char **)c->data;
                for (size_t i = 0; i < c->len; i++) free(arr[i]);
            }
            free(c->data);
        } else {
            LxCell *cells = (LxCell *)c->data;
            for (size_t i = 0; i < c->len; i++) {
                if (cells[i].tag == LX_TAG_STRING) free(cells[i].s);
            }
            free(c->data);
        }
        free(c);
        g_collections[h] = NULL;
    }
    free(g_collections);
    g_collections = NULL;
    g_collections_n = 0;
    g_collections_cap = 0;

    for (size_t i = 0; i < g_files_n; i++) {
        if (!g_files[i]) continue;
        free(g_files[i]->path);
        free(g_files[i]);
        g_files[i] = NULL;
    }
    free(g_files);
    g_files = NULL;
    g_files_n = 0;
    g_files_cap = 0;
}

void lx_print_cell(int64_t handle, int64_t index) {
    LxCollection *c = rt_get_collection(handle);
    if (!c->is_table) {
        fprintf(stderr, "Runtime error: lx_print_cell requires a table handle.\n");
        exit(EXIT_FAILURE);
    }
    if (index < 0) { fprintf(stderr, "Runtime error: negative index.\n"); exit(EXIT_FAILURE); }
    size_t idx = (size_t)index;
    rt_bounds_check(idx, c->len);
    LxCell *cell = &((LxCell *)c->data)[idx];
    switch (cell->tag) {
    case LX_TAG_INT:    printf("%lld", (long long)cell->i); break;
    case LX_TAG_FLOAT:  printf("%.2f", cell->d); break;
    case LX_TAG_CHAR:   printf("%c", cell->c); break;
    case LX_TAG_STRING: printf("%s", cell->s ? cell->s : ""); break;
    case LX_TAG_BOOL:   printf("%s", cell->i ? "true" : "false"); break;
    case LX_TAG_EMPTY:
    default:            printf("<empty>"); break;
    }
}

static void rt_print_hline(size_t cells, int width) {
    for (size_t c = 0; c < cells; c++) {
        putchar('+');
        for (int i = 0; i < width + 2; i++) putchar('-');
    }
    puts("+");
}

static void rt_print_cell(const char *text, int width, int last) {
    printf("| %-*.*s ", width, width, text ? text : "");
    if (last) puts("|");
}

static void rt_format_array_value(char *buf, size_t bufsz,
                                  const LxCollection *c, size_t idx) {
    switch (c->elem_tag) {
    case LX_TAG_INT:
        snprintf(buf, bufsz, "%lld", (long long)((int64_t *)c->data)[idx]);
        break;
    case LX_TAG_FLOAT:
        snprintf(buf, bufsz, "%.2f", ((double *)c->data)[idx]);
        break;
    case LX_TAG_CHAR:
        snprintf(buf, bufsz, "'%c'", ((char *)c->data)[idx]);
        break;
    case LX_TAG_STRING:
        snprintf(buf, bufsz, "\"%.36s\"", ((char **)c->data)[idx] ? ((char **)c->data)[idx] : "");
        break;
    case LX_TAG_BOOL:
        snprintf(buf, bufsz, "%s", ((int64_t *)c->data)[idx] ? "true" : "false");
        break;
    default:
        snprintf(buf, bufsz, "?");
        break;
    }
}

static void rt_format_table_value(char *buf, size_t bufsz,
                                  const LxCollection *c, size_t idx) {
    const LxCell *cell = &((LxCell *)c->data)[idx];
    switch (cell->tag) {
    case LX_TAG_INT:
        snprintf(buf, bufsz, "%lld", (long long)cell->i);
        break;
    case LX_TAG_FLOAT:
        snprintf(buf, bufsz, "%.2f", cell->d);
        break;
    case LX_TAG_CHAR:
        snprintf(buf, bufsz, "'%c'", cell->c);
        break;
    case LX_TAG_STRING:
        snprintf(buf, bufsz, "\"%.36s\"", cell->s ? cell->s : "");
        break;
    case LX_TAG_BOOL:
        snprintf(buf, bufsz, "%s", cell->i ? "true" : "false");
        break;
    case LX_TAG_EMPTY:
    default:
        snprintf(buf, bufsz, "empty");
        break;
    }
}

void lx_print_array_ascii(int64_t handle, const char *name) {
    LxCollection *c = rt_get_collection(handle);
    if (c->is_table) {
        fprintf(stderr, "Runtime error: lx_print_array_ascii requires an array handle.\n");
        exit(EXIT_FAILURE);
    }

    printf("%s[%zu]\n", name ? name : "array", c->len);
    puts("idx   val");

    if (c->len == 0) {
        return;
    }

    for (size_t i = 0; i < c->len; i++) {
        char b[128];
        rt_format_array_value(b, sizeof b, c, i);
        printf("%-5zu %s\n", i, b);
    }
}

void lx_print_table_ascii(int64_t handle, const char *name) {
    LxCollection *c = rt_get_collection(handle);
    if (!c->is_table) {
        fprintf(stderr, "Runtime error: lx_print_table_ascii requires a table handle.\n");
        exit(EXIT_FAILURE);
    }

    const int width = 12;
    const size_t chunk = 8;
    printf("%s:\n", name ? name : "table");

    if (c->len == 0) {
        puts("<empty>");
        puts("size = 0");
        return;
    }

    for (size_t start = 0; start < c->len; start += chunk) {
        size_t end = start + chunk;
        if (end > c->len) end = c->len;
        size_t cols = end - start;

        rt_print_hline(cols + 1, width);
        rt_print_cell("idx", width, 0);
        for (size_t i = start; i < end; i++) {
            char b[32];
            snprintf(b, sizeof b, "%zu", i);
            rt_print_cell(b, width, i + 1 == end);
        }

        rt_print_hline(cols + 1, width);
        rt_print_cell("val", width, 0);
        for (size_t i = start; i < end; i++) {
            char b[128];
            rt_format_table_value(b, sizeof b, c, i);
            rt_print_cell(b, width, i + 1 == end);
        }
        rt_print_hline(cols + 1, width);
    }

    printf("size = %zu\n", c->len);
}

typedef enum {
    CG_SCALAR,
    CG_ARRAY,
    CG_TABLE,
    CG_MATRIX
} CGVarKind;

typedef struct {
    char *name;
    CGVarKind kind;
    TypeKind type;
    LLVMValueRef alloca_;
} CGVar;

typedef struct { CGVar *v; size_t n; size_t cap; } CGVars;

static CGVars g_global_vars;
static LLVMTypeRef llvm_type(TypeKind k);
static LLVMValueRef default_value(TypeKind k);

static void cgvars_init(CGVars *t) { t->v = NULL; t->n = 0; t->cap = 0; }
static void cgvars_free(CGVars *t) { for (size_t i = 0; i < t->n; i++) free(t->v[i].name); free(t->v); }

static void cgvars_add(CGVars *t, const char *name, CGVarKind kind, TypeKind type, LLVMValueRef a) {
    if (t->n == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 16;
        t->v = (CGVar *)realloc(t->v, t->cap * sizeof(CGVar));
        if (!t->v) { fprintf(stderr, "OOM\n"); exit(1); }
    }
    size_t l = strlen(name);
    char *d = (char *)malloc(l + 1);
    if (!d) { fprintf(stderr, "OOM\n"); exit(1); }
    memcpy(d, name, l + 1);
    t->v[t->n++] = (CGVar){ d, kind, type, a };
}

static CGVar *cgvars_find_local(CGVars *t, const char *name) {
    for (size_t i = 0; i < t->n; i++) {
        if (strcmp(t->v[i].name, name) == 0) return &t->v[i];
    }
    return NULL;
}

static CGVar *cgvars_find(CGVars *t, const char *name) {
    CGVar *v = cgvars_find_local(t, name);
    if (v) return v;
    return cgvars_find_local(&g_global_vars, name);
}

static void declare_object_globals_from_stmt(const Stmt *s, LLVMModuleRef mod) {
    if (!s) return;
    switch (s->kind) {
    case STMT_DECL: {
        const DeclStmt *d = &s->as.decl;
        for (size_t i = 0; i < d->count; i++) {
            if (!strchr(d->names[i], '_')) continue;
            if (cgvars_find_local(&g_global_vars, d->names[i])) continue;
            LLVMValueRef g = LLVMAddGlobal(mod, llvm_type(d->type), d->names[i]);
            LLVMSetInitializer(g, default_value(d->type));
            cgvars_add(&g_global_vars, d->names[i], CG_SCALAR, d->type, g);
        }
        break;
    }
    case STMT_BLOCK:
        for (size_t i = 0; i < s->as.block.count; i++)
            declare_object_globals_from_stmt(s->as.block.stmts[i], mod);
        break;
    case STMT_FUNCDEF:
        break;
    default:
        break;
    }
}

typedef struct {
    char       *name;
    TypeKind    return_type;
    TypeKind   *param_types;
    size_t      param_count;
    LLVMTypeRef fn_type;
    LLVMValueRef fn;
} CGFunc;

typedef struct { CGFunc *f; size_t n; size_t cap; } CGFuncs;

static void cgfuncs_init(CGFuncs *t) { t->f = NULL; t->n = 0; t->cap = 0; }

static void cgfuncs_free(CGFuncs *t) {
    for (size_t i = 0; i < t->n; i++) {
        free(t->f[i].name);
        free(t->f[i].param_types);
    }
    free(t->f);
}

static CGFunc *cgfuncs_find(CGFuncs *t, const char *name) {
    for (size_t i = 0; i < t->n; i++) {
        if (strcmp(t->f[i].name, name) == 0) return &t->f[i];
    }
    return NULL;
}

static void cgfuncs_add(CGFuncs *t, const char *name, TypeKind return_type,
                        const Param *params, size_t param_count,
                        LLVMTypeRef fn_type, LLVMValueRef fn) {
    if (t->n == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 8;
        t->f = (CGFunc *)realloc(t->f, t->cap * sizeof(CGFunc));
        if (!t->f) { fprintf(stderr, "OOM\n"); exit(1); }
    }

    size_t l = strlen(name);
    char *d = (char *)malloc(l + 1);
    if (!d) { fprintf(stderr, "OOM\n"); exit(1); }
    memcpy(d, name, l + 1);

    TypeKind *pt = NULL;
    if (param_count) {
        pt = (TypeKind *)malloc(param_count * sizeof(TypeKind));
        if (!pt) { fprintf(stderr, "OOM\n"); exit(1); }
        for (size_t i = 0; i < param_count; i++)
            pt[i] = params[i].type;
    }

    t->f[t->n++] = (CGFunc){ d, return_type, pt, param_count, fn_type, fn };
}

typedef struct {
    LLVMValueRef array_new;
    LLVMValueRef table_new;
    LLVMValueRef matrix_new;
    LLVMValueRef len;
    LLVMValueRef rows;
    LLVMValueRef cols;
    LLVMValueRef strlen_fn;
    LLVMValueRef str_at;
    LLVMValueRef str_concat;
    LLVMValueRef str_minus;
    LLVMValueRef i64_to_string;
    LLVMValueRef f64_to_string;
    LLVMValueRef char_to_string;
    LLVMValueRef bool_to_string;
    LLVMValueRef string_to_i64;
    LLVMValueRef string_to_f64;
    LLVMValueRef string_to_char;
    LLVMValueRef extract;
    LLVMValueRef extract_range;
    LLVMValueRef str_contains;
    LLVMValueRef str_contains_char;
    LLVMValueRef str_position;
    LLVMValueRef str_position_char;
    LLVMValueRef count;
    LLVMValueRef position;
    LLVMValueRef sort;
    LLVMValueRef set;
    LLVMValueRef append;
    LLVMValueRef get_tag;
    LLVMValueRef get_i;
    LLVMValueRef get_d;
    LLVMValueRef get_c;
    LLVMValueRef get_s;
    LLVMValueRef matrix_set;
    LLVMValueRef matrix_get_i;
    LLVMValueRef matrix_get_d;
    LLVMValueRef matrix_get_c;
    LLVMValueRef matrix_get_s;
    LLVMValueRef print_cell;
    LLVMValueRef print_array_ascii;
    LLVMValueRef print_table_ascii;
    LLVMValueRef print_matrix_ascii;
    LLVMValueRef file_open;
    LLVMValueRef file_close;
    LLVMValueRef file_write;
    LLVMValueRef file_write_line;
    LLVMValueRef file_read_all;
    LLVMValueRef file_read_line;
    LLVMValueRef file_read_first;
    LLVMValueRef file_read_range;
    LLVMValueRef file_clear;
    LLVMValueRef file_clear_line;
    LLVMValueRef file_clear_range;
    LLVMValueRef file_set_title;
    LLVMValueRef file_line_count;
    LLVMValueRef cleanup;
} RuntimeFns;

static int fmt_counter = 0;
static int label_counter = 0;

static LLVMValueRef build_global_str(LLVMModuleRef mod, LLVMBuilderRef b, const char *s) {
    char name[32];
    snprintf(name, sizeof name, ".fmt.%d", fmt_counter++);
    LLVMValueRef gs = LLVMBuildGlobalStringPtr(b, s, name);
    (void)mod;
    return gs;
}

static LLVMTypeRef llvm_type(TypeKind k) {
    switch (k) {
        case TYPE_INT:    return LLVMInt64Type();
        case TYPE_FLOAT:  return LLVMDoubleType();
        case TYPE_CHAR:   return LLVMInt8Type();
        case TYPE_STRING: return LLVMPointerType(LLVMInt8Type(), 0);
        case TYPE_BOOL:   return LLVMInt1Type();
    }
    return LLVMInt64Type();
}

static LLVMValueRef default_value(TypeKind k) {
    switch (k) {
        case TYPE_INT:    return LLVMConstInt(LLVMInt64Type(), 0, 0);
        case TYPE_FLOAT:  return LLVMConstReal(LLVMDoubleType(), 0.0);
        case TYPE_CHAR:   return LLVMConstInt(LLVMInt8Type(), 0, 0);
        case TYPE_STRING: return LLVMConstPointerNull(LLVMPointerType(LLVMInt8Type(), 0));
        case TYPE_BOOL:   return LLVMConstInt(LLVMInt1Type(), 0, 0);
    }
    return LLVMConstInt(LLVMInt64Type(), 0, 0);
}

static LLVMValueRef declare_printf(LLVMModuleRef mod) {
    LLVMTypeRef param = LLVMPointerType(LLVMInt8Type(), 0);
    LLVMTypeRef ft = LLVMFunctionType(LLVMInt32Type(), &param, 1, 1);
    LLVMValueRef fn = LLVMGetNamedFunction(mod, "printf");
    if (!fn) fn = LLVMAddFunction(mod, "printf", ft);
    return fn;
}

static LLVMValueRef declare_scanf(LLVMModuleRef mod) {
    LLVMTypeRef param = LLVMPointerType(LLVMInt8Type(), 0);
    LLVMTypeRef ft = LLVMFunctionType(LLVMInt32Type(), &param, 1, 1);
    LLVMValueRef fn = LLVMGetNamedFunction(mod, "scanf");
    if (!fn) fn = LLVMAddFunction(mod, "scanf", ft);
    return fn;
}

static RuntimeFns declare_runtime(LLVMModuleRef mod) {
    RuntimeFns rt;

    LLVMTypeRef i64 = LLVMInt64Type();
    LLVMTypeRef i8p = LLVMPointerType(LLVMInt8Type(), 0);
    LLVMTypeRef f64 = LLVMDoubleType();
    LLVMTypeRef voidt = LLVMVoidType();

    rt.array_new = LLVMAddFunction(mod, "lx_array_new", LLVMFunctionType(i64, (LLVMTypeRef[]){i64, i64}, 2, 0));
    rt.table_new = LLVMAddFunction(mod, "lx_table_new", LLVMFunctionType(i64, (LLVMTypeRef[]){i64}, 1, 0));
    rt.matrix_new= LLVMAddFunction(mod, "lx_matrix_new", LLVMFunctionType(i64, (LLVMTypeRef[]){i64, i64, i64}, 3, 0));
    rt.len       = LLVMAddFunction(mod, "lx_len", LLVMFunctionType(i64, (LLVMTypeRef[]){i64}, 1, 0));
    rt.rows      = LLVMAddFunction(mod, "lx_rows", LLVMFunctionType(i64, (LLVMTypeRef[]){i64}, 1, 0));
    rt.cols      = LLVMAddFunction(mod, "lx_cols", LLVMFunctionType(i64, (LLVMTypeRef[]){i64}, 1, 0));
    rt.strlen_fn = LLVMAddFunction(mod, "lx_strlen", LLVMFunctionType(i64, (LLVMTypeRef[]){i8p}, 1, 0));
    rt.str_at    = LLVMAddFunction(mod, "lx_str_at", LLVMFunctionType(i64, (LLVMTypeRef[]){i8p,i64}, 2, 0));
    rt.str_concat= LLVMAddFunction(mod, "lx_str_concat", LLVMFunctionType(i8p, (LLVMTypeRef[]){i8p,i8p}, 2, 0));
    rt.str_minus = LLVMAddFunction(mod, "lx_str_minus", LLVMFunctionType(i8p, (LLVMTypeRef[]){i8p,i8p}, 2, 0));
    rt.i64_to_string = LLVMAddFunction(mod, "lx_i64_to_string", LLVMFunctionType(i8p, (LLVMTypeRef[]){i64}, 1, 0));
    rt.f64_to_string = LLVMAddFunction(mod, "lx_f64_to_string", LLVMFunctionType(i8p, (LLVMTypeRef[]){f64}, 1, 0));
    rt.char_to_string = LLVMAddFunction(mod, "lx_char_to_string", LLVMFunctionType(i8p, (LLVMTypeRef[]){i64}, 1, 0));
    rt.bool_to_string = LLVMAddFunction(mod, "lx_bool_to_string", LLVMFunctionType(i8p, (LLVMTypeRef[]){i64}, 1, 0));
    rt.string_to_i64 = LLVMAddFunction(mod, "lx_string_to_i64", LLVMFunctionType(i64, (LLVMTypeRef[]){i8p}, 1, 0));
    rt.string_to_f64 = LLVMAddFunction(mod, "lx_string_to_f64", LLVMFunctionType(f64, (LLVMTypeRef[]){i8p}, 1, 0));
    rt.string_to_char = LLVMAddFunction(mod, "lx_string_to_char", LLVMFunctionType(i64, (LLVMTypeRef[]){i8p}, 1, 0));
    rt.extract   = LLVMAddFunction(mod, "lx_extract_from", LLVMFunctionType(i8p, (LLVMTypeRef[]){i8p,i8p}, 2, 0));
    rt.extract_range = LLVMAddFunction(mod, "lx_extract_range", LLVMFunctionType(i8p, (LLVMTypeRef[]){i64,i64,i8p}, 3, 0));
    rt.str_contains = LLVMAddFunction(mod, "lx_str_contains", LLVMFunctionType(i64, (LLVMTypeRef[]){i8p,i8p}, 2, 0));
    rt.str_contains_char = LLVMAddFunction(mod, "lx_str_contains_char", LLVMFunctionType(i64, (LLVMTypeRef[]){i8p,i64}, 2, 0));
    rt.str_position = LLVMAddFunction(mod, "lx_str_position", LLVMFunctionType(i64, (LLVMTypeRef[]){i8p,i8p}, 2, 0));
    rt.str_position_char = LLVMAddFunction(mod, "lx_str_position_char", LLVMFunctionType(i64, (LLVMTypeRef[]){i8p,i64}, 2, 0));
    rt.count     = LLVMAddFunction(mod, "lx_count", LLVMFunctionType(i64, (LLVMTypeRef[]){i64,i64,i64,f64,i64,i8p}, 6, 0));
    rt.position  = LLVMAddFunction(mod, "lx_position", LLVMFunctionType(i64, (LLVMTypeRef[]){i64,i64,i64,f64,i64,i8p}, 6, 0));
    rt.sort      = LLVMAddFunction(mod, "lx_sort", LLVMFunctionType(voidt, (LLVMTypeRef[]){i64,i64}, 2, 0));
    rt.set       = LLVMAddFunction(mod, "lx_set", LLVMFunctionType(voidt, (LLVMTypeRef[]){i64,i64,i64,i64,f64,i64,i8p}, 7, 0));
    rt.append    = LLVMAddFunction(mod, "lx_append", LLVMFunctionType(voidt, (LLVMTypeRef[]){i64,i64,i64,i64,f64,i64,i8p}, 7, 0));
    rt.get_tag   = LLVMAddFunction(mod, "lx_get_tag", LLVMFunctionType(i64, (LLVMTypeRef[]){i64,i64}, 2, 0));
    rt.get_i     = LLVMAddFunction(mod, "lx_get_i", LLVMFunctionType(i64, (LLVMTypeRef[]){i64,i64}, 2, 0));
    rt.get_d     = LLVMAddFunction(mod, "lx_get_d", LLVMFunctionType(f64, (LLVMTypeRef[]){i64,i64}, 2, 0));
    rt.get_c     = LLVMAddFunction(mod, "lx_get_c", LLVMFunctionType(i64, (LLVMTypeRef[]){i64,i64}, 2, 0));
    rt.get_s     = LLVMAddFunction(mod, "lx_get_s", LLVMFunctionType(i8p, (LLVMTypeRef[]){i64,i64}, 2, 0));
    rt.matrix_set= LLVMAddFunction(mod, "lx_matrix_set", LLVMFunctionType(voidt, (LLVMTypeRef[]){i64,i64,i64,i64,i64,f64,i64,i8p}, 8, 0));
    rt.matrix_get_i = LLVMAddFunction(mod, "lx_matrix_get_i", LLVMFunctionType(i64, (LLVMTypeRef[]){i64,i64,i64}, 3, 0));
    rt.matrix_get_d = LLVMAddFunction(mod, "lx_matrix_get_d", LLVMFunctionType(f64, (LLVMTypeRef[]){i64,i64,i64}, 3, 0));
    rt.matrix_get_c = LLVMAddFunction(mod, "lx_matrix_get_c", LLVMFunctionType(i64, (LLVMTypeRef[]){i64,i64,i64}, 3, 0));
    rt.matrix_get_s = LLVMAddFunction(mod, "lx_matrix_get_s", LLVMFunctionType(i8p, (LLVMTypeRef[]){i64,i64,i64}, 3, 0));
    rt.print_cell= LLVMAddFunction(mod, "lx_print_cell", LLVMFunctionType(voidt, (LLVMTypeRef[]){i64,i64}, 2, 0));
    rt.print_array_ascii = LLVMAddFunction(mod, "lx_print_array_ascii", LLVMFunctionType(voidt, (LLVMTypeRef[]){i64,i8p}, 2, 0));
    rt.print_table_ascii = LLVMAddFunction(mod, "lx_print_table_ascii", LLVMFunctionType(voidt, (LLVMTypeRef[]){i64,i8p}, 2, 0));
    rt.print_matrix_ascii = LLVMAddFunction(mod, "lx_print_matrix_ascii", LLVMFunctionType(voidt, (LLVMTypeRef[]){i64,i8p}, 2, 0));
    rt.file_open = LLVMAddFunction(mod, "lx_file_open", LLVMFunctionType(i64, (LLVMTypeRef[]){i8p, i64}, 2, 0));
    rt.file_close = LLVMAddFunction(mod, "lx_file_close", LLVMFunctionType(voidt, (LLVMTypeRef[]){i64}, 1, 0));
    rt.file_write = LLVMAddFunction(mod, "lx_file_write", LLVMFunctionType(voidt, (LLVMTypeRef[]){i64, i8p}, 2, 0));
    rt.file_write_line = LLVMAddFunction(mod, "lx_file_write_line", LLVMFunctionType(voidt, (LLVMTypeRef[]){i64, i64, i8p}, 3, 0));
    rt.file_read_all = LLVMAddFunction(mod, "lx_file_read_all", LLVMFunctionType(i64, (LLVMTypeRef[]){i64}, 1, 0));
    rt.file_read_line = LLVMAddFunction(mod, "lx_file_read_line", LLVMFunctionType(i8p, (LLVMTypeRef[]){i64, i64}, 2, 0));
    rt.file_read_first = LLVMAddFunction(mod, "lx_file_read_first", LLVMFunctionType(i64, (LLVMTypeRef[]){i64, i64}, 2, 0));
    rt.file_read_range = LLVMAddFunction(mod, "lx_file_read_range", LLVMFunctionType(i64, (LLVMTypeRef[]){i64, i64, i64}, 3, 0));
    rt.file_clear = LLVMAddFunction(mod, "lx_file_clear", LLVMFunctionType(voidt, (LLVMTypeRef[]){i64}, 1, 0));
    rt.file_clear_line = LLVMAddFunction(mod, "lx_file_clear_line", LLVMFunctionType(voidt, (LLVMTypeRef[]){i64, i64}, 2, 0));
    rt.file_clear_range = LLVMAddFunction(mod, "lx_file_clear_range", LLVMFunctionType(voidt, (LLVMTypeRef[]){i64, i64, i64}, 3, 0));
    rt.file_set_title = LLVMAddFunction(mod, "lx_file_set_title", LLVMFunctionType(voidt, (LLVMTypeRef[]){i64, i8p}, 2, 0));
    rt.file_line_count = LLVMAddFunction(mod, "lx_file_line_count", LLVMFunctionType(i64, (LLVMTypeRef[]){i64}, 1, 0));
    rt.cleanup   = LLVMAddFunction(mod, "lx_cleanup", LLVMFunctionType(voidt, NULL, 0, 0));

    return rt;
}

static LLVMValueRef emit_literal(LLVMBuilderRef b, LLVMModuleRef mod, const Literal *lit) {
    switch (lit->kind) {
        case LIT_INT:    return LLVMConstInt(LLVMInt64Type(), (unsigned long long)lit->as.i, 1);
        case LIT_FLOAT:  return LLVMConstReal(LLVMDoubleType(), lit->as.d);
        case LIT_CHAR:   return LLVMConstInt(LLVMInt8Type(), (unsigned char)lit->as.c, 0);
        case LIT_STRING: return build_global_str(mod, b, lit->as.s);
        case LIT_BOOL:   return LLVMConstInt(LLVMInt1Type(), lit->as.b ? 1 : 0, 0);
    }
    return LLVMConstInt(LLVMInt64Type(), 0, 0);
}

static const char *fmt_for_type(TypeKind k) {
    switch (k) {
        case TYPE_INT:    return "%lld";
        case TYPE_FLOAT:  return "%.2f";
        case TYPE_CHAR:   return "%c";
        case TYPE_STRING: return "%s";
        case TYPE_BOOL:   return "%s";
    }
    return "%lld";
}

static TypeKind type_from_llvm_val(LLVMValueRef v) {
    LLVMTypeRef t = LLVMTypeOf(v);
    LLVMTypeKind tk = LLVMGetTypeKind(t);
    if (tk == LLVMDoubleTypeKind) return TYPE_FLOAT;
    if (tk == LLVMPointerTypeKind) return TYPE_STRING;
    if (tk == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(t) == 1) return TYPE_BOOL;
    if (tk == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(t) == 8) return TYPE_CHAR;
    return TYPE_INT;
}

static LLVMValueRef coerce_value(LLVMBuilderRef bld, LLVMValueRef v, TypeKind expected) {
    LLVMTypeKind vk = LLVMGetTypeKind(LLVMTypeOf(v));

    if (expected == TYPE_FLOAT && vk == LLVMIntegerTypeKind)
        return LLVMBuildSIToFP(bld, v, LLVMDoubleType(), "itof");

    if (expected == TYPE_INT && vk == LLVMDoubleTypeKind)
        return LLVMBuildFPToSI(bld, v, LLVMInt64Type(), "ftoi");

    if (expected == TYPE_CHAR && vk == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(LLVMTypeOf(v)) != 8)
        return LLVMBuildTrunc(bld, v, LLVMInt8Type(), "trunc8");

    if (expected == TYPE_BOOL) {
        if (vk == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(LLVMTypeOf(v)) == 1)
            return v;
        if (vk == LLVMIntegerTypeKind)
            return LLVMBuildICmp(bld, LLVMIntNE, v, LLVMConstInt(LLVMTypeOf(v), 0, 0), "itob");
        if (vk == LLVMDoubleTypeKind)
            return LLVMBuildFCmp(bld, LLVMRealONE, v, LLVMConstReal(LLVMDoubleType(), 0.0), "ftob");
    }

    return v;
}

static LLVMValueRef convert_value(LLVMBuilderRef bld, LLVMValueRef v,
                                  TypeKind from, TypeKind to, RuntimeFns *rt) {
    if (from == to) return v;

    switch (to) {
    case TYPE_STRING:
        switch (from) {
        case TYPE_STRING: return v;
        case TYPE_INT:
            return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->i64_to_string), rt->i64_to_string,
                                  (LLVMValueRef[]){coerce_value(bld, v, TYPE_INT)}, 1, "i2s");
        case TYPE_FLOAT:
            return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->f64_to_string), rt->f64_to_string,
                                  (LLVMValueRef[]){coerce_value(bld, v, TYPE_FLOAT)}, 1, "f2s");
        case TYPE_CHAR: {
            LLVMValueRef c64 = LLVMBuildZExt(bld, coerce_value(bld, v, TYPE_CHAR), LLVMInt64Type(), "c2s64");
            return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->char_to_string), rt->char_to_string,
                                  (LLVMValueRef[]){c64}, 1, "c2s");
        }
        case TYPE_BOOL: {
            LLVMValueRef b64 = LLVMBuildZExt(bld, coerce_value(bld, v, TYPE_BOOL), LLVMInt64Type(), "b2s64");
            return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->bool_to_string), rt->bool_to_string,
                                  (LLVMValueRef[]){b64}, 1, "b2s");
        }
        default: return v;
        }

    case TYPE_INT:
        switch (from) {
        case TYPE_INT: return coerce_value(bld, v, TYPE_INT);
        case TYPE_FLOAT: return LLVMBuildFPToSI(bld, coerce_value(bld, v, TYPE_FLOAT), LLVMInt64Type(), "f2i");
        case TYPE_CHAR: return LLVMBuildSExt(bld, coerce_value(bld, v, TYPE_CHAR), LLVMInt64Type(), "c2i");
        case TYPE_BOOL: return LLVMBuildZExt(bld, coerce_value(bld, v, TYPE_BOOL), LLVMInt64Type(), "b2i");
        case TYPE_STRING:
            return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->string_to_i64), rt->string_to_i64,
                                  (LLVMValueRef[]){coerce_value(bld, v, TYPE_STRING)}, 1, "s2i");
        default: return v;
        }

    case TYPE_FLOAT:
        switch (from) {
        case TYPE_FLOAT: return coerce_value(bld, v, TYPE_FLOAT);
        case TYPE_INT: return LLVMBuildSIToFP(bld, coerce_value(bld, v, TYPE_INT), LLVMDoubleType(), "i2f");
        case TYPE_CHAR: {
            LLVMValueRef c64 = LLVMBuildSExt(bld, coerce_value(bld, v, TYPE_CHAR), LLVMInt64Type(), "c2f64");
            return LLVMBuildSIToFP(bld, c64, LLVMDoubleType(), "c2f");
        }
        case TYPE_BOOL: {
            LLVMValueRef b64 = LLVMBuildZExt(bld, coerce_value(bld, v, TYPE_BOOL), LLVMInt64Type(), "b2f64");
            return LLVMBuildSIToFP(bld, b64, LLVMDoubleType(), "b2f");
        }
        case TYPE_STRING:
            return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->string_to_f64), rt->string_to_f64,
                                  (LLVMValueRef[]){coerce_value(bld, v, TYPE_STRING)}, 1, "s2f");
        default: return v;
        }

    case TYPE_CHAR:
        switch (from) {
        case TYPE_CHAR: return coerce_value(bld, v, TYPE_CHAR);
        case TYPE_INT: return LLVMBuildTrunc(bld, coerce_value(bld, v, TYPE_INT), LLVMInt8Type(), "i2c");
        case TYPE_FLOAT: {
            LLVMValueRef i64v = LLVMBuildFPToSI(bld, coerce_value(bld, v, TYPE_FLOAT), LLVMInt64Type(), "f2c64");
            return LLVMBuildTrunc(bld, i64v, LLVMInt8Type(), "f2c");
        }
        case TYPE_BOOL: {
            LLVMValueRef b64 = LLVMBuildZExt(bld, coerce_value(bld, v, TYPE_BOOL), LLVMInt64Type(), "b2c64");
            return LLVMBuildTrunc(bld, b64, LLVMInt8Type(), "b2c");
        }
        case TYPE_STRING: {
            LLVMValueRef c64 = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->string_to_char), rt->string_to_char,
                                              (LLVMValueRef[]){coerce_value(bld, v, TYPE_STRING)}, 1, "s2c64");
            return LLVMBuildTrunc(bld, c64, LLVMInt8Type(), "s2c");
        }
        default: return v;
        }

    default:
        return v;
    }
}

static int64_t tag_for_type(TypeKind t) {
    if (t == TYPE_DYNAMIC) return LX_TAG_EMPTY;
    switch (t) {
        case TYPE_INT: return LX_TAG_INT;
        case TYPE_FLOAT: return LX_TAG_FLOAT;
        case TYPE_CHAR: return LX_TAG_CHAR;
        case TYPE_STRING: return LX_TAG_STRING;
        case TYPE_BOOL: return LX_TAG_BOOL;
    }
    return LX_TAG_INT;
}

static LLVMValueRef emit_expr(LLVMBuilderRef bld, LLVMModuleRef mod,
                              const Expr *e, CGVars *vars, CGFuncs *funcs,
                              RuntimeFns *rt);

static void emit_stmts(const Stmt *const *stmts, size_t count,
                       LLVMModuleRef mod, LLVMBuilderRef bld,
                       LLVMValueRef cur_fn, LLVMValueRef printf_fn,
                       LLVMValueRef scanf_fn, CGVars *vars,
                       CGFuncs *funcs, TypeKind current_return,
                       RuntimeFns *rt, const char *collection_context,
                       const char *collection_index_context,
                       const char *matrix_context,
                       LLVMValueRef file_context);

static void cg_declare_funcdefs_from_stmt(const Stmt *s, LLVMModuleRef mod, CGFuncs *funcs) {
    if (!s) return;
    switch (s->kind) {
    case STMT_FUNCDEF: {
        const FuncDefStmt *fd = &s->as.funcdef;
        LLVMTypeRef *params = fd->param_count ? (LLVMTypeRef *)malloc(fd->param_count * sizeof(LLVMTypeRef)) : NULL;
        if (fd->param_count && !params) { fprintf(stderr, "OOM\n"); exit(1); }
        for (size_t p = 0; p < fd->param_count; p++) params[p] = llvm_type(fd->params[p].type);
        LLVMTypeRef fn_ty = LLVMFunctionType(llvm_type(fd->return_type), params, (unsigned)fd->param_count, 0);
        LLVMValueRef fn = LLVMAddFunction(mod, fd->name, fn_ty);
        cgfuncs_add(funcs, fd->name, fd->return_type, fd->params, fd->param_count, fn_ty, fn);
        free(params);
        cg_declare_funcdefs_from_stmt(fd->body, mod, funcs);
        break;
    }
    case STMT_BLOCK:
        for (size_t i = 0; i < s->as.block.count; i++)
            cg_declare_funcdefs_from_stmt(s->as.block.stmts[i], mod, funcs);
        break;
    case STMT_IF:
        cg_declare_funcdefs_from_stmt(s->as.if_.then_block, mod, funcs);
        cg_declare_funcdefs_from_stmt(s->as.if_.else_block, mod, funcs);
        break;
    case STMT_WHILE:
        cg_declare_funcdefs_from_stmt(s->as.while_.body, mod, funcs);
        break;
    case STMT_FOR:
        cg_declare_funcdefs_from_stmt(s->as.for_.body, mod, funcs);
        break;
    case STMT_FOR_EACH:
        cg_declare_funcdefs_from_stmt(s->as.for_each.body, mod, funcs);
        break;
    case STMT_FOR_MATRIX:
        cg_declare_funcdefs_from_stmt(s->as.for_matrix.body, mod, funcs);
        break;
    case STMT_REPEAT:
        cg_declare_funcdefs_from_stmt(s->as.repeat_.body, mod, funcs);
        break;
    case STMT_ATTEMPT:
        cg_declare_funcdefs_from_stmt(s->as.attempt_.body, mod, funcs);
        cg_declare_funcdefs_from_stmt(s->as.attempt_.failure, mod, funcs);
        break;
    case STMT_FILE_OPEN:
        cg_declare_funcdefs_from_stmt(s->as.file_open.body, mod, funcs);
        break;
    default:
        break;
    }
}

static void cg_emit_funcdef_bodies_from_stmt(const Stmt *s,
                                             LLVMModuleRef mod, LLVMBuilderRef bld,
                                             LLVMValueRef printf_fn, LLVMValueRef scanf_fn,
                                             CGFuncs *funcs, RuntimeFns *rt) {
    if (!s) return;
    switch (s->kind) {
    case STMT_FUNCDEF: {
        const FuncDefStmt *fd = &s->as.funcdef;
        CGFunc *cf = cgfuncs_find(funcs, fd->name);
        if (!cf) { fprintf(stderr, "ICE: missing routine '%s'\n", fd->name); exit(1); }

        LLVMBasicBlockRef entry = LLVMAppendBasicBlock(cf->fn, "entry");
        LLVMPositionBuilderAtEnd(bld, entry);

        CGVars vars;
        cgvars_init(&vars);

        for (size_t i = 0; i < g_global_vars.n; i++) {
            cgvars_add(&vars,
                       g_global_vars.v[i].name,
                       g_global_vars.v[i].kind,
                       g_global_vars.v[i].type,
                       g_global_vars.v[i].alloca_);
        }

        for (size_t p = 0; p < fd->param_count; p++) {
            LLVMValueRef param = LLVMGetParam(cf->fn, (unsigned)p);
            LLVMValueRef alloca_ = LLVMBuildAlloca(bld, llvm_type(fd->params[p].type), fd->params[p].name);
            LLVMBuildStore(bld, param, alloca_);
            cgvars_add(&vars, fd->params[p].name, CG_SCALAR, fd->params[p].type, alloca_);
        }

        emit_stmts((const Stmt *const *)fd->body->as.block.stmts,
                   fd->body->as.block.count,
                   mod, bld, cf->fn, printf_fn, scanf_fn,
                   &vars, funcs, fd->return_type, rt, NULL, NULL, NULL, NULL);

        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(bld)))
            LLVMBuildRet(bld, default_value(fd->return_type));

        cgvars_free(&vars);
        cg_emit_funcdef_bodies_from_stmt(fd->body, mod, bld, printf_fn, scanf_fn, funcs, rt);
        break;
    }
    case STMT_BLOCK:
        for (size_t i = 0; i < s->as.block.count; i++)
            cg_emit_funcdef_bodies_from_stmt(s->as.block.stmts[i], mod, bld, printf_fn, scanf_fn, funcs, rt);
        break;
    case STMT_IF:
        cg_emit_funcdef_bodies_from_stmt(s->as.if_.then_block, mod, bld, printf_fn, scanf_fn, funcs, rt);
        cg_emit_funcdef_bodies_from_stmt(s->as.if_.else_block, mod, bld, printf_fn, scanf_fn, funcs, rt);
        break;
    case STMT_WHILE:
        cg_emit_funcdef_bodies_from_stmt(s->as.while_.body, mod, bld, printf_fn, scanf_fn, funcs, rt);
        break;
    case STMT_FOR:
        cg_emit_funcdef_bodies_from_stmt(s->as.for_.body, mod, bld, printf_fn, scanf_fn, funcs, rt);
        break;
    case STMT_FOR_EACH:
        cg_emit_funcdef_bodies_from_stmt(s->as.for_each.body, mod, bld, printf_fn, scanf_fn, funcs, rt);
        break;
    case STMT_FOR_MATRIX:
        cg_emit_funcdef_bodies_from_stmt(s->as.for_matrix.body, mod, bld, printf_fn, scanf_fn, funcs, rt);
        break;
    case STMT_REPEAT:
        cg_emit_funcdef_bodies_from_stmt(s->as.repeat_.body, mod, bld, printf_fn, scanf_fn, funcs, rt);
        break;
    case STMT_ATTEMPT:
        cg_emit_funcdef_bodies_from_stmt(s->as.attempt_.body, mod, bld, printf_fn, scanf_fn, funcs, rt);
        cg_emit_funcdef_bodies_from_stmt(s->as.attempt_.failure, mod, bld, printf_fn, scanf_fn, funcs, rt);
        break;
    case STMT_FILE_OPEN:
        cg_emit_funcdef_bodies_from_stmt(s->as.file_open.body, mod, bld, printf_fn, scanf_fn, funcs, rt);
        break;
    default:
        break;
    }
}

static LLVMValueRef emit_index_value(LLVMBuilderRef bld, LLVMModuleRef mod,
                                     const Expr *e, CGVars *vars, CGFuncs *funcs,
                                     RuntimeFns *rt) {
    CGVar *cv = cgvars_find(vars, e->as.index.name);
    if (!cv) { fprintf(stderr, "ICE: unknown collection '%s'\n", e->as.index.name); exit(1); }
    LLVMValueRef idx = emit_expr(bld, mod, e->as.index.index, vars, funcs, rt);
    idx = coerce_value(bld, idx, TYPE_INT);

    if (cv->kind == CG_SCALAR && cv->type == TYPE_STRING) {
        LLVMValueRef s = LLVMBuildLoad2(bld, LLVMPointerType(LLVMInt8Type(), 0), cv->alloca_, "strsrc");
        LLVMValueRef c64 = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->str_at), rt->str_at,
                                          (LLVMValueRef[]){s, idx}, 2, "strc64");
        return LLVMBuildTrunc(bld, c64, LLVMInt8Type(), "strc8");
    }

    LLVMValueRef handle = LLVMBuildLoad2(bld, LLVMInt64Type(), cv->alloca_, "h");

    if (cv->kind == CG_ARRAY) {
        switch (cv->type) {
        case TYPE_INT:
            return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->get_i), rt->get_i,
                                  (LLVMValueRef[]){handle, idx}, 2, "arr_i");
        case TYPE_FLOAT:
            return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->get_d), rt->get_d,
                                  (LLVMValueRef[]){handle, idx}, 2, "arr_d");
        case TYPE_CHAR: {
            LLVMValueRef c64 = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->get_c), rt->get_c,
                                              (LLVMValueRef[]){handle, idx}, 2, "arr_c64");
            return LLVMBuildTrunc(bld, c64, LLVMInt8Type(), "arr_c8");
        }
        case TYPE_STRING:
            return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->get_s), rt->get_s,
                                  (LLVMValueRef[]){handle, idx}, 2, "arr_s");
        case TYPE_BOOL: {
            LLVMValueRef b64 = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->get_i), rt->get_i,
                                              (LLVMValueRef[]){handle, idx}, 2, "arr_b64");
            return LLVMBuildICmp(bld, LLVMIntNE, b64, LLVMConstInt(LLVMInt64Type(), 0, 0), "arr_b1");
        }
        }
    }

    if (cv->kind == CG_MATRIX) {
        fprintf(stderr, "ICE: matrix index requires row and col.");
        exit(1);
    }

    return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->get_i), rt->get_i,
                          (LLVMValueRef[]){handle, idx}, 2, "tbl_i");
}

static LLVMValueRef emit_expr(LLVMBuilderRef bld, LLVMModuleRef mod,
                              const Expr *e, CGVars *vars, CGFuncs *funcs,
                              RuntimeFns *rt) {
    switch (e->kind) {
    case EXPR_LITERAL:
        return emit_literal(bld, mod, &e->as.lit);

    case EXPR_IDENT: {
        CGVar *cv = cgvars_find(vars, e->as.ident);
        if (!cv) { fprintf(stderr, "ICE: unknown var '%s'\n", e->as.ident); exit(1); }
        return LLVMBuildLoad2(bld, llvm_type(cv->type), cv->alloca_, e->as.ident);
    }

    case EXPR_BINARY: {
        LLVMValueRef L = emit_expr(bld, mod, e->as.bin.left, vars, funcs, rt);
        LLVMValueRef R = emit_expr(bld, mod, e->as.bin.right, vars, funcs, rt);

        TypeKind lt = type_from_llvm_val(L);
        TypeKind rt_ty = type_from_llvm_val(R);
        if (lt == TYPE_STRING || rt_ty == TYPE_STRING) {
            if (lt != TYPE_STRING || rt_ty != TYPE_STRING) {
                fprintf(stderr, "ICE: mixed string/non-string binary expression.\n");
                exit(1);
            }
            if (e->as.bin.op == OP_ADD) {
                return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->str_concat), rt->str_concat,
                                      (LLVMValueRef[]){L, R}, 2, "sadd");
            }
            if (e->as.bin.op == OP_SUB) {
                return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->str_minus), rt->str_minus,
                                      (LLVMValueRef[]){L, R}, 2, "ssub");
            }
            fprintf(stderr, "ICE: unsupported string binary operator.\n");
            exit(1);
        }

        int l_fp = (LLVMGetTypeKind(LLVMTypeOf(L)) == LLVMDoubleTypeKind);
        int r_fp = (LLVMGetTypeKind(LLVMTypeOf(R)) == LLVMDoubleTypeKind);
        if (l_fp && !r_fp) R = LLVMBuildSIToFP(bld, R, LLVMDoubleType(), "promo");
        else if (!l_fp && r_fp) L = LLVMBuildSIToFP(bld, L, LLVMDoubleType(), "promo");
        int is_fp = l_fp || r_fp;

        switch (e->as.bin.op) {
        case OP_ADD: return is_fp ? LLVMBuildFAdd(bld, L, R, "add") : LLVMBuildAdd(bld, L, R, "add");
        case OP_SUB: return is_fp ? LLVMBuildFSub(bld, L, R, "sub") : LLVMBuildSub(bld, L, R, "sub");
        case OP_MUL: return is_fp ? LLVMBuildFMul(bld, L, R, "mul") : LLVMBuildMul(bld, L, R, "mul");
        case OP_DIV: return is_fp ? LLVMBuildFDiv(bld, L, R, "div") : LLVMBuildSDiv(bld, L, R, "div");
        case OP_MOD: return is_fp ? LLVMBuildFRem(bld, L, R, "mod") : LLVMBuildSRem(bld, L, R, "mod");
        }
        break;
    }

    case EXPR_UNARY_NEG: {
        LLVMValueRef v = emit_expr(bld, mod, e->as.operand, vars, funcs, rt);
        if (LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMDoubleTypeKind)
            return LLVMBuildFNeg(bld, v, "neg");
        return LLVMBuildNeg(bld, v, "neg");
    }

    case EXPR_CONVERT: {
        LLVMValueRef v = emit_expr(bld, mod, e->as.convert.value, vars, funcs, rt);
        TypeKind from = type_from_llvm_val(v);
        return convert_value(bld, v, from, e->as.convert.to_type, rt);
    }

    case EXPR_CALL: {
        CGFunc *cf = cgfuncs_find(funcs, e->as.call.name);
        if (!cf) { fprintf(stderr, "ICE: unknown routine '%s'\n", e->as.call.name); exit(1); }
        size_t argc = e->as.call.count;
        LLVMValueRef *argv = argc ? (LLVMValueRef *)malloc(argc * sizeof(LLVMValueRef)) : NULL;
        if (argc && !argv) { fprintf(stderr, "OOM\n"); exit(1); }
        for (size_t i = 0; i < argc; i++) {
            LLVMValueRef av = emit_expr(bld, mod, e->as.call.args[i], vars, funcs, rt);
            if (i < cf->param_count) av = coerce_value(bld, av, cf->param_types[i]);
            argv[i] = av;
        }
        LLVMValueRef call = LLVMBuildCall2(bld, cf->fn_type, cf->fn, argv, (unsigned)argc, "calltmp");
        free(argv);
        return call;
    }

    case EXPR_LENGTH: {
        CGVar *cv = cgvars_find(vars, e->as.length_name);
        if (!cv || (cv->kind != CG_ARRAY && cv->kind != CG_TABLE && cv->kind != CG_MATRIX)) {
            fprintf(stderr, "ICE: '%s' is not a collection for size-of expression.\n", e->as.length_name);
            exit(1);
        }
        LLVMValueRef handle = LLVMBuildLoad2(bld, LLVMInt64Type(), cv->alloca_, "h");
        return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->len), rt->len,
                              (LLVMValueRef[]){handle}, 1, "len");
    }

    case EXPR_ROW_LENGTH: {
        CGVar *cv = cgvars_find(vars, e->as.row_length_name);
        if (!cv || cv->kind != CG_MATRIX) {
            fprintf(stderr, "ICE: '%s' is not a matrix for row length expression.\n", e->as.row_length_name);
            exit(1);
        }
        LLVMValueRef handle = LLVMBuildLoad2(bld, LLVMInt64Type(), cv->alloca_, "rh");
        return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->rows), rt->rows,
                              (LLVMValueRef[]){handle}, 1, "rlen");
    }

    case EXPR_COL_LENGTH: {
        CGVar *cv = cgvars_find(vars, e->as.col_length_name);
        if (!cv || cv->kind != CG_MATRIX) {
            fprintf(stderr, "ICE: '%s' is not a matrix for column length expression.\n", e->as.col_length_name);
            exit(1);
        }
        LLVMValueRef handle = LLVMBuildLoad2(bld, LLVMInt64Type(), cv->alloca_, "ch");
        return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->cols), rt->cols,
                              (LLVMValueRef[]){handle}, 1, "clen");
    }

    case EXPR_STRLEN: {
        LLVMValueRef s = emit_expr(bld, mod, e->as.strlen_value, vars, funcs, rt);
        if (LLVMGetTypeKind(LLVMTypeOf(s)) != LLVMPointerTypeKind) {
            fprintf(stderr, "ICE: length operand is not a string value.\n");
            exit(1);
        }
        return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->strlen_fn), rt->strlen_fn,
                              (LLVMValueRef[]){s}, 1, "slen");
    }

    case EXPR_EXTRACT: {
        CGVar *cv = cgvars_find(vars, e->as.extract.source_name);
        if (!cv || cv->kind != CG_SCALAR || cv->type != TYPE_STRING) {
            fprintf(stderr, "ICE: '%s' is not a string scalar for extract expression.\n", e->as.extract.source_name);
            exit(1);
        }
        LLVMValueRef needle = emit_expr(bld, mod, e->as.extract.value, vars, funcs, rt);
        if (LLVMGetTypeKind(LLVMTypeOf(needle)) != LLVMPointerTypeKind) {
            fprintf(stderr, "ICE: extract search value is not a string.\n");
            exit(1);
        }
        LLVMValueRef source = LLVMBuildLoad2(bld, LLVMPointerType(LLVMInt8Type(), 0), cv->alloca_, "extractsrc");
        return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->extract), rt->extract,
                              (LLVMValueRef[]){needle, source}, 2, "extract");
    }

    case EXPR_EXTRACT_RANGE: {
        CGVar *cv = cgvars_find(vars, e->as.extract_range.source_name);
        if (!cv || cv->kind != CG_SCALAR || cv->type != TYPE_STRING) {
            fprintf(stderr, "ICE: '%s' is not a string scalar for extract range expression.\n", e->as.extract_range.source_name);
            exit(1);
        }
        LLVMValueRef from = coerce_value(bld, emit_expr(bld, mod, e->as.extract_range.from, vars, funcs, rt), TYPE_INT);
        LLVMValueRef to = coerce_value(bld, emit_expr(bld, mod, e->as.extract_range.to, vars, funcs, rt), TYPE_INT);
        LLVMValueRef source = LLVMBuildLoad2(bld, LLVMPointerType(LLVMInt8Type(), 0), cv->alloca_, "extractrangesrc");
        return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->extract_range), rt->extract_range,
                              (LLVMValueRef[]){from, to, source}, 3, "extractrange");
    }

    case EXPR_COUNT: {
        CGVar *cv = cgvars_find(vars, e->as.count.collection_name);
        if (!cv || (cv->kind != CG_ARRAY && cv->kind != CG_TABLE && cv->kind != CG_MATRIX)) {
            fprintf(stderr, "ICE: '%s' is not a collection for count expression.\n", e->as.count.collection_name);
            exit(1);
        }

        LLVMValueRef handle = LLVMBuildLoad2(bld, LLVMInt64Type(), cv->alloca_, "ch");
        LLVMValueRef val = emit_expr(bld, mod, e->as.count.value, vars, funcs, rt);
        TypeKind vt = type_from_llvm_val(val);
        int64_t tag = tag_for_type(vt);

        LLVMValueRef i64v = LLVMConstInt(LLVMInt64Type(), 0, 0);
        LLVMValueRef f64v = LLVMConstReal(LLVMDoubleType(), 0.0);
        LLVMValueRef c64v = LLVMConstInt(LLVMInt64Type(), 0, 0);
        LLVMValueRef sval = LLVMConstPointerNull(LLVMPointerType(LLVMInt8Type(), 0));

        if (vt == TYPE_INT) i64v = coerce_value(bld, val, TYPE_INT);
        else if (vt == TYPE_FLOAT) f64v = coerce_value(bld, val, TYPE_FLOAT);
        else if (vt == TYPE_CHAR) c64v = LLVMBuildZExt(bld, coerce_value(bld, val, TYPE_CHAR), LLVMInt64Type(), "cc64");
        else if (vt == TYPE_STRING) sval = val;
        else if (vt == TYPE_BOOL) i64v = LLVMBuildZExt(bld, coerce_value(bld, val, TYPE_BOOL), LLVMInt64Type(), "bc64");

        return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->count), rt->count,
                              (LLVMValueRef[]){handle,
                                               LLVMConstInt(LLVMInt64Type(), (unsigned long long)tag, 0),
                                               i64v, f64v, c64v, sval},
                              6, "cnt");
    }

    case EXPR_CHECK: {
        CGVar *cv = cgvars_find(vars, e->as.check.collection_name);
        if (!cv) {
            fprintf(stderr, "ICE: '%s' is not found for check expression.\n", e->as.check.collection_name);
            exit(1);
        }

        if (cv->kind == CG_SCALAR && cv->type == TYPE_STRING) {
            LLVMValueRef source = LLVMBuildLoad2(bld, LLVMPointerType(LLVMInt8Type(), 0), cv->alloca_, "schksrc");
            LLVMValueRef val = emit_expr(bld, mod, e->as.check.value, vars, funcs, rt);
            TypeKind vt = type_from_llvm_val(val);
            LLVMValueRef ok64 = NULL;
            if (vt == TYPE_STRING) {
                ok64 = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->str_contains), rt->str_contains,
                                      (LLVMValueRef[]){source, val}, 2, "schks64");
            } else if (vt == TYPE_CHAR) {
                LLVMValueRef c64 = LLVMBuildZExt(bld, coerce_value(bld, val, TYPE_CHAR), LLVMInt64Type(), "schkc64");
                ok64 = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->str_contains_char), rt->str_contains_char,
                                      (LLVMValueRef[]){source, c64}, 2, "schkc64ret");
            } else {
                fprintf(stderr, "ICE: check in string expects char/string value.\n");
                exit(1);
            }
            return LLVMBuildICmp(bld, LLVMIntNE, ok64, LLVMConstInt(LLVMInt64Type(), 0, 0), "schkin");
        }

        if (cv->kind != CG_ARRAY && cv->kind != CG_TABLE && cv->kind != CG_MATRIX) {
            fprintf(stderr, "ICE: '%s' is not a collection for check expression.\n", e->as.check.collection_name);
            exit(1);
        }

        LLVMValueRef handle = LLVMBuildLoad2(bld, LLVMInt64Type(), cv->alloca_, "ch");
        LLVMValueRef val = emit_expr(bld, mod, e->as.check.value, vars, funcs, rt);
        TypeKind vt = type_from_llvm_val(val);
        int64_t tag = tag_for_type(vt);

        LLVMValueRef i64v = LLVMConstInt(LLVMInt64Type(), 0, 0);
        LLVMValueRef f64v = LLVMConstReal(LLVMDoubleType(), 0.0);
        LLVMValueRef c64v = LLVMConstInt(LLVMInt64Type(), 0, 0);
        LLVMValueRef sval = LLVMConstPointerNull(LLVMPointerType(LLVMInt8Type(), 0));

        if (vt == TYPE_INT) i64v = coerce_value(bld, val, TYPE_INT);
        else if (vt == TYPE_FLOAT) f64v = coerce_value(bld, val, TYPE_FLOAT);
        else if (vt == TYPE_CHAR) c64v = LLVMBuildZExt(bld, coerce_value(bld, val, TYPE_CHAR), LLVMInt64Type(), "cc64");
        else if (vt == TYPE_STRING) sval = val;
        else if (vt == TYPE_BOOL) i64v = LLVMBuildZExt(bld, coerce_value(bld, val, TYPE_BOOL), LLVMInt64Type(), "bc64");

        LLVMValueRef cnt = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->count), rt->count,
                                          (LLVMValueRef[]){handle,
                                                           LLVMConstInt(LLVMInt64Type(), (unsigned long long)tag, 0),
                                                           i64v, f64v, c64v, sval},
                                          6, "chkcnt");
        return LLVMBuildICmp(bld, LLVMIntNE, cnt, LLVMConstInt(LLVMInt64Type(), 0, 0), "isin");
    }

    case EXPR_POSITION: {
        CGVar *cv = cgvars_find(vars, e->as.position.collection_name);
        if (!cv) {
            fprintf(stderr, "ICE: '%s' is not found for position expression.\n", e->as.position.collection_name);
            exit(1);
        }

        if (cv->kind == CG_SCALAR && cv->type == TYPE_STRING) {
            LLVMValueRef source = LLVMBuildLoad2(bld, LLVMPointerType(LLVMInt8Type(), 0), cv->alloca_, "spossrc");
            LLVMValueRef val = emit_expr(bld, mod, e->as.position.value, vars, funcs, rt);
            TypeKind vt = type_from_llvm_val(val);
            if (vt == TYPE_STRING) {
                return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->str_position), rt->str_position,
                                      (LLVMValueRef[]){source, val}, 2, "sposs");
            }
            if (vt == TYPE_CHAR) {
                LLVMValueRef c64 = LLVMBuildZExt(bld, coerce_value(bld, val, TYPE_CHAR), LLVMInt64Type(), "sposc64");
                return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->str_position_char), rt->str_position_char,
                                      (LLVMValueRef[]){source, c64}, 2, "sposc");
            }
            fprintf(stderr, "ICE: position in string expects char/string value.\n");
            exit(1);
        }

        if (cv->kind != CG_ARRAY && cv->kind != CG_TABLE) {
            fprintf(stderr, "ICE: '%s' is not a collection for position expression.\n", e->as.position.collection_name);
            exit(1);
        }

        LLVMValueRef handle = LLVMBuildLoad2(bld, LLVMInt64Type(), cv->alloca_, "ph");
        LLVMValueRef val = emit_expr(bld, mod, e->as.position.value, vars, funcs, rt);
        TypeKind vt = type_from_llvm_val(val);
        int64_t tag = tag_for_type(vt);

        LLVMValueRef i64v = LLVMConstInt(LLVMInt64Type(), 0, 0);
        LLVMValueRef f64v = LLVMConstReal(LLVMDoubleType(), 0.0);
        LLVMValueRef c64v = LLVMConstInt(LLVMInt64Type(), 0, 0);
        LLVMValueRef sval = LLVMConstPointerNull(LLVMPointerType(LLVMInt8Type(), 0));

        if (vt == TYPE_INT) i64v = coerce_value(bld, val, TYPE_INT);
        else if (vt == TYPE_FLOAT) f64v = coerce_value(bld, val, TYPE_FLOAT);
        else if (vt == TYPE_CHAR) c64v = LLVMBuildZExt(bld, coerce_value(bld, val, TYPE_CHAR), LLVMInt64Type(), "pc64");
        else if (vt == TYPE_STRING) sval = val;
        else if (vt == TYPE_BOOL) i64v = LLVMBuildZExt(bld, coerce_value(bld, val, TYPE_BOOL), LLVMInt64Type(), "pb64");

        return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->position), rt->position,
                              (LLVMValueRef[]){handle,
                                               LLVMConstInt(LLVMInt64Type(), (unsigned long long)tag, 0),
                                               i64v, f64v, c64v, sval},
                              6, "pos");
    }

    case EXPR_MATRIX_INDEX: {
        CGVar *cv = cgvars_find(vars, e->as.matrix_index.name);
        if (!cv || cv->kind != CG_MATRIX) {
            fprintf(stderr, "ICE: '%s' is not a matrix for matrix index expression.\n", e->as.matrix_index.name);
            exit(1);
        }
        LLVMValueRef handle = LLVMBuildLoad2(bld, LLVMInt64Type(), cv->alloca_, "mh");
        LLVMValueRef row = coerce_value(bld, emit_expr(bld, mod, e->as.matrix_index.row, vars, funcs, rt), TYPE_INT);
        LLVMValueRef col = coerce_value(bld, emit_expr(bld, mod, e->as.matrix_index.col, vars, funcs, rt), TYPE_INT);

        switch (cv->type) {
        case TYPE_INT:
            return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->matrix_get_i), rt->matrix_get_i,
                                  (LLVMValueRef[]){handle, row, col}, 3, "m_i");
        case TYPE_FLOAT:
            return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->matrix_get_d), rt->matrix_get_d,
                                  (LLVMValueRef[]){handle, row, col}, 3, "m_d");
        case TYPE_CHAR: {
            LLVMValueRef c64 = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->matrix_get_c), rt->matrix_get_c,
                                              (LLVMValueRef[]){handle, row, col}, 3, "m_c64");
            return LLVMBuildTrunc(bld, c64, LLVMInt8Type(), "m_c8");
        }
        case TYPE_STRING:
            return LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->matrix_get_s), rt->matrix_get_s,
                                  (LLVMValueRef[]){handle, row, col}, 3, "m_s");
        case TYPE_BOOL: {
            LLVMValueRef b64 = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->matrix_get_i), rt->matrix_get_i,
                                              (LLVMValueRef[]){handle, row, col}, 3, "m_b64");
            return LLVMBuildICmp(bld, LLVMIntNE, b64, LLVMConstInt(LLVMInt64Type(), 0, 0), "m_b1");
        }
        }
        break;
    }

    case EXPR_INDEX:
        return emit_index_value(bld, mod, e, vars, funcs, rt);
    }

    return LLVMConstInt(LLVMInt64Type(), 0, 0);
}

static LLVMValueRef emit_cond(LLVMBuilderRef bld, LLVMModuleRef mod,
                              const CondExpr *c, CGVars *vars, CGFuncs *funcs,
                              RuntimeFns *rt) {
    switch (c->kind) {
    case COND_CMP: {
        LLVMValueRef L = emit_expr(bld, mod, c->as.cmp.left, vars, funcs, rt);
        LLVMValueRef R = emit_expr(bld, mod, c->as.cmp.right, vars, funcs, rt);
        int l_fp = (LLVMGetTypeKind(LLVMTypeOf(L)) == LLVMDoubleTypeKind);
        int r_fp = (LLVMGetTypeKind(LLVMTypeOf(R)) == LLVMDoubleTypeKind);
        if (l_fp && !r_fp) R = LLVMBuildSIToFP(bld, R, LLVMDoubleType(), "promo");
        else if (!l_fp && r_fp) L = LLVMBuildSIToFP(bld, L, LLVMDoubleType(), "promo");
        int is_fp = l_fp || r_fp;

        if (is_fp) {
            LLVMRealPredicate pred;
            switch (c->as.cmp.op) {
            case CMP_EQ: pred = LLVMRealOEQ; break;
            case CMP_NEQ: pred = LLVMRealONE; break;
            case CMP_GT: pred = LLVMRealOGT; break;
            case CMP_LT: pred = LLVMRealOLT; break;
            case CMP_GTE: pred = LLVMRealOGE; break;
            case CMP_LTE: pred = LLVMRealOLE; break;
            default: pred = LLVMRealOEQ; break;
            }
            return LLVMBuildFCmp(bld, pred, L, R, "cmp");
        }

        LLVMIntPredicate pred;
        switch (c->as.cmp.op) {
        case CMP_EQ: pred = LLVMIntEQ; break;
        case CMP_NEQ: pred = LLVMIntNE; break;
        case CMP_GT: pred = LLVMIntSGT; break;
        case CMP_LT: pred = LLVMIntSLT; break;
        case CMP_GTE: pred = LLVMIntSGE; break;
        case CMP_LTE: pred = LLVMIntSLE; break;
        default: pred = LLVMIntEQ; break;
        }
        return LLVMBuildICmp(bld, pred, L, R, "cmp");
    }
    case COND_AND: {
        LLVMValueRef lv = emit_cond(bld, mod, c->as.logic.left, vars, funcs, rt);
        LLVMValueRef rv = emit_cond(bld, mod, c->as.logic.right, vars, funcs, rt);
        return LLVMBuildAnd(bld, lv, rv, "cand");
    }
    case COND_OR: {
        LLVMValueRef lv = emit_cond(bld, mod, c->as.logic.left, vars, funcs, rt);
        LLVMValueRef rv = emit_cond(bld, mod, c->as.logic.right, vars, funcs, rt);
        return LLVMBuildOr(bld, lv, rv, "cor");
    }
    case COND_NOT: {
        LLVMValueRef v = emit_cond(bld, mod, c->as.operand, vars, funcs, rt);
        return LLVMBuildNot(bld, v, "cnot");
    }
    }

    return LLVMConstInt(LLVMInt1Type(), 0, 0);
}

static void emit_print_expr(const Expr *e,
                            LLVMModuleRef mod, LLVMBuilderRef bld,
                            LLVMValueRef printf_fn,
                            CGVars *vars, CGFuncs *funcs,
                            RuntimeFns *rt) {
    if (e->kind == EXPR_IDENT) {
        CGVar *cv = cgvars_find(vars, e->as.ident);
        if (cv && (cv->kind == CG_ARRAY || cv->kind == CG_TABLE || cv->kind == CG_MATRIX)) {
            LLVMValueRef handle = LLVMBuildLoad2(bld, LLVMInt64Type(), cv->alloca_, "ph");
            LLVMValueRef namev = build_global_str(mod, bld, e->as.ident);
            LLVMValueRef fn = (cv->kind == CG_ARRAY) ? rt->print_array_ascii
                            : (cv->kind == CG_TABLE) ? rt->print_table_ascii
                            : rt->print_matrix_ascii;
            LLVMBuildCall2(bld, LLVMGlobalGetValueType(fn), fn,
                           (LLVMValueRef[]){handle, namev}, 2, "");
            return;
        }
    }

    if (e->kind == EXPR_INDEX) {
        CGVar *cv = cgvars_find(vars, e->as.index.name);
        if (cv && cv->kind == CG_TABLE) {
            LLVMValueRef handle = LLVMBuildLoad2(bld, LLVMInt64Type(), cv->alloca_, "th");
            LLVMValueRef idx = emit_expr(bld, mod, e->as.index.index, vars, funcs, rt);
            idx = coerce_value(bld, idx, TYPE_INT);
            LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->print_cell), rt->print_cell,
                           (LLVMValueRef[]){handle, idx}, 2, "");
            return;
        }
    }

    LLVMValueRef v = emit_expr(bld, mod, e, vars, funcs, rt);
    TypeKind ty = type_from_llvm_val(v);
    if (ty == TYPE_BOOL) {
        LLVMValueRef s = LLVMBuildSelect(bld, v,
                                         build_global_str(mod, bld, "true"),
                                         build_global_str(mod, bld, "false"),
                                         "bstr");
        LLVMBuildCall2(bld,
                       LLVMFunctionType(LLVMInt32Type(), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(),0)}, 1, 1),
                       printf_fn,
                       (LLVMValueRef[]){build_global_str(mod, bld, "%s"), s}, 2, "");
        return;
    }
    if (ty == TYPE_CHAR) v = LLVMBuildZExt(bld, v, LLVMInt32Type(), "cext");
    LLVMBuildCall2(bld,
                   LLVMFunctionType(LLVMInt32Type(), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(),0)}, 1, 1),
                   printf_fn,
                   (LLVMValueRef[]){build_global_str(mod, bld, fmt_for_type(ty)), v}, 2, "");
}

static void emit_collection_set(LLVMBuilderRef bld, LLVMModuleRef mod,
                                const CollectionSetStmt *cs,
                                CGVars *vars, CGFuncs *funcs,
                                RuntimeFns *rt,
                                const char *collection_context,
                                const char *collection_index_context) {
    const char *target = cs->name ? cs->name : collection_context;
    if (!target) { fprintf(stderr, "ICE: missing collection context for contextual set.\n"); exit(1); }

    CGVar *cv = cgvars_find(vars, target);
    if (!cv || (cv->kind != CG_ARRAY && cv->kind != CG_TABLE)) {
        fprintf(stderr, "ICE: collection target '%s' not found.\n", target);
        exit(1);
    }

    LLVMValueRef handle = LLVMBuildLoad2(bld, LLVMInt64Type(), cv->alloca_, "h");
    LLVMValueRef idx = NULL;
    if (cs->index) {
        idx = emit_expr(bld, mod, cs->index, vars, funcs, rt);
        idx = coerce_value(bld, idx, TYPE_INT);
    } else {
        if (!collection_index_context) {
            fprintf(stderr, "ICE: missing loop index context for 'set value to ...'.\n");
            exit(1);
        }
        CGVar *iv = cgvars_find(vars, collection_index_context);
        if (!iv) {
            fprintf(stderr, "ICE: unknown loop index '%s' in contextual set.\n", collection_index_context);
            exit(1);
        }
        idx = LLVMBuildLoad2(bld, LLVMInt64Type(), iv->alloca_, "ctxi");
    }
    LLVMValueRef val = emit_expr(bld, mod, cs->value, vars, funcs, rt);
    TypeKind vt = type_from_llvm_val(val);

    int64_t tag = cv->kind == CG_ARRAY ? tag_for_type(cv->type) : tag_for_type(vt);

    LLVMValueRef i = LLVMConstInt(LLVMInt64Type(), 0, 0);
    LLVMValueRef d = LLVMConstReal(LLVMDoubleType(), 0.0);
    LLVMValueRef c = LLVMConstInt(LLVMInt64Type(), 0, 0);
    LLVMValueRef s = LLVMConstPointerNull(LLVMPointerType(LLVMInt8Type(), 0));

    if (vt == TYPE_INT) i = coerce_value(bld, val, TYPE_INT);
    else if (vt == TYPE_FLOAT) d = coerce_value(bld, val, TYPE_FLOAT);
    else if (vt == TYPE_CHAR) c = LLVMBuildZExt(bld, coerce_value(bld, val, TYPE_CHAR), LLVMInt64Type(), "c64");
    else if (vt == TYPE_STRING) s = val;
    else if (vt == TYPE_BOOL) i = LLVMBuildZExt(bld, coerce_value(bld, val, TYPE_BOOL), LLVMInt64Type(), "b64");

    LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->set), rt->set,
                   (LLVMValueRef[]){handle, idx,
                                    LLVMConstInt(LLVMInt64Type(), (unsigned long long)tag, 0),
                                    i, d, c, s},
                   7, "");
}

static void emit_stmts(const Stmt *const *stmts, size_t count,
                       LLVMModuleRef mod, LLVMBuilderRef bld,
                       LLVMValueRef cur_fn, LLVMValueRef printf_fn,
                       LLVMValueRef scanf_fn, CGVars *vars,
                       CGFuncs *funcs, TypeKind current_return,
                       RuntimeFns *rt, const char *collection_context,
                       const char *collection_index_context,
                       const char *matrix_context,
                       LLVMValueRef file_context) {
    for (size_t i = 0; i < count; i++) {
        const Stmt *s = stmts[i];

        switch (s->kind) {
        case STMT_DECL: {
            const DeclStmt *d = &s->as.decl;
            for (size_t j = 0; j < d->count; j++) {
                LLVMValueRef a = NULL;
                CGVar *g = cgvars_find_local(&g_global_vars, d->names[j]);
                if (g) {
                    a = g->alloca_;
                } else {
                    LLVMTypeRef lt = llvm_type(d->type);
                    a = LLVMBuildAlloca(bld, lt, d->names[j]);
                    cgvars_add(vars, d->names[j], CG_SCALAR, d->type, a);
                }
                LLVMValueRef v = emit_expr(bld, mod, d->values[j], vars, funcs, rt);
                v = coerce_value(bld, v, d->type);
                LLVMBuildStore(bld, v, a);
            }
            break;
        }

        case STMT_ARRAY_DECL: {
            const ArrayDeclStmt *ad = &s->as.array_decl;
            LLVMValueRef a = LLVMBuildAlloca(bld, LLVMInt64Type(), ad->name);
            LLVMValueRef sz = emit_expr(bld, mod, ad->size, vars, funcs, rt);
            sz = coerce_value(bld, sz, TYPE_INT);
            LLVMValueRef h = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->array_new), rt->array_new,
                                            (LLVMValueRef[]){LLVMConstInt(LLVMInt64Type(), (unsigned long long)tag_for_type(ad->elem_type), 0), sz},
                                            2, "arrh");
            LLVMBuildStore(bld, h, a);
            cgvars_add(vars, ad->name, CG_ARRAY, ad->elem_type, a);
            break;
        }

        case STMT_TABLE_DECL: {
            const TableDeclStmt *td = &s->as.table_decl;
            LLVMValueRef a = LLVMBuildAlloca(bld, LLVMInt64Type(), td->name);
            LLVMValueRef sz = emit_expr(bld, mod, td->size, vars, funcs, rt);
            sz = coerce_value(bld, sz, TYPE_INT);
            LLVMValueRef h = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->table_new), rt->table_new,
                                            (LLVMValueRef[]){sz}, 1, "tblh");
            LLVMBuildStore(bld, h, a);
            cgvars_add(vars, td->name, CG_TABLE, TYPE_INT, a);
            break;
        }

        case STMT_MATRIX_DECL: {
            const MatrixDeclStmt *md = &s->as.matrix_decl;
            LLVMValueRef a = LLVMBuildAlloca(bld, LLVMInt64Type(), md->name);
            LLVMValueRef rows = coerce_value(bld, emit_expr(bld, mod, md->rows, vars, funcs, rt), TYPE_INT);
            LLVMValueRef cols = coerce_value(bld, emit_expr(bld, mod, md->cols, vars, funcs, rt), TYPE_INT);
            LLVMValueRef h = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->matrix_new), rt->matrix_new,
                                            (LLVMValueRef[]){LLVMConstInt(LLVMInt64Type(), (unsigned long long)tag_for_type(md->is_dynamic ? TYPE_DYNAMIC : md->elem_type), 0), rows, cols},
                                            3, "mth");
            LLVMBuildStore(bld, h, a);
            cgvars_add(vars, md->name, CG_MATRIX, md->is_dynamic ? TYPE_DYNAMIC : md->elem_type, a);
            break;
        }

        case STMT_COLLECTION_SET:
            emit_collection_set(bld, mod, &s->as.collection_set, vars, funcs, rt,
                                collection_context, collection_index_context);
            break;

        case STMT_MATRIX_SET: {
            const MatrixSetStmt *ms = &s->as.matrix_set;
            const char *target = ms->name ? ms->name : matrix_context;
            if (!target) {
                fprintf(stderr, "ICE: contextual matrix set used without bound matrix context.\n");
                exit(1);
            }

            CGVar *cv = cgvars_find(vars, target);
            if (!cv || cv->kind != CG_MATRIX) {
                fprintf(stderr, "ICE: matrix target '%s' not found.\n", target);
                exit(1);
            }

            LLVMValueRef handle = LLVMBuildLoad2(bld, LLVMInt64Type(), cv->alloca_, "mh");
            LLVMValueRef row = coerce_value(bld, emit_expr(bld, mod, ms->row, vars, funcs, rt), TYPE_INT);
            LLVMValueRef col = coerce_value(bld, emit_expr(bld, mod, ms->col, vars, funcs, rt), TYPE_INT);
            LLVMValueRef val = emit_expr(bld, mod, ms->value, vars, funcs, rt);
            TypeKind vt = type_from_llvm_val(val);

            int64_t tag = (cv->type == TYPE_DYNAMIC || cv->type == TYPE_STRING || cv->type == TYPE_CHAR)
                ? tag_for_type(vt)
                : tag_for_type(cv->type);

            LLVMValueRef i64v = LLVMConstInt(LLVMInt64Type(), 0, 0);
            LLVMValueRef f64v = LLVMConstReal(LLVMDoubleType(), 0.0);
            LLVMValueRef c64v = LLVMConstInt(LLVMInt64Type(), 0, 0);
            LLVMValueRef sval = LLVMConstPointerNull(LLVMPointerType(LLVMInt8Type(), 0));

            if (vt == TYPE_INT) i64v = coerce_value(bld, val, TYPE_INT);
            else if (vt == TYPE_FLOAT) f64v = coerce_value(bld, val, TYPE_FLOAT);
            else if (vt == TYPE_CHAR) c64v = LLVMBuildZExt(bld, coerce_value(bld, val, TYPE_CHAR), LLVMInt64Type(), "mc64");
            else if (vt == TYPE_STRING) sval = val;
            else if (vt == TYPE_BOOL) i64v = LLVMBuildZExt(bld, coerce_value(bld, val, TYPE_BOOL), LLVMInt64Type(), "mb64");

            LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->matrix_set), rt->matrix_set,
                           (LLVMValueRef[]){handle, row, col,
                                            LLVMConstInt(LLVMInt64Type(), (unsigned long long)tag, 0),
                                            i64v, f64v, c64v, sval},
                           8, "");
            break;
        }

        case STMT_COLLECTION_APPEND: {
            const CollectionAppendStmt *ca = &s->as.collection_append;
            const char *target = ca->name ? ca->name : collection_context;
            if (!target) {
                fprintf(stderr, "ICE: contextual append used without bound collection context.\n");
                exit(1);
            }

            CGVar *cv = cgvars_find(vars, target);
            if (!cv || (cv->kind != CG_ARRAY && cv->kind != CG_TABLE && cv->kind != CG_MATRIX)) {
                fprintf(stderr, "ICE: append target '%s' not found.\n", target);
                exit(1);
            }

            LLVMValueRef handle = LLVMBuildLoad2(bld, LLVMInt64Type(), cv->alloca_, "ah");
            LLVMValueRef val = emit_expr(bld, mod, ca->value, vars, funcs, rt);
            TypeKind vt = type_from_llvm_val(val);

            int64_t tag = 0;
            if (cv->kind == CG_ARRAY || cv->kind == CG_MATRIX) {
                if (cv->type == TYPE_DYNAMIC || cv->type == TYPE_STRING || cv->type == TYPE_CHAR) {
                    /* Preserve source tag so runtime can coerce to text mesh element type. */
                    tag = tag_for_type(vt);
                } else {
                    tag = tag_for_type(cv->type);
                }
            } else {
                tag = tag_for_type(vt);
            }
            LLVMValueRef idx = ca->index
                ? coerce_value(bld, emit_expr(bld, mod, ca->index, vars, funcs, rt), TYPE_INT)
                : LLVMConstInt(LLVMInt64Type(), (unsigned long long)-1, 1);

            LLVMValueRef i64v = LLVMConstInt(LLVMInt64Type(), 0, 0);
            LLVMValueRef f64v = LLVMConstReal(LLVMDoubleType(), 0.0);
            LLVMValueRef c64v = LLVMConstInt(LLVMInt64Type(), 0, 0);
            LLVMValueRef sval = LLVMConstPointerNull(LLVMPointerType(LLVMInt8Type(), 0));

            if (vt == TYPE_INT) i64v = coerce_value(bld, val, TYPE_INT);
            else if (vt == TYPE_FLOAT) f64v = coerce_value(bld, val, TYPE_FLOAT);
            else if (vt == TYPE_CHAR) c64v = LLVMBuildZExt(bld, coerce_value(bld, val, TYPE_CHAR), LLVMInt64Type(), "ac64");
            else if (vt == TYPE_STRING) sval = val;
            else if (vt == TYPE_BOOL) i64v = LLVMBuildZExt(bld, coerce_value(bld, val, TYPE_BOOL), LLVMInt64Type(), "ab64");

            LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->append), rt->append,
                           (LLVMValueRef[]){handle, idx,
                                            LLVMConstInt(LLVMInt64Type(), (unsigned long long)tag, 0),
                                            i64v, f64v, c64v, sval},
                           7, "");
            break;
        }

        case STMT_COLLECTION_SORT: {
            const CollectionSortStmt *cs = &s->as.collection_sort;
            CGVar *cv = cgvars_find(vars, cs->name);
            if (!cv || cv->kind != CG_ARRAY) {
                fprintf(stderr, "ICE: sort target '%s' is not a typed mesh.\n", cs->name);
                exit(1);
            }
            LLVMValueRef handle = LLVMBuildLoad2(bld, LLVMInt64Type(), cv->alloca_, "sh");
            LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->sort), rt->sort,
                           (LLVMValueRef[]){handle,
                                            LLVMConstInt(LLVMInt64Type(), (unsigned long long)(cs->ascending ? 1 : 0), 0)},
                           2, "");
            break;
        }

        case STMT_COLLECTION_SET_INPUT: {
            const CollectionSetInputStmt *cs = &s->as.collection_set_input;
            const char *target = cs->name ? cs->name : collection_context;
            if (!target) { fprintf(stderr, "ICE: missing collection context for collection input set.\n"); exit(1); }

            CGVar *cv = cgvars_find(vars, target);
            if (!cv || (cv->kind != CG_ARRAY && cv->kind != CG_TABLE)) {
                fprintf(stderr, "ICE: collection target '%s' not found for input set.\n", target);
                exit(1);
            }

            for (size_t m = 0; m < cs->message_count; m++)
                emit_print_expr(cs->message_parts[m], mod, bld, printf_fn, vars, funcs, rt);

            LLVMValueRef handle = LLVMBuildLoad2(bld, LLVMInt64Type(), cv->alloca_, "ih");
            LLVMValueRef idx = NULL;
            if (cs->index) {
                idx = emit_expr(bld, mod, cs->index, vars, funcs, rt);
                idx = coerce_value(bld, idx, TYPE_INT);
            } else {
                if (!collection_index_context) {
                    fprintf(stderr, "ICE: missing loop index context for contextual input set.\n");
                    exit(1);
                }
                CGVar *iv = cgvars_find(vars, collection_index_context);
                if (!iv) {
                    fprintf(stderr, "ICE: unknown loop index '%s' in contextual input set.\n", collection_index_context);
                    exit(1);
                }
                idx = LLVMBuildLoad2(bld, LLVMInt64Type(), iv->alloca_, "ctxii");
            }

            LLVMValueRef i = LLVMConstInt(LLVMInt64Type(), 0, 0);
            LLVMValueRef d = LLVMConstReal(LLVMDoubleType(), 0.0);
            LLVMValueRef c = LLVMConstInt(LLVMInt64Type(), 0, 0);
            LLVMValueRef sval = LLVMConstPointerNull(LLVMPointerType(LLVMInt8Type(), 0));
            int64_t tag = LX_TAG_STRING;

            if (cv->kind == CG_ARRAY) {
                switch (cv->type) {
                case TYPE_INT: {
                    LLVMValueRef tmp = LLVMBuildAlloca(bld, LLVMInt64Type(), "in.arr.i");
                    LLVMBuildCall2(bld,
                                   LLVMFunctionType(LLVMInt32Type(), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(),0)}, 1, 1),
                                   scanf_fn,
                                   (LLVMValueRef[]){build_global_str(mod, bld, "%lld"), tmp}, 2, "");
                    i = LLVMBuildLoad2(bld, LLVMInt64Type(), tmp, "arr.i");
                    tag = LX_TAG_INT;
                    break;
                }
                case TYPE_FLOAT: {
                    LLVMValueRef tmp = LLVMBuildAlloca(bld, LLVMDoubleType(), "in.arr.d");
                    LLVMBuildCall2(bld,
                                   LLVMFunctionType(LLVMInt32Type(), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(),0)}, 1, 1),
                                   scanf_fn,
                                   (LLVMValueRef[]){build_global_str(mod, bld, "%lf"), tmp}, 2, "");
                    d = LLVMBuildLoad2(bld, LLVMDoubleType(), tmp, "arr.d");
                    tag = LX_TAG_FLOAT;
                    break;
                }
                case TYPE_CHAR: {
                    LLVMValueRef tmp = LLVMBuildAlloca(bld, LLVMInt8Type(), "in.arr.c");
                    LLVMBuildCall2(bld,
                                   LLVMFunctionType(LLVMInt32Type(), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(),0)}, 1, 1),
                                   scanf_fn,
                                   (LLVMValueRef[]){build_global_str(mod, bld, " %c"), tmp}, 2, "");
                    LLVMValueRef c8 = LLVMBuildLoad2(bld, LLVMInt8Type(), tmp, "arr.c");
                    c = LLVMBuildZExt(bld, c8, LLVMInt64Type(), "arr.c64");
                    tag = LX_TAG_CHAR;
                    break;
                }
                case TYPE_STRING: {
                    LLVMTypeRef arr_ty = LLVMArrayType(LLVMInt8Type(), 1024);
                    LLVMValueRef buf = LLVMBuildAlloca(bld, arr_ty, "in.arr.sbuf");
                    LLVMValueRef z = LLVMConstInt(LLVMInt32Type(), 0, 0);
                    LLVMValueRef gep_idx[2] = { z, z };
                    LLVMValueRef ptr = LLVMBuildGEP2(bld, arr_ty, buf, gep_idx, 2, "in.arr.sptr");
                    LLVMBuildCall2(bld,
                                   LLVMFunctionType(LLVMInt32Type(), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(),0)}, 1, 1),
                                   scanf_fn,
                                   (LLVMValueRef[]){build_global_str(mod, bld, "%1023s"), ptr}, 2, "");
                    sval = ptr;
                    tag = LX_TAG_STRING;
                    break;
                }
                case TYPE_BOOL: {
                    LLVMValueRef tmp = LLVMBuildAlloca(bld, LLVMInt64Type(), "in.arr.b");
                    LLVMBuildCall2(bld,
                                   LLVMFunctionType(LLVMInt32Type(), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(),0)}, 1, 1),
                                   scanf_fn,
                                   (LLVMValueRef[]){build_global_str(mod, bld, "%lld"), tmp}, 2, "");
                    LLVMValueRef b64 = LLVMBuildLoad2(bld, LLVMInt64Type(), tmp, "arr.b");
                    i = LLVMBuildSelect(bld,
                                        LLVMBuildICmp(bld, LLVMIntNE, b64, LLVMConstInt(LLVMInt64Type(), 0, 0), "bne"),
                                        LLVMConstInt(LLVMInt64Type(), 1, 0),
                                        LLVMConstInt(LLVMInt64Type(), 0, 0),
                                        "b01");
                    tag = LX_TAG_BOOL;
                    break;
                }
                }
            } else {
                LLVMTypeRef arr_ty = LLVMArrayType(LLVMInt8Type(), 1024);
                LLVMValueRef buf = LLVMBuildAlloca(bld, arr_ty, "in.tbl.sbuf");
                LLVMValueRef z = LLVMConstInt(LLVMInt32Type(), 0, 0);
                LLVMValueRef gep_idx[2] = { z, z };
                LLVMValueRef ptr = LLVMBuildGEP2(bld, arr_ty, buf, gep_idx, 2, "in.tbl.sptr");
                LLVMBuildCall2(bld,
                               LLVMFunctionType(LLVMInt32Type(), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(),0)}, 1, 1),
                               scanf_fn,
                               (LLVMValueRef[]){build_global_str(mod, bld, "%1023s"), ptr}, 2, "");
                sval = ptr;
                tag = LX_TAG_STRING;
            }

            LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->set), rt->set,
                           (LLVMValueRef[]){handle, idx,
                                            LLVMConstInt(LLVMInt64Type(), (unsigned long long)tag, 0),
                                            i, d, c, sval},
                           7, "");
            break;
        }

        case STMT_FILE_OPEN: {
            const FileOpenStmt *fo = &s->as.file_open;
            LLVMValueRef path = emit_expr(bld, mod, fo->path, vars, funcs, rt);
            LLVMValueRef makev = LLVMConstInt(LLVMInt64Type(), (unsigned long long)(fo->make_if_missing ? 1 : 0), 0);
            LLVMValueRef fh = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->file_open), rt->file_open,
                                             (LLVMValueRef[]){path, makev}, 2, "fopenh");
            emit_stmts((const Stmt *const *)fo->body->as.block.stmts,
                       fo->body->as.block.count,
                       mod, bld, cur_fn, printf_fn, scanf_fn,
                       vars, funcs, current_return, rt,
                       collection_context, collection_index_context,
                       matrix_context,
                       fh);
            LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->file_close), rt->file_close,
                           (LLVMValueRef[]){fh}, 1, "");
            break;
        }

        case STMT_FILE_WRITE: {
            if (!file_context) {
                fprintf(stderr, "ICE: file write used outside active file context.\n");
                exit(1);
            }
            const FileWriteStmt *fw = &s->as.file_write;
            LLVMValueRef value = emit_expr(bld, mod, fw->value, vars, funcs, rt);
            if (fw->has_line) {
                LLVMValueRef line = coerce_value(bld, emit_expr(bld, mod, fw->line, vars, funcs, rt), TYPE_INT);
                LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->file_write_line), rt->file_write_line,
                               (LLVMValueRef[]){file_context, line, value}, 3, "");
            } else {
                LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->file_write), rt->file_write,
                               (LLVMValueRef[]){file_context, value}, 2, "");
            }
            break;
        }

        case STMT_FILE_READ: {
            if (!file_context) {
                fprintf(stderr, "ICE: file read used outside active file context.\n");
                exit(1);
            }
            const FileReadStmt *fr = &s->as.file_read;
            CGVar *dst = cgvars_find(vars, fr->target);
            if (!dst) {
                fprintf(stderr, "ICE: unknown file read target '%s'.\n", fr->target);
                exit(1);
            }

            if (fr->mode == FILE_READ_LINE) {
                LLVMValueRef line = coerce_value(bld, emit_expr(bld, mod, fr->line, vars, funcs, rt), TYPE_INT);
                LLVMValueRef text = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->file_read_line), rt->file_read_line,
                                                   (LLVMValueRef[]){file_context, line}, 2, "frline");
                LLVMBuildStore(bld, text, dst->alloca_);
            } else {
                LLVMValueRef table_h = NULL;
                if (fr->mode == FILE_READ_ALL) {
                    table_h = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->file_read_all), rt->file_read_all,
                                             (LLVMValueRef[]){file_context}, 1, "frall");
                } else if (fr->mode == FILE_READ_FIRST) {
                    LLVMValueRef cnt = coerce_value(bld, emit_expr(bld, mod, fr->count, vars, funcs, rt), TYPE_INT);
                    table_h = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->file_read_first), rt->file_read_first,
                                             (LLVMValueRef[]){file_context, cnt}, 2, "frfirst");
                } else {
                    LLVMValueRef cnt = coerce_value(bld, emit_expr(bld, mod, fr->count, vars, funcs, rt), TYPE_INT);
                    LLVMValueRef from = coerce_value(bld, emit_expr(bld, mod, fr->from_line, vars, funcs, rt), TYPE_INT);
                    table_h = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->file_read_range), rt->file_read_range,
                                             (LLVMValueRef[]){file_context, cnt, from}, 3, "frrange");
                }
                LLVMBuildStore(bld, table_h, dst->alloca_);
            }
            break;
        }

        case STMT_FILE_CLEAR: {
            if (!file_context) {
                fprintf(stderr, "ICE: file clear used outside active file context.\n");
                exit(1);
            }
            const FileClearStmt *fc = &s->as.file_clear;
            if (fc->mode == FILE_CLEAR_ALL) {
                LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->file_clear), rt->file_clear,
                               (LLVMValueRef[]){file_context}, 1, "");
            } else if (fc->mode == FILE_CLEAR_LINE) {
                LLVMValueRef line = coerce_value(bld, emit_expr(bld, mod, fc->line, vars, funcs, rt), TYPE_INT);
                LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->file_clear_line), rt->file_clear_line,
                               (LLVMValueRef[]){file_context, line}, 2, "");
            } else {
                LLVMValueRef from = coerce_value(bld, emit_expr(bld, mod, fc->from_line, vars, funcs, rt), TYPE_INT);
                LLVMValueRef to = coerce_value(bld, emit_expr(bld, mod, fc->to_line, vars, funcs, rt), TYPE_INT);
                LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->file_clear_range), rt->file_clear_range,
                               (LLVMValueRef[]){file_context, from, to}, 3, "");
            }
            break;
        }

        case STMT_FILE_SET_TITLE: {
            if (!file_context) {
                fprintf(stderr, "ICE: file title update used outside active file context.\n");
                exit(1);
            }
            LLVMValueRef title = emit_expr(bld, mod, s->as.file_set_title.value, vars, funcs, rt);
            LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->file_set_title), rt->file_set_title,
                           (LLVMValueRef[]){file_context, title}, 2, "");
            break;
        }

        case STMT_FILE_CLOSE:
            if (!file_context) {
                fprintf(stderr, "ICE: close file used outside active file context.\n");
                exit(1);
            }
            LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->file_close), rt->file_close,
                           (LLVMValueRef[]){file_context}, 1, "");
            break;

        case STMT_PRINT: {
            const PrintStmt *p = &s->as.print;
            for (size_t j = 0; j < p->count; j++) {
                emit_print_expr(p->args[j], mod, bld, printf_fn, vars, funcs, rt);
                if (j + 1 < p->count) {
                    LLVMBuildCall2(bld,
                                   LLVMFunctionType(LLVMInt32Type(), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(),0)}, 1, 1),
                                   printf_fn,
                                   (LLVMValueRef[]){build_global_str(mod, bld, " ")}, 1, "");
                }
            }
            LLVMBuildCall2(bld,
                           LLVMFunctionType(LLVMInt32Type(), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(),0)}, 1, 1),
                           printf_fn,
                           (LLVMValueRef[]){build_global_str(mod, bld, "\n")}, 1, "");
            break;
        }

        case STMT_SET: {
            const SetStmt *st = &s->as.set;
            CGVar *cv = cgvars_find(vars, st->target);
            LLVMValueRef v = emit_expr(bld, mod, st->value, vars, funcs, rt);
            v = coerce_value(bld, v, cv->type);
            LLVMBuildStore(bld, v, cv->alloca_);
            break;
        }

        case STMT_CONVERT: {
            const ConvertStmt *ct = &s->as.convert;
            CGVar *cv = cgvars_find(vars, ct->target);
            if (!cv || cv->kind != CG_SCALAR) {
                fprintf(stderr, "ICE: convert target '%s' is not a scalar variable.\n", ct->target);
                exit(1);
            }

            if (cv->type != ct->to_type) {
                LLVMValueRef oldv = LLVMBuildLoad2(bld, llvm_type(cv->type), cv->alloca_, "conv_old");
                LLVMValueRef newv = convert_value(bld, oldv, cv->type, ct->to_type, rt);
                LLVMValueRef newa = LLVMBuildAlloca(bld, llvm_type(ct->to_type), "conv_tmp");
                LLVMBuildStore(bld, newv, newa);
                cv->alloca_ = newa;
                cv->type = ct->to_type;
            }
            break;
        }

        case STMT_SET_MATRIX_CTX: {
            const SetMatrixCtxStmt *sm = &s->as.set_matrix_ctx;
            if (!matrix_context) {
                fprintf(stderr, "ICE: contextual matrix read used outside matrix loop context.\n");
                exit(1);
            }

            CGVar *target = cgvars_find(vars, sm->target);
            if (!target || target->kind != CG_SCALAR) {
                fprintf(stderr, "ICE: contextual matrix read target '%s' is not a scalar.\n", sm->target);
                exit(1);
            }

            CGVar *mv = cgvars_find(vars, matrix_context);
            if (!mv || mv->kind != CG_MATRIX) {
                fprintf(stderr, "ICE: contextual matrix read matrix '%s' not found.\n", matrix_context);
                exit(1);
            }

            LLVMValueRef handle = LLVMBuildLoad2(bld, LLVMInt64Type(), mv->alloca_, "rmh");
            LLVMValueRef row = coerce_value(bld, emit_expr(bld, mod, sm->row, vars, funcs, rt), TYPE_INT);
            LLVMValueRef col = coerce_value(bld, emit_expr(bld, mod, sm->col, vars, funcs, rt), TYPE_INT);

            LLVMValueRef raw = NULL;
            switch (mv->type) {
            case TYPE_INT:
                raw = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->matrix_get_i), rt->matrix_get_i,
                                     (LLVMValueRef[]){handle, row, col}, 3, "rm_i");
                break;
            case TYPE_FLOAT:
                raw = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->matrix_get_d), rt->matrix_get_d,
                                     (LLVMValueRef[]){handle, row, col}, 3, "rm_d");
                break;
            case TYPE_CHAR: {
                LLVMValueRef c64 = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->matrix_get_c), rt->matrix_get_c,
                                                  (LLVMValueRef[]){handle, row, col}, 3, "rm_c64");
                raw = LLVMBuildTrunc(bld, c64, LLVMInt8Type(), "rm_c8");
                break;
            }
            case TYPE_STRING:
                raw = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->matrix_get_s), rt->matrix_get_s,
                                     (LLVMValueRef[]){handle, row, col}, 3, "rm_s");
                break;
            case TYPE_BOOL: {
                LLVMValueRef b64 = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->matrix_get_i), rt->matrix_get_i,
                                                  (LLVMValueRef[]){handle, row, col}, 3, "rm_b64");
                raw = LLVMBuildICmp(bld, LLVMIntNE, b64, LLVMConstInt(LLVMInt64Type(), 0, 0), "rm_b1");
                break;
            }
            default:
                fprintf(stderr, "ICE: unsupported matrix type in contextual matrix read.\n");
                exit(1);
            }

            LLVMValueRef v = coerce_value(bld, raw, target->type);
            LLVMBuildStore(bld, v, target->alloca_);
            break;
        }

        case STMT_INPUT: {
            const InputStmt *in = &s->as.input;
            CGVar *cv = cgvars_find(vars, in->target);
            if (!cv) { fprintf(stderr, "ICE: unknown input target '%s'\n", in->target); exit(1); }

            for (size_t m = 0; m < in->message_count; m++)
                emit_print_expr(in->message_parts[m], mod, bld, printf_fn, vars, funcs, rt);

            LLVMValueRef sargv[2];
            switch (cv->type) {
            case TYPE_INT:
                sargv[0] = build_global_str(mod, bld, "%lld");
                sargv[1] = cv->alloca_;
                break;
            case TYPE_FLOAT:
                sargv[0] = build_global_str(mod, bld, "%lf");
                sargv[1] = cv->alloca_;
                break;
            case TYPE_CHAR:
                sargv[0] = build_global_str(mod, bld, " %c");
                sargv[1] = cv->alloca_;
                break;
            case TYPE_STRING: {
                LLVMTypeRef arr_ty = LLVMArrayType(LLVMInt8Type(), 1024);
                LLVMValueRef buf = LLVMBuildAlloca(bld, arr_ty, "inbuf");
                LLVMValueRef z = LLVMConstInt(LLVMInt32Type(), 0, 0);
                LLVMValueRef idx[2] = { z, z };
                LLVMValueRef ptr = LLVMBuildGEP2(bld, arr_ty, buf, idx, 2, "inptr");
                sargv[0] = build_global_str(mod, bld, "%1023s");
                sargv[1] = ptr;
                LLVMBuildStore(bld, ptr, cv->alloca_);
                break;
            }
            case TYPE_BOOL: {
                LLVMValueRef tmp = LLVMBuildAlloca(bld, LLVMInt64Type(), "in.bool.tmp");
                LLVMBuildCall2(bld,
                               LLVMFunctionType(LLVMInt32Type(), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(),0)}, 1, 1),
                               scanf_fn,
                               (LLVMValueRef[]){build_global_str(mod, bld, "%lld"), tmp}, 2, "");
                LLVMValueRef b64 = LLVMBuildLoad2(bld, LLVMInt64Type(), tmp, "b64");
                LLVMValueRef b1 = LLVMBuildICmp(bld, LLVMIntNE, b64, LLVMConstInt(LLVMInt64Type(), 0, 0), "b1");
                LLVMBuildStore(bld, b1, cv->alloca_);
                sargv[0] = build_global_str(mod, bld, "%d");
                sargv[1] = LLVMConstPointerNull(LLVMPointerType(LLVMInt8Type(), 0));
                break;
            }
            }

            if (cv->type != TYPE_BOOL) {
                LLVMBuildCall2(bld,
                               LLVMFunctionType(LLVMInt32Type(), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(),0)}, 1, 1),
                               scanf_fn, sargv, 2, "");
            }
            break;
        }

        case STMT_EMIT: {
            const EmitStmt *em = &s->as.emit;
            CGFunc *cf = cgfuncs_find(funcs, em->name);
            if (!cf) { fprintf(stderr, "ICE: unknown routine '%s'\n", em->name); exit(1); }

            size_t argc = em->count;
            LLVMValueRef *argv = argc ? (LLVMValueRef *)malloc(argc * sizeof(LLVMValueRef)) : NULL;
            if (argc && !argv) { fprintf(stderr, "OOM\n"); exit(1); }
            for (size_t a = 0; a < argc; a++) {
                LLVMValueRef av = emit_expr(bld, mod, em->args[a], vars, funcs, rt);
                if (a < cf->param_count) av = coerce_value(bld, av, cf->param_types[a]);
                argv[a] = av;
            }
            LLVMBuildCall2(bld, cf->fn_type, cf->fn, argv, (unsigned)argc, "");
            free(argv);
            break;
        }

        case STMT_GIVEBACK: {
            const GivebackStmt *gb = &s->as.giveback;
            LLVMValueRef rv = emit_expr(bld, mod, gb->value, vars, funcs, rt);
            rv = coerce_value(bld, rv, current_return);
            LLVMBuildRet(bld, rv);
            break;
        }

        case STMT_FUNCDEF:
            break;

        case STMT_IF: {
            const IfStmt *fi = &s->as.if_;
            LLVMValueRef cond_val = emit_cond(bld, mod, fi->cond, vars, funcs, rt);

            int id = label_counter++;
            char then_name[32], else_name[32], merge_name[32];
            snprintf(then_name, sizeof then_name, "then.%d", id);
            snprintf(else_name, sizeof else_name, "else.%d", id);
            snprintf(merge_name, sizeof merge_name, "merge.%d", id);

            LLVMBasicBlockRef then_bb = LLVMAppendBasicBlock(cur_fn, then_name);
            LLVMBasicBlockRef else_bb = fi->else_block ? LLVMAppendBasicBlock(cur_fn, else_name) : NULL;
            LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlock(cur_fn, merge_name);

            if (else_bb) LLVMBuildCondBr(bld, cond_val, then_bb, else_bb);
            else LLVMBuildCondBr(bld, cond_val, then_bb, merge_bb);

            LLVMPositionBuilderAtEnd(bld, then_bb);
            emit_stmts((const Stmt *const *)fi->then_block->as.block.stmts,
                       fi->then_block->as.block.count,
                       mod, bld, cur_fn, printf_fn, scanf_fn,
                       vars, funcs, current_return, rt,
                       collection_context, collection_index_context,
                       matrix_context,
                       file_context);
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(bld))) LLVMBuildBr(bld, merge_bb);

            if (else_bb) {
                LLVMPositionBuilderAtEnd(bld, else_bb);
                const Stmt *eb = fi->else_block;
                emit_stmts(&eb, 1, mod, bld, cur_fn, printf_fn, scanf_fn,
                           vars, funcs, current_return, rt,
                           collection_context, collection_index_context,
                           matrix_context,
                           file_context);
                if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(bld))) LLVMBuildBr(bld, merge_bb);
            }
            LLVMPositionBuilderAtEnd(bld, merge_bb);
            break;
        }

        case STMT_BLOCK:
            emit_stmts((const Stmt *const *)s->as.block.stmts, s->as.block.count,
                       mod, bld, cur_fn, printf_fn, scanf_fn,
                       vars, funcs, current_return, rt,
                       collection_context, collection_index_context,
                       matrix_context,
                       file_context);
            break;

        case STMT_WHILE: {
            const WhileStmt *w = &s->as.while_;
            int id = label_counter++;
            char cond_name[32], body_name[32], after_name[32];
            snprintf(cond_name, sizeof cond_name, "wh.cond.%d", id);
            snprintf(body_name, sizeof body_name, "wh.body.%d", id);
            snprintf(after_name, sizeof after_name, "wh.after.%d", id);

            LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlock(cur_fn, cond_name);
            LLVMBasicBlockRef body_bb = LLVMAppendBasicBlock(cur_fn, body_name);
            LLVMBasicBlockRef after_bb = LLVMAppendBasicBlock(cur_fn, after_name);

            LLVMBuildBr(bld, cond_bb);
            LLVMPositionBuilderAtEnd(bld, cond_bb);
            LLVMValueRef c = emit_cond(bld, mod, w->cond, vars, funcs, rt);
            LLVMBuildCondBr(bld, c, body_bb, after_bb);

            LLVMPositionBuilderAtEnd(bld, body_bb);
            emit_stmts((const Stmt *const *)w->body->as.block.stmts,
                       w->body->as.block.count,
                       mod, bld, cur_fn, printf_fn, scanf_fn,
                       vars, funcs, current_return, rt,
                       collection_context, collection_index_context,
                       matrix_context,
                       file_context);
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(bld))) LLVMBuildBr(bld, cond_bb);

            LLVMPositionBuilderAtEnd(bld, after_bb);
            break;
        }

        case STMT_FOR: {
            const ForStmt *f = &s->as.for_;
            int id = label_counter++;
            char chk_name[32], body_name[32], after_name[32];
            snprintf(chk_name, sizeof chk_name, "for.chk.%d", id);
            snprintf(body_name, sizeof body_name, "for.body.%d", id);
            snprintf(after_name, sizeof after_name, "for.after.%d", id);

            size_t saved_vars_n = vars->n;
            LLVMValueRef i_alloca = LLVMBuildAlloca(bld, LLVMInt64Type(), f->var);
            cgvars_add(vars, f->var, CG_SCALAR, TYPE_INT, i_alloca);

            LLVMValueRef from_val = emit_expr(bld, mod, f->from, vars, funcs, rt);
            from_val = coerce_value(bld, from_val, TYPE_INT);
            LLVMBuildStore(bld, from_val, i_alloca);

            LLVMValueRef end_val = emit_expr(bld, mod, f->to, vars, funcs, rt);
            end_val = coerce_value(bld, end_val, TYPE_INT);
            LLVMValueRef end_alloca = LLVMBuildAlloca(bld, LLVMInt64Type(), "for.end");
            LLVMBuildStore(bld, end_val, end_alloca);

            LLVMValueRef step_val = f->step ? emit_expr(bld, mod, f->step, vars, funcs, rt) : LLVMConstInt(LLVMInt64Type(), 1, 1);
            step_val = coerce_value(bld, step_val, TYPE_INT);
            LLVMValueRef step_alloca = LLVMBuildAlloca(bld, LLVMInt64Type(), "for.step");
            LLVMBuildStore(bld, step_val, step_alloca);

            LLVMBasicBlockRef chk_bb = LLVMAppendBasicBlock(cur_fn, chk_name);
            LLVMBasicBlockRef body_bb = LLVMAppendBasicBlock(cur_fn, body_name);
            LLVMBasicBlockRef after_bb = LLVMAppendBasicBlock(cur_fn, after_name);
            LLVMBuildBr(bld, chk_bb);

            LLVMPositionBuilderAtEnd(bld, chk_bb);
            LLVMValueRef i_cur = LLVMBuildLoad2(bld, LLVMInt64Type(), i_alloca, "i");
            LLVMValueRef end_cur = LLVMBuildLoad2(bld, LLVMInt64Type(), end_alloca, "end");
            LLVMValueRef step_cur = LLVMBuildLoad2(bld, LLVMInt64Type(), step_alloca, "stp");
            LLVMValueRef zero = LLVMConstInt(LLVMInt64Type(), 0, 1);
            LLVMValueRef step_fwd = LLVMBuildICmp(bld, LLVMIntSGT, step_cur, zero, "sfwd");
            LLVMValueRef fwd_ok = LLVMBuildICmp(bld, LLVMIntSLE, i_cur, end_cur, "fok");
            LLVMValueRef bwd_ok = LLVMBuildICmp(bld, LLVMIntSGE, i_cur, end_cur, "bok");
            LLVMValueRef cond = LLVMBuildSelect(bld, step_fwd, fwd_ok, bwd_ok, "fcond");
            LLVMBuildCondBr(bld, cond, body_bb, after_bb);

            LLVMPositionBuilderAtEnd(bld, body_bb);
            emit_stmts((const Stmt *const *)f->body->as.block.stmts,
                       f->body->as.block.count,
                       mod, bld, cur_fn, printf_fn, scanf_fn,
                       vars, funcs, current_return, rt,
                       collection_context, collection_index_context,
                       matrix_context,
                       file_context);
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(bld))) {
                LLVMValueRef i_r = LLVMBuildLoad2(bld, LLVMInt64Type(), i_alloca, "ir");
                LLVMValueRef s_r = LLVMBuildLoad2(bld, LLVMInt64Type(), step_alloca, "sr");
                LLVMValueRef i_n = LLVMBuildAdd(bld, i_r, s_r, "inext");
                LLVMBuildStore(bld, i_n, i_alloca);
                LLVMBuildBr(bld, chk_bb);
            }

            LLVMPositionBuilderAtEnd(bld, after_bb);
            for (size_t k = saved_vars_n; k < vars->n; k++) free(vars->v[k].name);
            vars->n = saved_vars_n;
            break;
        }

        case STMT_FOR_EACH: {
            const ForEachStmt *fe = &s->as.for_each;
            CGVar *col = cgvars_find(vars, fe->collection);
            if (!col) { fprintf(stderr, "ICE: unknown collection '%s'\n", fe->collection); exit(1); }
            (void)col;

            size_t saved_vars_n = vars->n;
            LLVMValueRef idx_alloca = LLVMBuildAlloca(bld, LLVMInt64Type(), fe->index_var);
            LLVMValueRef from_val = emit_expr(bld, mod, fe->from, vars, funcs, rt);
            from_val = coerce_value(bld, from_val, TYPE_INT);
            LLVMBuildStore(bld, from_val, idx_alloca);
            cgvars_add(vars, fe->index_var, CG_SCALAR, TYPE_INT, idx_alloca);

            LLVMValueRef to_val = emit_expr(bld, mod, fe->to, vars, funcs, rt);
            to_val = coerce_value(bld, to_val, TYPE_INT);
            LLVMValueRef to_alloca = LLVMBuildAlloca(bld, LLVMInt64Type(), "fi.to");
            LLVMBuildStore(bld, to_val, to_alloca);

            LLVMValueRef step_val = fe->step ? emit_expr(bld, mod, fe->step, vars, funcs, rt) : LLVMConstInt(LLVMInt64Type(), 1, 1);
            step_val = coerce_value(bld, step_val, TYPE_INT);
            LLVMValueRef step_alloca = LLVMBuildAlloca(bld, LLVMInt64Type(), "fi.step");
            LLVMBuildStore(bld, step_val, step_alloca);

            int id = label_counter++;
            char chk_name[32], body_name[32], after_name[32];
            snprintf(chk_name, sizeof chk_name, "fe.chk.%d", id);
            snprintf(body_name, sizeof body_name, "fe.body.%d", id);
            snprintf(after_name, sizeof after_name, "fe.after.%d", id);

            LLVMBasicBlockRef chk_bb = LLVMAppendBasicBlock(cur_fn, chk_name);
            LLVMBasicBlockRef body_bb = LLVMAppendBasicBlock(cur_fn, body_name);
            LLVMBasicBlockRef after_bb = LLVMAppendBasicBlock(cur_fn, after_name);
            LLVMBuildBr(bld, chk_bb);

            LLVMPositionBuilderAtEnd(bld, chk_bb);
            LLVMValueRef idx = LLVMBuildLoad2(bld, LLVMInt64Type(), idx_alloca, "idx");
            LLVMValueRef endv = LLVMBuildLoad2(bld, LLVMInt64Type(), to_alloca, "to");
            LLVMValueRef step_cur = LLVMBuildLoad2(bld, LLVMInt64Type(), step_alloca, "fstep");
            LLVMValueRef zero = LLVMConstInt(LLVMInt64Type(), 0, 1);
            LLVMValueRef step_fwd = LLVMBuildICmp(bld, LLVMIntSGT, step_cur, zero, "fesfwd");
            LLVMValueRef fwd_ok = LLVMBuildICmp(bld, LLVMIntSLE, idx, endv, "fefok");
            LLVMValueRef bwd_ok = LLVMBuildICmp(bld, LLVMIntSGE, idx, endv, "febok");
            LLVMValueRef cond = LLVMBuildSelect(bld, step_fwd, fwd_ok, bwd_ok, "fecond");
            LLVMBuildCondBr(bld, cond, body_bb, after_bb);

            LLVMPositionBuilderAtEnd(bld, body_bb);
            emit_stmts((const Stmt *const *)fe->body->as.block.stmts,
                       fe->body->as.block.count,
                       mod, bld, cur_fn, printf_fn, scanf_fn,
                       vars, funcs, current_return, rt,
                       fe->collection, fe->index_var,
                       matrix_context,
                       file_context);
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(bld))) {
                LLVMValueRef cur = LLVMBuildLoad2(bld, LLVMInt64Type(), idx_alloca, "curi");
                LLVMValueRef st = LLVMBuildLoad2(bld, LLVMInt64Type(), step_alloca, "st");
                LLVMValueRef next = LLVMBuildAdd(bld, cur, st, "nexti");
                LLVMBuildStore(bld, next, idx_alloca);
                LLVMBuildBr(bld, chk_bb);
            }

            LLVMPositionBuilderAtEnd(bld, after_bb);
            for (size_t k = saved_vars_n; k < vars->n; k++) free(vars->v[k].name);
            vars->n = saved_vars_n;
            break;
        }

        case STMT_FOR_MATRIX: {
            const ForMatrixStmt *fm = &s->as.for_matrix;
            CGVar *mv = cgvars_find(vars, fm->matrix);
            if (!mv || mv->kind != CG_MATRIX) {
                fprintf(stderr, "ICE: matrix '%s' not found for matrix loop.\n", fm->matrix);
                exit(1);
            }

            size_t saved_vars_n = vars->n;
            LLVMValueRef i_alloca = LLVMBuildAlloca(bld, LLVMInt64Type(), fm->row_var);
            LLVMValueRef j_alloca = LLVMBuildAlloca(bld, LLVMInt64Type(), fm->col_var);
            cgvars_add(vars, fm->row_var, CG_SCALAR, TYPE_INT, i_alloca);
            cgvars_add(vars, fm->col_var, CG_SCALAR, TYPE_INT, j_alloca);

            LLVMValueRef handle = LLVMBuildLoad2(bld, LLVMInt64Type(), mv->alloca_, "mfh");
            LLVMValueRef rows = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->rows), rt->rows,
                                               (LLVMValueRef[]){handle}, 1, "mrows");
            LLVMValueRef cols = LLVMBuildCall2(bld, LLVMGlobalGetValueType(rt->cols), rt->cols,
                                               (LLVMValueRef[]){handle}, 1, "mcols");

            LLVMValueRef row_from = NULL;
            LLVMValueRef row_to = NULL;
            LLVMValueRef col_from = NULL;
            LLVMValueRef col_to = NULL;

            if (fm->row_from && fm->row_to && fm->col_from && fm->col_to) {
                row_from = coerce_value(bld, emit_expr(bld, mod, fm->row_from, vars, funcs, rt), TYPE_INT);
                row_to   = coerce_value(bld, emit_expr(bld, mod, fm->row_to, vars, funcs, rt), TYPE_INT);
                col_from = coerce_value(bld, emit_expr(bld, mod, fm->col_from, vars, funcs, rt), TYPE_INT);
                col_to   = coerce_value(bld, emit_expr(bld, mod, fm->col_to, vars, funcs, rt), TYPE_INT);
            } else {
                row_from = LLVMConstInt(LLVMInt64Type(), 0, 0);
                row_to   = LLVMBuildSub(bld, rows, LLVMConstInt(LLVMInt64Type(), 1, 0), "fm.rto");
                col_from = LLVMConstInt(LLVMInt64Type(), 0, 0);
                col_to   = LLVMBuildSub(bld, cols, LLVMConstInt(LLVMInt64Type(), 1, 0), "fm.cto");
            }

            LLVMBuildStore(bld, row_from, i_alloca);

            int id = label_counter++;
            char ochk_name[32], obody_name[32], oafter_name[32];
            char ichk_name[32], ibody_name[32], iafter_name[32];
            snprintf(ochk_name, sizeof ochk_name, "fm.ochk.%d", id);
            snprintf(obody_name, sizeof obody_name, "fm.obody.%d", id);
            snprintf(oafter_name, sizeof oafter_name, "fm.oafter.%d", id);
            snprintf(ichk_name, sizeof ichk_name, "fm.ichk.%d", id);
            snprintf(ibody_name, sizeof ibody_name, "fm.ibody.%d", id);
            snprintf(iafter_name, sizeof iafter_name, "fm.iafter.%d", id);

            LLVMBasicBlockRef ochk_bb = LLVMAppendBasicBlock(cur_fn, ochk_name);
            LLVMBasicBlockRef obody_bb = LLVMAppendBasicBlock(cur_fn, obody_name);
            LLVMBasicBlockRef oafter_bb = LLVMAppendBasicBlock(cur_fn, oafter_name);
            LLVMBasicBlockRef ichk_bb = LLVMAppendBasicBlock(cur_fn, ichk_name);
            LLVMBasicBlockRef ibody_bb = LLVMAppendBasicBlock(cur_fn, ibody_name);
            LLVMBasicBlockRef iafter_bb = LLVMAppendBasicBlock(cur_fn, iafter_name);

            LLVMBuildBr(bld, ochk_bb);

            LLVMPositionBuilderAtEnd(bld, ochk_bb);
            LLVMValueRef i_cur = LLVMBuildLoad2(bld, LLVMInt64Type(), i_alloca, "fm.i");
            LLVMValueRef ocond = LLVMBuildICmp(bld, LLVMIntSLE, i_cur, row_to, "fm.ocond");
            LLVMBuildCondBr(bld, ocond, obody_bb, oafter_bb);

            LLVMPositionBuilderAtEnd(bld, obody_bb);
            LLVMBuildStore(bld, col_from, j_alloca);
            LLVMBuildBr(bld, ichk_bb);

            LLVMPositionBuilderAtEnd(bld, ichk_bb);
            LLVMValueRef j_cur = LLVMBuildLoad2(bld, LLVMInt64Type(), j_alloca, "fm.j");
            LLVMValueRef icond = LLVMBuildICmp(bld, LLVMIntSLE, j_cur, col_to, "fm.icond");
            LLVMBuildCondBr(bld, icond, ibody_bb, iafter_bb);

            LLVMPositionBuilderAtEnd(bld, ibody_bb);
            emit_stmts((const Stmt *const *)fm->body->as.block.stmts,
                       fm->body->as.block.count,
                       mod, bld, cur_fn, printf_fn, scanf_fn,
                       vars, funcs, current_return, rt,
                       collection_context, collection_index_context,
                       fm->matrix,
                       file_context);
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(bld))) {
                LLVMValueRef j_now = LLVMBuildLoad2(bld, LLVMInt64Type(), j_alloca, "fm.jn");
                LLVMValueRef j_next = LLVMBuildAdd(bld, j_now, LLVMConstInt(LLVMInt64Type(), 1, 0), "fm.jnext");
                LLVMBuildStore(bld, j_next, j_alloca);
                LLVMBuildBr(bld, ichk_bb);
            }

            LLVMPositionBuilderAtEnd(bld, iafter_bb);
            LLVMValueRef i_now = LLVMBuildLoad2(bld, LLVMInt64Type(), i_alloca, "fm.in");
            LLVMValueRef i_next = LLVMBuildAdd(bld, i_now, LLVMConstInt(LLVMInt64Type(), 1, 0), "fm.inext");
            LLVMBuildStore(bld, i_next, i_alloca);
            LLVMBuildBr(bld, ochk_bb);

            LLVMPositionBuilderAtEnd(bld, oafter_bb);
            for (size_t k = saved_vars_n; k < vars->n; k++) free(vars->v[k].name);
            vars->n = saved_vars_n;
            break;
        }

        case STMT_REPEAT: {
            const RepeatStmt *r = &s->as.repeat_;
            int id = label_counter++;
            char body_name[32], cond_name[32], after_name[32];
            snprintf(body_name, sizeof body_name, "rep.body.%d", id);
            snprintf(cond_name, sizeof cond_name, "rep.cond.%d", id);
            snprintf(after_name, sizeof after_name, "rep.after.%d", id);

            LLVMBasicBlockRef body_bb = LLVMAppendBasicBlock(cur_fn, body_name);
            LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlock(cur_fn, cond_name);
            LLVMBasicBlockRef after_bb = LLVMAppendBasicBlock(cur_fn, after_name);

            LLVMBuildBr(bld, body_bb);

            LLVMPositionBuilderAtEnd(bld, body_bb);
            emit_stmts((const Stmt *const *)r->body->as.block.stmts,
                       r->body->as.block.count,
                       mod, bld, cur_fn, printf_fn, scanf_fn,
                       vars, funcs, current_return, rt,
                       collection_context, collection_index_context,
                       matrix_context,
                       file_context);
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(bld))) LLVMBuildBr(bld, cond_bb);

            LLVMPositionBuilderAtEnd(bld, cond_bb);
            LLVMValueRef c = emit_cond(bld, mod, r->cond, vars, funcs, rt);
            LLVMBuildCondBr(bld, c, after_bb, body_bb);

            LLVMPositionBuilderAtEnd(bld, after_bb);
            break;
        }

        case STMT_ATTEMPT: {
            const AttemptStmt *a = &s->as.attempt_;
            int id = label_counter++;
            char chk_name[32], body_name[32], post_name[32], fail_name[32], end_name[32];
            snprintf(chk_name, sizeof chk_name, "att.chk.%d", id);
            snprintf(body_name, sizeof body_name, "att.body.%d", id);
            snprintf(post_name, sizeof post_name, "att.post.%d", id);
            snprintf(fail_name, sizeof fail_name, "att.fail.%d", id);
            snprintf(end_name, sizeof end_name, "att.end.%d", id);

            LLVMValueRef cnt_a = LLVMBuildAlloca(bld, LLVMInt64Type(), "att.cnt");
            LLVMBuildStore(bld, LLVMConstInt(LLVMInt64Type(), 0, 0), cnt_a);

            LLVMValueRef max_v = emit_expr(bld, mod, a->max, vars, funcs, rt);
            max_v = coerce_value(bld, max_v, TYPE_INT);
            LLVMValueRef max_a = LLVMBuildAlloca(bld, LLVMInt64Type(), "att.max");
            LLVMBuildStore(bld, max_v, max_a);

            LLVMBasicBlockRef chk_bb = LLVMAppendBasicBlock(cur_fn, chk_name);
            LLVMBasicBlockRef body_bb = LLVMAppendBasicBlock(cur_fn, body_name);
            LLVMBasicBlockRef post_bb = LLVMAppendBasicBlock(cur_fn, post_name);
            LLVMBasicBlockRef fail_bb = a->failure ? LLVMAppendBasicBlock(cur_fn, fail_name) : NULL;
            LLVMBasicBlockRef end_bb = LLVMAppendBasicBlock(cur_fn, end_name);

            LLVMBuildBr(bld, chk_bb);

            LLVMPositionBuilderAtEnd(bld, chk_bb);
            LLVMValueRef cnt_c = LLVMBuildLoad2(bld, LLVMInt64Type(), cnt_a, "cnt");
            LLVMValueRef max_c = LLVMBuildLoad2(bld, LLVMInt64Type(), max_a, "max");
            LLVMValueRef cnt_ok = LLVMBuildICmp(bld, LLVMIntSLT, cnt_c, max_c, "cok");
            LLVMValueRef acd = emit_cond(bld, mod, a->cond, vars, funcs, rt);
            LLVMValueRef both = LLVMBuildAnd(bld, cnt_ok, acd, "both");
            LLVMBuildCondBr(bld, both, body_bb, post_bb);

            LLVMPositionBuilderAtEnd(bld, body_bb);
            emit_stmts((const Stmt *const *)a->body->as.block.stmts,
                       a->body->as.block.count,
                       mod, bld, cur_fn, printf_fn, scanf_fn,
                       vars, funcs, current_return, rt,
                       collection_context, collection_index_context,
                       matrix_context,
                       file_context);
            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(bld))) {
                LLVMValueRef cr = LLVMBuildLoad2(bld, LLVMInt64Type(), cnt_a, "cr");
                LLVMValueRef cn = LLVMBuildAdd(bld, cr, LLVMConstInt(LLVMInt64Type(), 1, 0), "cn");
                LLVMBuildStore(bld, cn, cnt_a);
                LLVMBuildBr(bld, chk_bb);
            }

            LLVMPositionBuilderAtEnd(bld, post_bb);
            if (a->failure) {
                LLVMValueRef cnt_f = LLVMBuildLoad2(bld, LLVMInt64Type(), cnt_a, "cf");
                LLVMValueRef max_f = LLVMBuildLoad2(bld, LLVMInt64Type(), max_a, "mf");
                LLVMValueRef at_lim = LLVMBuildICmp(bld, LLVMIntEQ, cnt_f, max_f, "lim");
                LLVMValueRef still = emit_cond(bld, mod, a->cond, vars, funcs, rt);
                LLVMValueRef failed = LLVMBuildAnd(bld, at_lim, still, "failed");
                LLVMBuildCondBr(bld, failed, fail_bb, end_bb);

                LLVMPositionBuilderAtEnd(bld, fail_bb);
                emit_stmts((const Stmt *const *)a->failure->as.block.stmts,
                           a->failure->as.block.count,
                           mod, bld, cur_fn, printf_fn, scanf_fn,
                           vars, funcs, current_return, rt,
                           collection_context, collection_index_context,
                           matrix_context,
                           file_context);
                if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(bld))) LLVMBuildBr(bld, end_bb);
            } else {
                LLVMBuildBr(bld, end_bb);
            }

            LLVMPositionBuilderAtEnd(bld, end_bb);
            break;
        }
        }
    }
}

int codegen_emit(const Program *prog, const SymTable *tab,
                 const char *out_path, LLVMModuleRef *out_mod) {
    (void)tab;

    if (out_mod) *out_mod = NULL;
    if (!prog || !out_path) return -1;

    fmt_counter = 0;
    label_counter = 0;

    g_codegen_ctx = LLVMContextCreate();
    LLVMModuleRef mod = LLVMModuleCreateWithNameInContext("lexico", g_codegen_ctx);
    LLVMBuilderRef bld = LLVMCreateBuilderInContext(g_codegen_ctx);

    LLVMValueRef printf_fn = declare_printf(mod);
    LLVMValueRef scanf_fn = declare_scanf(mod);
    RuntimeFns rt = declare_runtime(mod);

    CGFuncs funcs;
    cgfuncs_init(&funcs);
    cgvars_init(&g_global_vars);

    for (size_t i = 0; i < prog->count; i++)
        declare_object_globals_from_stmt(prog->stmts[i], mod);

    for (size_t i = 0; i < prog->count; i++)
        cg_declare_funcdefs_from_stmt(prog->stmts[i], mod, &funcs);

    for (size_t i = 0; i < prog->count; i++)
        cg_emit_funcdef_bodies_from_stmt(prog->stmts[i], mod, bld, printf_fn, scanf_fn, &funcs, &rt);

    LLVMTypeRef main_ty = LLVMFunctionType(LLVMInt32Type(), NULL, 0, 0);
    LLVMValueRef main_fn = LLVMAddFunction(mod, "main", main_ty);
    LLVMBasicBlockRef main_entry = LLVMAppendBasicBlock(main_fn, "entry");
    LLVMPositionBuilderAtEnd(bld, main_entry);

    CGVars main_vars;
    cgvars_init(&main_vars);

    emit_stmts((const Stmt *const *)prog->stmts, prog->count,
               mod, bld, main_fn, printf_fn, scanf_fn,
               &main_vars, &funcs, TYPE_INT, &rt, NULL, NULL, NULL, NULL);

    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(bld)))
        LLVMBuildRet(bld, LLVMConstInt(LLVMInt32Type(), 0, 0));

    char *err = NULL;
    if (LLVMVerifyModule(mod, LLVMReturnStatusAction, &err)) {
        fprintf(stderr, "LLVM verification failed:\n%s\n", err);
        LLVMDisposeMessage(err);
        cgvars_free(&main_vars);
        cgfuncs_free(&funcs);
        LLVMDisposeBuilder(bld);
        LLVMDisposeModule(mod);
        LLVMContextDispose(g_codegen_ctx);
        g_codegen_ctx = NULL;
        return -1;
    }
    LLVMDisposeMessage(err);

    if (LLVMPrintModuleToFile(mod, out_path, &err)) {
        fprintf(stderr, "Error writing IR: %s\n", err);
        LLVMDisposeMessage(err);
        cgvars_free(&main_vars);
        cgfuncs_free(&funcs);
        LLVMDisposeBuilder(bld);
        LLVMDisposeModule(mod);
        LLVMContextDispose(g_codegen_ctx);
        g_codegen_ctx = NULL;
        return -1;
    }

    cgvars_free(&main_vars);
    cgfuncs_free(&funcs);
    cgvars_free(&g_global_vars);
    LLVMDisposeBuilder(bld);

    if (out_mod) *out_mod = mod;
    else {
        LLVMDisposeModule(mod);
        LLVMContextDispose(g_codegen_ctx);
        g_codegen_ctx = NULL;
    }
    return 0;
}

int codegen_run(LLVMModuleRef mod) {
    if (!mod) return -1;

    LLVMLinkInMCJIT();
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    char *err = NULL;
    LLVMExecutionEngineRef engine = NULL;
    if (LLVMCreateExecutionEngineForModule(&engine, mod, &err)) {
        fprintf(stderr, "JIT error: %s\n", err);
        LLVMDisposeMessage(err);
        LLVMDisposeModule(mod);
        if (g_codegen_ctx) {
            LLVMContextDispose(g_codegen_ctx);
            g_codegen_ctx = NULL;
        }
        return -1;
    }

    LLVMValueRef fn = NULL;
    fn = LLVMGetNamedFunction(mod, "lx_array_new");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_array_new);
    fn = LLVMGetNamedFunction(mod, "lx_table_new");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_table_new);
    fn = LLVMGetNamedFunction(mod, "lx_matrix_new");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_matrix_new);
    fn = LLVMGetNamedFunction(mod, "lx_len");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_len);
    fn = LLVMGetNamedFunction(mod, "lx_rows");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_rows);
    fn = LLVMGetNamedFunction(mod, "lx_cols");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_cols);
    fn = LLVMGetNamedFunction(mod, "lx_strlen");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_strlen);
    fn = LLVMGetNamedFunction(mod, "lx_str_at");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_str_at);
    fn = LLVMGetNamedFunction(mod, "lx_str_concat");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_str_concat);
    fn = LLVMGetNamedFunction(mod, "lx_str_minus");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_str_minus);
    fn = LLVMGetNamedFunction(mod, "lx_i64_to_string");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_i64_to_string);
    fn = LLVMGetNamedFunction(mod, "lx_f64_to_string");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_f64_to_string);
    fn = LLVMGetNamedFunction(mod, "lx_char_to_string");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_char_to_string);
    fn = LLVMGetNamedFunction(mod, "lx_bool_to_string");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_bool_to_string);
    fn = LLVMGetNamedFunction(mod, "lx_string_to_i64");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_string_to_i64);
    fn = LLVMGetNamedFunction(mod, "lx_string_to_f64");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_string_to_f64);
    fn = LLVMGetNamedFunction(mod, "lx_string_to_char");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_string_to_char);
    fn = LLVMGetNamedFunction(mod, "lx_str_contains");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_str_contains);
    fn = LLVMGetNamedFunction(mod, "lx_str_contains_char");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_str_contains_char);
    fn = LLVMGetNamedFunction(mod, "lx_str_position");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_str_position);
    fn = LLVMGetNamedFunction(mod, "lx_str_position_char");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_str_position_char);
    fn = LLVMGetNamedFunction(mod, "lx_extract_from");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_extract_from);
    fn = LLVMGetNamedFunction(mod, "lx_extract_range");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_extract_range);
    fn = LLVMGetNamedFunction(mod, "lx_count");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_count);
    fn = LLVMGetNamedFunction(mod, "lx_position");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_position);
    fn = LLVMGetNamedFunction(mod, "lx_sort");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_sort);
    fn = LLVMGetNamedFunction(mod, "lx_set");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_set);
    fn = LLVMGetNamedFunction(mod, "lx_append");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_append);
    fn = LLVMGetNamedFunction(mod, "lx_get_tag");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_get_tag);
    fn = LLVMGetNamedFunction(mod, "lx_get_i");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_get_i);
    fn = LLVMGetNamedFunction(mod, "lx_get_d");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_get_d);
    fn = LLVMGetNamedFunction(mod, "lx_get_c");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_get_c);
    fn = LLVMGetNamedFunction(mod, "lx_get_s");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_get_s);
    fn = LLVMGetNamedFunction(mod, "lx_matrix_set");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_matrix_set);
    fn = LLVMGetNamedFunction(mod, "lx_matrix_get_i");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_matrix_get_i);
    fn = LLVMGetNamedFunction(mod, "lx_matrix_get_d");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_matrix_get_d);
    fn = LLVMGetNamedFunction(mod, "lx_matrix_get_c");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_matrix_get_c);
    fn = LLVMGetNamedFunction(mod, "lx_matrix_get_s");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_matrix_get_s);
    fn = LLVMGetNamedFunction(mod, "lx_print_cell");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_print_cell);
    fn = LLVMGetNamedFunction(mod, "lx_print_array_ascii");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_print_array_ascii);
    fn = LLVMGetNamedFunction(mod, "lx_print_table_ascii");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_print_table_ascii);
    fn = LLVMGetNamedFunction(mod, "lx_print_matrix_ascii");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_print_matrix_ascii);
    fn = LLVMGetNamedFunction(mod, "lx_file_open");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_file_open);
    fn = LLVMGetNamedFunction(mod, "lx_file_close");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_file_close);
    fn = LLVMGetNamedFunction(mod, "lx_file_write");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_file_write);
    fn = LLVMGetNamedFunction(mod, "lx_file_write_line");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_file_write_line);
    fn = LLVMGetNamedFunction(mod, "lx_file_read_all");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_file_read_all);
    fn = LLVMGetNamedFunction(mod, "lx_file_read_line");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_file_read_line);
    fn = LLVMGetNamedFunction(mod, "lx_file_read_first");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_file_read_first);
    fn = LLVMGetNamedFunction(mod, "lx_file_read_range");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_file_read_range);
    fn = LLVMGetNamedFunction(mod, "lx_file_clear");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_file_clear);
    fn = LLVMGetNamedFunction(mod, "lx_file_clear_line");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_file_clear_line);
    fn = LLVMGetNamedFunction(mod, "lx_file_clear_range");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_file_clear_range);
    fn = LLVMGetNamedFunction(mod, "lx_file_set_title");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_file_set_title);
    fn = LLVMGetNamedFunction(mod, "lx_file_line_count");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_file_line_count);
    fn = LLVMGetNamedFunction(mod, "lx_cleanup");
    if (fn) LLVMAddGlobalMapping(engine, fn, (void *)&lx_cleanup);

    LLVMValueRef main_fn = LLVMGetNamedFunction(mod, "main");
    if (!main_fn) {
        fprintf(stderr, "Error: main() not found in module.\n");
        LLVMDisposeExecutionEngine(engine);
        if (g_codegen_ctx) {
            LLVMContextDispose(g_codegen_ctx);
            g_codegen_ctx = NULL;
        }
        return -1;
    }

    LLVMGenericValueRef result = LLVMRunFunction(engine, main_fn, 0, NULL);
    int rc = (int)LLVMGenericValueToInt(result, 1);
    LLVMDisposeGenericValue(result);

    LLVMDisposeExecutionEngine(engine);
    if (g_codegen_ctx) {
        LLVMContextDispose(g_codegen_ctx);
        g_codegen_ctx = NULL;
    }
    return rc;
}

#ifdef __cplusplus
}
#endif