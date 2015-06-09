#ifndef compiler_h
#define compiler_h

#include <stdio.h>
#include <stdlib.h>
#include <core/mem.h>
#include <core/value.h>
#include <parser/parser.h>
#include <compiler/bytecode.h>

typedef struct compiler_t
{
	parser_t parser;
	list_t* buffer;
} compiler_t;

list_t* compile_buffer(compiler_t* compiler, const char* source);
list_t* compile_file(compiler_t* compiler, const char* filename);
void compiler_clear(compiler_t* compiler);

#endif
