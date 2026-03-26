#include <stdio.h>
#include <stdlib.h>

#include "../src/tokenizer.h"

// clang-format off
static const char *token_kind_str[] = {
    [TOKEN_STRING]	= "STRING",
    [TOKEN_COLON]	= "COLON",
    [TOKEN_COMMA]	= "COMMA",
    [TOKEN_DASH]	= "DASH",
    [TOKEN_DEDENT]	= "DEDENT",
    [TOKEN_END]		= "END",
    [TOKEN_INDENT]	= "INDENT",
    [TOKEN_LBRACKET]	= "LBRACKET",
    [TOKEN_PIPE]	= "PIPE",
    [TOKEN_RBRACKET]	= "RBRACKET",
    [TOKEN_START]	= "START",
};
// clang-format on

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Error: expected exactly one input path");
        return -1;
    }
    char* input_file_name = *++argv;

    FILE* input_file = fopen(input_file_name, "r");
    if (!input_file) {
        fprintf(stderr, "Error opening \"%s\"\n. Exit.", input_file_name);
        return 1;
    }

    char* buf = calloc(513, sizeof(char));
    if (!buf) {
        fprintf(stderr, "Error allocating memory. Exit.");
        fclose(input_file);
        return 1;
    }

    size_t len = fread(buf, sizeof(char), 512, input_file);
    if (ferror(input_file)) {
        fprintf(stderr, "Error reading \"%s\". Exit.", input_file_name);
        free(buf);
        fclose(input_file);
        return 1;
    }
    buf[len] = '\0';
    fclose(input_file);

    Cursor            cursor = {.data = buf, .len = len, .pos = 0, .line = 1, .col = 1};
    static TokenArray tokens = {.len = 0, .cap = 0, .items = NULL};
    if (tokenizer_tokenize(&cursor, &tokens)) {
        free(buf);
        return -1;
    };

    for (size_t i = 0; i < tokens.len; i++) {
        Token t = tokens.items[i];
        printf("%-10s indent=%zu \"%.*s\"\n", token_kind_str[t.kind], t.indent, (int)t.len,
               t.start);
    }

    free(buf);
    return 0;
}
