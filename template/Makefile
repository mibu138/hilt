CC = gcc
GLC = glslc

CFLAGS = -Wall -Wno-missing-braces -Wno-attributes -fPIC
LDFLAGS = -L/opt/hfs18.0/dsolib -L$(HOME)/lib
INFLAGS = -I$(HOME)/dev
LIBS = -lm -lcoal -ltanto -lvulkan -lxcb -lxcb-keysyms -lfreetype
GLFLAGS = --target-env=vulkan1.2
BIN = bin
LIB = $(HOME)/lib
LIBNAME = template

O = build
GLSL = shaders
SPV  = shaders/spv

NAME = template

DEPS =  \
		game.h \
		render.h \
		template.h \
		common.h \

OBJS =  \
		$(O)/game.o \
		$(O)/render.o \
		$(O)/template.o \

debug: CFLAGS += -g -DVERBOSE=1
debug: all

release: CFLAGS += -DNDEBUG -O3
release: all

all: tanto bin lib tags shaders

FRAGS := $(patsubst %.frag,$(SPV)/%-frag.spv,$(notdir $(wildcard $(GLSL)/*.frag)))
VERTS := $(patsubst %.vert,$(SPV)/%-vert.spv,$(notdir $(wildcard $(GLSL)/*.vert)))

shaders: $(FRAGS) $(VERTS)

clean: 
	rm -f $(O)/* $(LIB)/$(LIBNAME) $(BIN)/* $(SPV)/*

tags:
	ctags -R .

.PHONY: tanto
tanto:
	make -C tanto/ 

bin: main.c $(OBJS) $(DEPS) shaders
	$(CC) $(CFLAGS) $(INFLAGS) $(LDFLAGS) $(OBJS) $< -o $(BIN)/$(NAME) $(LIBS)

lib: $(OBJS) $(DEPS) shaders
	$(CC) -shared -o $(LIB)/lib$(LIBNAME).so $(OBJS)

staticlib: $(OBJS) $(DEPS) shaders
	ar rcs $(LIB)/lib$(NAME).a $(OBJS)

$(O)/%.o:  %.c $(DEPS)
	$(CC) $(CFLAGS) $(INFLAGS) -c $< -o $@

$(SPV)/%-vert.spv: $(GLSL)/%.vert $(DEPS)
	$(GLC) $(GLFLAGS) $< -o $@

$(SPV)/%-frag.spv: $(GLSL)/%.frag
	$(GLC) $(GLFLAGS) $< -o $@

$(SPV)/%-rchit.spv: $(GLSL)/%.rchit
	$(GLC) $(GLFLAGS) $< -o $@

$(SPV)/%-rgen.spv: $(GLSL)/%.rgen
	$(GLC) $(GLFLAGS) $< -o $@

$(SPV)/%-rmiss.spv: $(GLSL)/%.rmiss
	$(GLC) $(GLFLAGS) $< -o $@
