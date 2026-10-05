#ifndef LEXICO_DRIVER_H
#define LEXICO_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int enable_autofix;
	int interactive_confirm;
	int non_interactive_auto_apply;
	int max_autofix_attempts;
	int dump_expanded_source;
} LexicoCompileOptions;

int lexico_compile(const char *src_path);
int lexico_compile_ex(const char *src_path);
int lexico_compile_ex_opts(const char *src_path, int enable_autofix);
int lexico_compile_with_options(const char *src_path, const LexicoCompileOptions *opts);

#ifdef __cplusplus
}
#endif

#endif 
