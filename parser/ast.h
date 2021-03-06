/**
 * ast.h
 * Copyright (C) 2017 Alexander Koch
 * Abstract syntax tree (AST) representation
 *
 * Used for storing expressions in a tree-like structure
 * and converting them to final bytecode by the compiler.
 * Also the datatypes and the annotation types are defined here.
 */

#ifndef ast_h
#define ast_h

#include <stddef.h>
#include <stdbool.h>

#include "../lexis/lexer.h"
#include "../adt/list.h"
#include "../core/mem.h"
#include "../adt/hashmap.h"
#include "../parser/types.h"

typedef struct ast_s ast_t;

// Annotation types
// @Getter
// @Setter
// @Unused
typedef enum {
    ANN_GETTER = 1 << 1,
    ANN_SETTER = 1 << 2,
    ANN_UNUSED = 1 << 3
} annotation_t;

// ast_class_t
// Enumerates all the possible classes for an AST.
// AST_NULL       -> stores literally nothing, means it is invalid
// AST_IDENT      -> stores an identifier
// AST_FLOAT      -> stores a floating point value
// AST_INT        -> stores an integer value
// AST_BOOL       -> stores a boolean value
// AST_STRING     -> stores a string of characters
// AST_CHAR       -> stores a single character
// AST_ARRAY      -> stores an array structure
// AST_BINARY     -> stores a binary tree structure / operation
// AST_UNARY      -> stores a unary operation
// AST_SUBSCRIPT  -> stores a subscript access
// AST_CALL       -> stores a function call
// AST_DECLVAR    -> stores a variable declaration
// AST_DECLFUNC   -> stores a function definition
// AST_IF         -> stores a ifclauses / an if-statement
// AST_IFCLAUSE   -> stores a sub-if-clause, can also be else if there is no condition
// AST_WHILE      -> stores a while loop
// AST_IMPORT     -> stores an import / using statement
// AST_CLASS      -> stores a class
// AST_RETURN     -> stores a return statement
// AST_BLOCK      -> stores a list of ASTs
// AST_ANNOTATION -> stores an annotation
// AST_NONE       -> stores an option None type
typedef enum {
    AST_NULL,
    AST_IDENT,
    AST_FLOAT,
    AST_INT,
    AST_BOOL,
    AST_STRING,
    AST_CHAR,
    AST_ARRAY,
    AST_BINARY,
    AST_UNARY,
    AST_SUBSCRIPT,
    AST_CALL,
    AST_DECLVAR,
    AST_DECLFUNC,
    AST_IF,
    AST_IFCLAUSE,
    AST_WHILE,
    AST_IMPORT,
    AST_CLASS,
    AST_RETURN,
    AST_BLOCK,
    AST_ANNOTATION,
    AST_NONE,
} ast_class_t;

// Condition struct
typedef struct {
    ast_t* cond;
    list_t* body;
} ast_cond_t;

// Field index struct
typedef struct {
    ast_t* key;
    ast_t* expr;
} ast_field_t;

// Anonymous function struct (lambda)
typedef struct {
    list_t* formals;
    list_t* body;
} ast_lambda_t;

// Function struct (inherits lambda)
typedef struct {
    char* name;
    ast_lambda_t impl;
    datatype_t* rettype;
    int external;   // external syscall index, true if external (index > 0)
    int dynamic;    // function name is dynamically allocated
} ast_func_t;

// Declaration struct
typedef struct {
    char* name;
    bool mutate;
    ast_t* initializer;
    datatype_t* type;
} ast_decl_t;

// Class struct
typedef struct {
    char* name;
    list_t* body;
    list_t* formals;
    hashmap_t* fields;
} ast_struct_t;

typedef struct {
    datatype_t* type;
} ast_none_t;

// Structure of an AST-node
struct ast_s {
    ast_class_t class;
    location_t location;
    union {
        char* ident;
        char* string;
        char* import;
        char ch;
        list_t* block;
        list_t* ifstmt;
        int i;
        double f;
        bool b;
        ast_t* returnstmt;
        ast_field_t subscript;
        ast_func_t funcdecl;
        ast_decl_t vardecl;
        ast_cond_t ifclause;
        ast_cond_t whilestmt;
        ast_struct_t classstmt;
        annotation_t annotation;
        ast_none_t none;

        struct {
            list_t* elements;
            datatype_t* type;
        } array;

        struct {
            token_type_t op;
            ast_t* left;
            ast_t* right;
        } binary;

        struct {
            token_type_t op;
            ast_t* expr;
        } unary;

        struct {
            ast_t* callee;
            list_t* args;
        } call;
    };
};

#define ast_is_number(x) (x->class == AST_FLOAT || x->class == AST_INT)

// Main functions for AST-creation
ast_t* ast_class_create(ast_class_t class, location_t location);
const char* ast_classname(ast_class_t class);
void ast_free(ast_t* ast);

void ast_dump(ast_t* node, int level);

#endif
