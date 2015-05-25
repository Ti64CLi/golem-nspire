#ifndef parser_h
#define parser_h

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include <lexic/lexer.h>
#include <core/mem.h>
#include <core/api.h>
#include <ast.h>

typedef struct
{
    lexer_t lexer;
    token_t* buffer;
    size_t num_tokens;
    size_t cursor;

    int error;
} parser_t;

void parser_init(parser_t* parser);
ast_t* parser_run(parser_t* parser, const char* content);
void parser_free(parser_t* parser);
int parser_error(parser_t* parser);
#endif
