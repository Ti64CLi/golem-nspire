
#include "libdef.h"
#include <vm/vm.h>
extern float strtof(const char* str, char** endptr);

GOLEM_API val_t core_print(vm_t* vm)
{
	val_print(pop(vm));
	return NULL_VAL;
}

GOLEM_API val_t core_println(vm_t* vm)
{
	core_print(vm);
	putchar('\n');
	return NULL_VAL;
}

GOLEM_API val_t core_getline(vm_t* vm)
{
	// Get input to buffer
	char buf[512];
	fgets(buf, sizeof(buf), stdin);
	return STRING_VAL(buf);
}

GOLEM_API val_t core_parseFloat(vm_t* vm)
{
	char* str = AS_STRING(pop(vm));
	return NUM_VAL(strtof(str, 0));
}

GOLEM_API val_t core_break(vm_t* vm)
{
	getchar();
	return NULL_VAL;
}

GOLEM_API int core_gen_signatures(list_t* toplevel)
{
	signature_new();
	require_func();

	// print(T) -> void
	function_new("print", DATA_VOID, 1);
	function_add_param(DATA_GENERIC);
	function_upload(toplevel);

	// println(T) -> void
	function_new("println", DATA_VOID, 2);
	function_add_param(DATA_GENERIC);
	function_upload(toplevel);

	// getline() -> char[]
	function_new("getline", DATA_STRING, 3);
	function_upload(toplevel);

	// parseFloat(str:char[]) -> float
	function_new("parseFloat", DATA_FLOAT, 4);
	function_add_param(DATA_STRING);
	function_upload(toplevel);

	// break() -> void
	function_new("break", DATA_VOID, 5);
	function_upload(toplevel);

	return 0;
}
