#ifndef lexer_h
#define lexer_h

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <core/mem.h>
#include <core/util.h>

typedef struct
{
    unsigned line;
    unsigned column;
} location_t;

typedef struct
{
    location_t location;
    const char* source;
    const char* cursor;
    const char* lastline;
    int error;
    int eof;
} lexer_t;

typedef enum
{
    TOKEN_NEWLINE,
    TOKEN_SPACE,
    TOKEN_WORD,
    TOKEN_STRING,
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_BOOL,
    TOKEN_LPAREN,    // '('
    TOKEN_RPAREN,    // ')'
    TOKEN_LBRACE,    // '{'
    TOKEN_RBRACE,    // '}'
    TOKEN_LBRACKET,  // '['
    TOKEN_RBRACKET,  // ']'
    TOKEN_COMMA,     // ','
    TOKEN_SEMICOLON, // ';'
    TOKEN_ADD,       // '+' OP
    TOKEN_SUB,       // '-' OP
    TOKEN_MUL,       // '*' OP
    TOKEN_DIV,       // '/' OP
    TOKEN_MOD,       // '%' OP
    TOKEN_EQUAL,     // '==' OP
    TOKEN_ASSIGN,    // '=' OP
    TOKEN_NEQUAL,    // '!=' OP
    TOKEN_NOT,       // '!' OP
    TOKEN_DOT,       // '.'
    TOKEN_BITLSHIFT, // '<<' OP
    TOKEN_BITRSHIFT, // '>>' OP
    TOKEN_LEQUAL,    // '<=' OP
    TOKEN_GEQUAL,    // '>=' OP
    TOKEN_LESS,      // '<' OP
    TOKEN_GREATER,   // '>' OP
    TOKEN_AND,       // '&&' OP
    TOKEN_OR,        // '||' OP
    TOKEN_BITAND,    // '&' OP
    TOKEN_BITOR,     // '|' OP
    TOKEN_BITXOR,    // '^' OP
    TOKEN_BITNOT     // '~' OP
} token_type_t;

typedef struct
{
    location_t location;
    token_type_t type;
    char* value;
} token_t;

void lexer_init(lexer_t* lexer);
token_t* lexer_lex(lexer_t* lexer, const char* src, size_t* numTokens);
void lexer_print_tokens(token_t* tokens, size_t n);
void lexer_free_buffer(token_t* buffer, size_t n);

#endif
