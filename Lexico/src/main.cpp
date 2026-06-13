/*
 * main.cpp – Entry point for the Lexico compiler.
 *
 * Usage:  lexico <source.lx>
 *
 * Compiles the .lx source, then immediately executes it and
 * prints the output.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>

#include "driver.hpp"

static int has_lx_ext(const char *path) {
    size_t len = strlen(path);
    return len >= 3
        && path[len - 3] == '.'
        && path[len - 2] == 'l'
        && path[len - 1] == 'x';
}

static int file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static int has_path_separator(const char *path) {
    return path && (strchr(path, '/') != NULL || strchr(path, '\\') != NULL);
}

static const char *path_basename(const char *path) {
    const char *slash;
    const char *bslash;
    const char *base;

    if (!path) return "";
    slash = strrchr(path, '/');
    bslash = strrchr(path, '\\');
    base = slash;
    if (!base || (bslash && bslash > base)) base = bslash;
    return base ? base + 1 : path;
}

static int equals_ignore_case(const char *a, const char *b) {
    unsigned char ca;
    unsigned char cb;

    if (!a || !b) return 0;
    while (*a && *b) {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        if (tolower(ca) != tolower(cb)) return 0;
    }
    return *a == '\0' && *b == '\0';
}

static const char *resolve_source_path(const char *input_path,
                                       char *resolved,
                                       size_t resolved_size) {
    if (!input_path || !resolved || resolved_size == 0) return input_path;

    if (file_exists(input_path)) {
        return input_path;
    }

    /* Friendly fallback for repo-root usage: lexico test1.lx */
    if (!has_path_separator(input_path)) {
        if (snprintf(resolved, resolved_size, "build/%s", input_path) < (int)resolved_size
            && file_exists(resolved)) {
            return resolved;
        }
    }

    return input_path;
}

int main(int argc, char **argv) {
    char resolved_path[1024];
    const char *source_path = NULL;
    const char *exe_name = path_basename((argc > 0 && argv) ? argv[0] : NULL);
    LexicoCompileOptions opts;
    int i;

    opts.enable_autofix = 0;
    opts.interactive_confirm = 1;
    opts.non_interactive_auto_apply = 1;
    opts.max_autofix_attempts = 1;
    opts.dump_expanded_source = 0;

    if (argc < 2) {
        if (equals_ignore_case(exe_name, "workSpaceManager.exe") ||
            equals_ignore_case(exe_name, "workspacemanager.exe")) {
            source_path = "apps/workspace-manager/main.lx";
        } else if (equals_ignore_case(exe_name, "PasswordManager.exe") ||
                   equals_ignore_case(exe_name, "passwordmanager.exe")) {
            source_path = "apps/passwordManager/main.lx";
        } else {
            fprintf(stderr, "Usage: lexico [--autofix] <source.lx>\n");
            return EXIT_FAILURE;
        }
        if (!file_exists(source_path)) {
            char alt[1024];
            snprintf(alt, sizeof alt, "../%s", source_path);
            if (file_exists(alt)) source_path = alt;
        }
    }

    for (i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "--autofix") == 0) {
                opts.enable_autofix = 1;
                continue;
            }
            if (strcmp(argv[i], "--dump-expanded") == 0) {
                opts.dump_expanded_source = 1;
                continue;
            }
            fprintf(stderr, "Error: unknown option '%s'.\n", argv[i]);
            return EXIT_FAILURE;
        } else if (!source_path) {
            source_path = argv[i];
        } else {
            fprintf(stderr, "Error: multiple source files are not supported.\n");
            return EXIT_FAILURE;
        }
    }

    if (!source_path) {
        fprintf(stderr, "Error: source file is required.\n");
        return EXIT_FAILURE;
    }

    if (!has_lx_ext(source_path)) {
        fprintf(stderr,
                "Error: input file must have a .lx extension (got '%s').\n",
                source_path);
        return EXIT_FAILURE;
    }

    source_path = resolve_source_path(source_path, resolved_path, sizeof(resolved_path));

    return lexico_compile_with_options(source_path, &opts) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
