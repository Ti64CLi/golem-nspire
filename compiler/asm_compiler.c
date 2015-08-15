#include "asm_compiler.h"

int vars = 1;

const char* reg(int i) {
	switch(i)
	{
		case 0: return "rax";
		case 1: return "rcx";
		case 2: return "rdx";
		case 3: return "r8";
		case 4: return "r9";
		case 5: return "push";
		default: return "push";
	}
}

void asm_write_prologue(FILE* fp, int stack_size, int bias)
{
	fprintf(fp, "extern printf\nsection .data\n\tsize equ %d\n\tbias equ %d\n", stack_size, bias);
	fprintf(fp, "\tprintNum db \"%%d\", 10, 0\n\n");
	fprintf(fp, "section .text\n");
	fprintf(fp, "global instr_add\n");
	fprintf(fp, "global instr_sub\n");
	fprintf(fp, "global WinMain\n\n");

	fprintf(fp, "instr_add:\n");
	fprintf(fp, "\tadd rcx, rdx\n");
	fprintf(fp, "\tmov rax, rcx\n\tret\n\n");
	fprintf(fp, "instr_sub:\n");
	fprintf(fp, "\tsub rcx, rdx\n");
	fprintf(fp, "\tmov rax, rcx\n\tret\n");

	fprintf(fp, "\nWinMain:\n\t; prologue\n\tmov [rsp+8],rcx\n\tpush r14\n\tpush r13\n");
	fprintf(fp, "\tsub rsp, size\n\tlea r13,[bias+rsp]\n\n");
}

void asm_write_epilogue(FILE* fp)
{
	fprintf(fp, "\t; epilogue\n\tlea rsp,[r13+size-bias]\n");
	fprintf(fp, "\tpop r13\n\tpop r14\n\tret\n");
}

void translate(FILE* fp, instruction_t* instr)
{
	switch(instr->op)
	{
		case OP_PUSH:
		{
			fprintf(fp, "\tmov %s, %d\n", reg(vars), value_int(instr->v1));
			vars++;
			break;
		}
		case OP_GSTORE:
		{
			int offset = value_int(instr->v1);
			fprintf(fp, "\tmov [r13-%d], %s\n", (offset+1) * 4, reg(--vars));
			//vars--;
			break;
		}
		case OP_GLOAD:
		{
			int offset = value_int(instr->v1);
			fprintf(fp, "\tmov %s, [r13-%d]\n", reg(vars), (offset+1) * 4);
			vars++;
			break;
		}
		case OP_IADD:
		{
			//fprintf(fp, "\tpop rcx\n\tpop rdx\n");
			fprintf(fp, "\tmov rcx, %s\n", reg(vars));
			vars--;
			fprintf(fp, "\tmov rdx, %s\n", reg(vars));
			vars--;
			fprintf(fp, "\tadd rcx, rdx\n");
			fprintf(fp, "\tmov rax, rcx\n");
			break;
		}
		case OP_SYSCALL:
		{
			if(!strcmp(value_string(instr->v1), "println"))
			{
				fprintf(fp, "\tpop rdx\n");
				fprintf(fp, "\tmov rcx, printNum\n");
				fprintf(fp, "\tcall printf\n");
			}
			break;
		}
		default: break;
	}

	fprintf(fp, "\n");
}

int asm_write(compiler_t* compiler, const char* filename)
{
	FILE* fp = fopen(filename, "wb");
	if(!fp) return 1;
	asm_write_prologue(fp, 256, 8);

	fprintf(fp, "\t; main code\n");
	list_iterator_t* iter = list_iterator_create(compiler->buffer);
	while(!list_iterator_end(iter))
	{
		instruction_t* instr = (instruction_t*)list_iterator_next(iter);
		translate(fp, instr);
	}
	list_iterator_free(iter);
	fprintf(fp, "\n");

	asm_write_epilogue(fp);
	fclose(fp);
	return 0;
}
