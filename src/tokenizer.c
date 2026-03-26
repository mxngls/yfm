#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tokenizer.h"

#define array_push(arr, val)                                                                       \
    ({                                                                                             \
        int _ret = 0;                                                                              \
        if ((arr)->len >= (arr)->cap) {                                                            \
            size_t _new_cap = (arr)->cap == 0 ? 128 : (arr)->cap * 2;                              \
            void*  _grown = realloc((arr)->items, _new_cap * sizeof(*(arr)->items));               \
            if (!_grown) {                                                                         \
                fprintf(stderr, "Error: re-allocation failed\n");                                  \
                _ret = -1;                                                                         \
            } else {                                                                               \
                (arr)->items = _grown;                                                             \
                (arr)->cap = _new_cap;                                                             \
                (arr)->items[(arr)->len++] = (val);                                                \
            }                                                                                      \
        } else {                                                                                   \
            (arr)->items[(arr)->len++] = (val);                                                    \
        }                                                                                          \
        _ret;                                                                                      \
    })
#define array_pop(arr)                                                                             \
    ({                                                                                             \
        int _ret = 0;                                                                              \
        if ((arr)->len == 0) {                                                                     \
            fprintf(stderr, "Error: array empty\n");                                               \
            _ret = -1;                                                                             \
        } else {                                                                                   \
            (arr)->len--;                                                                          \
        }                                                                                          \
        _ret;                                                                                      \
    })
#define array_back(arr) ((arr)->items[(arr)->len - 1])

#define NPOS (size_t)-1
#define WS ' '

static void cursor_advance(Cursor* c, size_t n) {
    for (size_t i = 0; i < n && c->pos < c->len; i++) {
        if (c->data[c->pos] == '\n') {
            c->line++;
            c->col = 1;
        } else {
            c->col++;
        }
        c->pos++;
    }
}

static char cursor_peek(Cursor* c) { return c->data[c->pos]; }

static char* cursor_current(Cursor* c) { return c->data + c->pos; }

static size_t cursor_remaining_count(Cursor* c) { return c->len - c->pos; }

static int is_separator(char* str, size_t str_len, char ch) {
    if (str_len == 0 || str[0] != ch)
        return 0;
    return str_len == 1 || str[1] == ' ' || str[1] == '\n';
}

static int str_starts_with(char* str, size_t str_len, char* prefix, size_t prefix_len) {
    if (prefix_len > str_len)
        return 0;
    for (size_t i = 0; i < prefix_len; i++) {
        if (str[i] != prefix[i])
            return 0;
    }
    return 1;
}

static size_t str_first_not_of(char* str, size_t str_len, char marker) {
    for (size_t i = 0; i < str_len; i++) {
        if (str[i] != marker)
            return i;
    }
    return NPOS;
}

static size_t str_find(char* str, size_t str_len, char marker) {
    for (size_t i = 0; i < str_len; i++) {
        if (str[i] == '\\' && i + 1 < str_len) {
            i++;
            continue;
        }
        if (str[i] == marker) {
            return i;
        }
    }
    return NPOS;
}

static int tokenizer_indent(TokenArray* token_arr, int64_t depth, IndentArray* indent_arr) {
    if (array_push(
            token_arr,
            ((Token){
                .kind = TOKEN_INDENT, .start = NULL, .len = 0, .indent = array_back(indent_arr)})))
        return -1;
    if (array_push(indent_arr, depth))
        return -1;
    return 0;
}

static int tokenzier_dedent(TokenArray* token_arr, IndentArray* indent_arr) {
    if (array_push(token_arr, ((Token){
                                  .kind = TOKEN_DEDENT,
                                  .start = NULL,
                                  .len = 0,
                                  .indent = array_back(indent_arr),
                              }))) {
        return -1;
    };
    if (array_pop(indent_arr)) {
        return -1;
    };
    return 0;
}

static void cursor_skip_line(Cursor* c) {
    while (cursor_remaining_count(c) > 0) {
        if (cursor_peek(c) == '\n') {
            cursor_advance(c, 1);
            return;
        }
        cursor_advance(c, 1);
    }
}

static void cursor_skip_ws(Cursor* c) {
    while (cursor_remaining_count(c) > 0 && cursor_peek(c) == ' ') {
        cursor_advance(c, 1);
    }
}

static int tokenizer_tokenize_bare_block(Cursor* c, size_t parent_indent, TokenArray* tokens) {
    char*  start = cursor_current(c);
    size_t start_pos = c->pos;

    while (cursor_remaining_count(c) > 0) {
        if (cursor_peek(c) == '\n') {
            cursor_advance(c, 1);
            size_t indent = 0;
            while (cursor_remaining_count(c) > 0 &&
                   (cursor_peek(c) == WS || cursor_peek(c) == '\n')) {
                indent++;
                cursor_advance(c, 1);
            }
            // end of block reached
            if (indent <= parent_indent) {
                if (array_push(tokens, ((Token){
                                           .kind = TOKEN_STRING,
                                           .start = start,
                                           .len = c->pos - start_pos,
                                           .indent = parent_indent + 1,
                                       }))) {
                    return -1;
                };
                return 0;
            }
        } else {
            cursor_advance(c, 1);
        }
    }

    // reached end of input
    if (array_push(tokens, ((Token){
                               .kind = TOKEN_STRING,
                               .start = start,
                               .len = c->pos - start_pos,
                               .indent = parent_indent + 1,
                           }))) {
        return -1;
    };
    return 0;
}

static int tokenizer_tokenize_bare_str(Cursor* c, TokenArray* tokens, IndentArray* indents) {
    char* start = cursor_current(c);
    while (cursor_remaining_count(c) > 0) {
        char ch = cursor_peek(c);
        // clang-format off
		if (ch == '\n'
		    || ch == '['
		    || ch == '"'
		    || ch == '|'
		    || ch == ' '
		    || is_separator(cursor_current(c), cursor_remaining_count(c), ':')
		    ) {
		    break;
		}
        // clang-format on
        cursor_advance(c, 1);
    }
    size_t len = cursor_current(c) - start;
    cursor_skip_ws(c);
    if (len > 0) {
        if (array_push(tokens, ((Token){.kind = TOKEN_STRING,
                                        .start = start,
                                        .len = len,
                                        .indent = array_back(indents)}))) {
            return -1;
        };
    }
    return 0;
}

static int tokenizer_tokenize_str(Cursor* c, char end, TokenArray* tokens, IndentArray* indents) {
    // skip opening delimiter
    cursor_advance(c, 1);

    char*  start = cursor_current(c);
    size_t pos = str_find(start, cursor_remaining_count(c), end);

    if (pos == NPOS) {
        fprintf(stderr, "%zu:%zu: Error: Unterminated string literal\n", c->line, c->col);
        return -1;
    }

    if (array_push(tokens, ((Token){
                               .kind = TOKEN_STRING,
                               .start = start,
                               .len = pos,
                               .indent = array_back(indents),
                           }))) {
        return -1;
    };

    // skip past content + closing delimiter
    cursor_advance(c, pos + 1);
    return 0;
}

static int tokenizer_tokenize_list(Cursor* c, TokenArray* tokens, IndentArray* indents) {
    if (array_push(tokens, ((Token){
                               .kind = TOKEN_LBRACKET,
                               .start = cursor_current(c),
                               .len = 1,
                               .indent = array_back(indents),
                           }))) {
        return -1;
    };
    cursor_advance(c, 1);

    size_t pos = 0;
    while (cursor_remaining_count(c) != 0) {
        if ((pos = str_first_not_of(cursor_current(c), cursor_remaining_count(c), WS)) != 0) {
            cursor_advance(c, pos);
            continue;
        }
        if (cursor_peek(c) == '"') {
            if (tokenizer_tokenize_str(c, '"', tokens, indents)) {
                return -1;
            }
            continue;
        }
        if (cursor_peek(c) == ',') {
            if (array_push(tokens, ((Token){.kind = TOKEN_COMMA,
                                            .start = cursor_current(c),
                                            .len = 1,
                                            .indent = array_back(indents)}))) {
                return -1;
            };
            cursor_advance(c, 1);
            continue;
        }
        if (cursor_peek(c) == ']') {
            break;
        }
        fprintf(stderr, "%zu:%zu: Error: unexpected character in list: '%c'\n", c->line, c->col,
                cursor_peek(c));
        return -1;
    }

    if (cursor_remaining_count(c) == 0) {
        fprintf(stderr, "%zu:%zu: Error: unclosed list\n", c->line, c->col);
        return -1;
    }

    if (array_push(tokens, ((Token){.kind = TOKEN_RBRACKET,
                                    .start = cursor_current(c),
                                    .len = 1,
                                    .indent = array_back(indents)}))) {
        return -1;
    };

    cursor_advance(c, 1);
    cursor_skip_ws(c);

    if (cursor_peek(c) == '#') {
        cursor_skip_line(c);
        return 0;
    }

    if (cursor_remaining_count(c) == 0 || cursor_peek(c) != '\n') {
        fprintf(stderr, "%zu:%zu: Error: no newline after ']'\n", c->line, c->col);
        return -1;
    }
    cursor_advance(c, 1);
    return 0;
}

static int tokenizer_tokenize_line(Cursor* c, TokenArray* tokens, IndentArray* indents) {
    char* line_start = cursor_current(c);

    // find YAML start marker
    if (str_starts_with(cursor_current(c), cursor_remaining_count(c), "---", strlen("---"))) {
        while (indents->len > 1) {
            tokenzier_dedent(tokens, indents);
        }
        if (array_push(tokens, ((Token){
                                   .kind = TOKEN_START,
                                   .start = cursor_current(c),
                                   .len = 3,
                                   .indent = 0,
                               }))) {
            return -1;
        };
        cursor_skip_line(c);
        return 0;
    }

    // find YAML end marker
    if (str_starts_with(cursor_current(c), cursor_remaining_count(c), "...", strlen("..."))) {
        while (indents->len > 1) {
            tokenzier_dedent(tokens, indents);
        }
        if (array_push(tokens, ((Token){
                                   .kind = TOKEN_END,
                                   .start = cursor_current(c),
                                   .len = 3,
                                   .indent = 0,
                               }))) {
            return -1;
        };
        cursor_advance(c, cursor_remaining_count(c));
        return 0;
    }

    size_t pos = str_first_not_of(cursor_current(c), cursor_remaining_count(c), WS);
    if (pos == NPOS || cursor_current(c)[pos] == '#' || cursor_current(c)[pos] == '\n') {
        cursor_skip_line(c);
        return 0;
    }

    // track possible indentation changes
    if (pos != (size_t)array_back(indents)) {
        if ((int64_t)pos > array_back(indents)) {
            if (tokenizer_indent(tokens, pos, indents)) {
                return -1;
            };
        } else {
            while (pos != (size_t)array_back(indents)) {
                if (pos < (size_t)array_back(indents)) {
                    tokenzier_dedent(tokens, indents);
                } else {
                    fprintf(stderr, "%zu:%zu: Error: bad indent\n", c->line, c->col);
                    return -1;
                }
            }
        }
    }

    cursor_advance(c, pos);

    while (cursor_remaining_count(c) != 0) {
        if (cursor_peek(c) == '\n') {
            cursor_advance(c, 1);
            return 0;
        }

        if (cursor_peek(c) == '[') {
            return tokenizer_tokenize_list(c, tokens, indents);
        }

        if (cursor_peek(c) == '"') {
            if (tokenizer_tokenize_str(c, '"', tokens, indents)) {
                return -1;
            }
            continue;
        }

        if (cursor_peek(c) == '|') {
            if (array_push(tokens, ((Token){.kind = TOKEN_PIPE,
                                            .start = cursor_current(c),
                                            .len = 1,
                                            .indent = array_back(indents)}))) {
                return -1;
            };
            cursor_advance(c, 1);
            if (cursor_remaining_count(c) == 0 || cursor_peek(c) != '\n') {
                fprintf(stderr, "%zu:%zu: Error: newline expected\n", c->line, c->col);
                return -1;
            }
            cursor_advance(c, 1);
            if (cursor_remaining_count(c) == 0 || cursor_peek(c) != ' ') {
                fprintf(stderr, "%zu:%zu: Error: indent expected\n", c->line, c->col);
                return -1;
            }
            if (tokenizer_tokenize_bare_block(c, indents->items[indents->len - 1], tokens)) {
                return -1;
            }
            return 0;
        }

        if (str_starts_with(cursor_current(c), cursor_remaining_count(c), "- ", 2)) {
            if (array_push(tokens, ((Token){.kind = TOKEN_DASH,
                                            .start = cursor_current(c),
                                            .len = 1,
                                            .indent = array_back(indents)}))) {
                return -1;
            };
            cursor_advance(c, 1);
            size_t pos = str_first_not_of(cursor_current(c), cursor_remaining_count(c), WS);
            if (pos == NPOS || cursor_current(c)[pos] == '\n') {
                cursor_skip_line(c);
                return 0;
            }
            cursor_advance(c, 1);
            if (tokenizer_indent(tokens, cursor_current(c) - line_start, indents)) {
                return -1;
            };
            continue;
        }

        if (cursor_peek(c) == '#') {
            cursor_skip_line(c);
            return 0;
        }

        if (is_separator(cursor_current(c), cursor_remaining_count(c), ':')) {
            if (array_push(tokens, ((Token){.kind = TOKEN_COLON,
                                            .start = cursor_current(c),
                                            .len = 1,
                                            .indent = array_back(indents)}))) {
                return -1;
            };
            cursor_advance(c, 1);
            size_t pos = str_first_not_of(cursor_current(c), cursor_remaining_count(c), WS);
            if (pos == NPOS || cursor_current(c)[pos] == '\n') {
                cursor_skip_line(c);
                return 0;
            }
            cursor_advance(c, pos);
            continue;
        }

        if (tokenizer_tokenize_bare_str(c, tokens, indents)) {
            return -1;
        }
    }

    return 0;
}

int tokenizer_tokenize(Cursor* c, TokenArray* tokens) {
    int ret = 0;

    IndentArray indents = {.len = 0, .cap = 0, .items = NULL};
    array_push(&indents, 0);

    while (cursor_remaining_count(c) != 0) {
        if (tokenizer_tokenize_line(c, tokens, &indents)) {
            ret = -1;
            goto error;
        }
    }
    while (indents.len > 1) {
        if (tokenzier_dedent(tokens, &indents)) {
            ret = -1;
            goto error;
        };
    }
    array_push(tokens, ((Token){.kind = TOKEN_END, .start = NULL, .len = 0, .indent = 0}));
    goto done;

error:
    ret = -1;

done:
    free(indents.items);

    return ret;
}
