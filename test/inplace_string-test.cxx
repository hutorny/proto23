/*
 * Copyright (C) 2026 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * inplace_string-test.cxx - Unit tests with inplace_string.
 *
 * Licensed under MIT License, see full text in LICENSE
 * or visit page https://opensource.org/license/mit/
 */
#include <spanstream>
#include <mp/inplace_string.h>
#include <proto23/proto23.h>
#include "functional-tests.h"
namespace testing {
using namespace proto23;
using mp::inplace_string;

struct InPlaceString {
    inplace_string<14> v { };
    bool operator==(const InPlaceString&) const = default;
    using Model = Fields<Field<&InPlaceString::v, 1>>;
};

const Item<InPlaceString> InPlaceString_tests[] {
  {{"simple string"}, "\x0A\x0D\x73\x69\x6D\x70\x6C\x65\x20\x73\x74\x72\x69\x6E\x67"sv },
};
const Item<InPlaceString> ExcessiveInPlaceString_tests
  {{"string with ex"}, "\x0A\x1C\x73\x74\x72\x69\x6E\x67\x20\x77\x69\x74\x68\x20\x65\x78\x63\x65\x73\x73\x69\x76\x65\x20\x6C\x65\x6E\x67\x74\x68"sv };


suite<"inplace_string"> inplace_string_suite = [] {
    add_tests(InPlaceString_tests);
    test("overrun") = [] {
        InPlaceString cute{};
        std::ispanstream in {ExcessiveInPlaceString_tests.second};
        const auto result = proto23::deserialize(in, cute);
        expect(that % all(result));
        expect(result.errors() == parse_error::string_overrun);
        expect(eq(cute.v.size(), cute.v.max_size()));
        expect(eq(cute.v, ExcessiveInPlaceString_tests.first.v));
    };
};


}
