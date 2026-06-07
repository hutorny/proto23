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
PLUGINDIR = $(DESTDIR:%=%/$(PREFIX:%=%/))bin/
INCLUDES  = include \
            test/3rd/ut/include \
            test/3rd/nameof/include \
            test/3rd/inplace_vector/include \
            test/3rd/inplace_string/src/include \

CXX_FLAGS = -std=c++$(STANDARD) $(filter-out -,$(OPTION)) \
            -O2 -g3 -pedantic \
            -Wall -Wextra -Wconversion -Wcast-align -Wcast-qual \
            -Wold-style-cast -Wshadow -Wsign-conversion

MD_FLAGS  = -MMD -MP

TEST_SRC    := $(wildcard test/*.cxx)

TEST_OBJ    = $(TEST_SRC:%.cxx=$(BUILDDIR)/%.o) 
TEST_BIN    = $(BUILDDIR)/proto-test
TEST_DEPS   = $(BUILDDIR)/proto-test.d

EX_SRC      = example/example.cxx
EX_BIN      = $(BUILDDIR)/example
EX_DEPS     = $(BUILDDIR)/example.d

PLUGIN_SRC  = plugin/plugin.cxx
PLUGIN_BIN  = $(BUILDDIR)/protoc-gen-proto23
PLUGIN_DEPS = $(BUILDDIR)/plugin.d

# Extra include dir for the plugin (descriptor.h lives in plugin/)
PLUGIN_EXTRA_I  = plugin

all: 
	$(info Run "make install" to install or "make run" to build and run tests)

install:
	install -d $(INSTALLDIR)
	cp -r include/proto23 $(INSTALLDIR)

install-plugin: plugin
	install -d $(PLUGINDIR)
	install $(PLUGIN_BIN) $(PLUGINDIR)


test: $(TEST_BIN)
	

example: $(EX_BIN)

plugin: $(PLUGIN_BIN)

$(BUILDDIR)/ $(BUILDDIR)/test/:
	@mkdir -p $@

$(TEST_BIN): $(TEST_OBJ) | $(BUILDDIR)/
	$(CXX) $(CXX_FLAGS) $(INCLUDES:%=-I%) $(MD_FLAGS) -MF$(TEST_DEPS) -MT$@ -o $@ $^

$(BUILDDIR)/test/%.o: test/%.cxx | test/test-data.inc $(BUILDDIR)/test/
	$(CXX) -c $(CXX_FLAGS) $(INCLUDES:%=-I%) $(MD_FLAGS) -MF$(TEST_DEPS) -MT$@ -o $@ $<

$(EX_BIN): $(EX_SRC) | $(BUILDDIR)/
	$(CXX) $(CXX_FLAGS) $(INCLUDES:%=-I%) $(MD_FLAGS) -MF$(EX_DEPS) -MT$@ -o $@ $<

$(PLUGIN_BIN): $(PLUGIN_SRC) | $(BUILDDIR)/
	$(CXX) $(CXX_FLAGS) $(INCLUDES:%=-I%) -I$(PLUGIN_EXTRA_I) $(MD_FLAGS) -MF$(PLUGIN_DEPS) -MT$@ -o $@ $<

test/test-data.inc: test/scripts/encode.sh $(wildcard test/proto/*.proto)
	cd ./test/scripts && /usr/bin/bash ./encode.sh > ../test-data.inc

run: $(TEST_BIN).run $(EX_BIN).run

test-plugin: $(PLUGIN_BIN)
	@mkdir -p build/out
	protoc -Itest/plugin -Iinclude --plugin=protoc-gen-proto23=$(PLUGIN_BIN) --proto23_out=build/out /usr/include/google/protobuf/*.proto
	protoc -Itest/plugin -Iinclude --plugin=protoc-gen-proto23=$(PLUGIN_BIN) --proto23_out=build/out test/plugin/google/protobuf/*.proto
	protoc -Itest/plugin -Iinclude --plugin=protoc-gen-proto23=$(PLUGIN_BIN) --proto23_out=build/out test/plugin/*.proto
	cd build/out && $(CXX) -c $(CXX_FLAGS) $(INCLUDES:%=-I../../%) -I. ../../test/plugin/*.cxx

test-master: $(PLUGIN_BIN)
	mkdir -p test/master
	protoc -Itest/plugin -Iinclude --plugin=protoc-gen-proto23=$(PLUGIN_BIN) --proto23_out=test/master test/plugin/*.proto

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

.PHONY: all test example plugin run clean clean-all install-plugin test-plugin

$(TEST_DEPS) $(EX_DEPS) $(PLUGIN_DEPS):
-include $(TEST_DEPS) $(EX_DEPS) $(PLUGIN_DEPS)

# protoc -Iproto --plugin=protoc-gen-proto23=./g++-23/protoc-gen-proto23 --proto23_out=out proto/*.proto
# g++ -c -Wall -pedantic -Wextra -std=c++23 -I include/ -I build/out test/plugin/a.cxx -o build/out/a.o
