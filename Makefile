DEBUG = FALSE

GCC = nspire-gcc
#AS  = nspire-as
#GXX = nspire-g++
LD  = nspire-ld
GENZEHN = genzehn
INC = -I.

#GCCFLAGS = -W -Wall -marm
GCCFLAGS = -std=c99 -W -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -marm
LDFLAGS = -Wl,-lm,--nspireio,--gc-sections
#LDFLAGS = -Wl,--nspireio,--gc-sections
ZEHNFLAGS = --name "golem-nspire" --author "Ti64CLi++"

ifeq ($(DEBUG),FALSE)
	#GCCFLAGS += -Os
	GCCFLAGS += -O3 -fno-gcse -fno-crossjumping -DNO_IR -DNO_MEMINFO -DNO_AST
else
	#GCCFLAGS += -O0 -g
	GCCFLAGS += -O2 -g
endif

FILES := adt/bytebuffer.c \
		adt/hashmap.c \
		adt/list.c \
		adt/vector.c \
		compiler/compiler.c \
		compiler/graphviz.c \
		compiler/scope.c \
		compiler/serializer.c \
		core/util.c \
		lexis/lexer.c \
		lib/corelib.c \
		lib/iolib.c \
		lib/mathlib.c \
		parser/ast.c \
		parser/parser.c \
		parser/types.c \
		vm/bytecode.c \
		vm/val.c \
		vm/vm.c

OBJDIR := bin
OBJS := $(FILES:.c=.o)
OBJECTS := $(addprefix $(OBJDIR)/, $(notdir $(OBJS)))

#OBJS = $(patsubst %.c, %.o, $(shell find . -name \*.c))
#OBJS += $(patsubst %.cpp, %.o, $(shell find . -name \*.cpp))
#OBJS += $(patsubst %.S, %.o, $(shell find . -name \*.S))
EXE = golem-nspire
DISTDIR = .
vpath %.tns $(DISTDIR)
vpath %.elf $(DISTDIR)

all: $(EXE).tns

# Immediate representation tool / print .gvm bytecode
ir: $(OBJDIR) $(OBJS)
	@echo tools/ir.c
	@$(CC) $(CFLAGS) $(INC) -c tools/ir.c -o $(OBJDIR)/ir.o
	@$(CC) $(LDFLAGS) $(OBJECTS) $(OBJDIR)/ir.o -o $(MODULE_IR)

$(OBJDIR):
	@test -d $@ || mkdir $@

%.o: %.c
	$(GCC) $(GCCFLAGS) $(INC) -c $< -o $(OBJDIR)/$(notdir $@)

#%.o: %.cpp
#	$(GXX) $(GCCFLAGS) -c $< -o $@
	
#%.o: %.S
# 	$(AS) -c $< -o $@

$(EXE).elf: $(OBJDIR) $(OBJS) main.o
	mkdir -p $(DISTDIR)
	$(LD) $(OBJECTS) $(OBJDIR)/main.o -o $@ $(LDFLAGS)

$(EXE).tns: $(EXE).elf
	$(GENZEHN) --input $^ --output $@.zehn $(ZEHNFLAGS)
	make-prg $@.zehn $@
	rm $@.zehn

clean:
	rm -f $(OBJECTS) $(DISTDIR)/$(EXE).tns $(DISTDIR)/$(EXE).elf 
	#$(DISTDIR)/$(EXE).tns.zehn

#install: all
#	install $(MODULE_EXEC) $(PREFIX)/bin

#uninstall:
#	rm $(PREFIX)/bin/$(MODULE_EXEC)

# Graphviz *.dot to *.svg
#dot:
#	dot -Tsvg -o ast.svg ast.dot

#.PHONY: clean install uninstall