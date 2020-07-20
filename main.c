/**
 * Copyright (c) 2015-2017
 * @author Alexander Koch
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
//for nspire support
#include <libndls.h>

#define USE_MEM_IMPLEMENTATION
#include "core/mem.h"
#include "core/util.h"
#include "parser/ast.h"
#include "parser/parser.h"
#include "parser/types.h"
#include "vm/vm.h"
#include "compiler/compiler.h"
#include "compiler/serializer.h"
#include "compiler/graphviz.h"

void print_info(void) {
    printf("Golem compiler / Nspire port v2.0b\n");
    printf("Copyright (c) Alexander Koch 2016\nNspire port by Ti64CLi++\n\n");
    printf("Golem is now installed.\nYou will need to install it again after every reboot though.\nYou can now execute .gs file directly by open them.\n");
}

int main(int argc, char** argv) {
	enable_relative_paths(argv); //allow relative paths. Don't know if it'll work for every function ?

    seed_prng(time(0));
    vm_t vm = {0};

    if(argc == 2) {
        // Generate and execute bytecode (Interpreter)
        vector_t* buffer = compile_file(argv[1]);
        if(buffer) {
            vm_run_args(&vm, buffer, argc, argv);
        }
        bytecode_buffer_free(buffer);
        }*/
    } else {
    	//regsiter extensions
    	cfg_register_fileext("gs", "golem-nspire");

        print_info();
    }

#ifndef NO_MEMINFO
    mem_leak_check();
#endif

    printf("\nPress any key to exit...\n");
    wait_key_pressed();

    return 0;
}
