#ifndef bytecode_h
#define bytecode_h

#include <core/mem.h>
#include <lexis/lexer.h>
#include <adt/vector.h>
#include <parser/ast.h>
#include <vm/val.h>

typedef enum
{
	// Basic stack
	OP_PUSH,
	OP_POP,
	OP_HLT,

	// Store
	OP_STORE,
	OP_LOAD,
	OP_GSTORE,
	OP_GLOAD,
	OP_LDARG0,
	OP_SETARG0,

	// Arithmetic
	// Integer
	OP_IADD,
	OP_ISUB,
	OP_IMUL,
	OP_IDIV,
	OP_MOD,
	OP_BITL,
	OP_BITR,
	OP_BITAND,
	OP_BITOR,
	OP_BITXOR,
	OP_BITNOT,
	OP_IMINUS,

	// Float
	OP_FADD,
	OP_FSUB,
	OP_FMUL,
	OP_FDIV,
	OP_FMINUS,

	// Boolean
	OP_NOT,

	// Special
	OP_SYSCALL,
	OP_INVOKE,
	OP_INVOKEVIRTUAL,
	OP_RESERVE,
	OP_RET,
	OP_RETVIRTUAL,
	OP_JMP,
	OP_JMPF,
	OP_ARR,
	OP_STR,
	OP_LDLIB,

	// Compare
	OP_BEQ,
	OP_IEQ,
	OP_FEQ,
	OP_CEQ,
	OP_ARREQ,
	OP_BNE,
	OP_INE,
	OP_FNE,
	OP_CNE,
	OP_LT,
	OP_GT,
	OP_LE,
	OP_GE,
	OP_BAND,
	OP_BOR,

	// Subscript
	OP_GETSUB,
	OP_SETSUB,
	OP_LEN,
	OP_APPEND,
	OP_CONS,

	// Upval
	OP_UPVAL,
	OP_UPSTORE,

	// Class
	OP_CLASS,
	OP_SETFIELD,
	OP_GETFIELD
} opcode_t;

typedef struct
{
	opcode_t op;
	val_t v1;
	val_t v2;
} instruction_t;

const char* op2str(opcode_t code);

void insert(vector_t* buffer, opcode_t op);
void insert_v1(vector_t* buffer, opcode_t op, val_t v1);
void insert_v2(vector_t* buffer, opcode_t op, val_t v1, val_t v2);

void emit_bool(vector_t* buffer, bool b);
void emit_int(vector_t* buffer, I64 v);
void emit_float(vector_t* buffer, F64 f);
void emit_string(vector_t* buffer, char* str);
void emit_char(vector_t* buffer, char c);
void emit_pop(vector_t* buffer);
void emit_op(vector_t* buffer, opcode_t op);
bool emit_tok2op(vector_t* buffer, token_type_t tok, datatype_t type);
void emit_syscall(vector_t* buffer, char* name, size_t args);
void emit_invoke(vector_t* buffer, size_t address, size_t args);
void emit_return(vector_t* buffer);
void emit_store(vector_t* buffer, int address, bool global);
void emit_load(vector_t* buffer, int address, bool global);
void emit_load_upval(vector_t* buffer, int depth, int address);
void emit_store_upval(vector_t* buffer, int depth, int address);
void emit_class_setfield(vector_t* buffer, int address);
void emit_class_getfield(vector_t* buffer, int address);
void emit_lib_load(vector_t* buffer, char* name);

val_t* emit_jmp(vector_t* buffer, int address);
val_t* emit_jmpf(vector_t* buffer, int address);

#endif