# proto23 build and test runner
# usage: make [run|test|example] [COMPILER=g++] [STANDARD=23] [OPTION=...]
#
# Requires a C++23-capable compiler (GCC >= 13, Clang >= 16).
# Boost.UT header is taken from the sibling test/3rd tree.

COMPILER  = g++
STANDARD  = 23
OPTION    =
MAKEFLAGS+= --no-builtin-rules

BUILDDIR  = build/$(COMPILER)-$(STANDARD)$(filter-out -,$(OPTION))
CXX       = $(COMPILER)
PREFIX		= local
DESTDIR   = /usr
INSTALLDIR = $(DESTDIR:%=%/$(PREFIX:%=%/))include/
INCLUDES  = include \
            test/3rd/ut/include \
            test/3rd/nameof/include \
            test/3rd/inplace_vector/include \
            test/3rd/inplace_string/src/include \

CXX_FLAGS = -std=c++$(STANDARD) $(filter-out -,$(OPTION)) \
            -O2 -g3 -pedantic \
            -Wall -Wextra -Wconversion -Wcast-align -Wcast-qual \
            -Wold-style-cast -Wshadow -Wsign-conversion \
            $(INCLUDES:%=-I%)

MD_FLAGS  = -MMD -MP

TEST_SRC    := $(wildcard test/*.cxx)

TEST_OBJ    = $(TEST_SRC:%.cxx=$(BUILDDIR)/%.o) 
TEST_BIN    = $(BUILDDIR)/proto-test
TEST_DEPS   = $(BUILDDIR)/proto-test.d

EX_SRC      = example/example.cxx
EX_BIN      = $(BUILDDIR)/example
EX_DEPS     = $(BUILDDIR)/example.d

PLUGIN_SRC  = plugin/plugin.cxx
PLUGIN_BIN  = $(BUILDDIR)/proto23c
PLUGIN_DEPS = $(BUILDDIR)/plugin.d

# Extra include dir for the plugin (descriptor.h lives in plugin/)
PLUGIN_EXTRA_I  = plugin

all: 
	$(info Run "make install" to install or "make run" to build and run tests)

install:
	install -d $(INSTALLDIR)
	cp -r include/proto23 $(INSTALLDIR)

test: $(TEST_BIN)

example: $(EX_BIN)

plugin: $(PLUGIN_BIN)

$(BUILDDIR)/ $(BUILDDIR)/test/:
	@mkdir -p $@

$(TEST_BIN): $(TEST_OBJ) | $(BUILDDIR)/
	$(CXX) $(CXX_FLAGS) $(MD_FLAGS) -MF$(TEST_DEPS) -MT$@ -o $@ $^

$(BUILDDIR)/test/%.o: test/%.cxx | test/test-data.inc $(BUILDDIR)/test/
	$(CXX) -c $(CXX_FLAGS) $(MD_FLAGS) -MF$(TEST_DEPS) -MT$@ -o $@ $<

$(EX_BIN): $(EX_SRC) | $(BUILDDIR)/
	$(CXX) $(CXX_FLAGS) $(MD_FLAGS) -MF$(EX_DEPS) -MT$@ -o $@ $<

$(PLUGIN_BIN): $(PLUGIN_SRC) | $(BUILDDIR)/
	$(CXX) $(CXX_FLAGS) -I$(PLUGIN_EXTRA_I) $(MD_FLAGS) -MF$(PLUGIN_DEPS) -MT$@ -o $@ $<

test/test-data.inc: test/scripts/encode.sh $(wildcard test/proto/*.proto)
	cd ./test/scripts && /usr/bin/bash ./encode.sh > ../test-data.inc

run: $(TEST_BIN).run $(EX_BIN).run

$(TEST_BIN).run: $(TEST_BIN)
	$(info Running tests...)
	@$<

$(EX_BIN).run: $(EX_BIN)
	$(info Running example...)
	@$<

clean:
	rm -rf $(BUILDDIR)

clean-all:
	rm -rf build

.PHONY: all test example plugin run clean clean-all

$(TEST_DEPS) $(EX_DEPS) $(PLUGIN_DEPS):
-include $(TEST_DEPS) $(EX_DEPS) $(PLUGIN_DEPS)
