/*
 * Copyright (C) 2026 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * diagnostic-test.cxx - Unit tests with proto23::diagnostic::explain.
 *
 * Licensed under MIT License, see full text in LICENSE
 * or visit page https://opensource.org/license/mit/
 */

#include <spanstream>
#include <proto23/proto23.h>
#include <proto23/diagnostic.h>
#include "functional-tests.h"
namespace testing {
using namespace proto23;
using namespace proto23::diagnostic;

suite<"diagnostic"> diagnostic_suite = [] {
    test("explanation contains success on parse_error::none") = [] {
        const std::string explanation = explain{parse_error::none};
        expect(eq(explanation, std::string{"success"}));
    };
    test("explanation contains data type on parse_error::type_mismatch") = [] {
        const std::string explanation = explain{parse_error::type_mismatch};
        expect(explanation.contains("data type"));
    };
    test("explanation does contains string length on parse_error::invalid_length") = [] {
        const std::string explanation = explain{parse_error::invalid_length};
        expect(explanation.contains("string length"));
    };
    test("explanation written to ostream contains tellg parse_error::tellg_fail") = [] {
        std::ostringstream out{};
        out << explain{parse_error::tellg_fail};
        expect(out.str().contains("tellg"));
    };
    test("explanation written to ostream contains 6 lines on all errors") = [] {
        std::ostringstream out{};
        out << explain{parse_error::type_mismatch | parse_error::integer_overflow | parse_error::array_overrun
               | parse_error::string_overrun | parse_error::invalid_length | parse_error::tellg_fail};
        const auto str = out.str();
        expect(eq(std::count(str.begin(), str.end(), '\n'), 5));
    };
    test("explanation contains expected number of delimiters") = [] {
        const std::string str = explain{parse_error::type_mismatch | parse_error::integer_overflow | parse_error::array_overrun
               | parse_error::string_overrun | parse_error::tellg_fail, "\t"};
        expect(eq(std::count(str.begin(), str.end(), '\t'), 4));
    };
};


} // namespace testing
