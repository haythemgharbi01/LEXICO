/*
 * driver.cpp – Orchestrates lex → parse → semantic → codegen pipeline.
 *
 * Memory contract:
 *   - On every exit path the AST, symbol table, and LLVM resources are freed.
 */

#include "driver.hpp"
#include "ast.hpp"
#include "symtab.hpp"
#include "codegen.hpp"

#include <llvm-c/Core.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

/* Flex / Bison glue */
extern FILE    *yyin;
extern int      yyparse(void);
extern int      yylineno;
extern void     yyrestart(FILE *input_file);
extern Program *lexico_parsed_program;   /* set by parser on success */
extern void     lexico_class_registry_reset(void);
extern char     lexico_last_parse_error[512];

static LexicoCompileOptions default_compile_options(void) {
    LexicoCompileOptions opts;
    opts.enable_autofix = 0;
    opts.interactive_confirm = 1;
    opts.non_interactive_auto_apply = 1;
    opts.max_autofix_attempts = 1;
    opts.dump_expanded_source = 0;
    return opts;
}

static LexicoCompileOptions normalize_compile_options(const LexicoCompileOptions *in) {
    LexicoCompileOptions opts = default_compile_options();
    if (!in) return opts;
    opts.enable_autofix = in->enable_autofix ? 1 : 0;
    opts.interactive_confirm = in->interactive_confirm ? 1 : 0;
    opts.non_interactive_auto_apply = in->non_interactive_auto_apply ? 1 : 0;
    opts.max_autofix_attempts = in->max_autofix_attempts;
    if (opts.max_autofix_attempts < 0) opts.max_autofix_attempts = 0;
    opts.dump_expanded_source = in->dump_expanded_source ? 1 : 0;
    return opts;
}

static char *read_text_file(const char *path) {
    FILE *fp;
    long sz;
    char *buf;

    if (!path) return NULL;
    fp = fopen(path, "rb");
    if (!fp) return NULL;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    if (sz > 0 && fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
        fclose(fp);
        free(buf);
        return NULL;
    }
    fclose(fp);
    buf[sz] = '\0';
    return buf;
}

static int write_text_file(const char *path, const char *text) {
    FILE *fp;
    size_t n;

    if (!path || !text) return -1;
    fp = fopen(path, "wb");
    if (!fp) return -1;

    n = strlen(text);
    if (n > 0 && fwrite(text, 1, n, fp) != n) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

static int env_truthy(const char *name) {
    const char *v = getenv(name);
    if (!v || !*v) return 0;
    return strcmp(v, "0") != 0 &&
           strcmp(v, "false") != 0 &&
           strcmp(v, "FALSE") != 0 &&
           strcmp(v, "off") != 0 &&
           strcmp(v, "OFF") != 0;
}

static const char *getenv_or_user_env(const char *name) {
    const char *v = getenv(name);
#ifdef _WIN32
    static char openrouter_key[8192];
    static char openrouter_model[256];
    char *buf = NULL;
    DWORD buf_size = 0;

    if (v && *v) return v;
    if (!name || !*name) return NULL;

    if (strcmp(name, "OPENROUTER_API_KEY") == 0) {
        buf = openrouter_key;
        buf_size = sizeof(openrouter_key);
    } else if (strcmp(name, "OPENROUTER_MODEL") == 0) {
        buf = openrouter_model;
        buf_size = sizeof(openrouter_model);
    } else {
        return NULL;
    }

    buf[0] = '\0';
    if (RegGetValueA(HKEY_CURRENT_USER,
                     "Environment",
                     name,
                     RRF_RT_REG_SZ,
                     NULL,
                     buf,
                     &buf_size) == ERROR_SUCCESS && buf[0]) {
        return buf;
    }
#endif
    return (v && *v) ? v : NULL;
}

static int make_temp_path(char *out, size_t out_size,
                          const char *prefix, const char *suffix) {
    const char *base = getenv("TEMP");
    unsigned a;
    unsigned b;
    int n;

    if (!base || !*base) base = getenv("TMP");
    if (!base || !*base) base = ".";

    a = (unsigned)time(NULL);
    b = (unsigned)rand();
#ifdef _WIN32
    n = snprintf(out, out_size, "%s\\%s_%u_%u%s",
                 base,
                 prefix ? prefix : "lexico",
                 a,
                 b,
                 suffix ? suffix : "");
#else
    n = snprintf(out, out_size, "%s/%s_%u_%u%s",
                 base,
                 prefix ? prefix : "lexico",
                 a,
                 b,
                 suffix ? suffix : "");
#endif
    return (n > 0 && (size_t)n < out_size) ? 0 : -1;
}

static int ai_hints_enabled(void) {
    /* Default OFF to avoid network calls unless explicitly requested. */
    return env_truthy("LEXICO_AI_HINTS");
}

static void json_write_escaped(FILE *fp, const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    if (!fp || !s) return;
    for (; *p; ++p) {
        switch (*p) {
        case '"': fputs("\\\"", fp); break;
        case '\\': fputs("\\\\", fp); break;
        case '\n': fputs("\\n", fp); break;
        case '\r': fputs("\\r", fp); break;
        case '\t': fputs("\\t", fp); break;
        default:
            if (*p < 0x20) {
                fprintf(fp, "\\u%04x", (unsigned)*p);
            } else {
                fputc(*p, fp);
            }
            break;
        }
    }
}

static int extract_first_json_content(const char *json, char *out, size_t out_sz) {
    const char *key = "\"content\":\"";
    const char *p;
    size_t w = 0;

    if (!json || !out || out_sz == 0) return -1;
    out[0] = '\0';

    p = strstr(json, key);
    if (!p) return -1;
    p += strlen(key);

    while (*p && w + 1 < out_sz) {
        if (*p == '"') {
            out[w] = '\0';
            return 0;
        }
        if (*p == '\\') {
            ++p;
            if (!*p) break;
            switch (*p) {
            case 'n': out[w++] = '\n'; break;
            case 'r': out[w++] = '\r'; break;
            case 't': out[w++] = '\t'; break;
            case '"': out[w++] = '"'; break;
            case '\\': out[w++] = '\\'; break;
            default: out[w++] = *p; break;
            }
            ++p;
            continue;
        }
        out[w++] = *p++;
    }

    out[w] = '\0';
    return w ? 0 : -1;
}

static char *extract_json_string_after_key_alloc(const char *json,
                                                 const char *key,
                                                 size_t max_chars) {
    const char *p;
    size_t cap;
    size_t w = 0;
    char *out;

    if (!json || !key || max_chars == 0) return NULL;
    p = strstr(json, key);
    if (!p) return NULL;
    p += strlen(key);

    cap = max_chars + 1;
    out = static_cast<char *>(malloc(cap));
    if (!out) return NULL;

    while (*p && w + 1 < cap) {
        if (*p == '"') {
            out[w] = '\0';
            return out;
        }
        if (*p == '\\') {
            ++p;
            if (!*p) break;
            switch (*p) {
            case 'n': out[w++] = '\n'; break;
            case 'r': out[w++] = '\r'; break;
            case 't': out[w++] = '\t'; break;
            case '"': out[w++] = '"'; break;
            case '\\': out[w++] = '\\'; break;
            default: out[w++] = *p; break;
            }
            ++p;
            continue;
        }
        out[w++] = *p++;
    }

    out[w] = '\0';
    return w ? out : (free(out), static_cast<char *>(NULL));
}

static char *extract_first_json_content_alloc(const char *json, size_t max_chars) {
    char *out;

    if (!json || max_chars == 0) return NULL;

    out = extract_json_string_after_key_alloc(json, "\"content\":\"", max_chars);
    if (out && *out) return out;
    free(out);

    /* OpenRouter may return message content as typed blocks: [{"type":"text","text":"..."}] */
    out = extract_json_string_after_key_alloc(json, "\"text\":\"", max_chars);
    if (out && *out) return out;
    free(out);

    return NULL;
}

static char *trim_dup(const char *s) {
    size_t start = 0;
    size_t end;
    char *out;

    if (!s) return NULL;
    end = strlen(s);
    while (start < end && isspace((unsigned char)s[start])) start++;
    while (end > start && isspace((unsigned char)s[end - 1])) end--;

    out = static_cast<char *>(malloc(end - start + 1));
    if (!out) return NULL;
    memcpy(out, s + start, end - start);
    out[end - start] = '\0';
    return out;
}

static char *extract_code_block_or_text(const char *text) {
    const char *fence;
    const char *start;
    const char *end;
    char *out;

    if (!text) return NULL;

    fence = strstr(text, "```");
    if (!fence) return trim_dup(text);

    start = strchr(fence + 3, '\n');
    if (!start) return trim_dup(text);
    start++;

    end = strstr(start, "```");
    if (!end || end <= start) return trim_dup(text);

    out = static_cast<char *>(malloc((size_t)(end - start) + 1));
    if (!out) return NULL;
    memcpy(out, start, (size_t)(end - start));
    out[(size_t)(end - start)] = '\0';
    {
        char *trimmed = trim_dup(out);
        free(out);
        return trimmed;
    }
}

static int looks_like_lexico_source(const char *text) {
    const char *p = text;
    if (!p) return 0;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return 0;

    /* Reject likely JSON/API payloads. */
    if ((*p == '{' || *p == '[' || *p == '"') &&
        (strstr(p, "\"error\"") || strstr(p, "\"message\""))) return 0;
    if (strstr(p, "User not found") || strstr(p, "Unauthorized") || strstr(p, "Invalid API key")) return 0;

    if (!strchr(p, ';') && !strchr(p, '\n')) return 0;
    if (strstr(p, "create ") || strstr(p, "print ") || strstr(p, "set ") || strstr(p, "define ")) {
        return 1;
    }
    if (strstr(p, "open ") || strstr(p, "emit ") || strstr(p, "convert ") || strstr(p, "if ")) {
        return 1;
    }
    if (strchr(p, ';')) return 1;
    return 0;
}

static char *extract_openrouter_error_message(const char *json) {
    if (!json) return NULL;
    if (!strstr(json, "\"error\"")) return NULL;
    return extract_json_string_after_key_alloc(json, "\"message\":\"", 1024);
}

static char *read_lexico_syntax_reference(void) {
    const char *path = getenv("LEXICO_SYNTAX_REF");
    char *txt = NULL;
    if (path && *path) txt = read_text_file(path);
    if (!txt) txt = read_text_file("src/lexico.output");
    if (!txt) txt = read_text_file("../src/lexico.output");
    if (!txt) txt = read_text_file("..\\src\\lexico.output");
    return txt;
}

static int stdin_is_tty(void) {
#ifdef _WIN32
    return _isatty(_fileno(stdin));
#else
    return isatty(fileno(stdin));
#endif
}

static void print_fix_preview(const char *before, const char *after) {
    size_t i = 0;
    size_t line = 1;
    size_t s1;
    size_t e1;
    size_t s2;
    size_t e2;

    if (!before || !after) return;

    while (before[i] && after[i] && before[i] == after[i]) {
        if (before[i] == '\n') line++;
        i++;
    }

    s1 = i;
    while (s1 > 0 && before[s1 - 1] != '\n') s1--;
    e1 = i;
    while (before[e1] && before[e1] != '\n') e1++;

    s2 = i;
    while (s2 > 0 && after[s2 - 1] != '\n') s2--;
    e2 = i;
    while (after[e2] && after[e2] != '\n') e2++;

    fprintf(stderr, "Fix preview around line %zu:\n", line);
    fprintf(stderr, "  before: %.*s\n", (int)(e1 - s1), before + s1);
    fprintf(stderr, "  after : %.*s\n", (int)(e2 - s2), after + s2);
}

static int confirm_apply_fix(const LexicoCompileOptions *opts,
                             const char *src_path,
                             const char *before,
                             const char *after,
                             const char *explanation) {
    char answer[32];
    int apply;

    fprintf(stderr, "Autofix explanation: %s\n", explanation ? explanation : "Proposed parser fix.");
    if (before && after) {
        print_fix_preview(before, after);
    }

    apply = 0;
    if (!opts || !opts->interactive_confirm) {
        apply = 1;
    } else if (stdin_is_tty()) {
        fprintf(stderr, "Apply this fix to '%s'? [y/N]: ", src_path ? src_path : "<unknown>");
        fflush(stderr);
        if (fgets(answer, sizeof(answer), stdin)) {
            char c = (char)tolower((unsigned char)answer[0]);
            apply = (c == 'y');
        }
    } else {
        apply = opts->non_interactive_auto_apply ? 1 : 0;
    }

    if (!apply) {
        fprintf(stderr, "Autofix skipped: user declined proposed fix.\n");
        return 1;
    }

    if (before && after) {
        size_t i = 0, j = 0;
        while (before[i] || after[j]) {
            while (before[i] && isspace((unsigned char)before[i])) i++;
            while (after[j] && isspace((unsigned char)after[j])) j++;
            if (before[i] != after[j]) break;
            if (!before[i] && !after[j]) {
                fprintf(stderr, "Autofix skipped: proposed change only adjusts whitespace and does not fix syntax.\n");
                return 1;
            }
            if (before[i]) i++;
            if (after[j]) j++;
        }
    }

    if (write_text_file(src_path, after) != 0) {
        fprintf(stderr, "Autofix failed: could not write fixed file '%s'.\n", src_path);
        return -1;
    }

    fprintf(stderr, "Autofix applied to '%s'. Retrying compile...\n", src_path);
    return 0;
}

static int maybe_get_ai_autofix_candidate(const char *src_path,
                                          int line,
                                          const char *diagnostic,
                                          const char *source_text,
                                          char **out_fixed) {
    const char *api_key;
    const char *model;
    char req_path[1024];
    char resp_path[1024];
    FILE *req;
    char cmd[8192];
    int cmd_rc;
    char *resp = NULL;
    char *content = NULL;
    char *fixed = NULL;
    char *syntax = NULL;
    size_t src_len;
    size_t syntax_len;

    if (out_fixed) *out_fixed = NULL;

    api_key = getenv_or_user_env("OPENROUTER_API_KEY");
    if (!api_key || !*api_key) {
        fprintf(stderr, "Autofix skipped: set OPENROUTER_API_KEY to enable AI autofix.\n");
        return -1;
    }

    model = getenv_or_user_env("OPENROUTER_MODEL");
    if (!model || !*model) model = "openai/gpt-4o-mini";

    if (make_temp_path(req_path, sizeof(req_path), "lexico_fix_req", ".json") != 0 ||
        make_temp_path(resp_path, sizeof(resp_path), "lexico_fix_resp", ".json") != 0) {
        fprintf(stderr, "Autofix skipped: failed to create temp files.\n");
        return -1;
    }

    syntax = read_lexico_syntax_reference();
    src_len = source_text ? strlen(source_text) : 0;
    syntax_len = syntax ? strlen(syntax) : 0;

    req = fopen(req_path, "wb");
    if (!req) {
        fprintf(stderr, "Autofix skipped: cannot write request file.\n");
        free(syntax);
        return -1;
    }

    fputs("{\"model\":\"", req);
    json_write_escaped(req, model);
    fputs("\",\"max_tokens\":2000,\"messages\":[", req);

    fputs("{\"role\":\"system\",\"content\":\"", req);
    json_write_escaped(req,
        "You fix Lexico source files. Return ONLY corrected Lexico source code in one fenced code block. "
        "Do not explain. Preserve functionality and only apply minimal syntax fixes. "
        "Lexico keywords are lowercase; use forms like: create int a set value 20; and print a; "
        "Do not uppercase keywords.");
    fputs("\"},", req);

    fputs("{\"role\":\"user\",\"content\":\"", req);
    json_write_escaped(req, "Fix this Lexico file that failed to parse.\\n");
    json_write_escaped(req, "Path: ");
    json_write_escaped(req, src_path ? src_path : "<unknown>");
    json_write_escaped(req, "\\nLine: ");
    fprintf(req, "%d", line);
    json_write_escaped(req, "\\nDiagnostic:\\n");
    json_write_escaped(req, diagnostic ? diagnostic : "syntax error");
    json_write_escaped(req, "\\n\\nCurrent source:\\n");
    if (source_text && src_len > 0) {
        size_t cap = src_len > 12000 ? 12000 : src_len;
        char *snippet = static_cast<char *>(malloc(cap + 1));
        if (snippet) {
            memcpy(snippet, source_text, cap);
            snippet[cap] = '\0';
            json_write_escaped(req, snippet);
            free(snippet);
        }
    }
    json_write_escaped(req, "\\n\\nLexico syntax reference (from lexico.output, truncated):\\n");
    if (syntax && syntax_len > 0) {
        size_t cap = syntax_len > 12000 ? 12000 : syntax_len;
        char *syn = static_cast<char *>(malloc(cap + 1));
        if (syn) {
            memcpy(syn, syntax, cap);
            syn[cap] = '\0';
            json_write_escaped(req, syn);
            free(syn);
        }
    }
    fputs("\"}]}", req);
    fclose(req);

    snprintf(cmd, sizeof(cmd),
             "curl -sS https://openrouter.ai/api/v1/chat/completions "
             "-H \"Authorization: Bearer %s\" "
             "-H \"Content-Type: application/json\" "
             "--data-binary @\"%s\" -o \"%s\"",
             api_key, req_path, resp_path);

    cmd_rc = system(cmd);
    if (cmd_rc != 0) {
        fprintf(stderr, "Autofix skipped: OpenRouter request failed (curl exit %d).\n", cmd_rc);
        free(syntax);
        remove(req_path);
        remove(resp_path);
        return -1;
    }

    resp = read_text_file(resp_path);
    if (!resp) {
        fprintf(stderr, "Autofix skipped: failed to read OpenRouter response.\n");
        free(syntax);
        remove(req_path);
        remove(resp_path);
        return -1;
    }

    content = extract_first_json_content_alloc(resp, 30000);
    if (!content) {
        char *api_error = extract_openrouter_error_message(resp);
        if (api_error && *api_error) {
            fprintf(stderr, "Autofix skipped: OpenRouter error: %s\n", api_error);
            free(api_error);
            free(syntax);
            free(resp);
            remove(req_path);
            remove(resp_path);
            return -1;
        }
        free(api_error);
    }
    fixed = extract_code_block_or_text(content ? content : resp);

    if (!fixed || !*fixed) {
        fprintf(stderr, "Autofix skipped: AI returned empty content.\n");
        free(syntax);
        free(resp);
        free(content);
        free(fixed);
        remove(req_path);
        remove(resp_path);
        return -1;
    }

    if (!looks_like_lexico_source(fixed)) {
        fprintf(stderr, "Autofix skipped: AI response was not valid Lexico source.\n");
        free(syntax);
        free(resp);
        free(content);
        free(fixed);
        remove(req_path);
        remove(resp_path);
        return -1;
    }

    if (out_fixed) {
        *out_fixed = fixed;
        fixed = NULL;
    }

    free(syntax);
    free(resp);
    free(content);
    free(fixed);
    remove(req_path);
    remove(resp_path);
    return 0;
}

static void maybe_print_ai_suggestion(const char *phase,
                                      const char *src_path,
                                      int line,
                                      const char *diagnostic,
                                      const char *source_text) {
    const char *api_key;
    const char *model;
    char req_path[1024];
    char resp_path[1024];
    FILE *req;
    char cmd[8192];
    int cmd_rc;
    char *resp = NULL;
    char suggestion[4096];
    const char *diag = diagnostic ? diagnostic : "<no diagnostic text>";
    size_t src_len = source_text ? strlen(source_text) : 0;
    size_t src_cap = 3500;

    if (!ai_hints_enabled()) return;

    api_key = getenv_or_user_env("OPENROUTER_API_KEY");
    if (!api_key || !*api_key) {
        fprintf(stderr,
                "AI hint skipped: set OPENROUTER_API_KEY and LEXICO_AI_HINTS=1 to enable.\n");
        return;
    }

    model = getenv_or_user_env("OPENROUTER_MODEL");
    if (!model || !*model) model = "openai/gpt-4o-mini";

    if (make_temp_path(req_path, sizeof(req_path), "lexico_hint_req", ".json") != 0 ||
        make_temp_path(resp_path, sizeof(resp_path), "lexico_hint_resp", ".json") != 0) {
        fprintf(stderr, "AI hint skipped: failed to create temp files.\n");
        return;
    }

    req = fopen(req_path, "wb");
    if (!req) {
        fprintf(stderr, "AI hint skipped: cannot write request file.\n");
        return;
    }

    fputs("{\"model\":\"", req);
    json_write_escaped(req, model);
    fputs("\",\"max_tokens\":2000,\"messages\":[", req);

    fputs("{\"role\":\"system\",\"content\":\"", req);
    json_write_escaped(req,
        "You are a compiler error assistant for the Lexico language. "
        "Provide: (1) root cause, (2) concrete fix, (3) corrected Lexico snippet. "
        "Be concise and practical.");
    fputs("\"},", req);

    fputs("{\"role\":\"user\",\"content\":\"", req);
    json_write_escaped(req, "Phase: ");
    json_write_escaped(req, phase ? phase : "unknown");
    json_write_escaped(req, "\\nFile: ");
    json_write_escaped(req, src_path ? src_path : "<unknown>");
    json_write_escaped(req, "\\nLine: ");
    fprintf(req, "%d", line);
    json_write_escaped(req, "\\nDiagnostic:\\n");
    json_write_escaped(req, diag);
    json_write_escaped(req, "\\nSource (possibly truncated):\\n");
    if (source_text && src_len > 0) {
        if (src_len > src_cap) src_len = src_cap;
        {
            char *snippet = static_cast<char *>(malloc(src_len + 1));
            if (snippet) {
                memcpy(snippet, source_text, src_len);
                snippet[src_len] = '\0';
                json_write_escaped(req, snippet);
                free(snippet);
            }
        }
    }
    fputs("\"}]}", req);
    fclose(req);

    snprintf(cmd, sizeof(cmd),
             "curl -sS https://openrouter.ai/api/v1/chat/completions "
             "-H \"Authorization: Bearer %s\" "
             "-H \"Content-Type: application/json\" "
             "--data-binary @\"%s\" -o \"%s\"",
             api_key, req_path, resp_path);

    cmd_rc = system(cmd);
    if (cmd_rc != 0) {
        fprintf(stderr,
                "AI hint skipped: network request failed (curl exit %d).\n",
                cmd_rc);
        remove(req_path);
        remove(resp_path);
        return;
    }

    resp = read_text_file(resp_path);
    if (!resp) {
        fprintf(stderr, "AI hint skipped: failed to read response.\n");
        remove(req_path);
        remove(resp_path);
        return;
    }

    if (extract_first_json_content(resp, suggestion, sizeof(suggestion)) == 0) {
        fprintf(stderr, "\nAI fix suggestion:\n%s\n\n", suggestion);
    } else {
        fprintf(stderr, "AI hint: received response but could not parse suggestion text.\n");
    }

    free(resp);
    remove(req_path);
    remove(resp_path);
}

typedef struct {
    int active;
    int saved_stderr_fd;
} StderrSilencer;

typedef struct {
    int active;
    int saved_stderr_fd;
    FILE *tmp;
} StderrCapture;

static void end_stderr_silence(StderrSilencer *sil) {
    if (!sil || !sil->active) return;
    fflush(stderr);
#ifdef _WIN32
    _dup2(sil->saved_stderr_fd, _fileno(stderr));
    _close(sil->saved_stderr_fd);
#else
    dup2(sil->saved_stderr_fd, fileno(stderr));
    close(sil->saved_stderr_fd);
#endif
    sil->saved_stderr_fd = -1;
    sil->active = 0;
}

static int begin_stderr_capture(StderrCapture *cap) {
    if (!cap || cap->active) return -1;

    fflush(stderr);
    cap->tmp = tmpfile();
    if (!cap->tmp) return -1;

#ifdef _WIN32
    cap->saved_stderr_fd = _dup(_fileno(stderr));
    if (cap->saved_stderr_fd < 0) {
        fclose(cap->tmp);
        cap->tmp = NULL;
        return -1;
    }
    if (_dup2(_fileno(cap->tmp), _fileno(stderr)) < 0) {
        _close(cap->saved_stderr_fd);
        cap->saved_stderr_fd = -1;
        fclose(cap->tmp);
        cap->tmp = NULL;
        return -1;
    }
#else
    cap->saved_stderr_fd = dup(fileno(stderr));
    if (cap->saved_stderr_fd < 0) {
        fclose(cap->tmp);
        cap->tmp = NULL;
        return -1;
    }
    if (dup2(fileno(cap->tmp), fileno(stderr)) < 0) {
        close(cap->saved_stderr_fd);
        cap->saved_stderr_fd = -1;
        fclose(cap->tmp);
        cap->tmp = NULL;
        return -1;
    }
#endif

    cap->active = 1;
    return 0;
}

static void end_stderr_capture(StderrCapture *cap, char *buf, size_t buf_size) {
    long nbytes;
    size_t to_read;

    if (buf && buf_size > 0) buf[0] = '\0';
    if (!cap || !cap->active) return;

    fflush(stderr);
#ifdef _WIN32
    _dup2(cap->saved_stderr_fd, _fileno(stderr));
    _close(cap->saved_stderr_fd);
#else
    dup2(cap->saved_stderr_fd, fileno(stderr));
    close(cap->saved_stderr_fd);
#endif
    cap->saved_stderr_fd = -1;

    if (cap->tmp) {
        fflush(cap->tmp);
        fseek(cap->tmp, 0, SEEK_END);
        nbytes = ftell(cap->tmp);
        if (nbytes > 0 && buf && buf_size > 0) {
            to_read = (size_t)nbytes;
            if (to_read >= buf_size) to_read = buf_size - 1;
            rewind(cap->tmp);
            if (fread(buf, 1, to_read, cap->tmp) == to_read) {
                buf[to_read] = '\0';
                /* Preserve previous behavior: diagnostics still print to console. */
                fputs(buf, stderr);
            }
        }
        fclose(cap->tmp);
        cap->tmp = NULL;
    }

    cap->active = 0;
}

static void trim_copy(char *dst, size_t dst_size, const char *src) {
    size_t start = 0;
    size_t end;

    if (!dst || dst_size == 0) return;
    dst[0] = '\0';
    if (!src) return;

    end = strlen(src);
    while (start < end && isspace((unsigned char)src[start])) start++;
    while (end > start && isspace((unsigned char)src[end - 1])) end--;

    if (end <= start) return;
    if ((end - start) >= dst_size) end = start + dst_size - 1;

    memcpy(dst, src + start, end - start);
    dst[end - start] = '\0';
}

static int starts_with_word(const char *text, const char *word) {
    size_t n;
    if (!text || !word) return 0;
    n = strlen(word);
    return strncmp(text, word, n) == 0;
}

static int looks_like_statement_start(const char *line) {
    return starts_with_word(line, "print ") ||
           starts_with_word(line, "set ") ||
           starts_with_word(line, "create ") ||
           starts_with_word(line, "open ") ||
           starts_with_word(line, "emit ") ||
           starts_with_word(line, "convert ") ||
           starts_with_word(line, "define ");
}

static int detect_missing_semicolon_before_line(const char *src_path, int line_no) {
    FILE *fp;
    char buf[1024];
    char prev_non_empty[1024] = {0};
    char target_line[1024] = {0};
    int current = 0;
    char prev_trim[1024];
    char target_trim[1024];
    size_t prev_len;

    if (!src_path || line_no <= 1) return 0;

    fp = fopen(src_path, "rb");
    if (!fp) return 0;

    while (fgets(buf, sizeof(buf), fp)) {
        current++;
        if (current < line_no) {
            char t[1024];
            trim_copy(t, sizeof(t), buf);
            if (t[0] != '\0') {
                strncpy(prev_non_empty, t, sizeof(prev_non_empty) - 1);
                prev_non_empty[sizeof(prev_non_empty) - 1] = '\0';
            }
        } else if (current == line_no) {
            trim_copy(target_line, sizeof(target_line), buf);
            break;
        }
    }
    fclose(fp);

    trim_copy(prev_trim, sizeof(prev_trim), prev_non_empty);
    trim_copy(target_trim, sizeof(target_trim), target_line);
    if (prev_trim[0] == '\0') return 0;

    prev_len = strlen(prev_trim);
    if (prev_len > 0 && prev_trim[prev_len - 1] == ';') return 0;
    if (prev_len >= 2 && strcmp(prev_trim + prev_len - 2, "do") == 0) return 0;
    if (prev_len >= 7 && strcmp(prev_trim + prev_len - 7, "include") == 0) return 0;

    return looks_like_statement_start(target_trim);
}

static int build_missing_semicolon_fix_before_line(const char *src_path,
                                                   int line_no,
                                                   char **out_fixed) {
    char *src;
    size_t len;
    size_t i;
    int current_line = 1;
    size_t stmt_start = 0;
    size_t stmt_end = 0;
    size_t stmt_trim_start;
    size_t stmt_trim_end;
    char *fixed;

    if (out_fixed) *out_fixed = NULL;
    if (!src_path || line_no <= 1) return -1;

    src = read_text_file(src_path);
    if (!src) return -1;

    len = strlen(src);

    for (i = 0; i <= len; ++i) {
        if (i == len || src[i] == '\n') {
            if (current_line == (line_no - 1)) {
                stmt_end = i;
                break;
            }
            current_line++;
            stmt_start = i + 1;
        }
    }

    if (stmt_end <= stmt_start) {
        free(src);
        return -1;
    }

    stmt_trim_start = stmt_start;
    while (stmt_trim_start < stmt_end &&
           (src[stmt_trim_start] == ' ' || src[stmt_trim_start] == '\t' ||
            src[stmt_trim_start] == '\r' || src[stmt_trim_start] == '\n')) {
        stmt_trim_start++;
    }

    stmt_trim_end = stmt_end;
    while (stmt_trim_end > stmt_trim_start &&
           (src[stmt_trim_end - 1] == ' ' || src[stmt_trim_end - 1] == '\t' ||
            src[stmt_trim_end - 1] == '\r' || src[stmt_trim_end - 1] == '\n')) {
        stmt_trim_end--;
    }

    if (stmt_trim_end <= stmt_trim_start || src[stmt_trim_end - 1] == ';') {
        free(src);
        return -1;
    }

    fixed = static_cast<char *>(malloc(len + 2));
    if (!fixed) {
        free(src);
        return -1;
    }

    memcpy(fixed, src, stmt_trim_end);
    fixed[stmt_trim_end] = ';';
    memcpy(fixed + stmt_trim_end + 1, src + stmt_trim_end, len - stmt_trim_end + 1);

    free(src);
    if (out_fixed) {
        *out_fixed = fixed;
    } else {
        free(fixed);
    }
    return 0; 
}

static int detect_missing_divided_by_on_line(const char *src_path, int line_no) {
    FILE *fp;
    char buf[2048];
    int current = 0;
    char *p;

    if (!src_path || line_no <= 0) return 0;
    fp = fopen(src_path, "rb");
    if (!fp) return 0;

    while (fgets(buf, sizeof(buf), fp)) {
        current++;
        if (current == line_no) break;
    }
    fclose(fp);
    if (current != line_no) return 0;

    p = strstr(buf, "divided");
    while (p) {
        char *q = p + 7;
        while (*q == ' ' || *q == '\t') q++;
        if (strncmp(q, "by", 2) != 0) {
            return 1;
        }
        p = strstr(q + 2, "divided");
    }
    return 0;
}

static int build_missing_divided_by_fix_on_line(const char *src_path,
                                                int line_no,
                                                char **out_fixed) {
    char *src;
    size_t len;
    size_t i;
    int current_line = 1;
    size_t line_start = 0;
    size_t line_end = 0;
    size_t insert_pos = 0;
    char *fixed;

    if (out_fixed) *out_fixed = NULL;
    if (!src_path || line_no <= 0) return -1;

    src = read_text_file(src_path);
    if (!src) return -1;
    len = strlen(src);

    for (i = 0; i <= len; ++i) {
        if (i == len || src[i] == '\n') {
            if (current_line == line_no) {
                line_end = i;
                break;
            }
            current_line++;
            line_start = i + 1;
        }
    }
    if (current_line != line_no || line_end <= line_start) {
        free(src);
        return -1;
    }

    for (i = line_start; i + 7 <= line_end; ++i) {
        if (strncmp(src + i, "divided", 7) == 0) {
            size_t q = i + 7;
            while (q < line_end && (src[q] == ' ' || src[q] == '\t')) q++;
            if (q + 2 <= line_end && strncmp(src + q, "by", 2) == 0) {
                i = q + 1;
                continue;
            }
            insert_pos = i + 7;
            break;
        }
    }

    if (insert_pos == 0) {
        free(src);
        return -1;
    }

    fixed = static_cast<char *>(malloc(len + 4));
    if (!fixed) {
        free(src);
        return -1;
    }

    memcpy(fixed, src, insert_pos);
    fixed[insert_pos] = ' ';
    fixed[insert_pos + 1] = 'b';
    fixed[insert_pos + 2] = 'y';
    memcpy(fixed + insert_pos + 3, src + insert_pos, len - insert_pos + 1);

    free(src);
    if (out_fixed) {
        *out_fixed = fixed;
    } else {
        free(fixed);
    }
    return 0;
}

/* ── Load-preprocessing helpers ─────────────────────────────────── */

typedef struct {
    char   *buf;
    size_t  len;
    size_t  cap;
} StrBuf;

typedef struct {
    char  **items;
    size_t  count;
    size_t  cap;
} PathList;

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p && n) {
        fprintf(stderr, "Fatal: out of memory.\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

static void *xrealloc(void *ptr, size_t n) {
    void *p = realloc(ptr, n);
    if (!p && n) {
        fprintf(stderr, "Fatal: out of memory.\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

static char *xstrdup(const char *s) {
    size_t len = strlen(s);
    char *d = static_cast<char *>(xmalloc(len + 1));
    memcpy(d, s, len + 1);
    return d;
}

static void sb_init(StrBuf *sb) {
    sb->buf = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static void sb_append_n(StrBuf *sb, const char *s, size_t n) {
    if (sb->len + n + 1 > sb->cap) {
        size_t new_cap = sb->cap ? sb->cap : 256;
        while (new_cap < sb->len + n + 1) new_cap *= 2;
        sb->buf = static_cast<char *>(xrealloc(sb->buf, new_cap));
        sb->cap = new_cap;
    }
    memcpy(sb->buf + sb->len, s, n);
    sb->len += n;
    sb->buf[sb->len] = '\0';
}

static void sb_append(StrBuf *sb, const char *s) {
    sb_append_n(sb, s, strlen(s));
}

static void sb_free(StrBuf *sb) {
    free(sb->buf);
    sb->buf = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static void pl_init(PathList *pl) {
    pl->items = NULL;
    pl->count = 0;
    pl->cap = 0;
}

static void pl_free(PathList *pl) {
    size_t i;
    for (i = 0; i < pl->count; ++i) free(pl->items[i]);
    free(pl->items);
    pl->items = NULL;
    pl->count = 0;
    pl->cap = 0;
}

static int pl_index_of(const PathList *pl, const char *path) {
    size_t i;
    for (i = 0; i < pl->count; ++i) {
        if (strcmp(pl->items[i], path) == 0) return (int)i;
    }
    return -1;
}

static void pl_push(PathList *pl, const char *path) {
    if (pl->count == pl->cap) {
        pl->cap = pl->cap ? pl->cap * 2 : 16;
        pl->items = static_cast<char **>(xrealloc(pl->items, pl->cap * sizeof(char *)));
    }
    pl->items[pl->count++] = xstrdup(path);
}

static void pl_pop(PathList *pl) {
    if (pl->count == 0) return;
    free(pl->items[pl->count - 1]);
    pl->count--;
}

static int has_lx_ext(const char *path) {
    size_t len = strlen(path);
    return len >= 3
        && path[len - 3] == '.'
        && path[len - 2] == 'l'
        && path[len - 1] == 'x';
}

static char *normalize_path(const char *path) {
#ifdef _WIN32
    char *full = _fullpath(NULL, path, 0);
    if (full) return full;
#endif
    return xstrdup(path);
}

static char *path_dirname(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *bslash = strrchr(path, '\\');
    const char *last = slash;
    size_t len;

    if (!last || (bslash && bslash > last)) last = bslash;
    if (!last) return xstrdup(".");

    len = (size_t)(last - path);
    if (len == 0) len = 1;

    {
        char *d = static_cast<char *>(xmalloc(len + 1));
        memcpy(d, path, len);
        d[len] = '\0';
        return d;
    }
}

static char *join_path(const char *base_dir, const char *rel) {
    size_t n1 = strlen(base_dir);
    size_t n2 = strlen(rel);
    int need_sep = 1;
    char *out;

    if (n1 == 0) need_sep = 0;
    if (n1 > 0 && (base_dir[n1 - 1] == '/' || base_dir[n1 - 1] == '\\')) need_sep = 0;
    if (n2 > 0 && (rel[0] == '/' || rel[0] == '\\')) need_sep = 0;

    out = static_cast<char *>(xmalloc(n1 + (need_sep ? 1 : 0) + n2 + 1));
    memcpy(out, base_dir, n1);
    if (need_sep) out[n1++] = '/';
    memcpy(out + n1, rel, n2);
    out[n1 + n2] = '\0';
    return out;
}

static int is_abs_path(const char *p) {
    if (!p || !*p) return 0;
    if (p[0] == '/' || p[0] == '\\') return 1;
    if (isalpha((unsigned char)p[0]) && p[1] == ':') return 1;
    return 0;
}

static int parse_load_line(const char *line, char **out_path) {
    const char *p = line;
    const char *start;
    const char *end;
    size_t n;

    *out_path = NULL;

    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "load", 4) != 0) return 0;
    p += 4;
    if (!isspace((unsigned char)*p)) return 0;

    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return 0;
    p++;
    start = p;

    while (*p && *p != '"') p++;
    if (*p != '"') return 0;

    end = p;
    p++;

    while (*p == ' ' || *p == '\t') p++;
    if (*p != ';') return 0;
    p++;

    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p != '\0') return 0;

    n = (size_t)(end - start);
    *out_path = static_cast<char *>(xmalloc(n + 1));
    memcpy(*out_path, start, n);
    (*out_path)[n] = '\0';
    return 1;
}

static int expand_file_recursive(const char *path,
                                 StrBuf *out,
                                 PathList *loaded,
                                 PathList *stack) {
    char *norm = normalize_path(path);
    FILE *f;
    char line[8192];
    char *dir;
    int rc = -1;

    if (!norm) {
        fprintf(stderr, "Fatal: failed to normalize path '%s'.\n", path);
        return -1;
    }

    if (!has_lx_ext(norm)) {
        fprintf(stderr, "Error: loaded file must have .lx extension ('%s').\n", norm);
        free(norm);
        return -1;
    }

    if (pl_index_of(stack, norm) >= 0) {
        fprintf(stderr, "Error: circular load detected at '%s'.\n", norm);
        free(norm);
        return -1;
    }

    if (pl_index_of(loaded, norm) >= 0) {
        free(norm);
        return 0;
    }

    f = fopen(norm, "r");
    if (!f) {
        perror(norm);
        free(norm);
        return -1;
    }

    dir = path_dirname(norm);
    pl_push(stack, norm);

    while (fgets(line, sizeof line, f)) {
        char *load_path = NULL;
        if (parse_load_line(line, &load_path)) {
            char *joined = NULL;
            char *target = NULL;

            if (is_abs_path(load_path)) {
                target = xstrdup(load_path); 
            } else {
                joined = join_path(dir, load_path);
                target = joined;
            }

            free(load_path);
            if (expand_file_recursive(target, out, loaded, stack) != 0) {
                free(target);
                goto cleanup;
            }
            free(target);
            sb_append(out, "\n");
        } else {
            sb_append(out, line);
        }
    }

    if (ferror(f)) {
        fprintf(stderr, "Error: failed while reading '%s'.\n", norm);
        goto cleanup;
    }

    pl_pop(stack);
    pl_push(loaded, norm);
    rc = 0;

cleanup:
    if (rc != 0) pl_pop(stack);
    fclose(f);
    free(dir);
    free(norm);
    return rc;
}

static int build_expanded_source(const char *entry_path, char **out_src) {
    StrBuf out;
    PathList loaded;
    PathList stack;
    int rc;

    *out_src = NULL;
    sb_init(&out);
    pl_init(&loaded);
    pl_init(&stack);

    rc = expand_file_recursive(entry_path, &out, &loaded, &stack);
    if (rc == 0) {
        if (!out.buf) {
            out.buf = xstrdup("");
        }
        *out_src = out.buf;
        out.buf = NULL;
    }

    sb_free(&out);
    pl_free(&loaded);
    pl_free(&stack);
    return rc;
}

/* Build the output path:  "foo.lx" → "foo.ll" */
static char *make_output_path(const char *src) {
    size_t len = strlen(src);
    /* Must end with ".lx" – caller guarantees this */
    char *out = static_cast<char *>(malloc(len + 1));
    if (!out) { fprintf(stderr, "Fatal: out of memory.\n"); return NULL; }
    memcpy(out, src, len + 1);
    /* Replace last two chars */
    out[len - 2] = 'l';
    out[len - 1] = 'l';
    return out;
}

static int lexico_compile_ex_internal(const char *src_path,
                                      const LexicoCompileOptions *opts,
                                      int autofix_attempted) {
    Program  *prog   = NULL;
    SymTable *tab    = NULL;
    char     *ll_path = NULL;
    char     *expanded_src = NULL;
    FILE     *f = NULL;
    LLVMModuleRef mod = NULL;
    StderrSilencer sil = {0, -1};
    int       rc     = -1;         /* assume failure */
    char semantic_err[4096] = {0};
    char codegen_err[4096] = {0};
    char *source_text = NULL;

    fprintf(stderr, "Compiler: parsing source (%s).\n", src_path ? src_path : "<unknown>");

    source_text = read_text_file(src_path);

    /* ── Expand load directives and create parse stream ─────────── */
    if (build_expanded_source(src_path, &expanded_src) != 0) {
        return -1;
    }

    if (opts && opts->dump_expanded_source && expanded_src) {
        fprintf(stderr, "=== EXPANDED SOURCE (%s) ===\n%s\n=== END EXPANDED SOURCE ===\n",
                src_path ? src_path : "<unknown>", expanded_src);
    }

    f = tmpfile();
    if (!f) {
        perror("tmpfile");
        free(expanded_src);
        return -1;
    }

    if (fwrite(expanded_src, 1, strlen(expanded_src), f) != strlen(expanded_src)) {
        fprintf(stderr, "Error: failed to prepare expanded source stream.\n");
        fclose(f);
        free(expanded_src);
        return -1;
    }
    rewind(f);
    free(expanded_src);

    yyin = f;
    yyrestart(yyin);
    yylineno = 1;
    lexico_parsed_program = NULL;
    lexico_class_registry_reset();

    /* Reset the indent stack for > block markers */
    extern void lexico_indent_reset(void);
    lexico_indent_reset();

    /* ── Parse ──────────────────────────────────────────────────── */
    if (yyparse() != 0 || !lexico_parsed_program) {
        char parse_diag[1024];
        int missing_semicolon_hint;
        int missing_divided_by_hint;

        if (sil.active) end_stderr_silence(&sil);
        fprintf(stderr, "Compilation aborted due to parse errors.\n");
        missing_semicolon_hint = detect_missing_semicolon_before_line(src_path, yylineno);
        missing_divided_by_hint = detect_missing_divided_by_on_line(src_path, yylineno);
        if (missing_semicolon_hint) {
            fprintf(stderr, "Fix hint: Missing ';' at end of line %d.\n", yylineno - 1);
        } else if (missing_divided_by_hint) {
            fprintf(stderr, "Fix hint: Missing 'by' after 'divided' on line %d.\n", yylineno);
        } else {
            fprintf(stderr, "Fix hint: Check syntax near line %d.\n", yylineno);
        }

        snprintf(parse_diag, sizeof(parse_diag), "%s",
                 lexico_last_parse_error[0] ? lexico_last_parse_error : "syntax error");

        if (opts && opts->enable_autofix && autofix_attempted < opts->max_autofix_attempts) {
            if (missing_semicolon_hint) {
                char *fixed_local = NULL;
                if (build_missing_semicolon_fix_before_line(src_path, yylineno, &fixed_local) == 0 && fixed_local) {
                    int apply_rc = confirm_apply_fix(opts,
                                                     src_path,
                                                     source_text,
                                                     fixed_local,
                                                     "Parser expected ';' before the next statement. "
                                                     "This fix inserts the missing semicolon.");
                    free(fixed_local);
                    if (apply_rc == 0) {
                        fclose(f);
                        ast_free_program(lexico_parsed_program);
                        lexico_class_registry_reset();
                        free(source_text);
                        return lexico_compile_ex_internal(src_path, opts, autofix_attempted + 1);
                    }
                }
            }

            if (missing_divided_by_hint) {
                char *fixed_div = NULL;
                if (build_missing_divided_by_fix_on_line(src_path, yylineno, &fixed_div) == 0 && fixed_div) {
                    int apply_rc = confirm_apply_fix(opts,
                                                     src_path,
                                                     source_text,
                                                     fixed_div,
                                                     "Parser expected the two-word operator 'divided by'. "
                                                     "This fix inserts the missing 'by'.");
                    free(fixed_div);
                    if (apply_rc == 0) {
                        fclose(f);
                        ast_free_program(lexico_parsed_program);
                        lexico_class_registry_reset();
                        free(source_text);
                        return lexico_compile_ex_internal(src_path, opts, autofix_attempted + 1);
                    }
                }
            }

            {
                char *fixed_ai = NULL;
                if (maybe_get_ai_autofix_candidate(src_path, yylineno, parse_diag, source_text, &fixed_ai) == 0 && fixed_ai) {
                    int apply_rc = confirm_apply_fix(opts,
                                                     src_path,
                                                     source_text,
                                                     fixed_ai,
                                                     "AI proposed a parse-error fix based on your file and grammar context.");
                    free(fixed_ai);
                    if (apply_rc == 0) {
                        fclose(f);
                        ast_free_program(lexico_parsed_program);
                        lexico_class_registry_reset();
                        free(source_text);
                        return lexico_compile_ex_internal(src_path, opts, autofix_attempted + 1);
                    }
                }
            }
        }

        maybe_print_ai_suggestion("parse", src_path, yylineno, parse_diag, source_text);

        fclose(f);
        ast_free_program(lexico_parsed_program);
        lexico_class_registry_reset();
        free(source_text);
        return -1;
    }
    fclose(f);
    prog = lexico_parsed_program;
    fprintf(stderr, "Compiler: parse success.\n");

    /* ── Semantic analysis ──────────────────────────────────────── */
    tab = symtab_new();
    {
        StderrCapture sem_cap = {0, -1, NULL};
        int sem_rc;
        if (begin_stderr_capture(&sem_cap) == 0) {
            sem_rc = symtab_analyze(tab, prog);
            end_stderr_capture(&sem_cap, semantic_err, sizeof(semantic_err));
        } else {
            sem_rc = symtab_analyze(tab, prog);
        }

        if (sem_rc != 0) {
        if (sil.active) end_stderr_silence(&sil);
        fprintf(stderr, "Compilation aborted due to semantic errors.\n");
        maybe_print_ai_suggestion("semantic", src_path, yylineno,
                                  semantic_err[0] ? semantic_err : "semantic analysis failed",
                                  source_text);
        goto cleanup;
        }
    }
    fprintf(stderr, "Compiler: semantic analysis success.\n");

    /* ── Code generation ────────────────────────────────────────── */
    ll_path = make_output_path(src_path);
    if (!ll_path) goto cleanup;

    {
        StderrCapture cg_cap = {0, -1, NULL};
        int cg_rc;
        if (begin_stderr_capture(&cg_cap) == 0) {
            cg_rc = codegen_emit(prog, tab, ll_path, &mod);
            end_stderr_capture(&cg_cap, codegen_err, sizeof(codegen_err));
        } else {
            cg_rc = codegen_emit(prog, tab, ll_path, &mod);
        }

        if (cg_rc != 0) {
        if (sil.active) end_stderr_silence(&sil);
        fprintf(stderr, "Compilation aborted due to codegen errors.\n");
        maybe_print_ai_suggestion("codegen", src_path, yylineno,
                                  codegen_err[0] ? codegen_err : "code generation failed",
                                  source_text);
        goto cleanup;
        }
    }
    fprintf(stderr, "Compiler: code generation success (%s).\n", ll_path ? ll_path : "<unknown>");

    if (sil.active) end_stderr_silence(&sil);

    /* ── Execute ────────────────────────────────────────────────── */
    rc = codegen_run(mod);   /* disposes mod internally */

cleanup:
    if (sil.active) end_stderr_silence(&sil);
    lexico_class_registry_reset();
    free(ll_path);
    free(source_text);
    symtab_free(tab);
    ast_free_program(prog);
    return rc;
}

int lexico_compile(const char *src_path) {
    return lexico_compile_ex(src_path);
}

int lexico_compile_ex(const char *src_path) {
    LexicoCompileOptions opts = default_compile_options();
    opts.enable_autofix = 0;
    return lexico_compile_ex_internal(src_path, &opts, 0);
}

int lexico_compile_ex_opts(const char *src_path, int enable_autofix) {
    LexicoCompileOptions opts = default_compile_options();
    opts.enable_autofix = enable_autofix ? 1 : 0;
    return lexico_compile_ex_internal(src_path, &opts, 0);
}

int lexico_compile_with_options(const char *src_path, const LexicoCompileOptions *opts) {
    LexicoCompileOptions effective = normalize_compile_options(opts);
    return lexico_compile_ex_internal(src_path, &effective, 0);
}

