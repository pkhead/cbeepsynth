DYNASM=luajit ../../third_party/dynasm/dynasm.lua
DASC_SOURCES=$(wildcard *.dasc)
C_OUTPUTS=$(DASC_SOURCES:.dasc=.c)

build: $(C_OUTPUTS)

%.c: %.dasc
	@$(DYNASM) -o $@ $(realpath $^)

.PHONY: build