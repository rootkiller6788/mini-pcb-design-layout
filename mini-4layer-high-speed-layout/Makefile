# Makefile for mini-4layer-high-speed-layout
# Part of mini-electronic-info / mini-pcb-design-layout

CC       = gcc
CFLAGS   = -std=gnu11 -Wall -Wextra -O2
LDFLAGS  = -lm
INCDIR   = include
SRCDIR   = src
TSTDIR   = tests
EXAMDIR  = examples
BUILDDIR = build

SRCS = $(SRCDIR)/hs_stackup.c \
       $(SRCDIR)/hs_impedance.c \
       $(SRCDIR)/hs_transmission.c \
       $(SRCDIR)/hs_crosstalk.c \
       $(SRCDIR)/hs_pdn.c \
       $(SRCDIR)/hs_via.c

OBJS = $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

TEST_SRC = $(TSTDIR)/test_main.c
TEST_BIN = $(BUILDDIR)/test_runner

HEADERS = $(INCDIR)/hs_stackup.h \
          $(INCDIR)/hs_impedance.h \
          $(INCDIR)/hs_transmission.h \
          $(INCDIR)/hs_crosstalk.h \
          $(INCDIR)/hs_pdn.h \
          $(INCDIR)/hs_via.h

.PHONY: all clean test examples

all: $(BUILDDIR) $(OBJS) test examples
	@echo "=== Build complete ==="

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(HEADERS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(TEST_BIN): $(OBJS) $(TEST_SRC) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -o $@ $(TEST_SRC) $(OBJS) $(LDFLAGS)

test: $(TEST_BIN)
	@echo "=== Running tests ==="
	$(TEST_BIN)

examples: $(HEADERS) $(SRCS)
	@for f in $(EXAMDIR)/*.c; do \
		name=$$(basename $$f .c); \
		echo "Building $$name..."; \
		$(CC) $(CFLAGS) -I$(INCDIR) -o $(BUILDDIR)/$$name $$f $(SRCS) $(LDFLAGS); \
	done

clean:
	rm -rf $(BUILDDIR)

help:
	@echo "Targets: all test examples clean"
