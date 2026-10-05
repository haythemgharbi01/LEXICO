
#ifndef LEXICO_CODEGEN_H
#define LEXICO_CODEGEN_H

#ifdef __cplusplus
struct Program;
struct SymTable;
#else
typedef struct Program Program;
typedef struct SymTable SymTable;
#endif

/*
 *   Keep this public header independent of the LLVM installation.  The
 * interface only passes the module opaquely, so the full LLVM C API header
 * is unnecessary here and may not be available to consumers. */
typedef struct LLVMOpaqueModule *LLVMModuleRef;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Generate LLVM IR for the whole program.
 * On success writes the .ll text to `out_path`, stores the module
 * in *out_mod (caller must dispose), and returns 0.
 */
int codegen_emit(const Program *prog, const SymTable *tab,
                 const char *out_path, LLVMModuleRef *out_mod);

/*
 * JIT-execute the main() inside `mod`, print any output, and
 * return the exit code of the user program.  Disposes `mod`.
 */
int codegen_run(LLVMModuleRef mod);

#ifdef __cplusplus
}
#endif

/*
 * Lexico runtime helpers exposed to generated code.
 * extern "C" prevents C++ name mangling so LLVM IR can link them.
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

int64_t lx_array_new(int64_t elem_tag, int64_t size);
int64_t lx_table_new(int64_t size);
int64_t lx_matrix_new(int64_t elem_tag, int64_t rows, int64_t cols);
int64_t lx_matrix_get_i(int64_t handle, int64_t row, int64_t col);
int64_t lx_matrix_get_c(int64_t handle, int64_t row, int64_t col);
int64_t lx_len(int64_t handle);
int64_t lx_rows(int64_t handle);
int64_t lx_cols(int64_t handle);
int64_t lx_strlen(const char *s);
int64_t lx_str_at(const char *s, int64_t index);
int64_t lx_str_contains(const char *source, const char *needle);
int64_t lx_str_contains_char(const char *source, int64_t ch);
int64_t lx_str_position(const char *source, const char *needle);
int64_t lx_str_position_char(const char *source, int64_t ch);
int64_t lx_count(int64_t handle, int64_t value);
int64_t lx_position(int64_t handle, int64_t value);
int64_t lx_file_open(const char *path, int64_t make_if_missing);
int64_t lx_file_read_all(int64_t handle);
int64_t lx_file_read_first(int64_t handle, int64_t n);
int64_t lx_file_read_range(int64_t handle, int64_t n, int64_t from_line);
int64_t lx_file_line_count(int64_t handle);
int64_t lx_get_tag(int64_t handle, int64_t index);
int64_t lx_get_i(int64_t handle, int64_t index);
int64_t lx_get_c(int64_t handle, int64_t index);
double    lx_get_d(int64_t handle, int64_t index);
int64_t lx_file_write_line(int64_t handle, const char *text);
int64_t lx_file_clear(int64_t handle);
int64_t lx_file_close(int64_t handle);
int64_t lx_str_find_char(const char *s, int64_t ch);
int64_t lx_str_find_str(const char *s, const char *sub);
int64_t lx_str_split(const char *s, const char *sep, int64_t max_parts);
int64_t lx_str_replace(const char *s, const char *old, const char *rep);
int64_t lx_trim(const char *s);
int64_t lx_atoi(const char *s);
double    lx_atof(const char *s);
char     *lx_itoa(int64_t v);
char     *lx_dtoa(double v);
int64_t lx_sqrt(int64_t v);
double    lx_sqrt_d(double v);
int64_t lx_pow(int64_t base, int64_t exp);
double    lx_pow_d(double base, double exp);
int64_t lx_random_int(int64_t lo, int64_t hi);
int64_t lx_now_unix(void);
char     *lx_now_datetime(void);
int64_t lx_append_file(const char *path, const char *text);
int64_t lx_read_line(const char *path, int64_t line_no, char *out, int64_t out_cap);
int64_t lx_write_line(const char *path, int64_t line_no, const char *text);
int64_t lx_file_exists(const char *path);
int64_t lx_delete_file(const char *path);
int64_t lx_rename_file(const char *old_path, const char *new_path);
int64_t lx_copy_file(const char *src_path, const char *dst_path);
int64_t lx_get_env(const char *name);
int64_t lx_system(const char *cmd);
int64_t lx_abs(int64_t v);
double    lx_fabs(double v);

#ifdef __cplusplus
}
#endif

#endif /* LEXICO_CODEGEN_H */
