#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "parser.h"
#include "tokenizer.h"

int parser_parse_element(TokenArray* token_arr, size_t* pos, TayNode* out) {
    TayToken curr_token = token_arr->items[*pos];
    TayToken next_curr_token = token_arr->items[*pos + 1];

    if (curr_token.kind == TOKEN_INDENT) {
        // handle indent
        (*pos)++;
        if (parser_parse_element(token_arr, pos, out)) {
            return -1;
        }
        // handle dedent
        (*pos)++;
        return 0;
    }

    if (curr_token.kind == TOKEN_DASH) {
        return parser_parse_list(token_arr, pos, out);
    }

    if (token_arr->len > 2 && curr_token.kind == TOKEN_STRING &&
        next_curr_token.kind == TOKEN_COLON) {
        return parser_parse_map(token_arr, pos, out);
    }

    return parser_parse_flow_element(token_arr, pos, out);
}

int parser_parse_flow_element(TokenArray* token_arr, size_t* pos, TayNode* out) {
    TayToken curr_token = token_arr->items[*pos];

    if (curr_token.kind == TOKEN_LBRACKET) {
        // handle opening bracket
        (*pos)++;
        return parser_parse_flow_list(token_arr, pos, out);
        // handle closing bracket
        (*pos)++;
    }

    if (curr_token.kind != TOKEN_STRING) {
        fprintf(stderr, "Error: string expected\n");
        return -1;
    }

    *out = ((TayNode){
        .kind = TAY_STRING,
        .string =
            (TayString){
                .str = curr_token.start,
                .len = curr_token.len,
            },
    });

    (*pos)++;

    return 0;
}

int parser_parse_flow_list(TokenArray* token_arr, size_t* pos, TayNode* out) {
    TayToken curr_token = token_arr->items[*pos];

    out->kind = TAY_LIST;
    out->list.items = NULL;
    out->list.cap = 0;
    out->list.len = 0;

    while (curr_token.kind != TOKEN_RBRACKET && curr_token.kind != TOKEN_END) {
        array_push(&out->list, (TayNode){0});
        if (parser_parse_flow_element(token_arr, pos, &out->list.items[out->list.len - 1])) {
            return -1;
        }
        if (curr_token.kind == TOKEN_LBRACKET) {
            break;
        }
        if (curr_token.kind != TOKEN_COMMA) {
            fprintf(stderr, "Error: comma expected\n");
            return -1;
        }
        // adance past comma token
        (*pos)++;
    }

    if (curr_token.kind == TOKEN_END) {
        fprintf(stderr, "Error: unterminated flow list\n");
        return -1;
    }

    return 0;
}

int parser_parse_map(TokenArray* token_arr, size_t* pos, TayNode* out) {
    TayToken curr_token = token_arr->items[*pos];

    out->kind = TAY_MAP;
    out->map.entries = NULL;
    out->map.cap = 0;
    out->map.len = 0;

    while (curr_token.kind != TOKEN_END && curr_token.kind != TOKEN_DEDENT) {
        if (token_arr->len < 2 || curr_token.kind != TOKEN_STRING ||
            token_arr->items[(*pos)++].kind != TOKEN_COLON) {
            fprintf(stderr, "Error: expected valid map key\n");
            return -1;
        }

        // advance past colon token and indent
        (*pos) += 2;

        array_push(&out->list, (TayNode){0});
        if (parser_parse_element(token_arr, pos, &out->map.entries[out->map.len - 1])) {
            return -1;
        }
    }

    return -1;
};

int parser_parse_list(TokenArray* token_arr, size_t* pos, TayNode* out) {
    TayToken curr_token = token_arr->items[*pos];

    out->kind = TAY_LIST;
    out->list.items = NULL;
    out->list.cap = 0;
    out->list.len = 0;

    while (curr_token.kind != TOKEN_END && curr_token.kind != TOKEN_DEDENT) {
        if (curr_token.kind != TOKEN_DASH) {
            fprintf(stderr, "Error: expected list element\n");
            return -1;
        }
        // adance past dash token
        (*pos)++;

        array_push(&out->list, (TayNode){0});
        if (parser_parse_element(token_arr, pos, &out->list.items[out->list.len - 1])) {
            return -1;
        }

        return -1;
    };

    return 0;
}
