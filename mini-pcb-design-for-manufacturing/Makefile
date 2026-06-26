# mini-pcb-design-for-manufacturing Makefile
# PCB Design for Manufacturing - DFM Analysis Library

CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -std=c11 -pedantic
LDFLAGS  = -lm

INCDIR   = include
SRCDIR   = src
BUILDDIR = build

SRCS = src/dfm_core.c src/dfm_rules.c src/dfm_cost.c src/dfm_panel.c src/dfm_thermal.c src/dfm_yield.c
OBJS = build/dfm_core.o build/dfm_rules.o build/dfm_cost.o build/dfm_panel.o build/dfm_thermal.o build/dfm_yield.o

HEADERS = include/dfm_core.h include/dfm_rules.h include/dfm_cost.h include/dfm_panel.h include/dfm_thermal.h include/dfm_yield.h

.PHONY: all test clean

all: test

build:
	mkdir -p build

build/%.o: src/%.c $(HEADERS) | build
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

build/test_dfm: $(OBJS) tests/test_dfm.c | build
	$(CC) $(CFLAGS) -I$(INCDIR) $(OBJS) tests/test_dfm.c -o $@ $(LDFLAGS)

build/ex1: $(OBJS) examples/example_4layer_design.c | build
	$(CC) $(CFLAGS) -I$(INCDIR) $(OBJS) examples/example_4layer_design.c -o $@ $(LDFLAGS)

build/ex2: $(OBJS) examples/example_panel_optimization.c | build
	$(CC) $(CFLAGS) -I$(INCDIR) $(OBJS) examples/example_panel_optimization.c -o $@ $(LDFLAGS)

build/ex3: $(OBJS) examples/example_yield_cost.c | build
	$(CC) $(CFLAGS) -I$(INCDIR) $(OBJS) examples/example_yield_cost.c -o $@ $(LDFLAGS)

test: build/test_dfm
	build/test_dfm

clean:
	rm -rf build

rebuild: clean all
