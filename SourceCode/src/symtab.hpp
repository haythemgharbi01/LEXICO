/*
 * symtab.hpp – Symbol table for semantic analysis.
 *
 * - Dynamic array that grows as needed (never writes out of bounds).
 * - Rejects duplicate names.
 * - All name strings are owned copies.
 */

#ifndef LEXICO_SYMTAB_H
#define LEXICO_SYMTAB_H

#include "ast.hpp"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char     *name;      /* owned copy */
    int       kind;      /* SymbolKind */
    TypeKind  type;      /* variable type or array element type */
    int       line;      /* line where declared */
} Symbol;

typedef enum {
    SYM_VAR,
    SYM_ARRAY,
    SYM_TABLE,
    SYM_MATRIX
} SymbolKind;

typedef struct {
    Symbol *entries;
    size_t  count;
    size_t  cap;
} SymTable;

SymTable *symtab_new (void);
void      symtab_free(SymTable *t);

/* Returns 0 on success, -1 on duplicate (prints error). */
int       symtab_add   (SymTable *t, const char *name, TypeKind type, int line);
/* Returns entry or NULL. */
Symbol   *symtab_lookup(SymTable *t, const char *name);

/* Run full semantic checks on the AST.  Returns 0 on success. */
int       symtab_analyze(SymTable *t, const Program *prog);

#ifdef __cplusplus
}
#endif

#endif /* LEXICO_SYMTAB_H */
