/**
 * Copyright (c) 2015-2017
 * @author Alexander Koch
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include <libndls.h>

#include <string.h>
#include <assert.h>

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
    printf("Golem compiler / Nspire port v1.0\n");
    printf("Copyright (c) Alexander Koch 2016\nNspire port by Ti64CLi++\n\n");
    /*printf("Usage:\n");
    printf("  golem <file>       (Run a file)\n");
    printf("  golem -r <file>    (Run a *.gvm file)\n");
    printf("  golem -c <file>    (Convert to bytecode file *.gvm)\n");
    printf("  golem --ast <file> (Convert generated AST to graph *.dot)\n");*/
    printf("Golem is now installed.\nYou will need to install it again after every reboot though.\nYou can now execute .gs file directly by open them.\n");
}

int main(int argc, char** argv) {
    //register .gs file as golem file
    cfg_register_fileext("gs","golem");

    seed_prng(time(0));
    vm_t vm = {0};

    if(argc == 2) {
        // Generate and execute bytecode (Interpreter)
        vector_t* buffer = compile_file(argv[1]);
        if(buffer) {
            vm_run_args(&vm, buffer, argc, argv);
        }
        bytecode_buffer_free(buffer);
    } else if(argc == 3) {
        if(!strcmp(argv[1], "-c")) {
            // Compile to bytecode
            vector_t* buffer = compile_file(argv[2]);
            if(buffer) {
                // Write to file
                char* out = replaceExt(argv[2], ".gvm.tns", 4);
                serialize(out, buffer);
                printf("Wrote bytecode to file '%s'\n", out);
                free(out);
            }
            bytecode_buffer_free(buffer);
        } else if(!strcmp(argv[1], "-r")) {
            // Run compiled bytecode file
            vector_t* buffer = vector_new();
            if(deserialize(argv[2], &buffer)) {
                vm_run_args(&vm, buffer, argc, argv);
            }
            bytecode_buffer_free(buffer);
        } else if(!strcmp(argv[1], "--ast")) {
            // Generate ast.dot graphviz file
            char* path = argv[2];
            char* source = readFile(path);
            if(!source) {
                printf("File '%s' does not exist\n", path);
                return 1;
            }

            context_t* context = context_new();
            parser_t* parser = parser_new(path, context);
            ast_t* root = parser_run(parser, source);
            free(source);
            parser_free(parser);
            if(root) {
                graphviz_build(root);
            }
            ast_free(root);
            context_free(context);
        } else {
            printf("Flag: '%s' is invalid\n\n", argv[1]);
            return 1;
        }
    } else {
        print_info();
    }

#ifndef NO_MEMINFO
    mem_leak_check();
#endif
    printf("\nPress any key to exit...\n");
    wait_key_pressed();
    
    return 0;
}
