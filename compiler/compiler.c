#include "compiler.h"

/**
 * compiler_throw:
 * If an error occurs, this function can be used as an exception.
 * It halts the compilation and prints an error message to the console.
 * The parameter @node is needed to print the location in the source code that
 * contains the error.
 */
void compiler_throw(compiler_t* compiler, ast_t* node, const char* format, ...) {
    compiler->error = true;
    location_t loc = node->location;
    printf("%s:%d:%d (Semantic): ", compiler->parser->name, loc.line, loc.column);
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stdout, format, argptr);
    va_end(argptr);
    putchar('\n');
}

datatype_t compiler_eval(compiler_t* compiler, ast_t* node);

/**
 * push_scope:
 * Create a new scope to work on.
 * This will create a subscope and mounts it to the parent scope.
 * Symbols, classes, etc. can be stored.
 */
void push_scope(compiler_t* compiler, ast_t* node) {
    // Create scope
    scope_t* scope = scope_new();
    scope->super = compiler->scope;
    scope->node = node;

    // Register new scope in parent
    list_push(compiler->scope->subscopes, scope);

    // Set child active scope
    compiler->scope = scope;
    compiler->depth++;
    compiler->scope->address = 0;
}

void push_scope_virtual(compiler_t* compiler, ast_t* node) {
    // Create scope
    scope_t* scope = scope_new();
    scope->super = compiler->scope;
    scope->node = node;
    scope->address = scope->super->address;
    scope->virtual = true;

    // Register new scope in parent
    list_push(compiler->scope->subscopes, scope);

    // Set child active scope
    compiler->scope = scope;
}

void pop_scope_virtual(compiler_t* compiler) {
    scope_t* super = compiler->scope->super;
    compiler->scope = super;
}

/**
 * pop_scope:
 * Move from current scope to the parent scope (super scope).
 */
void pop_scope(compiler_t* compiler) {
    // Get parent scope
    scope_t* super = compiler->scope->super;

    // Set parent scope to active
    compiler->scope = super;
    compiler->depth--;
}

/**
 * scope_is_class:
 * Test if the current scope is within a class.
 */
bool scope_is_class(scope_t* scope, ast_class_t class, ast_t** node) {
    if(scope->node && scope) {
        if(scope->node->class == class) {
            (*node) = scope->node;
            return true;
        }
        return scope_is_class(scope->super, class, node);
    }
    return false;
}

/**
 * symbol_new:
 * Stores an indentifier with an address, an AST and a type
 * If the scope depth is zero, the  global flag is set to true
 *
 * @node The corresponding AST-node
 * @address Bytecode address
 * @type Datatype
 * @global If GSTORE or STORE is used (scope level is equal to zero)
 * @isClassParam Self-explanatory
 * @ref Referenced by a class (hold the class symbol)
 * @arraySize Array size of the symbol (if it is of type array)
 */
symbol_t* symbol_new(compiler_t* compiler, ast_t* node, int address, datatype_t type) {
    symbol_t* symbol = malloc(sizeof(*symbol));
    symbol->node = node;
    symbol->address = address;
    symbol->type = type;
    symbol->global = (compiler->depth == 0);
    symbol->isClassParam = false;
    symbol->ref = 0;
    symbol->arraySize = -1;
    return symbol;
}

/**
 * symbol_get_ext:
 * Tries to find a symbol by the given identifier.
 * Returns the symbol if found, otherwise NULL.
 * Depth returns the number of scopes it had to go upwards.
 */
symbol_t* symbol_get_ext(scope_t* scope, char* ident, int* depth) {
    // If found, return
    void* val = hashmap_find(scope->symbols, ident);
    if(val) return val;

    // If there is a super scope, search in that
    if(scope->super) {
        if(!scope->virtual)(*depth)++;
        return symbol_get_ext(scope->super, ident, depth);
    }
    return 0;
}

/**
 * symbol_get:
 * Tries to find a symbol by the given identifier.
 * This function works recursive and searches upwards,
 * until there is no parent available.
 * Returns the symbol if found, otherwise NULL.
 */
symbol_t* symbol_get(scope_t* scope, char* ident) {
    void* val = hashmap_find(scope->symbols, ident);
    if(val) return val;
    if(scope->super) return symbol_get(scope->super, ident);
    return 0;
}

/**
 * cmpId:
 * Small helper function for comparing IDs.
 * Tests if given symbol id and the id match.
 * Actual definition:
 * cmpdId(symbol_t symbol, unsigned long id)
 * Returns 1 if ids are the same.
 */
static symbol_t* tmp = 0;
int cmpId(void* val, void* arg) {
    unsigned long* id = arg;
    symbol_t* symbol = val;
    if(symbol->type.id == *id) {
        tmp = symbol;
        return 1;
    }
    return 0;
}

/**
 * class_find:
 * Look in the scopes class hashmap for the given class id.
 * If it fails, it will look in super scope.
 */
symbol_t* class_find(scope_t* scope, unsigned long id) {
    if(hashmap_foreach(scope->classes, cmpId, &id)) return tmp;
    if(scope->super) return class_find(scope->super, id);
    return 0;
}

/**
 * symbol_exists:
 * Test if given identifier exists in the symbol-table.
 * If so, an exception is thrown and true returned.
 * @param node The current node we are visiting.
 */
bool symbol_exists(compiler_t* compiler, ast_t* node, char* ident) {
    symbol_t* symbol = symbol_get(compiler->scope, ident);
    if(symbol) {
        location_t loc = symbol->node->location;
        compiler_throw(compiler, node,
            "Redefinition of symbol '%s'.\nPrevious definition: [line %d column %d]",
            ident, loc.line, loc.column);
        return true;
    }
    return false;
}

/**
 * symbol_replace:
 * Used to emit a replacement operation for a given symbol (store).
 * Tries to get the symbol, and emits a store operation based on the depth
 */
void symbol_replace(compiler_t* compiler, symbol_t* symbol) {
    // Get the depth
    int depth = 0;
    symbol_get_ext(compiler->scope, symbol->node->vardecl.name, &depth);

    // Global?
    if(depth == 0 || symbol->global) {
        emit_store(compiler->buffer, symbol->address, symbol->global);
    }
    // Up!
    else {
        emit_store_upval(compiler->buffer, depth, symbol->address);
    }
}

/**
 * eval_block:
 * Evaluate a list of abstract syntax trees.
 * The evaluated datatype of the last entry is returned.
 * If there is no entry, datatype_new(DATA_NULL) is returned.
 */
datatype_t eval_block(compiler_t* compiler, list_t* block) {
    datatype_t ret = datatype_new(DATA_NULL);
    list_iterator_t* iter = list_iterator_create(block);
    while(!list_iterator_end(iter)) {
        // Evaluate each list item.
        ret = compiler_eval(compiler, list_iterator_next(iter));
    }
    list_iterator_free(iter);
    return ret;
}

/**
 * eval_declfunc:
 * Compiles / evaluates a function declaration.
 *
 * Algorithm: (simplified)
 * 1. Check the symbols.
 * 2. Create a jump at the beginning to prevent runtime execution.
 * 3. Register signature.
 * 4. Create a new scope to evaluate it's content.
 * 5. Create parameter symbols.
 * 6. Analyse body.
 * 7. Emit return.
 * 8. Set jump address.
 */
datatype_t eval_declfunc(compiler_t* compiler, ast_t* node) {
    // Check if existing and non-external
    if(symbol_exists(compiler, node, node->funcdecl.name)) return datatype_new(DATA_NULL);

    // If it is external, just register the symbol.
    if(node->funcdecl.external) {
        symbol_t* sym = symbol_new(compiler, node, -1, node->funcdecl.rettype);
        hashmap_set(compiler->scope->symbols, node->funcdecl.name, sym);
        return datatype_new(DATA_NULL);
    }

    // Add a jump, set the position at the end
    val_t* addr = emit_jmp(compiler->buffer, 0);

    // Register a symbol for the function, save the bytecode address
    int byte_address = vector_size(compiler->buffer);
    symbol_t* fnSymbol = symbol_new(compiler, node, byte_address, datatype_new(DATA_LAMBDA));
    hashmap_set(compiler->scope->symbols, node->funcdecl.name, fnSymbol);

    // Create a new scope and create the parameter-variables.
    // Treat each parameter as a local variable, with no type or value
    push_scope(compiler, node);
    list_iterator_t* iter = list_iterator_create(node->funcdecl.impl.formals);
    int i = -(list_size(node->funcdecl.impl.formals) + 3);
    while(!list_iterator_end(iter)) {
        // Create parameter in symbols list
         ast_t* param = list_iterator_next(iter);

        if(symbol_exists(compiler, param, param->vardecl.name)) return datatype_new(DATA_NULL);
        symbol_t* symbol = symbol_new(compiler, param, i, param->vardecl.type);
        hashmap_set(compiler->scope->symbols, param->vardecl.name, symbol);
        i++;
    }
    list_iterator_free(iter);

    // Body analysis
    iter = list_iterator_create(node->funcdecl.impl.body);
    bool hasReturn = false;
    while(!list_iterator_end(iter)) {
        ast_t* sub = list_iterator_next(iter);
        if(sub->class == AST_RETURN && !list_iterator_end(iter)) {
            compiler_throw(compiler, node, "Return statement declared before end was reached");
            break;
        }

        if(sub->class == AST_RETURN) {
            hasReturn = true;
            if(sub->returnstmt && node->funcdecl.rettype.type == DATA_VOID) {
                compiler_throw(compiler, node, "Functions with type void do not return a value");
                break;
            }
            else if(!sub->returnstmt && node->funcdecl.rettype.type != DATA_VOID) {
                compiler_throw(compiler, node, "Return statement without a value");
                break;
            }
        }
        compiler_eval(compiler, sub);
    }

    list_iterator_free(iter);
    pop_scope(compiler);

    // Handle void return
    if(node->funcdecl.rettype.type == DATA_VOID) {
        // Just return 0 if void
        // (as needed by retvirtual / return)
        emit_int(compiler->buffer, 0);
        ast_t* refNode = 0;
        if(scope_is_class(compiler->scope, AST_CLASS, &refNode)) {
            emit_op(compiler->buffer, OP_RETVIRTUAL);
        }
        else {
            emit_return(compiler->buffer);
        }
    }
    else {
        if(!hasReturn) {
            compiler_throw(compiler, node, "Warning: Function without return statement");
        }
    }

    // Set beginning jump address to end
    byte_address = vector_size(compiler->buffer);
    *addr = INT32_VAL(byte_address);

    return datatype_new(DATA_LAMBDA);
}

/**
 * eval_declvar:
 * The function evaluates a variable declaration.
 * This registers a new symbol with the name
 * and the current available address.
 * The return value is NULL.
 */
datatype_t eval_declvar(compiler_t* compiler, ast_t* node) {
    // First check the unused annotation
    if(scope_requests(compiler->scope, ANN_UNUSED)) {
        scope_unflag(compiler->scope);
        return datatype_new(DATA_NULL);
    }

    // Check if already in existance
    if(symbol_exists(compiler, node, node->vardecl.name)) return datatype_new(DATA_NULL);

    // First eval initializer to get type
    datatype_t vartype = compiler_eval(compiler, node->vardecl.initializer);

    // Then test for non-valid types
    if(vartype.type == DATA_VOID) {
        compiler_throw(compiler, node, "Variable initializer is of type VOID");
        return datatype_new(DATA_NULL);
    }
    else if(vartype.type == DATA_NULL) {
        compiler_throw(compiler, node, "Variable initializer is NULL");
        return datatype_new(DATA_NULL);
    }
    else if(vartype.type == DATA_LAMBDA) {
        compiler_throw(compiler, node, "Trying to assign a function to a value (Currently not supported)");
        return datatype_new(DATA_NULL);
    }

    // Verify if within a class
    if(compiler->scope->node) {
        ast_t* nd = compiler->scope->node;
        if(nd->class == AST_CLASS) {
            // Attribute of class
            symbol_t* class = symbol_get(compiler->scope, nd->classstmt.name);
            if(!class) {
                compiler_throw(compiler, node, "The attributes class is not found");
                return datatype_new(DATA_NULL);
            }

            if(vartype.id == class->type.id) {
                compiler_throw(compiler, node, "Circular reference");
                return datatype_new(DATA_NULL);
            }
        }
    }

    // Store the symbol, increase the scope address
    symbol_t* symbol = symbol_new(compiler, node, compiler->scope->address++, vartype);
    symbol->node->vardecl.type = vartype;
    hashmap_set(compiler->scope->symbols, node->vardecl.name, symbol);

    // Array secure size information
    if((vartype.type & DATA_ARRAY) == DATA_ARRAY) {
        // What if variable assigns to variables that points to an array?
        // 1. variable is identifier
        // 2. identifier points to symbol
        // 3. symbol points to array
        // 4. array has the elements and therefore the size

        // HACK:(#1) using bytecodes to set the array size
        instruction_t* instr = vector_top(compiler->buffer);
        if(instr->op == OP_ARR || instr->op == OP_STR) {
            int sz = AS_INT32(instr->v1);
            symbol->arraySize = sz;
        }
    }

    // Emit bytecode
    emit_store(compiler->buffer, symbol->address, symbol->global);

    // Debug variables if flag is set
#ifdef DB_VARS
    printf("Created %s variable '%s' of data type <%s>\n", node->vardecl.mutate ? "mutable" : "immutable", node->vardecl.name, datatype2str(vartype));
#endif
    return datatype_new(DATA_NULL);
}

/**
 * eval_number:
 * The function evaluates a number.
 * The number can only be of type float or integer.
 * Emits only one push instruction
 */
datatype_t eval_number(compiler_t* compiler, ast_t* node) {
    if(node->class == AST_FLOAT) {
        emit_float(compiler->buffer, node->f);
        return datatype_new(DATA_FLOAT);
    }

    emit_int(compiler->buffer, node->i);
    return datatype_new(DATA_INT);
}

/**
 * eval_bool:
 * As the name says, evaluates a boolean node.
 * Emits a push instruction.
 */
datatype_t eval_bool(compiler_t* compiler, ast_t* node) {
    emit_bool(compiler->buffer, node->b);
    return datatype_new(DATA_BOOL);
}

// Eval.binary(node)
// This function evaluates a binary node.
// A binary node consists of two seperate nodes
// connected with an operator.
// The contents will be optimized.
// ---------------
// Example:
// bin(a, b, +) -> a + b
// bin(a, bin(b, c, *), +) -> a + (b * c)
datatype_t eval_binary(compiler_t* compiler, ast_t* node)
{
    ast_t* lhs = node->binary.left;
    ast_t* rhs = node->binary.right;
    token_type_t op = node->binary.op;

    // Integer check -> first optimization pass
    if(lhs->class == AST_INT && rhs->class == AST_INT) {
        switch(op) {
            case TOKEN_ADD: lhs->i += rhs->i; break;
            case TOKEN_SUB: lhs->i -= rhs->i; break;
            case TOKEN_MUL: lhs->i *= rhs->i; break;
            case TOKEN_DIV: lhs->i /= rhs->i; break;
            case TOKEN_MOD: lhs->i %= rhs->i; break;
            case TOKEN_BITLSHIFT: lhs->i <<= rhs->i; break;
            case TOKEN_BITRSHIFT: lhs->i >>= rhs->i; break;
            case TOKEN_BITAND: lhs->i %= rhs->i; break;
            case TOKEN_BITOR: lhs->i |= rhs->i; break;
            case TOKEN_BITXOR: lhs->i ^= rhs->i; break;
            case TOKEN_EQUAL: {
                lhs->class = AST_BOOL;
                lhs->b = (lhs->i == rhs->i);
                break;
            }
            case TOKEN_NEQUAL: {
                lhs->class = AST_BOOL;
                lhs->b = (lhs->i != rhs->i);
                break;
            }
            case TOKEN_LEQUAL: {
                lhs->class = AST_BOOL;
                lhs->b = (lhs->i <= rhs->i);
                break;
            }
            case TOKEN_GEQUAL: {
                lhs->class = AST_BOOL;
                lhs->b = (lhs->i >= rhs->i);
                break;
            }
            case TOKEN_LESS: {
                lhs->class = AST_BOOL;
                lhs->b = (lhs->i < rhs->i);
                break;
            }
            case TOKEN_GREATER: {
                lhs->class = AST_BOOL;
                lhs->b = (lhs->i > rhs->i);
                break;
            }
            default: {
                compiler_throw(compiler, node, "Invalid operator. Operator might not be available for integers");
                return datatype_new(DATA_NULL);
            }
        }

        return compiler_eval(compiler, lhs);
    }

    // Float checking -> second optimization pass
    if(lhs->class == AST_FLOAT && rhs->class == AST_FLOAT) {
        switch(op) {
            case TOKEN_ADD: lhs->f += rhs->f; break;
            case TOKEN_SUB: lhs->f -= rhs->f; break;
            case TOKEN_MUL: lhs->f *= rhs->f; break;
            case TOKEN_DIV: lhs->f /= rhs->f; break;
            case TOKEN_EQUAL: {
                lhs->class = AST_BOOL;
                lhs->b = (lhs->f == rhs->f);
                break;
            }
            case TOKEN_NEQUAL: {
                lhs->class = AST_BOOL;
                lhs->b = (lhs->f != rhs->f);
                break;
            }
            case TOKEN_LEQUAL: {
                lhs->class = AST_BOOL;
                lhs->b = (lhs->f <= lhs->f);
                break;
            }
            case TOKEN_GEQUAL: {
                lhs->class = AST_BOOL;
                lhs->b = (lhs->f >= lhs->f);
                break;
            }
            case TOKEN_LESS: {
                lhs->class = AST_BOOL;
                lhs->b = (lhs->f < lhs->f);
                break;
            }
            case TOKEN_GREATER: {
                lhs->class = AST_BOOL;
                lhs->b = (lhs->f > lhs->f);
                break;
            }
            default: {
                compiler_throw(compiler, node, "Invalid operator. Operator might not be available for floats");
                return datatype_new(DATA_NULL);
            }
        }

        return compiler_eval(compiler, lhs);
    }

    // Assignment operator: special case
    if(op == TOKEN_ASSIGN)
    {
        if(lhs->class == AST_IDENT)
        {
            symbol_t* symbol = symbol_get(compiler->scope, lhs->ident);
            if(symbol)
            {
                if(symbol->node->class == AST_DECLVAR)
                {
                    if(!symbol->node->vardecl.mutate)
                    {
                        compiler_throw(compiler, node, "Invalid statement, trying to modifiy an immutable variable");
                        return datatype_new(DATA_NULL);
                    }

                    if(symbol->ref)
                    {
                        // ldarg0
                        // <...>
                        // setfield
                        // setarg0

                        // Access class field, load class first
                        emit_op(compiler->buffer, OP_LDARG0);
                    }

                    datatype_t dt = compiler_eval(compiler, rhs);
                    if(!datatype_match(dt, symbol->node->vardecl.type))
                    {
                        compiler_throw(compiler, node, "Warning: Change of types is not permitted");
                        return datatype_new(DATA_NULL);
                    }

                    if(symbol->ref)
                    {
                        // Class field load from class
                        symbol_t* classRef = symbol->ref;
                        ast_t* classNode = classRef->node;

                        symbol_t* symbol = hashmap_find(classNode->classstmt.fields, lhs->ident);
                        if(!symbol) {
                            compiler_throw(compiler, node, "No such class field");
                            return datatype_new(DATA_NULL);
                        }

                        emit_class_setfield(compiler->buffer, symbol->address);
                        emit_op(compiler->buffer, OP_SETARG0);
                        return datatype_new(DATA_NULL);
                    }

                    symbol_replace(compiler, symbol);
                }
                else
                {
                    // Left-hand side must be a variable
                    // Example 5 = 4 does not make any sense
                    // Also you can't replace a function <- function is stored in a variable
                    compiler_throw(compiler, node, "Left hand side value must be a variable");
                    return datatype_new(DATA_NULL);
                }
            }
            else
            {
                // If the symbol is not found, throw an error
                compiler_throw(compiler, node, "Warning: Implicit declaration of field '%s'", lhs->ident);
                return datatype_new(DATA_NULL);
            }
        }
        // If it's not an identifier
        // e.g.: myArray[5] := 28
        else if(lhs->class == AST_SUBSCRIPT)
        {
            // Subscript: expr, key: myArray[key]
            // expr must be an identifier
            ast_t* expr = lhs->subscript.expr;
            ast_t* key = lhs->subscript.key;

            if(expr->class != AST_IDENT)
            {
                // [1,2,3,4][0] := 4
                // not permitted
                compiler_throw(compiler, node, "Warning: Identifier for index access expected");
            }
            else
            {
                symbol_t* symbol = symbol_get(compiler->scope, expr->ident);
                if(symbol)
                {
                    ast_t* symNode = symbol->node;
                    if(symNode->class != AST_DECLVAR)
                    {
                        compiler_throw(compiler, node, "Subscripts are only allowed for variables");
                    }
                    else
                    {
                        if(!symNode->vardecl.mutate)
                        {
                            compiler_throw(compiler, node, "The field '%s' is immutable", expr->ident);
                        }
                        else
                        {
                            // Example:
                            // let var = "Hello World"
                            // var[0] = "B"

                            // If it is a class, reload for modifying
                            if(symbol->ref)
                            {
                                emit_op(compiler->buffer, OP_LDARG0);
                            }

                            // If we found a subscript, it has to be an array
                            // Evaluate the rhs and lhs
                            datatype_t rhsType = compiler_eval(compiler, rhs);
                            datatype_t lhsType = compiler_eval(compiler, expr);
                            datatype_t arrType = datatype_new(lhsType.type & ~DATA_ARRAY);

                            if(!datatype_match(arrType, rhsType))
                            {
                                compiler_throw(compiler, node, "Assignment value has the wrong type");
                                return datatype_new(DATA_NULL);
                            }

                            // rhs -> int / string / anyhing but null and void
                            // lhs -> string / array otherwise error
                            // lhs -> vardecl / namespace varaible declaration

                            compiler_eval(compiler, key);
                            emit_op(compiler->buffer, OP_SETSUB);
                            //emit_store(compiler->buffer, symbol->address, symbol->global);

                            // If it is a class field, we have to reassign it to the actual field / class
                            if(symbol->ref)
                            {
                                // Emit setfield
                                symbol_t* classRef = symbol->ref;
                                ast_t* classNode = classRef->node;

                                symbol_t* symbol = hashmap_find(classNode->classstmt.fields, expr->ident);
                                if(!symbol) {
                                    compiler_throw(compiler, node, "No such class field");
                                    return datatype_new(DATA_NULL);
                                }

                                emit_class_setfield(compiler->buffer, symbol->address);
                                emit_op(compiler->buffer, OP_SETARG0);
                            }
                            else
                            {
                                symbol_replace(compiler, symbol);
                            }

                        }
                    }
                }
                else
                {
                    compiler_throw(compiler, node, "Warning: Implicit declaration of field '%s'", expr->ident);
                }
            }
            return datatype_new(DATA_NULL);
        }
        else
        {
            // Example: [1,2,3] := 5
            // Completely invalid!
            compiler_throw(compiler, node, "Unknown assignment operation");
            return datatype_new(DATA_NULL);
        }
    }
    else
    {
        // Simple binary instruction
        // Can be evaluated easier.

        // Emit node op-codes
        datatype_t lhs_type = compiler_eval(compiler, lhs);
        datatype_t rhs_type = compiler_eval(compiler, rhs);
        if(!datatype_match(lhs_type, rhs_type))
        {
            compiler_throw(compiler, node, "Cannot perform operation '%s' on the types '%s' and '%s'", tok2str(op), datatype2str(lhs_type), datatype2str(rhs_type));
            return datatype_new(DATA_NULL);
        }

        // Emit operator and test if allowed
        bool valid = emit_tok2op(compiler->buffer, op, lhs_type);
        if(!valid)
        {
            compiler_throw(compiler, node, "Cannot perform operation '%s' on the types '%s' and '%s'", tok2str(op), datatype2str(lhs_type), datatype2str(rhs_type));
            return datatype_new(DATA_NULL);
        }

        // Get the type for the compiler
        switch(op) {
            case TOKEN_EQUAL:
            case TOKEN_NEQUAL:
            case TOKEN_LESS:
            case TOKEN_GREATER:
            case TOKEN_LEQUAL:
            case TOKEN_GEQUAL:
            case TOKEN_AND:
            case TOKEN_OR: return datatype_new(DATA_BOOL);
            default: {
                return lhs_type;
            }
        }
    }
    return datatype_new(DATA_NULL);
}

/**
 * eval_ident:
 * This function evaluates an occuring symbol and loads its content.
 */
datatype_t eval_ident(compiler_t* compiler, ast_t* node) {
    int depth = 0;
    symbol_t* ptr = symbol_get_ext(compiler->scope, node->ident, &depth);
    if(ptr) {
        if(ptr->node->class == AST_DECLVAR) {
            // If it is a field of a class
            if(ptr->ref) {
                // ldarg0
                // getfield <addr>

                symbol_t* classRef = ptr->ref;
                ast_t* classNode = classRef->node;
                void* val = 0;
                if(hashmap_get(classNode->classstmt.fields, node->ident, &val) == HMAP_MISSING) {
                    compiler_throw(compiler, node, "No such class field");
                    return datatype_new(DATA_NULL);
                }

                ptr = (symbol_t*)val;

                if(compiler->scope->node->class == AST_CLASS) {
                    compiler_throw(compiler, node, "Accessing class fields within the constructor is not permitted");
                    return datatype_new(DATA_NULL);
                }

                emit_op(compiler->buffer, OP_LDARG0);
                emit_class_getfield(compiler->buffer, ptr->address);
                return ptr->type;
            }

            // If it is a class constructor parameter
            if(ptr->isClassParam) {
                // Depth must be zero, otherwise out of scope
                if(depth != 0) {
                    compiler_throw(compiler, node, "Trying to access a constructor parameter");
                    return datatype_new(DATA_NULL);
                }
            }

            // If local or global, otherwise search in upper scope
            if(depth == 0 || ptr->global) {
                emit_load(compiler->buffer, ptr->address, ptr->global);
            }
            else {
                emit_load_upval(compiler->buffer, depth, ptr->address);
            }
        }
        return ptr->type;
    }

    compiler_throw(compiler, node, "Warning: Implicit declaration of field '%s'", node->ident);
    return datatype_new(DATA_NULL);
}

/**
 * eval_compare_and_call:
 * @param func Function declaration node
 * @param node Node that causes the call
 * @param address Address to call, if the function is internal
 *
 * Helper function for eval_call
 * Checks the given values and compares them with the function parameters.
 * If they match, the bytecode is emitted.
 */
bool eval_compare_and_call(compiler_t* compiler, ast_t* func, ast_t* node, int address) {
    size_t argc = list_size(node->call.args);
    ast_t* call = node->call.callee;

    list_t* formals = 0;
    bool external = false;
    if(func->class == AST_DECLFUNC) {
        formals = func->funcdecl.impl.formals;
        external = func->funcdecl.external;
    }
    else if(func->class == AST_CLASS) {
        formals = func->classstmt.formals;
    }
    size_t paramc = list_size(formals);

    // Param checking
    if(argc > paramc) {
        compiler_throw(compiler, node, "Too many arguments for function '%s'. Expected: %d", call->ident, paramc);
        return false;
    }
    else if(argc < paramc) {
        compiler_throw(compiler, node, "Too few arguments for function '%s'. Expected: %d", call->ident, paramc);
        return false;
    }

    if(paramc > 0) {
        // Valid, test parameter types
        list_iterator_t* iter = list_iterator_create(formals);
        list_iterator_t* args_iter = list_iterator_create(node->call.args);
        int i = 1;
        while(!list_iterator_end(iter)) {
            // Get the datatypes
            ast_t* param = list_iterator_next(iter);
            datatype_t argType = compiler_eval(compiler, list_iterator_next(args_iter));
            datatype_t paramType = param->vardecl.type;

            // Do template test
            if((paramType.type & DATA_GENERIC) == DATA_GENERIC) {
                type_t tp = paramType.type & ~DATA_GENERIC;
                if((argType.type & tp) == tp) continue;
            }

            // Test datatypes
            if(!datatype_match(argType, paramType)) {
                compiler_throw(compiler, node,
                    "Parameter %d has the wrong type.\nFound: %s, expected: %s",
                    i, datatype2str(argType), datatype2str(paramType));
                break;
            }
            i++;
        }
        list_iterator_free(args_iter);
        list_iterator_free(iter);
    }

    // Emit invocation
    if(external) {
        emit_syscall(compiler->buffer, func->funcdecl.external-1);
    }
    else {
        // Reserve some memory
        emit_reserve(compiler->buffer, compiler->scope->address);
        emit_invoke(compiler->buffer, address, argc);
    }

    return true;
}

datatype_t eval_bool_func(compiler_t* compiler, ast_t* node, datatype_t dt) {
    ast_t* call = node->call.callee;
    ast_t* key = call->subscript.key;
    size_t argc = list_size(node->call.args);

    if(argc != 0) {
        compiler_throw(compiler, node, "No such function / Expected zero arguments");
    }

    if(!strcmp(key->ident, "to_i")) {
        emit_op(compiler->buffer, OP_B2I);
        return datatype_new(DATA_INT);
    }
    else if(!strcmp(key->ident, "to_str")) {
        emit_op(compiler->buffer, OP_TOSTR);
        return datatype_new(DATA_STRING);
    }

    return datatype_new(DATA_NULL);
}

datatype_t eval_int32_func(compiler_t* compiler, ast_t* node, datatype_t dt) {
    ast_t* call = node->call.callee;
    ast_t* key = call->subscript.key;
    size_t argc = list_size(node->call.args);

    if(argc != 0) {
        compiler_throw(compiler, node, "No such function / Expected zero arguments");
    }

    if(!strcmp(key->ident, "to_f")) {
        // only int32 can be turned into a float
        emit_op(compiler->buffer, OP_I2F);
        return datatype_new(DATA_FLOAT);
    }
    else if(!strcmp(key->ident, "to_c") && dt.type == DATA_INT) {
        // no conversion needed, internally it is the same type
        // so just trick the compiler and return char
        return datatype_new(DATA_CHAR);
    }
    else if(!strcmp(key->ident, "to_i") && dt.type == DATA_CHAR) {
        return datatype_new(DATA_INT);
    }
    else if(!strcmp(key->ident, "to_str")) {
        // basically everything can be converted to a string
        emit_op(compiler->buffer, OP_TOSTR);
        return datatype_new(DATA_STRING);
    }
    else {
        compiler_throw(compiler, node, "No such function");
    }

    return datatype_new(DATA_NULL);
}

datatype_t eval_float_func(compiler_t* compiler, ast_t* node, datatype_t dt) {
    ast_t* call = node->call.callee;
    ast_t* key = call->subscript.key;
    size_t argc = list_size(node->call.args);

    if(argc != 0) {
        compiler_throw(compiler, node, "No such function / Expected zero arguments");
    }

    if(!strcmp(key->ident, "to_i")) {
        // only doubles can be converted to integers
        emit_op(compiler->buffer, OP_F2I);
        return datatype_new(DATA_INT);
    }
    else if(!strcmp(key->ident, "to_c")) {
        // Same function, just different 'type'
        emit_op(compiler->buffer, OP_F2I);
        return datatype_new(DATA_CHAR);
    }
    else if(!strcmp(key->ident, "to_str")) {
        emit_op(compiler->buffer, OP_TOSTR);
        return datatype_new(DATA_STRING);
    }
    else {
        compiler_throw(compiler, node, "No such function");
    }

    return datatype_new(DATA_NULL);
}

#define ASSERT_ZERO_ARGS() \
    if(ls != 0) { \
        compiler_throw(compiler, node, "Expected zero arguments"); \
        return datatype_new(DATA_NULL); }

datatype_t eval_array_func(compiler_t* compiler, ast_t* node, datatype_t dt) {
    // TODO: tail, head, insert, slice, pop and other datatypes
    ast_t* call = node->call.callee;
    ast_t* key = call->subscript.key;

    list_t* formals = node->call.args;
    size_t ls = list_size(formals);

    datatype_t subtype = datatype_new(dt.type & ~DATA_ARRAY);
    subtype.id = dt.id;

    if(!strcmp(key->ident, "length")) {
        ASSERT_ZERO_ARGS()

        // Length operation
        emit_op(compiler->buffer, OP_LEN);
        return datatype_new(DATA_INT);
    }
    else if(!strcmp(key->ident, "empty")) {
        ASSERT_ZERO_ARGS()

        // empty? = (len <= 0)
        emit_op(compiler->buffer, OP_LEN);
        emit_int(compiler->buffer, 0);
        emit_op(compiler->buffer, OP_ILE);
        return datatype_new(DATA_BOOL);
    }
    else if(!strcmp(key->ident, "append")) {
        if(ls != 1) {
            compiler_throw(compiler, node, "Expected one argument of type array");
            return datatype_new(DATA_NULL);
        }

        ast_t* param = list_get(formals, 0);
        datatype_t subelem = compiler_eval(compiler, param);
        if(!datatype_match(dt, subelem)) {
            compiler_throw(compiler, node, "Argument has the wrong type");
            return datatype_new(DATA_NULL);
        }

        emit_op(compiler->buffer, OP_APPEND);
        return dt;
    }
    else if(!strcmp(key->ident, "cons")) {
        if(ls != 1) {
            compiler_throw(compiler, node, "Expected one argument");
            return datatype_new(DATA_NULL);
        }

        ast_t* param = list_get(formals, 0);
        datatype_t subelem = compiler_eval(compiler, param);
        if(!datatype_match(subtype, subelem)) {
            compiler_throw(compiler, node, "Argument has the wrong type");
            return datatype_new(DATA_NULL);
        }

        emit_op(compiler->buffer, OP_CONS);
        return dt;
    }
    /*else if(!strcmp(key->ident, "equals"))
    {
        if(ls != 1)
        {
            compiler_throw(compiler, node, "Expected one argument");
            return datatype_new(DATA_NULL);
        }

        ast_t* param = list_get(formals, 0);
        datatype_t subelem = compiler_eval(compiler, param);
        if(!datatype_match(dt, subelem))
        {
            compiler_throw(compiler, node, "Argument has the wrong type");
            return datatype_new(DATA_NULL);
        }

        //emit_op(compiler->buffer, OP_ARREQ);
        return datatype_new(DATA_BOOL);
    }*/
    else if(!strcmp(key->ident, "at")) {
        // TODO: Wrap these Typechecks into functions
        // like: raiseOneArgErr(compiler, node, DATA_INT);
        // ==> compiler_throw("Expected one argument of type %d", datatype2str(DATA_INT));
        if(ls != 1) {
            compiler_throw(compiler, node, "Expected one argument of type int");
            return datatype_new(DATA_NULL);
        }

        // get at index
        ast_t* param = list_get(formals, 0);
        datatype_t paramT = compiler_eval(compiler, param);
        if(!datatype_match(paramT, datatype_new(DATA_INT))) {
            compiler_throw(compiler, node, "Argument has the wrong type");
            return datatype_new(DATA_NULL);
        }

        emit_op(compiler->buffer, OP_GETSUB);
        return subtype;
    }
    else {
        compiler_throw(compiler, node, "Invalid array operation");
    }
    return datatype_new(DATA_NULL);
}

datatype_t eval_class_call(compiler_t* compiler, ast_t* node, datatype_t dt) {
    // Extract data
    ast_t* call = node->call.callee;
    ast_t* key = call->subscript.key;
    ast_t* expr = call->subscript.expr;

    // Retrive the id and the corresponding class
    unsigned long id = dt.id;
    symbol_t* class = class_find(compiler->scope, id);
    if(!class) {
        compiler_throw(compiler, node, "Class does not exist");
        return datatype_new(DATA_NULL);
    }

    // Test if the field of the class is valid
    ast_t* cls = class->node;
    void* val = 0;
    if(hashmap_get(cls->classstmt.fields, key->ident, &val) == HMAP_MISSING) {
        compiler_throw(compiler, node, "Class field '%s' does not exist in class '%s'", key->ident, cls->classstmt.name);
        return datatype_new(DATA_NULL);
    }

    // Retrive the field
    symbol_t* func = (symbol_t*)val;
    if(eval_compare_and_call(compiler, func->node, node, func->address)) {
        // Get the last instruction (invoke) and convert it to invokevirtual
        instruction_t* ins = vector_top(compiler->buffer);
        ins->op = OP_INVOKEVIRTUAL;

        // Class is on top, reassign it
        symbol_t* sym = symbol_get(compiler->scope, expr->vardecl.name);
        if(sym) {
            symbol_replace(compiler, sym);
        }
        // Else replace
        else {
            emit_pop(compiler->buffer);
        }

        // Return the type
        return func->node->funcdecl.rettype;
    }

    return datatype_new(DATA_NULL);
}

datatype_t eval_datatype_call(compiler_t* compiler, ast_t* node, datatype_t dt) {
    // Class based internal calls
    // Can either be:
    // float
    // int32
    // array
    // bool
    //
    // Not allowed:
    // void
    // null
    // lambda

    // Evaluate accordingly
    if(dt.type == DATA_CLASS) {
        return eval_class_call(compiler, node, dt);
    }
    else {
        if((dt.type & DATA_ARRAY) == DATA_ARRAY) {
            return eval_array_func(compiler, node, dt);
        }
        // DATA_INT and DATA_CHAR are internally both int32->types
        else if(dt.type == DATA_INT || dt.type == DATA_CHAR) {
            return eval_int32_func(compiler, node, dt);
        }
        else if(dt.type == DATA_BOOL) {
            return eval_bool_func(compiler, node, dt);
        }
        else if(dt.type == DATA_FLOAT) {
            return eval_float_func(compiler, node, dt);
        }
        else {
            compiler_throw(compiler, node, "Unsupported operation");
        }
    }
    return datatype_new(DATA_NULL);
}

// Eval.call(node)
// Evaluates a call:
// Tries to find the given function,
// compares the given parameters with the requested ones and
// returns the functions return type.
datatype_t eval_call(compiler_t* compiler, ast_t* node) {
    ast_t* call = node->call.callee;
    if(call->class == AST_IDENT) {
        symbol_t* symbol = symbol_get(compiler->scope, call->ident);
        if(symbol) {
            if(symbol->node->class == AST_DECLFUNC) {
                bool isClass = false;
                if(symbol->ref) {
                    // Suspect class function
                    symbol = symbol->ref;
                    ast_t* class = symbol->node;
                    void* val = 0;
                    if(hashmap_get(class->classstmt.fields, call->ident, &val) == HMAP_MISSING)
                    {
                        compiler_throw(compiler, node, "Function does not exists in class");
                        return datatype_new(DATA_NULL);
                    }

                    symbol = (symbol_t*)val;
                    isClass = true;
                    emit_op(compiler->buffer, OP_LDARG0);
                }

                // Normal function call
                if(eval_compare_and_call(compiler, symbol->node, node, symbol->address)) {
                    // Convert to class-method-call
                    if(isClass) {
                        // Move stack down if function call within class
                        instruction_t* ins = vector_top(compiler->buffer);
                        ins->op = OP_INVOKEVIRTUAL;
                        emit_op(compiler->buffer, OP_SETARG0);
                    }

                    return symbol->node->funcdecl.rettype;
                }
            }
            else if(symbol->node->class == AST_CLASS) {
                // Class constructor call
                if(eval_compare_and_call(compiler, symbol->node, node, symbol->address)) {
                    return symbol->type;
                }
            }
            else {
                compiler_throw(compiler, node, "Identifier '%s' is not a function", call->ident);
            }

            return datatype_new(DATA_NULL);
        }

        compiler_throw(compiler, node, "Implicit declaration of function '%s'", call->ident);
    }
    else if(call->class == AST_SUBSCRIPT) {
        // Calling a class function

        // Evaluate the lhs
        ast_t* expr = call->subscript.expr;
        datatype_t dt = compiler_eval(compiler, expr);
        return eval_datatype_call(compiler, node, dt);
    }
    else {
        // lamdba(x,y,z)- > void { body} (6,2,3) will never occur due to parsing (maybe in future versions)
        compiler_throw(compiler, node, "Callee has to be an identifier or a lambda");
    }

    return datatype_new(DATA_NULL);
}

// Helper function
bool append_interpolated(compiler_t* compiler, char* buffer) {
    if(strlen(buffer) == 0) return true;

    if(buffer[0] != '_' && !isalpha(buffer[0])) {
        printf("Expected an identifer at $'%s', aborting.\n", buffer);
        return false;
    }

    symbol_t* symbol = symbol_get(compiler->scope, buffer);
    if(!symbol) {
        printf("Symbol '%s' does not exist!\n", buffer);
        return false;
    }

    // Do operation based on datatype
    datatype_t dt = eval_ident(compiler, symbol->node);
    if(dt.type == DATA_STRING) {
        emit_op(compiler->buffer, OP_APPEND);
    }
    else if(dt.type == DATA_CHAR) {
        emit_op(compiler->buffer, OP_CONS);
    }
    else {
        emit_op(compiler->buffer, OP_TOSTR);
        emit_op(compiler->buffer, OP_APPEND);
    }

    return true;
}

void interpolate_string(compiler_t* compiler, char* str) {
    char content[96];
    char buffer[32];
    int bp = 0;

    char* c = str;
    char* start = c;
    bool reading_ident = false;
    bool string_on_stack = false;

    while(*c != '\0') {

        // Identifier found!
        if(*c == '$') {
            reading_ident = true;

            // Clear and set buffer
            memset(content, 0, 96 * sizeof(char));
            memcpy(content, start, c-start);

            emit_string(compiler->buffer, content);
            c++;
            continue;
        }

        // Ending suspected?
        if(!isalnum(*c) && *c != '_' && reading_ident == 1) {
            buffer[bp] = '\0';
            append_interpolated(compiler, buffer);

            if(string_on_stack) {
                emit_op(compiler->buffer, OP_APPEND);
            }

            reading_ident = false;
            string_on_stack = true;
            bp = 0;
            start = c;
        }

        // Still reading?
        if(reading_ident) {
            buffer[bp++] = *c;
        }

        c++;
    }

    // Final check (if ended on NULL-Terminator)
    if(reading_ident == 1) {
        buffer[bp] = '\0';
        append_interpolated(compiler, buffer);

        if(string_on_stack) {
            emit_op(compiler->buffer, OP_APPEND);
        }
    }
    else {
        // Check if there is any string left
        if(start != c) {
            memset(content, 0, 96 * sizeof(char));
            memcpy(content, start, c-start);

            emit_string(compiler->buffer, content);
            emit_op(compiler->buffer, OP_APPEND);
        }
    }
}

datatype_t eval_string(compiler_t* compiler, ast_t* node)
{
    char* str = node->string;
    if(strchr(str, '$')) {
        interpolate_string(compiler, str);
    }
    else {
        emit_string(compiler->buffer, str);
    }

    return datatype_new(DATA_STRING);
}

datatype_t eval_char(compiler_t* compiler, ast_t* node)
{
    emit_char(compiler->buffer, node->ch);
    return datatype_new(DATA_CHAR);
}

datatype_t eval_array(compiler_t* compiler, ast_t* node)
{
    datatype_t dt = node->array.type;
    size_t ls = list_size(node->array.elements);
    list_iterator_t* iter = list_iterator_create(node->array.elements);

    if(ls > 0)
    {
        dt = compiler_eval(compiler, list_iterator_next(iter));
        node->array.type = dt;
    }

    if(dt.type == DATA_VOID || dt.type == DATA_NULL)
    {
        list_iterator_free(iter);
        compiler_throw(compiler, node, "Invalid: Array is composed of NULL elements");
        return datatype_new(DATA_NULL);
    }
    else if((dt.type & DATA_ARRAY) == DATA_ARRAY)
    {
        list_iterator_free(iter);
        compiler_throw(compiler, node, "Multidimensional arrays are not permitted");
        return datatype_new(DATA_NULL);
    }

    // Create an index and iterate through the elements.
    // Typechecks ensure that the array is valid.
    // The index is mainly used for debugging reasons.
    // We already evaluated one element, so set the index to 2
    size_t idx = 2;
    while(!list_iterator_end(iter))
    {
        datatype_t tmp = compiler_eval(compiler, list_iterator_next(iter));
        if(!datatype_match(tmp, dt))
        {
            compiler_throw(compiler, node, "An array can only hold one type of elements (@element %d)", idx);
            list_iterator_free(iter);
            return datatype_new(DATA_NULL);
        }

        if(tmp.type == DATA_NULL)
        {
            compiler_throw(compiler, node, "Invalid array. (@element %d is NULL)", idx);
            list_iterator_free(iter);
            return tmp;
        }
        idx++;
    }
    list_iterator_free(iter);

    // Merge string
    if(dt.type == DATA_CHAR)
    {
        // Merge this to a string
        emit_string_merge(compiler->buffer, ls);
        return datatype_new(DATA_STRING);
    }

    // | STACK_BOTTOM
    // | ...
    // | OP_ARR, element size
    // | STACK_TOP
    emit_array_merge(compiler->buffer, ls);
    datatype_t ret = datatype_new(DATA_ARRAY | node->array.type.type);
    ret.id = node->array.type.id;
    return ret;
}

// Eval.if(node)
// The function evaluates ifclauses
// by emitting jumps around the instructions.
// ---------------
// Example:
// if(x == 5) {
//     println(5)
// } else if(x == 4) {
//     println(4)
// } else {
//     println(x)
// }
//
// Bytecode compilation:
// 01: load x
// 02: push 5
// 03: ieq            <-- condition 1 (x = 5)
// 04: jmpf 8         <-- jump if false to condition 2
// 05: push 5
// 06: syscall, println, 1
// 07: jmp 12         <-- end block, jump to end
// 08: load x         <-- condition 2 (x = 3)
// 09: push 3
// 10: ieq
// 11: jmpf ..        <-- jump to condition 3 and so on
datatype_t eval_if(compiler_t* compiler, ast_t* node)
{
    // Eval the sub-ifclauses
    list_t* jmps = list_new();
    list_iterator_t* iter = list_iterator_create(node->ifstmt);
    while(!list_iterator_end(iter))
    {
        // Get sub-ifclause
        ast_t* subnode = list_iterator_next(iter);

        // Jump #1 - declaration
        // Test if not an else-statement
        val_t* instr = 0;
        if(subnode->ifclause.cond)
        {
            // Eval the condition and generate an if-false jump
            compiler_eval(compiler, subnode->ifclause.cond);
            instr = emit_jmpf(compiler->buffer, 0);
        }

        // Eval execution block code
        push_scope_virtual(compiler, node);
        eval_block(compiler, subnode->ifclause.body);
        pop_scope_virtual(compiler);

        // Optimization:
        // If not an else statement and more ifclauses than one
        // Generate a jump to end; add new jump to the list
        if(list_size(node->ifstmt) > 1 && subnode->ifclause.cond) {
            list_push(jmps, emit_jmp(compiler->buffer, 0));
        }

        // Jump #1 - set
        // If not an else statement
        // Generate jump to next clause
        // (set the previously generated jump)
        if(subnode->ifclause.cond)
        {
            *instr = INT32_VAL(vector_size(compiler->buffer));
        }
    }

    list_iterator_free(iter);

    // Set the jump points to end after whole if block
    iter = list_iterator_create(jmps);
    while(!list_iterator_end(iter))
    {
        val_t* pos = list_iterator_next(iter);
        *pos = INT32_VAL(vector_size(compiler->buffer));
    }
    list_iterator_free(iter);
    list_free(jmps);
    return datatype_new(DATA_NULL);
}

// Eval.while(node)
// Evaluates a while statement.
// Example:
// 01: push condition
// 02: jmpf 5
// 03: <instruction block>
// 04: jmp 1
datatype_t eval_while(compiler_t* compiler, ast_t* node)
{
    size_t start = vector_size(compiler->buffer);
    compiler_eval(compiler, node->whilestmt.cond);
    val_t* instr = emit_jmpf(compiler->buffer, 0);

    push_scope_virtual(compiler, node);
    eval_block(compiler, node->whilestmt.body);
    pop_scope_virtual(compiler);

    emit_jmp(compiler->buffer, start);
    *instr = INT32_VAL(vector_size(compiler->buffer));
    return datatype_new(DATA_NULL);
}

// Eval.return(node)
// Simply compiles the return data and emits the bytecode
datatype_t eval_return(compiler_t* compiler, ast_t* node)
{
    datatype_t dt = datatype_new(DATA_VOID);
    if(node->returnstmt)
    {
        dt = compiler_eval(compiler, node->returnstmt);
    }

    ast_t* refNode = 0;
    if(!scope_is_class(compiler->scope, AST_DECLFUNC, &refNode))
    {
        compiler_throw(compiler, node, "Return statement is not within a function");
        return datatype_new(DATA_NULL);
    }

    if(!datatype_match(refNode->funcdecl.rettype, dt))
    {
        compiler_throw(compiler, node, "Return value doesn't match the return type");
        return datatype_new(DATA_NULL);
    }

    if(scope_is_class(compiler->scope, AST_CLASS, &refNode))
    {
        emit_op(compiler->buffer, OP_RETVIRTUAL);
    }
    else
    {
        emit_return(compiler->buffer);
    }

    return datatype_new(DATA_NULL);
}

// Eval.subscript(node)
// Compiles a subscript node.
// Currently only allowed for strings and arrays.
datatype_t eval_subscript(compiler_t* compiler, ast_t* node)
{
    ast_t* expr = node->subscript.expr;
    ast_t* key = node->subscript.key;
    datatype_t exprType = compiler_eval(compiler, expr);

    if(exprType.type == DATA_CLASS)
    {
        compiler_throw(compiler, node, "Field access of classes is not permitted");
        return datatype_new(DATA_NULL);
    }

    datatype_t keyType = compiler_eval(compiler, key);
    if(keyType.type != DATA_INT)
    {
        compiler_throw(compiler, node, "Key must be of type integer");
        return datatype_new(DATA_NULL);
    }

    if((exprType.type & DATA_ARRAY) == DATA_ARRAY)
    {
        // INDEX: Hack:#2-#3
        // Simplified testing if index is out of bounds
        // May not work for functions returning an array

        // HACK:(#2) expect that expression is an identifier
        if(expr->class == AST_IDENT && key->class == AST_INT) {
            char* ident = expr->ident;
            symbol_t* symbol = symbol_get(compiler->scope, ident);
            if(symbol) {
                int index = key->i;

                // @cond1 Index must be in range
                // @cond2 Variable must be immutable, we can't keep track of mutable values
                // @cond3 Array size must be available, we can't deduce the size by subscripts or calls
                bool cond1 = (index < 0 || index >= symbol->arraySize);
                bool cond2 = !symbol->node->vardecl.mutate;
                bool cond3 = (symbol->arraySize != -1);
                if(cond1 && cond2 && cond3) {
                    compiler_throw(compiler, node, "H2: Array index out of bounds");
                    return datatype_new(DATA_NULL);
                }
            }
        }
        // HACK:(#3) Count the array objects
        else if(expr->class == AST_ARRAY) {
            list_t* elements = expr->array.elements;
            int sz = list_size(elements);
            int index = key->i;
            if(index < 0 || index >= sz) {
                compiler_throw(compiler, node, "H3: Array index out of bounds");
                return datatype_new(DATA_NULL);
            }
        }

        // We got an array and want to access an element.
        // Remove the array flag to get the return type.
        emit_op(compiler->buffer, OP_GETSUB);
         datatype_t ret = datatype_new(exprType.type & ~DATA_ARRAY);
        ret.id = exprType.id;
        return ret;
    }
    else {
        compiler_throw(compiler, node, "Invalid subscript operation");
        return datatype_new(DATA_NULL);
    }

    return datatype_new(DATA_NULL);
}

// Eval.unary(node)
// Compiles prefix operators.
datatype_t eval_unary(compiler_t* compiler, ast_t* node)
{
    datatype_t type = compiler_eval(compiler, node->unary.expr);
    if(node->unary.op == TOKEN_ADD) return type;

    // ADD; SUB; NOT; BITNOT
    // OPCODES:
    // 1. F / I / B -> float / integer / boolean
    // 2. U -> unary

    switch(type.type) {
        case DATA_INT: {
            // SUB, BITNOT
            if(node->unary.op == TOKEN_NOT) {
                compiler_throw(compiler, node, "Logical negation can only be used with objects of type boolean");
            }
            else if(node->unary.op == TOKEN_SUB) {
                emit_op(compiler->buffer, OP_IMINUS);
            }
            else {
                // Can only be Bitnot
                emit_op(compiler->buffer, OP_BITNOT);
            }
            break;
        }
        case DATA_FLOAT: {
            // SUB

            if(node->unary.op == TOKEN_BITNOT || node->unary.op == TOKEN_NOT) {
                compiler_throw(compiler, node, "Bit operations / logical negation can only be used with objects of type int");
            }
            else {
                // Can only be float negation
                emit_op(compiler->buffer, OP_FMINUS);
            }
            break;
        }
        case DATA_BOOL: {
            if(node->unary.op != TOKEN_NOT) {
                compiler_throw(compiler, node, "Arithmetic operations cannot be applied on objects of type boolean");
            }
            else {
                emit_op(compiler->buffer, OP_NOT);
            }
            break;
        }
        case DATA_CLASS:
        case DATA_CHAR:
        case DATA_VOID:
        case DATA_NULL:
        default: {
            compiler_throw(compiler, node, "Invalid unary instruction, only applicable to numbers or booleans");
            break;
        }
    }

    return type;
}

datatype_t eval_annotation(compiler_t* compiler, ast_t* node);

// Eval.class(node)
// Concept compilation of a class structure (WIP)
datatype_t eval_class(compiler_t* compiler, ast_t* node) {
    // Approach:
    // Convert the constructor to a function that returns a 'class'-object value
    // Class declaration is the main function, instantiating the classes values and functions.
    // Functions and values are saved in class value + symbol

    // Get the class data
    char* name = node->classstmt.name;
    list_t* body = node->classstmt.body;

    // Generate the datatype with an id
    unsigned long id = djb2((unsigned char*)name);
    datatype_t dt;
    dt.type = DATA_CLASS;
    dt.id = id;

    // Emit a jump, get the bytecode address
    val_t* addr = emit_jmp(compiler->buffer, 0);
    size_t byte_address = vector_size(compiler->buffer);

    // Test if symbol exists
    void* tmp = 0;
    if(hashmap_get(compiler->scope->classes, name, &tmp) != HMAP_MISSING) {
        compiler_throw(compiler, node, "Class already exists");
        return datatype_new(DATA_NULL);
    }

    // Register the class and emit bytecode
    symbol_t* symbol = symbol_new(compiler, node, byte_address, dt);
    hashmap_set(compiler->scope->classes, name, symbol);
    hashmap_set(compiler->scope->symbols, name, symbol);

    // Create a class with zero fields
    val_t* fields = emit_class(compiler->buffer, 0);
    int field_count = 0;

    // Create a new scope
    push_scope(compiler, node);

    // Treat each parameter as a local variable, with no type or value
    list_iterator_t* iter = list_iterator_create(node->classstmt.formals);
    int i = -(list_size(node->classstmt.formals) + 3);
    while(!list_iterator_end(iter))
    {
        // Create parameter in symbols list
        ast_t* param = list_iterator_next(iter);

        if(symbol_exists(compiler, param, param->vardecl.name)) return datatype_new(DATA_NULL);
        symbol_t* paramSym = symbol_new(compiler, param, i, param->vardecl.type);
        paramSym->isClassParam = true;
        hashmap_set(compiler->scope->symbols, param->vardecl.name, paramSym);
        i++;
    }
    list_iterator_free(iter);

    // Iterate through body, emit the variables
    iter = list_iterator_create(body);
    while(!list_iterator_end(iter))
    {
        ast_t* sub = list_iterator_next(iter);
        if(sub->class == AST_ANNOTATION)
        {
            eval_annotation(compiler, sub);
        }
        else if(sub->class == AST_DECLVAR)
        {
            field_count++;
            size_t addr = compiler->scope->address;
            compiler_eval(compiler, sub);

            // Get the symbol, check if valid
            symbol_t* sym = symbol_get(compiler->scope, sub->vardecl.name);
            if(sym)
            {
                emit_load(compiler->buffer, addr, false);
                sym->ref = symbol;
                hashmap_set(node->classstmt.fields, sub->vardecl.name, sym);
                emit_class_setfield(compiler->buffer, compiler->scope->address-1);

                // Check annotations at last, TODO: Wrap to a function
                if(scope_requests(compiler->scope, ANN_GETTER))
                {
                    char* name = sub->vardecl.name;
                    datatype_t vartype = sub->vardecl.type;

                    // Create the function name
                    token_t* buffer = malloc(sizeof(token_t));
                    buffer[0].value = concat("get", name);
                    buffer[0].type = TOKEN_WORD;
                    if(isalpha(buffer[0].value[3]))
                    {
                        buffer[0].value[3] = toupper(buffer[0].value[3]);
                    }

                    // Create a subparser
                    parser_t* subparser = malloc(sizeof(*subparser));
                    parser_init(subparser, (const char*)buffer[0].value);
                    list_push(compiler->parsers, subparser);
                    subparser->buffer = buffer;
                    subparser->num_tokens = 1;

                    // Generate the ASTs
                    ast_t *fn = ast_class_create(AST_DECLFUNC, node->location);
                    fn->funcdecl.name = buffer[0].value;
                    fn->funcdecl.impl.formals = list_new();
                    fn->funcdecl.impl.body = list_new();
                    fn->funcdecl.rettype = vartype;
                    fn->funcdecl.external = false;

                    ast_t* ret = ast_class_create(AST_RETURN, node->location);
                    ret->returnstmt = ast_class_create(AST_IDENT, node->location);
                    ret->returnstmt->ident = name;
                    list_push(fn->funcdecl.impl.body, ret);

                    // Upload to parser
                    subparser->top = fn;

                    // Dump the syntax tree
#ifndef NO_AST
                    printf("Abstract syntax tree GET '%s'\n", name);
                    ast_dump(fn, 1);
                    printf("\n");
#endif

                    // Evaluate, returns lambda
                    compiler_eval(compiler, fn);

                    // 1. Retrive the class symbol
                    // 2. Get the function symbol
                    // 3. Register a new classfield -> the function
                    // 4. Set the reference of the function symbol to the class

                    // Get the symbols
                    ast_t* clazz = compiler->scope->node;
                    symbol_t* clazzSymbol = symbol_get(compiler->scope, clazz->classstmt.name);
                    symbol_t* funcSymbol = symbol_get(compiler->scope, fn->funcdecl.name);

                    // Assign as a class field
                    hashmap_set(clazz->classstmt.fields, fn->funcdecl.name, funcSymbol);
                    funcSymbol->ref = clazzSymbol;
                }
                if(scope_requests(compiler->scope, ANN_SETTER))
                {
                    if(!sub->vardecl.mutate)
                    {
                        compiler_throw(compiler, sub, "Setters are only valid for mutable variables");
                        break;
                    }

                    char* name = sub->vardecl.name;
                    datatype_t vartype = sub->vardecl.type;

                    // Reserve two tokens
                    token_t* buffer = malloc(sizeof(token_t)*2);
                    buffer[0].value = concat("set", name);
                    buffer[0].type = TOKEN_WORD;
                    if(isalpha(buffer[0].value[3]))
                    {
                        buffer[0].value[3] = toupper(buffer[0].value[3]);
                    }
                    buffer[1].value = strdup("p0");
                    buffer[1].type = TOKEN_WORD;

                    // Create a subparser
                    parser_t* subparser = malloc(sizeof(*subparser));
                    parser_init(subparser, (const char*)buffer[0].value);
                    list_push(compiler->parsers, subparser);
                    subparser->buffer = buffer;
                    subparser->num_tokens = 2;

                    // Generate the ASTs
                    ast_t *fn = ast_class_create(AST_DECLFUNC, node->location);
                    fn->funcdecl.name = buffer[0].value;
                    fn->funcdecl.impl.formals = list_new();
                    fn->funcdecl.impl.body = list_new();
                    fn->funcdecl.rettype = datatype_new(DATA_VOID);
                    fn->funcdecl.external = false;

                    // Create parameter p0
                    ast_t* p0 = ast_class_create(AST_DECLVAR, node->location);
                    p0->vardecl.name = buffer[1].value;
                    p0->vardecl.type = vartype;
                    p0->vardecl.mutate = false;
                    list_push(fn->funcdecl.impl.formals, p0);

                    // Create the assignment
                    ast_t* bin = ast_class_create(AST_BINARY, node->location);
                    ast_t* lhs = ast_class_create(AST_IDENT, node->location);
                    ast_t* rhs = ast_class_create(AST_IDENT, node->location);
                    lhs->ident = name;
                    rhs->ident = buffer[1].value;
                    bin->binary.left = lhs;
                    bin->binary.right = rhs;
                    bin->binary.op = TOKEN_ASSIGN;
                    list_push(fn->funcdecl.impl.body, bin);

                    // Upload to parser
                    subparser->top = fn;

#ifndef NO_AST
                    printf("Abstract syntax tree SET '%s'\n", name);
                    ast_dump(fn, 1);
                    printf("\n");
#endif

                    // Evaluate, returns lambda
                    compiler_eval(compiler, fn);

                    // Get the symbols
                    ast_t* clazz = compiler->scope->node;
                    symbol_t* clazzSymbol = symbol_get(compiler->scope, clazz->classstmt.name);
                    symbol_t* funcSymbol = symbol_get(compiler->scope, fn->funcdecl.name);

                    // Assign as a class field
                    hashmap_set(clazz->classstmt.fields, fn->funcdecl.name, funcSymbol);
                    funcSymbol->ref = clazzSymbol;
                }
                scope_unflag(compiler->scope);
            }
        }
        else if(sub->class == AST_DECLFUNC)
        {
            compiler_eval(compiler, sub);
            symbol_t* sym = symbol_get(compiler->scope, sub->funcdecl.name);
            if(sym)
            {
                sym->ref = symbol;
                hashmap_set(node->classstmt.fields, sub->funcdecl.name, sym);
            }
        }
        else
        {
            compiler_throw(compiler, node, "Statements are not allowed as a direct field of a class");
            break;
        }
    }
    list_iterator_free(iter);
    pop_scope(compiler);

    // Return the class object
    emit_return(compiler->buffer);

    // Set the beggining byte address; end
    byte_address = vector_size(compiler->buffer);
    *addr = INT32_VAL(byte_address);

    // Set the class fields count
    *fields = INT32_VAL(field_count);

    return datatype_new(DATA_NULL);
}

datatype_t eval_import(compiler_t* compiler, ast_t* node)
{
    // TODO: Better checking
    if(strstr(node->import, ".gs"))
    {
        // Create a new subparser for new file
        parser_t* subparser = malloc(sizeof(parser_t));
        parser_init(subparser, (const char*)node->import);
        list_push(compiler->parsers, subparser);

        char* source = readFile(node->import);
        if(source) {
            ast_t* root = parser_run(subparser, source);
            if(!root || parser_error(subparser))
            {
                ast_free(root);
                free(source);
                compiler_throw(compiler, node, "Could not compile file '%s'", node->import);
                return datatype_new(DATA_NULL);
            }

            // Store and swap to new parser
            parser_t* tmp = compiler->parser;
            compiler->parser = subparser;

#ifndef NO_AST
            printf("Abstract syntax tree '%s':\n", compiler->parser->name);
            ast_dump(root, 0);
            putchar('\n');
#endif

            // Evaluate and swap back
            compiler_eval(compiler, root);
            compiler->parser = tmp;
        }
        else
        {
            compiler_throw(compiler, node, "Could not read file named '%s'", node->import);
        }
        free(source);
        return datatype_new(DATA_NULL);
    }

    // Dynamic library checking
    if(strcmp(node->import, "core") != 0 && strcmp(node->import, "io") != 0 &&
        strcmp(node->import, "math") != 0) {

        printf("WARNING: Dynamic libraries are not supported (for %s)\n", node->import);
#ifdef USE_DYNLIB_FEATURE
        // 1. Create the formatted name for lib. e.g. png -> libpng.dll
        // 2. Feed instruction

        // free on scope end?
        // register in local scope?

        char* name = createSystemLibraryName(node->import);
        emit_dynlib(compiler->buffer, name);
        free(name);
#endif
    }

    return datatype_new(DATA_NULL);
}

datatype_t eval_annotation(compiler_t* compiler, ast_t* node) {
    // Test if flag is already set
    if((compiler->scope->flag & node->annotation) == node->annotation) {
        compiler_throw(compiler, node, "Annotation flag is already set");
        return datatype_new(DATA_NULL);
    }

    if(node->annotation != ANN_UNUSED) {
        if(!compiler->scope->node) {
            compiler_throw(compiler, node, "Annotations can only be used within classes");
            return datatype_new(DATA_NULL);
        }

        if(compiler->scope->node->class != AST_CLASS) {
            compiler_throw(compiler, node, "Annotations can only be used within classes");
            return datatype_new(DATA_NULL);
        }
    }

    // Set the flag
    compiler->scope->flag |= node->annotation;
    return datatype_new(DATA_NULL);
}

// Compiler.eval(node)
// Evaluates a node according to its class.
datatype_t compiler_eval(compiler_t* compiler, ast_t* node) {
    if(compiler->error) return datatype_new(DATA_NULL);

#ifdef DB_EVAL
    printf("Evaluating: %s\n", ast_classname(node->class));
#endif

    switch(node->class) {
        case AST_TOPLEVEL: return eval_block(compiler, node->toplevel);
        case AST_DECLVAR: return eval_declvar(compiler, node);
        case AST_DECLFUNC: return eval_declfunc(compiler, node);
        case AST_RETURN: return eval_return(compiler, node);
        case AST_FLOAT: return eval_number(compiler, node);
        case AST_INT: return eval_number(compiler, node);
        case AST_BOOL: return eval_bool(compiler, node);
        case AST_STRING: return eval_string(compiler, node);
        case AST_CHAR: return eval_char(compiler, node);
        case AST_ARRAY: return eval_array(compiler, node);
        case AST_BINARY: return eval_binary(compiler, node);
        case AST_IDENT: return eval_ident(compiler, node);
        case AST_CALL: return eval_call(compiler, node);
        case AST_IF: return eval_if(compiler, node);
        case AST_WHILE: return eval_while(compiler, node);
        case AST_UNARY: return eval_unary(compiler, node);
        case AST_SUBSCRIPT: return eval_subscript(compiler, node);
        case AST_CLASS: return eval_class(compiler, node);
        case AST_IMPORT: return eval_import(compiler, node);
        case AST_ANNOTATION: return eval_annotation(compiler, node);
        default: break;
    }

    return datatype_new(DATA_NULL);
}

// Compiler.compileBuffer(string code)
// Compiles code into bytecode instructions
vector_t* compile_buffer(const char* source, const char* name) {
    compiler_t compiler;
    compiler.parser = malloc(sizeof(parser_t));
    compiler.parsers = list_new();
    compiler.buffer = vector_new();
    compiler.scope = scope_new();
    compiler.error = false;
    compiler.depth = 0;
    list_push(compiler.parsers, compiler.parser);

    // Run the parser
    parser_init(compiler.parser, name);
    ast_t* root = parser_run(compiler.parser, source);
    if(root) {
#ifndef NO_AST
        printf("Abstract syntax tree '%s':\n", compiler.parser->name);
        ast_dump(root, 0);
        putchar('\n');
#endif

        // Evaluate AST
        // Add final HALT instruction to end
        compiler_eval(&compiler, root);
        emit_op(compiler.buffer, OP_HLT);
    } else {
        compiler.error = true;
    }

    vector_t* buffer = compiler.buffer;
    compiler_clear(&compiler);

    if(compiler.error) {
        bytecode_buffer_free(buffer);
        return 0;
    }

    return buffer;
}

// Compiler.compileFile(string filename)
// Compiles a file into an instruction set
vector_t* compile_file(const char* filename) {
    char* source = readFile(filename);
    if(!source) {
        printf("File '%s' does not exist\n", filename);
        return 0;
    }

    // Compile into instructions
    vector_t* buffer = compile_buffer(source, filename);
    free(source);
    return buffer;
}

// Compiler.clear()
// Clears the current instruction buffer of the compiler
void compiler_clear(compiler_t* compiler) {
    // Free Scope and parsers
    list_iterator_t* iter = list_iterator_create(compiler->parsers);
    while(!list_iterator_end(iter))
    {
        parser_t* parser = list_iterator_next(iter);
        parser_free(parser);
        ast_free(parser->top);
        free(parser);
    }
    list_iterator_free(iter);
    list_free(compiler->parsers);
    if(compiler->scope) scope_free(compiler->scope);
}
