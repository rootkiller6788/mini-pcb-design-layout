# Makefile for mini-schematic-design-kicad
# KiCad Schematic Design and Analysis Library
#
# Targets:
#   make          Build static library
#   make test     Build and run tests  
#   make clean    Remove build artifacts

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -O2 -g -Iinclude
LDFLAGS  = -lm

SRC = src/schematic_core.c src/schematic_netlist.c src/schematic_sexpr.c src/schematic_bom.c src/schematic_connectivity.c src/schematic_erc.c
OBJ = build/schematic_core.o build/schematic_netlist.o build/schematic_sexpr.o build/schematic_bom.o build/schematic_connectivity.o build/schematic_erc.o
LIB = build/libschematic.a

.PHONY: all test clean

all: $(LIB)

build:
	mkdir -p build

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(OBJ)
	ar rcs $@ $^

test: $(LIB)
	$(CC) $(CFLAGS) tests/test_schematic.c -Lbuild -lschematic $(LDFLAGS) -o build/test_schematic
	./build/test_schematic

clean:
	rm -rf build test_output.* test_bom.* test_fmt.*