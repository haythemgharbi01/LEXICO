/*
 * lexico.l – Flex lexer for the Lexico language.
 *
 * Layout-sensitive: lines starting with '>' denote block depth.
 *
 * Start-condition design:
 *   INITIAL     – normal token scanning
 *   BOL         – beginning-of-line: count '>' markers, then resolve
 *                 INDENT/DEDENT and switch back to INITIAL
 *   DQ          – "DEDENT queue": emit one more DEDENT per entry
 *
 * Flow:
 *   '\n' in INITIAL  →  BEGIN(BOL), reset bol_depth
 *   '>' in BOL       →  bol_depth++
 *   ' '|'\t' in BOL  →  skip
 *   '\n' in BOL      →  blank line, reset bol_depth, stay in BOL
 *   . in BOL         →  unput(c), compare bol_depth vs stack:
 *                         INDENT or first DEDENT → BEGIN(DQ or INITIAL)
 *   EOF in BOL       →  flush remaining DEDENTs
 *   . in DQ          →  unput(c), emit DEDENT; if pending==0 → BEGIN(INITIAL)
 */

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.hpp"
#include "lexico.tab.hpp"

/* Column tracking */
static int col = 1;
#define ADV_COL   (col += yyleng)
#define RESET_COL (col = 1)

#define MAX_IDENT 255
#define MAX_STR   4096

static char *safe_strdup(const char *s, size_t max) {
    size_t len = strlen(s);
    if (len > max) {
        fprintf(stderr, "Error (line %d, col %d): token too long.\n",
                yylineno, col);
        exit(EXIT_FAILURE);
    }
    char *d = static_cast<char *>(malloc(len + 1));
    if (!d) { fputs("Fatal: OOM\n", stderr); exit(EXIT_FAILURE); }
    return static_cast<char *>(memcpy(d, s, len + 1));
}

/* ── Indent stack ───────────────────────────────────────────────── */
#define MAX_INDENT 64
static int indent_stack[MAX_INDENT];
static int indent_top     = 0;
static int pending_dedents = 0;
static int bol_depth       = 0;

/* lexico_indent_reset() is defined after %% (needs BEGIN macro) */
%}

%option noyywrap yylineno
%x BOL DQ

%%

 /* ── DQ: flush one pending DEDENT ──────────────────────── */
<DQ>.|\n {
    unput(yytext[0]);
    pending_dedents--;
    if (pending_dedents <= 0) {
        pending_dedents = 0;
        BEGIN(INITIAL);
    }
    return DEDENT;
}

 /* ── INITIAL: newline → BOL ─────────────────────────────── */
<INITIAL>\n  { RESET_COL; bol_depth = 0; BEGIN(BOL); }

 /* ── BOL: count '>' markers ─────────────────────────────── */
<BOL>">"    { bol_depth++; col++; }
<BOL>[ \t]  { col++;               }
<BOL>\n     { bol_depth = 0; RESET_COL; /* blank line */ }
<BOL><<EOF>> {
    BEGIN(INITIAL);
    if (indent_top > 0) {
        pending_dedents = indent_top - 1;
        indent_top = 0;
        indent_stack[0] = 0;
        if (pending_dedents > 0) BEGIN(DQ);
        return DEDENT;
    }
}
<BOL>. {
    /* Resolve depth */
    unput(yytext[0]);
    BEGIN(INITIAL);

    int depth   = bol_depth;
    int current = indent_stack[indent_top];

    if (depth > current) {
        if (indent_top + 1 >= MAX_INDENT) {
            fprintf(stderr, "Error (line %d): nesting too deep.\n", yylineno);
            exit(EXIT_FAILURE);
        }
        indent_stack[++indent_top] = depth;
        return INDENT;
    } else if (depth < current) {
        /* Find how many levels to pop */
        int pops = 0;
        while (indent_top > 0 && indent_stack[indent_top] > depth) {
            indent_top--;
            pops++;
        }
        /* depth==0 is always valid (back to outermost) */
        if (depth != 0 && indent_stack[indent_top] != depth) {
            fprintf(stderr,
                    "Error (line %d): mismatched block depth %d.\n",
                    yylineno, depth);
            exit(EXIT_FAILURE);
        }
        pending_dedents = pops - 1;
        if (pending_dedents > 0) BEGIN(DQ);
        return DEDENT;
    }
    /* depth == current: same level, no token */
}

 /* ── EOF in INITIAL: flush remaining DEDENTs ────────────── */
<INITIAL><<EOF>> {
    if (indent_top > 0) {
        pending_dedents = indent_top - 1;
        indent_top = 0;
        indent_stack[0] = 0;
        if (pending_dedents > 0) BEGIN(DQ);
        return DEDENT;
    }
    yyterminate();
}

 /* ════════════════════════════════════════════════════════════
  *  INITIAL token rules
  * ════════════════════════════════════════════════════════════ */

 /* ── Whitespace ─────────────────────────────────────────── */
<INITIAL>[ \t\r]+  { ADV_COL; }

 /* ── Multi-word comparison operators ────────────────────── */
<INITIAL>"is"[ \t]+"not"[ \t]+"equal"[ \t]+"to"            { ADV_COL; return CMP_NEQ; }
<INITIAL>"is"[ \t]+"equal"[ \t]+"to"                       { ADV_COL; return CMP_EQ;  }
<INITIAL>"is"[ \t]+"greater"[ \t]+"or"[ \t]+"equal"[ \t]+"to" { ADV_COL; return CMP_GTE; }
<INITIAL>"is"[ \t]+"less"[ \t]+"or"[ \t]+"equal"[ \t]+"to"    { ADV_COL; return CMP_LTE; }
<INITIAL>"is"[ \t]+"greater"[ \t]+"than"                   { ADV_COL; return CMP_GT;  }
<INITIAL>"is"[ \t]+"less"[ \t]+"than"                      { ADV_COL; return CMP_LT;  }

 /* ── Keywords ────────────────────────────────────────────── */
<INITIAL>"create"    { ADV_COL; return CREATE;    }
<INITIAL>"define"    { ADV_COL; return DEFINE;    }
<INITIAL>"class"     { ADV_COL; return CLASS;     }
<INITIAL>"include"   { ADV_COL; return INCLUDE;   }
<INITIAL>"inherits"  { ADV_COL; return INHERITS;  }
<INITIAL>"profile"   { ADV_COL; return PROFILE;   }
<INITIAL>"using"     { ADV_COL; return USING;     }
<INITIAL>"routine"   { ADV_COL; return ROUTINE;   }
<INITIAL>"override"  { ADV_COL; return OVERRIDE;  }
<INITIAL>"returns"   { ADV_COL; return RETURNS;   }
<INITIAL>"open"      { ADV_COL; return OPEN;      }
<INITIAL>"make"      { ADV_COL; return MAKE;      }
<INITIAL>"file"      { ADV_COL; return FILE_T;    }
<INITIAL>"write"     { ADV_COL; return WRITE;     }
<INITIAL>"read"      { ADV_COL; return READ;      }
<INITIAL>"line"      { ADV_COL; return LINE;      }
<INITIAL>"lines"     { ADV_COL; return LINES;     }
<INITIAL>"clear"     { ADV_COL; return CLEAR;     }
<INITIAL>"begin"     { ADV_COL; return BEGIN_T;   }
<INITIAL>"close"     { ADV_COL; return CLOSE;     }
<INITIAL>"title"     { ADV_COL; return TITLE;     }
<INITIAL>"into"      { ADV_COL; return INTO;      }
<INITIAL>"array"     { ADV_COL; return ARRAY;     }
<INITIAL>"table"     { ADV_COL; return TABLE;     }
<INITIAL>"mesh"      { ADV_COL; return MESH;      }
<INITIAL>"matrix"    { ADV_COL; return MATRIX;    }
<INITIAL>"rows"      { ADV_COL; return ROWS;      }
<INITIAL>"cols"      { ADV_COL; return COLS;      }
<INITIAL>"row"       { ADV_COL; return ROW;       }
<INITIAL>"column"    { ADV_COL; return COLUMN;    }
<INITIAL>"of"        { ADV_COL; return OF;        }
<INITIAL>"size"      { ADV_COL; return SIZE;      }
<INITIAL>"length"    { ADV_COL; return LENGTH;    }
<INITIAL>"sized"     { ADV_COL; return SIZED;     }
<INITIAL>"at"        { ADV_COL; return AT;        }
<INITIAL>"each"      { ADV_COL; return EACH;      }
<INITIAL>"index"     { ADV_COL; return INDEX;     }
<INITIAL>"set"[ \t]+"value"[ \t]+"take"[ \t]+"user"[ \t]+"input" { ADV_COL; return SET_VALUE_TAKE_USER_INPUT; }
<INITIAL>"set"       { ADV_COL; return SET;       }
<INITIAL>"values"    { ADV_COL; return VALUES;    }
<INITIAL>"value"     { ADV_COL; return VALUE;     }
<INITIAL>"convert"   { ADV_COL; return CONVERT;   }
<INITIAL>"print"     { ADV_COL; return PRINT;     }
<INITIAL>"append"    { ADV_COL; return APPEND;    }
<INITIAL>"sort"      { ADV_COL; return SORT;      }
<INITIAL>"ascendantly" { ADV_COL; return ASCENDANTLY; }
<INITIAL>"descendantly" { ADV_COL; return DESCENDANTLY; }
<INITIAL>"count"     { ADV_COL; return COUNT;     }
<INITIAL>"check"     { ADV_COL; return CHECK;     }
<INITIAL>"position"  { ADV_COL; return POSITION;  }
<INITIAL>"extract"   { ADV_COL; return EXTRACT;   }
<INITIAL>"emit"      { ADV_COL; return EMIT;      }
<INITIAL>"giveback"  { ADV_COL; return GIVEBACK;  }
<INITIAL>"outcome"   { ADV_COL; return OUTCOME;   }
<INITIAL>"args"      { ADV_COL; return ARGS;      }
<INITIAL>"in"        { ADV_COL; return IN;        }
<INITIAL>"ask"       { ADV_COL; return ASK;       }
<INITIAL>"take"      { ADV_COL; return TAKE;      }
<INITIAL>"user"      { ADV_COL; return USER;      }
<INITIAL>"input"     { ADV_COL; return INPUT;     }
<INITIAL>"with"      { ADV_COL; return WITH;      }
<INITIAL>"message"   { ADV_COL; return MESSAGE;   }
<INITIAL>"to"        { ADV_COL; return TO;        }
<INITIAL>"plus"      { ADV_COL; return PLUS;      }
<INITIAL>"minus"     { ADV_COL; return MINUS;     }
<INITIAL>"times"[ \t]+"while" { ADV_COL; return TIMES_WHILE; }
<INITIAL>"times"     { ADV_COL; return TIMES;     }
<INITIAL>"mod"       { ADV_COL; return MOD;       }
<INITIAL>"divided"[ \t]+"by"  { ADV_COL; return DIV; }
<INITIAL>"if"        { ADV_COL; return IF;        }
<INITIAL>"then"      { ADV_COL; return THEN;      }
<INITIAL>"otherwise" { ADV_COL; return OTHERWISE; }
<INITIAL>"when"      { ADV_COL; return WHEN;      }
<INITIAL>"and"       { ADV_COL; return AND;       }
<INITIAL>"or"        { ADV_COL; return OR;        }
<INITIAL>"not"       { ADV_COL; return NOT;       }
<INITIAL>"while"     { ADV_COL; return WHILE;     }
<INITIAL>"do"        { ADV_COL; return DO;        }
<INITIAL>"for"       { ADV_COL; return FOR;       }
<INITIAL>"from"      { ADV_COL; return FROM;      }
<INITIAL>"step"      { ADV_COL; return STEP;      }
<INITIAL>"repeat"    { ADV_COL; return REPEAT;    }
<INITIAL>"until"     { ADV_COL; return UNTIL;     }
<INITIAL>"attempt"   { ADV_COL; return ATTEMPT;   }
<INITIAL>"up"[ \t]+"to"      { ADV_COL; return UPTO;      }
<INITIAL>"on"[ \t]+"failure" { ADV_COL; return ONFAILURE; }

 /* ── Type keywords ──────────────────────────────────────── */
<INITIAL>"int"       { ADV_COL; return T_INT;    }
<INITIAL>"float"     { ADV_COL; return T_FLOAT;  }
<INITIAL>"char"      { ADV_COL; return T_CHAR;   }
<INITIAL>"string"    { ADV_COL; return T_STRING; }
<INITIAL>"bool"      { ADV_COL; return T_BOOL;   }
<INITIAL>"true"      { ADV_COL; return TRUE;     }
<INITIAL>"false"     { ADV_COL; return FALSE;    }

 /* ── Float literal (must precede int) ───────────────────── */
<INITIAL>[0-9]+\.[0-9]+ {
    ADV_COL;
    yylval.dval = strtod(yytext, NULL);
    return FLOAT_LIT;
}

 /* ── Integer literal ────────────────────────────────────── */
<INITIAL>[0-9]+ {
    ADV_COL;
    yylval.ival = strtoll(yytext, NULL, 10);
    return INT_LIT;
}

 /* ── Char literal ───────────────────────────────────────── */
<INITIAL>'(\\.|[^'\\])' {
    ADV_COL;
    if (yytext[1] == '\\') {
        switch (yytext[2]) {
            case 'n':  yylval.cval = '\n'; break;
            case 't':  yylval.cval = '\t'; break;
            case '\\': yylval.cval = '\\'; break;
            case '\'': yylval.cval = '\''; break;
            case '0':  yylval.cval = '\0'; break;
            default:   yylval.cval = yytext[2]; break;
        }
    } else {
        yylval.cval = yytext[1];
    }
    return CHAR_LIT;
}

 /* ── String literal ─────────────────────────────────────── */
<INITIAL>\"([^\"\\]|\\.)*\" {
    ADV_COL;
    size_t inner = (size_t)yyleng - 2;
    if (inner > MAX_STR) {
        fprintf(stderr, "Error (line %d, col %d): string too long.\n",
                yylineno, col);
        exit(EXIT_FAILURE);
    }
    char *buf = static_cast<char *>(malloc(inner + 1));
    if (!buf) { fputs("Fatal: OOM\n", stderr); exit(EXIT_FAILURE); }
    memcpy(buf, yytext + 1, inner);
    buf[inner] = '\0';
    yylval.sval = buf;
    return STRING_LIT;
}

 /* ── Identifier ─────────────────────────────────────────── */
<INITIAL>[a-zA-Z_][a-zA-Z0-9_]* {
    ADV_COL;
    yylval.sval = safe_strdup(yytext, MAX_IDENT);
    return IDENT;
}

 /* ── Punctuation ────────────────────────────────────────── */
<INITIAL>","  { ADV_COL; return ','; }
<INITIAL>";"  { ADV_COL; return ';'; }
<INITIAL>"/"  { ADV_COL; return '/'; }
<INITIAL>"("  { ADV_COL; return '('; }
<INITIAL>")"  { ADV_COL; return ')'; }

 /* ── Catch-all ──────────────────────────────────────────── */
<INITIAL>. {
    fprintf(stderr, "Error (line %d, col %d): unexpected character '%c'.\n",
            yylineno, col, yytext[0]);
    ADV_COL;
}

%%
void lexico_indent_reset(void) {
    indent_stack[0]  = 0;
    indent_top       = 0;
    pending_dedents  = 0;
    bol_depth        = 0;
    BEGIN(INITIAL);
}