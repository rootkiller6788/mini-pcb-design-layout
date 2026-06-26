# Makefile for mini-flex-rigid-flex-design
# Flex and Rigid-Flex PCB Design Knowledge Module
#
# Targets:
#   make        — Build library (libflexrigidflex.a)
#   make test   — Build and run all tests
#   make clean  — Remove build artifacts

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -pedantic -O2 -g -fPIC
CFLAGS  += -I./include
LDFLAGS  = -lm
AR       = ar
ARFLAGS  = rcs

# Source files
SRC_DIR  = src
INC_DIR  = include
TEST_DIR = tests
EXAM_DIR = examples

SRCS     = $(SRC_DIR)/flex_material.c \
           $(SRC_DIR)/flex_bend.c \
           $(SRC_DIR)/flex_stackup.c \
           $(SRC_DIR)/flex_design_rule.c \
           $(SRC_DIR)/flex_impedance.c \
           $(SRC_DIR)/flex_rigid_transition.c \
           $(SRC_DIR)/flex_thermal.c

OBJS     = $(SRCS:.c=.o)
LIB      = libflexrigidflex.a

# Test files
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_BINS = $(TEST_SRCS:.c=.test)

# Example files
EXAM_SRCS = $(wildcard $(EXAM_DIR)/*.c)
EXAM_BINS = $(EXAM_SRCS:.c=.example)

.PHONY: all test clean examples

all: $(LIB)

# Static library
$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

# Compile source files
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(INC_DIR)/flex_material.h
	$(CC) $(CFLAGS) -c $< -o $@

# Tests
test: $(LIB) $(TEST_BINS)
	@echo "=== Running Tests ==="
	@passed=0; failed=0; \
	for t in $(TEST_BINS); do \
		echo "--- $$t ---"; \
		if ./$$t; then \
			echo "PASS: $$t"; \
			passed=$$((passed+1)); \
		else \
			echo "FAIL: $$t"; \
			failed=$$((failed+1)); \
		fi; \
	done; \
	echo "=== Results: $$passed passed, $$failed failed ==="; \
	test $$failed -eq 0

$(TEST_DIR)/%.test: $(TEST_DIR)/%.c $(LIB)
	$(CC) $(CFLAGS) $< -o $@ -L. -lflexrigidflex $(LDFLAGS)

# Examples
examples: $(LIB) $(EXAM_BINS)

$(EXAM_DIR)/%.example: $(EXAM_DIR)/%.c $(LIB)
	$(CC) $(CFLAGS) $< -o $@ -L. -lflexrigidflex $(LDFLAGS)

# Clean
clean:
	rm -f $(SRC_DIR)/*.o
	rm -f $(LIB)
	rm -f $(TEST_DIR)/*.test
	rm -f $(EXAM_DIR)/*.example
