# Makefile for mini-component-placement-strategy
CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -pedantic -O2 -g
CPPFLAGS = -Iinclude -D_GNU_SOURCE
SRCDIR   = src
TESTDIR  = tests
EXAMDIR  = examples
DEMODIR  = demos
BENCDIR  = benches
BUILDDIR = build

LIBSRCS  = $(wildcard $(SRCDIR)/*.c)
LIBOBJS  = $(LIBSRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
TESTSRCS = $(wildcard $(TESTDIR)/*.c)
TESTBINS = $(TESTSRCS:$(TESTDIR)/%.c=$(BUILDDIR)/test_%)
EXAMSRCS = $(wildcard $(EXAMDIR)/*.c)
EXAMBINS = $(EXAMSRCS:$(EXAMDIR)/%.c=$(BUILDDIR)/example_%)

.PHONY: all test clean

all: $(BUILDDIR) $(BUILDDIR)/libplacement.a $(TESTBINS) $(EXAMBINS)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/libplacement.a: $(LIBOBJS)
	ar rcs $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILDDIR)/test_%: $(TESTDIR)/%.c $(BUILDDIR)/libplacement.a | $(BUILDDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -L$(BUILDDIR) -lplacement -lm -o $@

$(BUILDDIR)/example_%: $(EXAMDIR)/%.c $(BUILDDIR)/libplacement.a | $(BUILDDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -L$(BUILDDIR) -lplacement -lm -o $@

test: $(TESTBINS)
	@echo === Running Tests ===; failed=0; for t in $(TESTBINS); do echo Running $$t...; LD_LIBRARY_PATH=$(BUILDDIR) $$t && echo PASS || { echo FAIL; failed=1; }; done; exit $$failed

examples: $(EXAMBINS)
	@for e in $(EXAMBINS); do LD_LIBRARY_PATH=$(BUILDDIR) $$e; done

clean:
	rm -rf $(BUILDDIR)
