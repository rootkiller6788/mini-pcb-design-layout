CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -Iinclude
LDFLAGS = -lm
SRCDIR = src
INCDIR = include
TESTDIR = tests
EXAMPLEDIR = examples
BUILDDIR = build

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))
TESTS = $(wildcard $(TESTDIR)/*.c)
TESTBINS = $(patsubst $(TESTDIR)/%.c,$(BUILDDIR)/test_%,$(TESTS))
EXAMPLES = $(wildcard $(EXAMPLEDIR)/*.c)
EXAMPLEBINS = $(patsubst $(EXAMPLEDIR)/%.c,$(BUILDDIR)/example_%,$(EXAMPLES))

.PHONY: all clean test examples

all: $(OBJS)
	@echo "=== Library compiled: $(words $(OBJS)) object files ==="

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/test_%: $(TESTDIR)/%.c $(OBJS)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

$(BUILDDIR)/example_%: $(EXAMPLEDIR)/%.c $(OBJS)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

test: $(TESTBINS)
	@for t in $(TESTBINS); do echo "Running $$t..."; $$t || exit 1; done
	@echo "=== All tests passed ==="

examples: $(EXAMPLEBINS)
	@for e in $(EXAMPLEBINS); do echo "Running $$e..."; $$e; done

clean:
	rm -rf $(BUILDDIR)
