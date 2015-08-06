#include "ast.h"

const char* datatype2str(datatype_t type)
{
	switch(type)
	{
		case DATA_NULL: return "null";
		case DATA_BOOL: return "bool";
		case DATA_INT: return "int";
		case DATA_FLOAT: return "float";
		case DATA_STRING: return "string";
		case DATA_OBJECT: return "object";
		case DATA_VOID: return "void";
		case DATA_ARRAY: return "array";
		case DATA_VARARGS: return "varargs";
		case DATA_LAMBDA: return "lambda";
		default: return "unknown";
	}
}

ast_t* ast_class_create(ast_class_t class, location_t location)
{
	ast_t* ast = malloc(sizeof(*ast));
	if(!ast) return ast;

	memset(ast, 0, sizeof(*ast));
	ast->class = class;
	ast->location = location;
    return ast;
}

const char* ast_classname(ast_class_t class)
{
	switch(class)
	{
		case AST_NULL: return "null";
		case AST_IDENT: return "identifier";
		case AST_FLOAT: return "float";
		case AST_INT: return "integer";
		case AST_STRING: return "string";
		case AST_ARRAY: return "array";
		case AST_BINARY: return "binary";
		case AST_UNARY: return "unary";
		case AST_SUBSCRIPT: return "subscript";
		case AST_CALL: return "call";
		case AST_DECLVAR: return "variable declaration";
		case AST_DECLFUNC: return "function declaration";
		case AST_IF: return "if condition";
		case AST_WHILE: return "while loop";
		case AST_IMPORT: return "import";
		case AST_CLASS: return "class";
		case AST_RETURN: return "return";
		case AST_TOPLEVEL: return "toplevel";
		default: return "null";
	}
}

void ast_free(ast_t* ast)
{
	if(!ast) return;

	list_iterator_t* iter = 0;
	switch(ast->class)
	{
		case AST_TOPLEVEL:
		{
			iter = list_iterator_create(ast->toplevel);
			while(!list_iterator_end(iter))
			{
				ast_free(list_iterator_next(iter));
			}
			list_iterator_free(iter);
			list_free(ast->toplevel);
			break;
		}
		case AST_DECLVAR:
		{
			ast_free(ast->vardecl.initializer);
			break;
		}
		case AST_DECLFUNC:
		{
			iter = list_iterator_create(ast->funcdecl.impl.body);
			while(!list_iterator_end(iter))
			{
				ast_free(list_iterator_next(iter));
			}
			list_iterator_free(iter);
			list_free(ast->funcdecl.impl.body);

			iter = list_iterator_create(ast->funcdecl.impl.formals);
			while(!list_iterator_end(iter))
			{
				param_t* param = list_iterator_next(iter);
				free(param);
			}
			list_iterator_free(iter);
			list_free(ast->funcdecl.impl.formals);

			if(ast->funcdecl.external)
			{
				free(ast->funcdecl.name);
			}
			break;
		}
		case AST_BINARY:
		{
			ast_free(ast->binary.left);
			ast_free(ast->binary.right);
			break;
		}
		case AST_UNARY:
		{
			ast_free(ast->unary.expr);
			break;
		}
		case AST_ARRAY:
		{
			iter = list_iterator_create(ast->array.elements);
			while(!list_iterator_end(iter))
			{
				ast_free(list_iterator_next(iter));
			}
			list_iterator_free(iter);
			list_free(ast->array.elements);
			break;
		}
		case AST_SUBSCRIPT:
		{
			ast_free(ast->subscript.key);
			ast_free(ast->subscript.expr);
			break;
		}
		case AST_IF:
		{
			iter = list_iterator_create(ast->ifstmt);
			while(!list_iterator_end(iter))
			{
				ast_free(list_iterator_next(iter));
			}
			list_iterator_free(iter);
			list_free(ast->ifstmt);
			break;
		}
		case AST_IFCLAUSE:
		{
			iter = list_iterator_create(ast->ifclause.body);
			while(!list_iterator_end(iter))
			{
				ast_free(list_iterator_next(iter));
			}
			list_iterator_free(iter);
			list_free(ast->ifclause.body);
			ast_free(ast->ifclause.cond);
			break;
		}
		case AST_WHILE:
		{
			iter = list_iterator_create(ast->whilestmt.body);
			while(!list_iterator_end(iter))
			{
				ast_free(list_iterator_next(iter));
			}
			list_iterator_free(iter);
			list_free(ast->whilestmt.body);
			ast_free(ast->whilestmt.cond);
			break;
		}
		case AST_CLASS:
		{
			iter = list_iterator_create(ast->classstmt.body);
			while(!list_iterator_end(iter))
			{
				ast_free(list_iterator_next(iter));
			}
			list_iterator_free(iter);
			list_free(ast->classstmt.body);
			break;
		}
		case AST_RETURN:
		{
			ast_free(ast->returnstmt);
			break;
		}
		case AST_CALL:
		{
			ast_free(ast->call.callee);

			iter = list_iterator_create(ast->call.args);
			while(!list_iterator_end(iter))
			{
				ast_free(list_iterator_next(iter));
			}
			list_iterator_free(iter);
			list_free(ast->call.args);
		}
		default: break;
	}
	free(ast);
}
