#ifndef PARSER_H

#define PARSER_H

#include <stdbool.h>
#include <stddef.h>

#include "tokenizer.h"

typedef enum {
    TAY_STRING,
    TAY_BLOCK_STRING,
    TAY_LIST,
    TAY_MAP,
} TayNodeKind;

typedef struct tay_string {
    char*  str;
    size_t len;
} TayString;

typedef struct tay_node {
    TayNodeKind kind;
    TayString   key;
    size_t      len;
    union {
        TayString string;
        struct {
            struct tay_node* items;
            size_t           len;
            size_t           cap;
        } list;
        struct {
            struct tay_node* items;
            size_t           len;
            size_t           cap;
        } map;
    };
} TayNode;

int parser_parse_element(TokenArray* token_arr, size_t* pos, TayNode* out);
int parser_parse_map(TokenArray* token_arr, size_t* pos, TayNode* out);
int parser_parse_list(TokenArray* token_arr, size_t* pos, TayNode* out);
int parser_parse_flow_list(TokenArray* token_arr, size_t* pos, TayNode* out);
int parser_parse_flow_element(TokenArray* token_arr, size_t* pos, TayNode* out);

#endif
