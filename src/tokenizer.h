#ifndef TOKENIZER_H

#define TOKENIZER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct cursor {
    char*  data;
    size_t len;
    size_t pos;
    size_t line;
    size_t col;
} Cursor;

typedef enum {
    TOKEN_STRING = 1,
    TOKEN_COLON,
    TOKEN_COMMA,
    TOKEN_DASH,
    TOKEN_DEDENT,
    TOKEN_END,
    TOKEN_INDENT,
    TOKEN_LBRACKET,
    TOKEN_PIPE,
    TOKEN_RBRACKET,
    TOKEN_START,
} TokenKind;

typedef struct token {
    TokenKind kind;
    char*     start;
    size_t    len;
    size_t    indent;
} Token;

typedef struct token_array {
    size_t len;
    size_t cap;
    Token* items;
} TokenArray;

typedef struct indent_array {
    size_t   len;
    size_t   cap;
    int64_t* items;
} IndentArray;

int tokenizer_tokenize(Cursor* c, TokenArray* tokens);

#endif
