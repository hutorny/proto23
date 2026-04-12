/*
 * Copyright (C) 2026 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * proto23/test/main.cxx - Test runner entry point.
 *
 * Licensed under MIT License, see full text in LICENSE
 */

#include <boost/ut.hpp>

int main(int argc, char* argv[]) {
    using namespace boost::ut;
    return cfg<>.run(run_cfg{.argc = argc,
                             .argv = const_cast<const char**>(argv)});
}
