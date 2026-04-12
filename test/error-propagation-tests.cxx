/*
 * Copyright (C) 2026 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * error-propagation-tests.cxx - proto23 tests for error propagation
 *
 * Licensed under MIT License, see full text in LICENSE
 * or visit page https://opensource.org/license/mit/
 */




// ---------------------------------------------------------------------------
// Nested message test structs
// ---------------------------------------------------------------------------
#include "functional-tests.h"
#include <spanstream>

namespace testing {
using namespace boost::ut;
using namespace proto23;
using namespace std::literals;

struct MismatchingInner {
    double value{};
    std::int8_t v{};
    using Model = Fields<Field<&MismatchingInner::value, 1>, Field<&MismatchingInner::v, 2>>;
};

struct MismatchingOuter {
    MismatchingInner inner{};
    std::uint8_t   v{};
    using Model = Fields<Field<&MismatchingOuter::inner, 1>, Field<&MismatchingOuter::v, 2>>;
};

struct MismatchingCombined {
    MismatchingInner inner{};
    MismatchingOuter outer{};
    int   v{};
    using Model = Fields<
        Field<&MismatchingCombined::inner, 1>,
        Field<&MismatchingCombined::outer, 2>,
        Field<&MismatchingCombined::v,     4>>;
};

const Item<MismatchingCombined> Overflow_in_outer
//messages Combined 'v:3;outer:{v:65535; inner:{v:1};};'
    {MismatchingCombined{{},{{{}, 1},255}, 3}, "\x12\x08\x0A\x02\x10\x02\x10\xFE\xFF\x07\x20\x06"sv };

const Item<MismatchingCombined> Type_mismatch_in_inner
//messages Combined 'v:4;outer:{v:255; inner:{status:OK};};'
    {MismatchingCombined{{},{{},255}, 4}, "\x12\x07\x0A\x02\x08\x2A\x10\xFE\x03\x20\x08"sv };

const Item<MismatchingCombined> Overflow_in_inner
//messages Combined 'v:5;outer:{v:1; inner:{v:8388607};};'
    {MismatchingCombined{{},{{0, 127},2}, 5}, "\x12\x09\x0A\x05\x10\xFE\xFF\xFF\x07\x10\x02\x20\x0A"sv };

suite<"error_propagation_test"> error_propagation_test = [] {
    test("result contains error when nested structure has int overflow") = [] {
        MismatchingCombined cute {};
        std::ispanstream in {Overflow_in_outer.second};
        const auto result = deserialize(in, cute);
        expect(result.errors() == parse_error::integer_overflow);
        expect(eq(cute, Overflow_in_outer.first));

    };
    test("result contains error when deeply nested structure has type mismatch") = [] {
        MismatchingCombined cute {};
        std::ispanstream in {Type_mismatch_in_inner.second};
        const auto result = deserialize(in, cute);
        expect((result.errors() & parse_error::type_mismatch) != parse_error::none);
    };
    test("result contains error when deeply nested structure has int overflow") = [] {
        MismatchingCombined cute {};
        std::ispanstream in {Overflow_in_inner.second};
        const auto result = deserialize(in, cute);
        expect(result.errors() == parse_error::integer_overflow);
        expect(eq(cute, Overflow_in_inner.first));

    };
};
} // namesapce testing
