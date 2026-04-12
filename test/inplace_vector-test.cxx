/*
 * Copyright (C) 2026 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * inplace-test.cxx - Unit tests with inplace_vector.
 *
 * Licensed under MIT License, see full text in LICENSE
 * or visit page https://opensource.org/license/mit/
 */

#include <spanstream>
#include <beman/inplace_vector/inplace_vector.hpp>
#include <proto23/proto23.h>
#include "functional-tests.h"
namespace testing {
using namespace proto23;
using namespace beman::inplace_vector;

struct UInt_32iv  {
  inplace_vector<uint32, 8> v{};
  bool operator==(const UInt_32iv&)  const = default;
  using Model = Fields<Field<&UInt_32iv::v,  1>>;
};

const Item<UInt_32iv> UInt_32iv_tests[] {
  {{{30_u32,31_u32,32_u32,33_u32,}}, "\x0A\x04\x1E\x1F\x20\x21"sv },
};

template<typename T>
bool span_equal(std::span<T> a, std::span<T> b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end());
}

suite<"inplace_vector"> inplace_vectors_suite = [] {
    add_tests(UInt_32iv_tests);
    test("vector->inplace_vector roundtrip with overrun") = [] {
        UInt_32v original{{1_u32,2_u32,3_u32,4_u32,5_u32,6_u32,7_u32,8_u32,9_u32,10_u32}};
        proto23::inplace_buffer<128> buf;
        std::ostream out(&buf);
        proto23::serialize(out, original);
        expect(that % !buf.overflowed());
        buf.flip();
        std::istream in(&buf);
        UInt_32iv under_test{};
        const auto result = proto23::deserialize(in, under_test);
        expect(that % all(result));
        expect(eq(under_test.v.size(), under_test.v.max_size()));
        expect(that % all(result));
        expect(that % span_equal(std::span{under_test.v}, std::span{original.v}.subspan(0, under_test.v.size())));
    };
};


} // namespace testing
