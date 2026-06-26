# Makefile — PCB Thermal Management Library
# make        : build library
# make test   : build and run tests
# make examples: build all examples
# make clean  : remove build artifacts

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -O2 -g
INCLUDES = -Iinclude
LDFLAGS  = -lm

SRC_DIR  = src
OBJ_DIR  = build
INC_DIR  = include

# Library source files (exclude main entry points)
LIB_SRCS = $(SRC_DIR)/pcb_thermal_material.c \
           $(SRC_DIR)/pcb_thermal_analysis.c \
           $(SRC_DIR)/pcb_thermal_via.c \
           $(SRC_DIR)/pcb_thermal_design.c \
           $(SRC_DIR)/pcb_thermal_simulation.c

LIB_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(LIB_SRCS))

TEST_SRC  = tests/test_thermal.c
TEST_BIN  = $(OBJ_DIR)/test_thermal

EXAMPLE_SRCS = $(wildcard examples/*.c)
EXAMPLE_BINS = $(patsubst examples/%.c,$(OBJ_DIR)/%,$(EXAMPLE_SRCS))

.PHONY: all test examples clean

all: $(OBJ_DIR)/libpcbthermal.a

# Object file compilation
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Static library
$(OBJ_DIR)/libpcbthermal.a: $(LIB_OBJS)
	@mkdir -p $(OBJ_DIR)
	ar rcs $@ $^

# Test executable
$(TEST_BIN): $(TEST_SRC) $(OBJ_DIR)/libpcbthermal.a
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(OBJ_DIR) -lpcbthermal $(LDFLAGS) -o $@

# Example executables
$(OBJ_DIR)/%: examples/%.c $(OBJ_DIR)/libpcbthermal.a
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(OBJ_DIR) -lpcbthermal $(LDFLAGS) -o $@

# Build and run tests
test: $(TEST_BIN)
	@echo "=== Running Thermal Management Tests ==="
	./$(TEST_BIN)
	@echo "=== All Tests Passed ==="

# Build examples
examples: $(EXAMPLE_BINS)
	@echo "=== Running Examples ==="
	@for ex in $(EXAMPLE_BINS); do echo "--- $$(basename $$ex) ---"; ./$$ex; done

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR)

# Direct build of examples
example-ldo-cooling: $(OBJ_DIR)/example_ldo_cooling
	./$(OBJ_DIR)/example_ldo_cooling

example-mosfet: $(OBJ_DIR)/example_mosfet_heatsink
	./$(OBJ_DIR)/example_mosfet_heatsink

example-via: $(OBJ_DIR)/example_via_optimization
	./$(OBJ_DIR)/example_via_optimization
