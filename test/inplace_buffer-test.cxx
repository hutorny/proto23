/*
 * Copyright (C) 2026 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * proto23/test/inplace_buffer-test.cxx - Unit tests for inplace_buffer.
 *
 * Licensed under MIT License, see full text in LICENSE
 */

#include <proto23/inplace_buffer.h>

#include "data.h"

#include <boost/ut.hpp>

#include <cstring>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>

namespace testing {

using namespace boost::ut;
using namespace std::string_literals;

// ===========================================================================
//  Construction
// ===========================================================================
suite<"inplace_buffer/construction"> ibuf_construction = [] {
    test("initial size is zero") = [] {
        proto23::inplace_buffer<64> buf;
        expect(eq(buf.size(), std::size_t{0}));
    };

    test("initial state is not overflowed") = [] {
        proto23::inplace_buffer<64> buf;
        expect(that % !buf.overflowed());
    };

    test("capacity matches template parameter") = [] {
        expect(eq(proto23::inplace_buffer<1>{}.capacity(),    std::size_t{1}));
        expect(eq(proto23::inplace_buffer<64>{}.capacity(),   std::size_t{64}));
        expect(eq(proto23::inplace_buffer<4096>{}.capacity(), std::size_t{4096}));
    };
};

// ===========================================================================
//  Single-character writes (exercises overflow() path)
// ===========================================================================
suite<"inplace_buffer/single_char_writes"> ibuf_put = [] {
    test("one char stored and counted") = [] {
        proto23::inplace_buffer<8> buf;
        std::ostream out(&buf);
        out.put('A');
        expect(eq(buf.size(), std::size_t{1}));
        expect(that % !buf.overflowed());
        expect(eq(buf.data()[0], 'A'));
    };

    test("fill to capacity - not overflowed") = [] {
        proto23::inplace_buffer<4> buf;
        std::ostream out(&buf);
        out.put('a'); out.put('b'); out.put('c'); out.put('d');
        expect(eq(buf.size(), std::size_t{4}));
        expect(that % !buf.overflowed());
        expect(that % (std::memcmp(buf.data(), "abcd", 4) == 0));
    };

    test("one char over capacity - overflowed, count correct") = [] {
        proto23::inplace_buffer<4> buf;
        std::ostream out(&buf);
        out.put('a'); out.put('b'); out.put('c'); out.put('d'); out.put('e');
        expect(eq(buf.size(), std::size_t{5}));
        expect(that % buf.overflowed());
        // first 4 bytes stored intact
        expect(that % (std::memcmp(buf.data(), "abcd", 4) == 0));
    };

    test("excess chars only counted, storage unchanged") = [] {
        proto23::inplace_buffer<2> buf;
        std::ostream out(&buf);
        out.put('X'); out.put('Y');  // fill
        out.put('1'); out.put('2'); out.put('3');  // overflow
        expect(eq(buf.size(), std::size_t{5}));
        expect(that % (std::memcmp(buf.data(), "XY", 2) == 0));
    };
};

// ===========================================================================
//  Bulk writes (exercises xsputn() path)
// ===========================================================================
suite<"inplace_buffer/bulk_writes"> ibuf_write = [] {
    test("write less than capacity") = [] {
        proto23::inplace_buffer<64> buf;
        std::ostream out(&buf);
        out.write("hello", 5);
        expect(eq(buf.size(), std::size_t{5}));
        expect(that % !buf.overflowed());
        expect(that % (std::memcmp(buf.data(), "hello", 5) == 0));
    };

    test("write exactly capacity") = [] {
        proto23::inplace_buffer<5> buf;
        std::ostream out(&buf);
        out.write("hello", 5);
        expect(eq(buf.size(), std::size_t{5}));
        expect(that % !buf.overflowed());
        expect(that % (std::memcmp(buf.data(), "hello", 5) == 0));
    };

    test("write exceeds capacity in one call") = [] {
        proto23::inplace_buffer<4> buf;
        std::ostream out(&buf);
        out.write("hello!", 6);
        expect(eq(buf.size(), std::size_t{6}));
        expect(that % buf.overflowed());
        expect(that % (std::memcmp(buf.data(), "hell", 4) == 0));
    };

    test("overflow spans two writes - boundary correct") = [] {
        proto23::inplace_buffer<8> buf;
        std::ostream out(&buf);
        out.write("abcde", 5);  // 5 bytes fit
        out.write("fghij", 5);  // 3 fit (fills to 8), 2 overflow
        expect(eq(buf.size(), std::size_t{10}));
        expect(that % buf.overflowed());
        expect(that % (std::memcmp(buf.data(), "abcdefgh", 8) == 0));
    };

    test("write after overflow only increments size") = [] {
        proto23::inplace_buffer<4> buf;
        std::ostream out(&buf);
        out.write("abcde", 5);   // 4 stored, 1 overflow
        out.write("xyz", 3);    // all overflow
        expect(eq(buf.size(), std::size_t{8}));
        expect(that % (std::memcmp(buf.data(), "abcd", 4) == 0));
    };

    test("multiple writes accumulate size correctly") = [] {
        proto23::inplace_buffer<32> buf;
        std::ostream out(&buf);
        out.write("aaa", 3);
        out.write("bb", 2);
        out.write("c", 1);
        expect(eq(buf.size(), std::size_t{6}));
        expect(that % !buf.overflowed());
        expect(that % (std::memcmp(buf.data(), "aaabbc", 6) == 0));
    };
};

// ===========================================================================
//  Mixed single-char and bulk writes
// ===========================================================================
suite<"inplace_buffer/mixed_writes"> ibuf_mixed = [] {
    test("put then write") = [] {
        proto23::inplace_buffer<8> buf;
        std::ostream out(&buf);
        out.put('!');
        out.write("hello", 5);
        expect(eq(buf.size(), std::size_t{6}));
        expect(that % (std::memcmp(buf.data(), "!hello", 6) == 0));
    };

    test("write then put - overflow via put") = [] {
        proto23::inplace_buffer<5> buf;
        std::ostream out(&buf);
        out.write("hello", 5);  // fills exactly
        out.put('!');           // triggers overflow
        expect(eq(buf.size(), std::size_t{6}));
        expect(that % buf.overflowed());
        expect(that % (std::memcmp(buf.data(), "hello", 5) == 0));
    };
};

// ===========================================================================
//  flip() — transition to read mode
// ===========================================================================
suite<"inplace_buffer/flip"> ibuf_flip = [] {
    test("read back all written bytes") = [] {
        proto23::inplace_buffer<16> buf;
        std::ostream out(&buf);
        out.write("hello world", 11);
        buf.flip();
        std::istream in(&buf);
        std::string result(11, '\0');
        in.read(result.data(), 11);
        expect(eq(static_cast<std::size_t>(in.gcount()), std::size_t{11}));
        expect(eq(result, "hello world"s));
    };

    test("read yields only stored bytes when overflowed") = [] {
        proto23::inplace_buffer<4> buf;
        std::ostream out(&buf);
        out.write("abcdefgh", 8);  // 4 stored, 4 overflow
        buf.flip();
        std::istream in(&buf);
        std::string result(8, '\0');
        in.read(result.data(), 8);
        expect(eq(static_cast<std::size_t>(in.gcount()), std::size_t{4}));
        expect(eq(result.substr(0, 4), "abcd"s));
    };

    test("single char reads after flip") = [] {
        proto23::inplace_buffer<8> buf;
        std::ostream out(&buf);
        out.put('X'); out.put('Y'); out.put('Z');
        buf.flip();
        std::istream in(&buf);
        expect(eq(in.get(), int{'X'}));
        expect(eq(in.get(), int{'Y'}));
        expect(eq(in.get(), int{'Z'}));
        expect(eq(in.get(), std::char_traits<char>::eof()));
    };

    test("read reports EOF after all bytes consumed") = [] {
        proto23::inplace_buffer<4> buf;
        std::ostream out(&buf);
        out.write("ab", 2);
        buf.flip();
        std::istream in(&buf);
        char c{};
        in.read(&c, 1); expect(eq(c, 'a'));
        in.read(&c, 1); expect(eq(c, 'b'));
        in.read(&c, 1);
        expect(that % in.eof());
    };
};

// ===========================================================================
//  reset()
// ===========================================================================
suite<"inplace_buffer/reset"> ibuf_reset = [] {
    test("reset clears size and overflow flag") = [] {
        proto23::inplace_buffer<4> buf;
        std::ostream out(&buf);
        out.write("hello!", 6);
        expect(that % buf.overflowed());
        buf.reset();
        expect(eq(buf.size(), std::size_t{0}));
        expect(that % !buf.overflowed());
    };

    test("reset allows writing fresh data") = [] {
        proto23::inplace_buffer<8> buf;
        std::ostream out(&buf);
        out.write("first", 5);
        buf.reset();
        out.write("second!!", 8);
        expect(eq(buf.size(), std::size_t{8}));
        expect(that % !buf.overflowed());
        expect(that % (std::memcmp(buf.data(), "second!!", 8) == 0));
    };

    test("reset then flip yields empty read area") = [] {
        proto23::inplace_buffer<8> buf;
        std::ostream out(&buf);
        out.write("data", 4);
        buf.reset();
        buf.flip();
        std::istream in(&buf);
        expect(eq(in.get(), std::char_traits<char>::eof()));
    };
};

// ===========================================================================
//  Proto23 integration
// ===========================================================================
suite<"inplace_buffer/proto23"> ibuf_proto23 = [] {
    test("serialize then deserialize roundtrip") = [] {
        const SInt_String original{42, "hello"};
        proto23::inplace_buffer<64> buf;
        std::ostream out(&buf);
        proto23::serialize(out, original);
        expect(that % !buf.overflowed());
        buf.flip();
        std::istream in(&buf);
        SInt_String result{};
        expect(that % any(proto23::deserialize(in, result)));
        expect(that % (result == original));
    };

    test("size matches ostringstream output") = [] {
        const SInt_String msg{99, "size check string"};
        std::ostringstream oss;
        proto23::serialize(oss, msg);
        const auto expected = oss.str().size();

        proto23::inplace_buffer<256> buf;
        std::ostream out(&buf);
        proto23::serialize(out, msg);
        expect(eq(buf.size(), expected));
    };

    test("overflowed size is still accurate") = [] {
        const SInt_String msg{99, "size check string"};
        std::ostringstream oss;
        proto23::serialize(oss, msg);
        const auto expected = oss.str().size();

        proto23::inplace_buffer<2> tiny;
        std::ostream out(&tiny);
        proto23::serialize(out, msg);
        expect(that % tiny.overflowed());
        expect(eq(tiny.size(), expected));
    };

    test("nested message roundtrip") = [] {
        const Outer original{{Status::OK, 7}, 3};
        proto23::inplace_buffer<64> buf;
        std::ostream out(&buf);
        proto23::serialize(out, original);
        expect(that % !buf.overflowed());
        buf.flip();
        std::istream in(&buf);
        Outer result{};
        expect(that % any(proto23::deserialize(in, result)));
        expect(that % (result.inner.status == original.inner.status));
        expect(eq(result.inner.v, original.inner.v));
        expect(eq(result.v,       original.v));
    };
};

// ===========================================================================
//  Serialization overflow — payload exceeds inplace_buf_size (256 bytes)
//
//  Four serialization paths each maintain a speculative inplace_buffer:
//    1. serialize_value<Msg>        — nested message
//    2. serialize_value<associative> — per map-entry embedded message
//    3. serialize_bounded_array     — packed C-array of varints
//    4. serialize_container (packed) — packed std::vector
//
//  When payload > inplace_buf_size the serializer writes:
//    tag + accumulated_length_prefix | re-serializes payload directly to out
//  These tests verify that path produces correct wire bytes (via roundtrip)
//  and that the byte count matches the expected length.
// ===========================================================================
namespace {

// Minimal message types for overflow scenarios

struct LargePayloadMsg {
    std::string data{};
    bool operator==(const LargePayloadMsg&) const = default;
    using Model = proto23::Fields<proto23::Field<&LargePayloadMsg::data, 1>>;
};

struct WrapperMsg {
    LargePayloadMsg inner{};
    int             tag{};
    bool operator==(const WrapperMsg&) const = default;
    using Model = proto23::Fields<
        proto23::Field<&WrapperMsg::inner, 1>,
        proto23::Field<&WrapperMsg::tag,   2>>;
};

struct LargeVarintArray {
    std::uint64_t arr[30]{};
    bool operator==(const LargeVarintArray& o) const noexcept {
        return std::memcmp(arr, o.arr, sizeof(arr)) == 0;
    }
    using Model = proto23::Fields<proto23::Field<&LargeVarintArray::arr, 1>>;
};

// Produce a string that, when proto-encoded, exceeds inplace_buf_size.
// A 300-char string encodes as tag(1)+length_varint(2)+data(300) = 303 bytes.
inline std::string big_payload(std::size_t n = 300) {
    return std::string(n, 'x');
}

// Serialize to bytes via ostringstream (reference path, does not use inplace_buf).
// Used only to capture bytes for roundtrip deserialization.
template<proto23::message Msg>
std::string capture_wire(const Msg& msg) {
    std::ostringstream oss;
    proto23::serialize(oss, msg);
    return oss.str();
}

} // anonymous namespace

suite<"inplace_buffer/serialization_overflow"> ser_overflow = [] {
    // -----------------------------------------------------------------------
    // 1. Nested message  (serialize_value<Msg> overflow path)
    //
    //   LargePayloadMsg wire: tag(1)+len_varint(2)+data(300) = 303 bytes
    //   WrapperMsg wire:
    //     field1: tag(1)+len_varint(303→2B)+303 = 306 bytes
    //     field2: tag(1)+varint(42) = 2 bytes
    //     total: 308 bytes
    // -----------------------------------------------------------------------
    test("nested/precondition: inner exceeds inplace_buf_size") = [] {
        const LargePayloadMsg inner{ big_payload() };
        const auto wire = capture_wire(inner);
        expect(that % (wire.size() > proto23::detail::inplace_buf_size<LargePayloadMsg>))
            << "inner payload must be larger than the speculative buffer";
    };

    test("nested/roundtrip") = [] {
        const WrapperMsg original{ LargePayloadMsg{big_payload()}, 42 };
        auto iss = std::istringstream{ capture_wire(original) };
        WrapperMsg result{};
        expect(that % any(proto23::deserialize(iss, result)));
        expect(that % (result == original));
    };

    test("nested/wire size") = [] {
        const WrapperMsg msg{ LargePayloadMsg{big_payload(300)}, 42 };
        expect(eq(capture_wire(msg).size(), std::size_t{308}));
    };

    // -----------------------------------------------------------------------
    // 2. Map entry  (serialize_value<associative> overflow path)
    //
    //   MapEntry(key=1, value=300 chars):
    //     key field:   tag(1)+varint(1) = 2 bytes
    //     value field: tag(1)+len_varint(2)+data(300) = 303 bytes
    //     entry total: 305 bytes   → overflows 256-byte speculative buffer
    //   Map wire:
    //     field1 (v=1): tag(1)+varint(1) = 2 bytes
    //     field2 (entry): tag(1)+len_varint(305→2B)+305 = 308 bytes
    //     total: 310 bytes
    // -----------------------------------------------------------------------
    test("map/precondition: entry exceeds inplace_buf_size") = [] {
        // A map entry's wire size = key_field + value_field.
        // Approximate lower bound: value field alone is > 256 bytes.
        expect(that % (big_payload().size() > proto23::detail::inplace_buf_size<Map>));
    };

    test("map/roundtrip") = [] {
        const Map original{ 1, {{1, big_payload()}} };
        auto iss = std::istringstream{ capture_wire(original) };
        Map result{};
        expect(that % any(proto23::deserialize(iss, result)));
        expect(eq(result.v, original.v));
        expect(eq(result.dict.at(1), original.dict.at(1)));
    };

    test("map/wire size") = [] {
        const Map msg{ 1, {{1, big_payload(300)}} };
        expect(eq(capture_wire(msg).size(), std::size_t{310}));
    };

    // -----------------------------------------------------------------------
    // 3. Bounded C-array  (serialize_bounded_array overflow path)
    //
    //   LargeVarintArray: arr[30] of UINT64_MAX, each 10-byte varint
    //   Packed payload: 30×10 = 300 bytes   → overflows 256
    //   Wire: tag(1)+len_varint(300→2B)+300 = 303 bytes
    // -----------------------------------------------------------------------
    test("bounded_array/precondition: packed payload exceeds inplace_buf_size") = [] {
        // 30 × 10-byte varint = 300 bytes packed payload
        constexpr std::size_t payload = 30U * 10U;
        expect(that % (payload > proto23::detail::inplace_buf_size<LargeVarintArray>));
    };

    test("bounded_array/roundtrip") = [] {
        LargeVarintArray original{};
        for (auto& x : original.arr) x = std::numeric_limits<std::uint64_t>::max();
        auto iss = std::istringstream{ capture_wire(original) };
        LargeVarintArray result{};
        expect(that % any(proto23::deserialize(iss, result)));
        expect(that % (result == original));
    };

    test("bounded_array/wire size") = [] {
        LargeVarintArray msg{};
        for (auto& x : msg.arr) x = std::numeric_limits<std::uint64_t>::max();
        expect(eq(capture_wire(msg).size(), std::size_t{303}));
    };

    // -----------------------------------------------------------------------
    // 4. Packed std::vector  (serialize_container packed path overflow)
    //
    //   Double_v: 40 doubles × 8 bytes = 320-byte packed payload → overflows 256
    //   Wire: tag(1)+len_varint(320→2B)+320 = 323 bytes
    // -----------------------------------------------------------------------
    test("packed_vector/precondition: packed payload exceeds inplace_buf_size") = [] {
        constexpr std::size_t payload = 40U * sizeof(double);
        expect(that % (payload > proto23::detail::inplace_buf_size<Double_v>));
    };

    test("packed_vector/roundtrip") = [] {
        Double_v original;
        original.v.reserve(40);
        for (int i = 0; i < 40; ++i)
            original.v.push_back(static_cast<double>(i) * 1.1);
        auto iss = std::istringstream{ capture_wire(original) };
        Double_v result{};
        expect(that % any(proto23::deserialize(iss, result)));
        expect(eq(result.v.size(), original.v.size()));
        expect(that % std::equal(result.v.begin(), result.v.end(),
                                 original.v.begin()));
    };

    test("packed_vector/wire size") = [] {
        Double_v msg;
        msg.v.reserve(40);
        for (int i = 0; i < 40; ++i)
            msg.v.push_back(static_cast<double>(i));
        expect(eq(capture_wire(msg).size(), std::size_t{323}));
    };
};

} // namespace testing
