/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Skeleton interface for Bison GLR parsers in C

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

#ifndef YY_YY_BUILD_LEXICO_TAB_HPP_INCLUDED
# define YY_YY_BUILD_LEXICO_TAB_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif
/* "%code requires" blocks.  */
#line 27 "src\\lexico.y"

typedef struct MatrixInit MatrixInit;

#line 48 "build\\lexico.tab.hpp"

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    CREATE = 258,                  /* CREATE  */
    DEFINE = 259,                  /* DEFINE  */
    CLASS = 260,                   /* CLASS  */
    INCLUDE = 261,                 /* INCLUDE  */
    INHERITS = 262,                /* INHERITS  */
    PROFILE = 263,                 /* PROFILE  */
    USING = 264,                   /* USING  */
    ROUTINE = 265,                 /* ROUTINE  */
    OVERRIDE = 266,                /* OVERRIDE  */
    RETURNS = 267,                 /* RETURNS  */
    SET = 268,                     /* SET  */
    VALUE = 269,                   /* VALUE  */
    VALUES = 270,                  /* VALUES  */
    CONVERT = 271,                 /* CONVERT  */
    PRINT = 272,                   /* PRINT  */
    APPEND = 273,                  /* APPEND  */
    SORT = 274,                    /* SORT  */
    ASCENDANTLY = 275,             /* ASCENDANTLY  */
    DESCENDANTLY = 276,            /* DESCENDANTLY  */
    COUNT = 277,                   /* COUNT  */
    CHECK = 278,                   /* CHECK  */
    POSITION = 279,                /* POSITION  */
    EXTRACT = 280,                 /* EXTRACT  */
    TO = 281,                      /* TO  */
    ASK = 282,                     /* ASK  */
    TAKE = 283,                    /* TAKE  */
    USER = 284,                    /* USER  */
    INPUT = 285,                   /* INPUT  */
    WITH = 286,                    /* WITH  */
    MESSAGE = 287,                 /* MESSAGE  */
    SET_VALUE_TAKE_USER_INPUT = 288, /* SET_VALUE_TAKE_USER_INPUT  */
    OPEN = 289,                    /* OPEN  */
    MAKE = 290,                    /* MAKE  */
    FILE_T = 291,                  /* FILE_T  */
    WRITE = 292,                   /* WRITE  */
    READ = 293,                    /* READ  */
    LINE = 294,                    /* LINE  */
    LINES = 295,                   /* LINES  */
    CLEAR = 296,                   /* CLEAR  */
    BEGIN_T = 297,                 /* BEGIN_T  */
    CLOSE = 298,                   /* CLOSE  */
    TITLE = 299,                   /* TITLE  */
    INTO = 300,                    /* INTO  */
    ARRAY = 301,                   /* ARRAY  */
    TABLE = 302,                   /* TABLE  */
    MESH = 303,                    /* MESH  */
    MATRIX = 304,                  /* MATRIX  */
    ROWS = 305,                    /* ROWS  */
    COLS = 306,                    /* COLS  */
    SIZE = 307,                    /* SIZE  */
    LENGTH = 308,                  /* LENGTH  */
    SIZED = 309,                   /* SIZED  */
    AT = 310,                      /* AT  */
    EACH = 311,                    /* EACH  */
    INDEX = 312,                   /* INDEX  */
    ROW = 313,                     /* ROW  */
    COLUMN = 314,                  /* COLUMN  */
    EMIT = 315,                    /* EMIT  */
    GIVEBACK = 316,                /* GIVEBACK  */
    OUTCOME = 317,                 /* OUTCOME  */
    OF = 318,                      /* OF  */
    ARGS = 319,                    /* ARGS  */
    IN = 320,                      /* IN  */
    PLUS = 321,                    /* PLUS  */
    MINUS = 322,                   /* MINUS  */
    TIMES = 323,                   /* TIMES  */
    DIV = 324,                     /* DIV  */
    MOD = 325,                     /* MOD  */
    IF = 326,                      /* IF  */
    THEN = 327,                    /* THEN  */
    OTHERWISE = 328,               /* OTHERWISE  */
    WHEN = 329,                    /* WHEN  */
    AND = 330,                     /* AND  */
    OR = 331,                      /* OR  */
    NOT = 332,                     /* NOT  */
    CMP_EQ = 333,                  /* CMP_EQ  */
    CMP_NEQ = 334,                 /* CMP_NEQ  */
    CMP_GT = 335,                  /* CMP_GT  */
    CMP_LT = 336,                  /* CMP_LT  */
    CMP_GTE = 337,                 /* CMP_GTE  */
    CMP_LTE = 338,                 /* CMP_LTE  */
    INDENT = 339,                  /* INDENT  */
    DEDENT = 340,                  /* DEDENT  */
    T_INT = 341,                   /* T_INT  */
    T_FLOAT = 342,                 /* T_FLOAT  */
    T_CHAR = 343,                  /* T_CHAR  */
    T_STRING = 344,                /* T_STRING  */
    T_BOOL = 345,                  /* T_BOOL  */
    TRUE = 346,                    /* TRUE  */
    FALSE = 347,                   /* FALSE  */
    WHILE = 348,                   /* WHILE  */
    DO = 349,                      /* DO  */
    FOR = 350,                     /* FOR  */
    FROM = 351,                    /* FROM  */
    STEP = 352,                    /* STEP  */
    REPEAT = 353,                  /* REPEAT  */
    UNTIL = 354,                   /* UNTIL  */
    ATTEMPT = 355,                 /* ATTEMPT  */
    UPTO = 356,                    /* UPTO  */
    ONFAILURE = 357,               /* ONFAILURE  */
    TIMES_WHILE = 358,             /* TIMES_WHILE  */
    INT_LIT = 359,                 /* INT_LIT  */
    FLOAT_LIT = 360,               /* FLOAT_LIT  */
    CHAR_LIT = 361,                /* CHAR_LIT  */
    STRING_LIT = 362,              /* STRING_LIT  */
    IDENT = 363,                   /* IDENT  */
    PREC_LOWEST = 364              /* PREC_LOWEST  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 968 "src\\lexico.y"

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

#line 194 "build\\lexico.tab.hpp"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_BUILD_LEXICO_TAB_HPP_INCLUDED  */
