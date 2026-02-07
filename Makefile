CC      = gcc
CLANG   ?= clang
CFLAGS  = -O3 -march=native -Wall
WFLAGS  = --target=wasm32 -O3 -nostdlib -Wl,--no-entry -Wl,--export-dynamic
BUILD   = build

all: $(BUILD)/solve $(BUILD)/mancala.wasm

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/solve: main.c solve.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ main.c

$(BUILD)/mancala.wasm: wasm.c solve.c | $(BUILD)
	$(CLANG) $(WFLAGS) -o $@ wasm.c

clean:
	rm -rf $(BUILD)

.PHONY: all clean
