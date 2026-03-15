#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define array_cap(arr) (sizeof((arr)->items) / sizeof((arr)->items[0]))
#define array_push(arr, val)                                                                       \
	({                                                                                         \
		int _ret = 0;                                                                      \
		if ((arr)->len >= array_cap(arr)) {                                                \
			fprintf(stderr, "Error: array full\n");                                    \
			_ret = -1;                                                                 \
		} else {                                                                           \
			(arr)->items[(arr)->len++] = (val);                                        \
		}                                                                                  \
		_ret;                                                                              \
	})
#define array_pop(arr)                                                                             \
	({                                                                                         \
		int _ret = 0;                                                                      \
		if ((arr)->len == 0) {                                                             \
			fprintf(stderr, "Error: array empty\n");                                   \
			_ret = -1;                                                                 \
		} else {                                                                           \
			(arr)->len--;                                                              \
		}                                                                                  \
		_ret;                                                                              \
	})
#define array_back(arr) ((arr)->items[(arr)->len - 1])

#define TOKEN_MAX 512
#define INDENTS_MAX 16
#define NPOS (size_t)-1

#define WS ' '

typedef struct post {
	char *title;
	char *created_at;
	char *updated_at;
	char *description;
	// TODO: Add support for tags
} Post;

typedef enum {
	TOKEN_STRING = 1,
	TOKEN_COLON,
	TOKEN_COMMA,
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
	char *start;
	size_t len;
	size_t indent;
} Token;

typedef struct token_array {
	size_t len;
	Token items[TOKEN_MAX];
} TokenArray;

typedef struct indent_array {
	size_t len;
	int64_t items[INDENTS_MAX];
} IndentArray;

typedef enum {
	YAML_STRING,
	YAML_BLOCK_STRING,
	YAML_LIST,
} YamlValueKind;

typedef struct yaml_value {
	YamlValueKind kind;
	union {
		struct {
			char *start;
			size_t len;
		} string;
		struct {
			struct YamlValue *items;
			size_t count;
		} list;
	};
} YamlValue;

typedef struct yaml_entry {
	char *key;
	size_t key_len;
	YamlValue value;
} YamlEntry;

typedef struct cursor {
	char *data;
	size_t len;
	size_t pos;
	size_t line;
	size_t col;
} Cursor;

static TokenArray tokens = {.len = 0};
static IndentArray indents = {.len = 1, .items = {0}};

void cursor_advance(Cursor *c, size_t n) {
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

char cursor_peek(Cursor *c) { return c->data[c->pos]; }

char *cursor_current(Cursor *c) { return c->data + c->pos; }

size_t cursor_remaining(Cursor *c) { return c->len - c->pos; }

int is_separator(char *str, size_t str_len, char ch) {
	if (str_len == 0 || str[0] != ch)
		return 0;
	return str_len == 1 || str[1] == ' ' || str[1] == '\n';
}

int str_starts_with(char *str, size_t str_len, char *prefix, size_t prefix_len) {
	if (prefix_len > str_len)
		return 0;
	for (size_t i = 0; i < prefix_len; i++) {
		if (str[i] != prefix[i])
			return 0;
	}
	return 1;
}

size_t str_first_not_of(char *str, size_t str_len, char marker) {
	for (size_t i = 0; i < str_len; i++) {
		if (str[i] != marker)
			return i;
	}
	return NPOS;
}

size_t str_find(char *str, size_t str_len, char marker) {
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

int tokenizer_indent(Cursor *c, TokenArray *token_arr, int64_t depth, IndentArray *indent_arr) {
	if (array_push(token_arr, ((Token){.kind = TOKEN_INDENT,
					   .start = cursor_current(c),
					   .len = cursor_remaining(c),
					   .indent = array_back(&indents)})))
		return -1;
	if (array_push(indent_arr, depth))
		return -1;
	return 0;
}

int tokenzier_dedent(Cursor *c, TokenArray *token_arr, IndentArray *indent_arr) {
	if (array_push(token_arr, ((Token){
				      .kind = TOKEN_DEDENT,
				      .start = cursor_current(c),
				      .len = cursor_remaining(c),
				      .indent = array_back(indent_arr),
				  }))) {
		return -1;
	};
	if (array_pop(indent_arr)) {
		return -1;
	};
	return 0;
}

void cursor_skip_line(Cursor *c) {
	while (cursor_remaining(c) > 0) {
		if (cursor_peek(c) == '\n') {
			cursor_advance(c, 1);
			return;
		}
		cursor_advance(c, 1);
	}
}

int tokenizer_tokenize_bare_block(Cursor *c, size_t parent_indent) {
	char *start = cursor_current(c);
	size_t start_pos = c->pos;

	while (cursor_remaining(c) > 0) {
		if (cursor_peek(c) == '\n') {
			cursor_advance(c, 1);
			size_t indent = 0;
			while (cursor_remaining(c) > 0 &&
			       (cursor_peek(c) == WS || cursor_peek(c) == '\n')) {
				indent++;
				cursor_advance(c, 1);
			}
			// end of block reached
			if (indent <= parent_indent) {
				if (array_push(&tokens, ((Token){
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
	if (array_push(&tokens, ((Token){
				    .kind = TOKEN_STRING,
				    .start = start,
				    .len = c->pos - start_pos,
				    .indent = parent_indent + 1,
				}))) {
		return -1;
	};
	return 0;
}

int tokenizer_tokenize_bare_str(Cursor *c) {
	char *start = cursor_current(c);
	while (cursor_remaining(c) > 0) {
		char ch = cursor_peek(c);
		// clang-format off
		if (ch == '\n'
		    || ch == '['
		    || ch == '"'
		    || ch == '|'
		    || ch == ' '
		    || is_separator(cursor_current(c), cursor_remaining(c), ':')
		    ) {
		    break;
		}
		// clang-format on
		cursor_advance(c, 1);
	}
	size_t len = cursor_current(c) - start;
	// skip trailing whitespace
	while (cursor_remaining(c) > 0 && cursor_peek(c) == ' ') {
		cursor_advance(c, 1);
	}
	if (len > 0) {
		if (array_push(&tokens, ((Token){.kind = TOKEN_STRING,
						 .start = start,
						 .len = len,
						 .indent = array_back(&indents)}))) {
			return -1;
		};
	}
	return 0;
}

int tokenizer_tokenize_str(Cursor *c, char end) {
	// skip opening delimiter
	cursor_advance(c, 1);

	char *start = cursor_current(c);
	size_t pos = str_find(start, cursor_remaining(c), end);

	if (pos == NPOS) {
		fprintf(stderr, "%zu:%zu: Error: Unterminated string literal\n", c->line, c->col);
		return -1;
	}

	if (array_push(&tokens, ((Token){
				    .kind = TOKEN_STRING,
				    .start = start,
				    .len = pos,
				    .indent = array_back(&indents),
				}))) {
		return -1;
	};

	// skip past content + closing delimiter
	cursor_advance(c, pos + 1);
	return 0;
}

int tokenizer_tokenize_list(Cursor *c) {
	if (array_push(&tokens, ((Token){
				    .kind = TOKEN_LBRACKET,
				    .start = cursor_current(c),
				    .len = 1,
				    .indent = array_back(&indents),
				}))) {
		return -1;
	};
	cursor_advance(c, 1);

	size_t pos = 0;
	while (cursor_remaining(c) != 0) {
		if ((pos = str_first_not_of(cursor_current(c), cursor_remaining(c), WS)) != 0) {
			cursor_advance(c, pos);
			continue;
		}
		if (cursor_peek(c) == '"') {
			if (tokenizer_tokenize_str(c, '"')) {
				return -1;
			}
			continue;
		}
		if (cursor_peek(c) == ',') {
			if (array_push(&tokens, ((Token){.kind = TOKEN_COMMA,
							 .start = cursor_current(c),
							 .len = 1,
							 .indent = array_back(&indents)}))) {
				return -1;
			};
			cursor_advance(c, 1);
			continue;
		}
		if (cursor_peek(c) == ']') {
			break;
		}
		fprintf(stderr, "%zu:%zu: Error: unexpected character in list: '%c'\n", c->line,
			c->col, cursor_peek(c));
		return -1;
	}

	if (cursor_remaining(c) == 0) {
		fprintf(stderr, "%zu:%zu: Error: unclosed list\n", c->line, c->col);
		return -1;
	}

	if (array_push(&tokens, ((Token){.kind = TOKEN_RBRACKET,
					 .start = cursor_current(c),
					 .len = 1,
					 .indent = array_back(&indents)}))) {
		return -1;
	};
	cursor_advance(c, 1);

	// skip trailing whitespace
	while (cursor_remaining(c) > 0 && cursor_peek(c) == ' ') {
		cursor_advance(c, 1);
	}

	if (cursor_peek(c) == '#') {
		cursor_skip_line(c);
		return 0;
	}

	if (cursor_remaining(c) == 0 || cursor_peek(c) != '\n') {
		fprintf(stderr, "%zu:%zu: Error: no newline after ']'\n", c->line, c->col);
		return -1;
	}
	cursor_advance(c, 1);
	return 0;
}

int tokenizer_tokenize_line(Cursor *c) {
	// find YAML start marker
	if (str_starts_with(cursor_current(c), cursor_remaining(c), "---", strlen("---"))) {
		while (indents.len > 1) {
			tokenzier_dedent(c, &tokens, &indents);
		}
		if (array_push(&tokens, ((Token){
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
	if (str_starts_with(cursor_current(c), cursor_remaining(c), "...", strlen("..."))) {
		while (indents.len > 1) {
			tokenzier_dedent(c, &tokens, &indents);
		}
		if (array_push(&tokens, ((Token){
					    .kind = TOKEN_END,
					    .start = cursor_current(c),
					    .len = 3,
					    .indent = 0,
					}))) {
			return -1;
		};
		cursor_advance(c, cursor_remaining(c));
		return 0;
	}

	size_t pos = str_first_not_of(cursor_current(c), cursor_remaining(c), WS);
	if (pos == NPOS || cursor_current(c)[pos] == '#' || cursor_current(c)[pos] == '\n') {
		cursor_skip_line(c);
		return 0;
	}

	// track possible indentation changes
	if (pos != (size_t)array_back(&indents)) {
		if (array_back(&indents)) {
			tokenizer_indent(c, &tokens, pos, &indents);
		} else {
			while (pos != (size_t)array_back(&indents)) {
				if (pos < (size_t)array_back(&indents)) {
					tokenzier_dedent(c, &tokens, &indents);
				} else {
					fprintf(stderr, "%zu:%zu: Error: bad indent\n", c->line,
						c->col);
					return -1;
				}
			}
		}
	}

	cursor_advance(c, pos);

	while (cursor_remaining(c) != 0) {
		if (cursor_peek(c) == '\n') {
			cursor_advance(c, 1);
			return 0;
		}

		if (cursor_peek(c) == '[') {
			return tokenizer_tokenize_list(c);
		}

		if (cursor_peek(c) == '"') {
			if (tokenizer_tokenize_str(c, '"')) {
				return -1;
			}
			continue;
		}

		if (cursor_peek(c) == '|') {
			if (array_push(&tokens, ((Token){.kind = TOKEN_PIPE,
							 .start = cursor_current(c),
							 .len = 1,
							 .indent = array_back(&indents)}))) {
				return -1;
			};
			cursor_advance(c, 1);
			if (cursor_remaining(c) == 0 || cursor_peek(c) != '\n') {
				fprintf(stderr, "%zu:%zu: Error: newline expected\n", c->line,
					c->col);
				return -1;
			}
			cursor_advance(c, 1);
			if (cursor_remaining(c) == 0 || cursor_peek(c) != ' ') {
				fprintf(stderr, "%zu:%zu: Error: indent expected\n", c->line,
					c->col);
				return -1;
			}
			if (tokenizer_tokenize_bare_block(c, indents.items[indents.len - 1])) {
				return -1;
			}
			continue;
		}

		if (cursor_peek(c) == '#') {
			cursor_skip_line(c);
			return 0;
		}

		if (is_separator(cursor_current(c), cursor_remaining(c), ':')) {
			if (array_push(&tokens, ((Token){.kind = TOKEN_COLON,
							 .start = cursor_current(c),
							 .len = 1,
							 .indent = array_back(&indents)}))) {
				return -1;
			};
			cursor_advance(c, 1);
			size_t pos = str_first_not_of(cursor_current(c), cursor_remaining(c), WS);
			if (pos == NPOS || cursor_current(c)[pos] == '\n') {
				cursor_skip_line(c);
				return 0;
			}
			cursor_advance(c, pos);
			continue;
		}

		if (tokenizer_tokenize_bare_str(c)) {
			return -1;
		}
	}
	return 0;
}

int tokenizer_tokenize(Cursor *c) {
	while (cursor_remaining(c) != 0) {
		if (tokenizer_tokenize_line(c)) {
			return -1;
		}
	}
	return 0;
}

int read_input_file(char **buf, size_t *len, char *input_file_name, FILE **input_file) {
	*input_file = fopen(input_file_name, "r");
	if (!input_file) {
		fprintf(stderr, "Error opening \"%s\"\n. Exit.", input_file_name);
		return 1;
	}

	if (!buf) {
		fprintf(stderr, "Error allocating memory. Exit.");
		return 1;
	}

	*len = fread(*buf, sizeof(char), 512, *input_file);
	if (ferror(*input_file)) {
		fprintf(stderr, "Error reading \"%s\". Exit.", input_file_name);
		return 1;
	}
	(*buf)[*len] = '\0';

	return 0;
}

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Error: expected exactly one input path");
		return -1;
	}
	char *input_file_name = *++argv;
	FILE *input_file = NULL;

	char *buf = calloc(513, sizeof(char));
	size_t len = 0;

	if (read_input_file(&buf, &len, input_file_name, &input_file)) {
		free(buf);
		fclose(input_file);
		return -1;
	};

	Cursor cursor = {.data = buf, .len = len, .pos = 0, .line = 1, .col = 1};
	tokenizer_tokenize(&cursor);

	// clang-format off
	const char *token_kind_str[] = {
	    [TOKEN_STRING]	= "STRING",
	    [TOKEN_COLON]	= "COLON",
	    [TOKEN_COMMA]	= "COMMA",
	    [TOKEN_DEDENT]	= "DEDENT",	  
	    [TOKEN_END]		= "END",
	    [TOKEN_INDENT]	= "INDENT",	   
	    [TOKEN_LBRACKET]	= "LBRACKET",
	    [TOKEN_PIPE]	= "PIPE",
	    [TOKEN_RBRACKET]	= "RBRACKET", 
	    [TOKEN_START]	= "START",
	};
	// clang-format on

	for (size_t i = 0; i < tokens.len; i++) {
		Token t = tokens.items[i];
		printf("%-10s indent=%zu \"%.*s\"\n", token_kind_str[t.kind], t.indent, (int)t.len,
		       t.start);
	}

	free(buf);
	fclose(input_file);

	return 0;
}
