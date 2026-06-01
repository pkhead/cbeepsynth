DYNASM=luajit ../../third_party/dynasm/dynasm.lua
OUT=fm_algo_x64.c

build: $(OUT)

%.c: %.dasc
	$(DYNASM) -o $@ $(realpath $^)

.PHONY: build