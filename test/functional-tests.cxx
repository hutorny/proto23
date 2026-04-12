#include "functional-tests.h"
#include "data.h"

#include <iostream>
#include <spanstream>

namespace testing {

template<typename Type>
concept signed_number = proto23::detail::anyof<is_same, Type, int32, sint32, sfixed32, int64, sint64, sfixed64>();

template<signed_number Type>
constexpr auto operator-(Type value) {
  return static_cast<Type>(-static_cast<underlying_type_t<Type>>(value));
}

static_assert(signed_number<sfixed32>);
static_assert(same_as<sfixed32,decltype(-sfixed32{1})>);

inline constexpr bool operator==(const Union& a, const Union& b) {
  return a.v == b.v && (
      (a.v == 1 && std::abs(a.value - b.value) < 1e-6) ||
      (a.v != 1 && a.count == b.count));
}

static void add_pointer_tests(std::span<const Item<Pointer>> tests) {
  for(auto& item : tests) {
    test(test_name(item.first)) = [&item] {
      std::ispanstream in { item.second };
      Pointer actual {};
      should("deserialize") = [&]() mutable {
        const auto result = deserialize(in, actual);
        expect(result.errors() == parse_error::none);
        expect(neq(any(result), item.second.empty()));
      };
      expect(eq(actual.v, item.first.v));
      expect(eq(!!actual.inner, !!item.first.inner));
      if (actual.inner && item.first.inner) {
        expect(eq(*actual.inner, *item.first.inner));
      }
    };
  }
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#include "test-data.inc"
#pragma GCC diagnostic pop

const Item<Bytes> Bytes_tests[] {
  {{make_bytes("simple bytes")}, "\x0A\x0C\x73\x69\x6D\x70\x6C\x65\x20\x62\x79\x74\x65\x73"sv },
};

const Item<ByteArray> ByteArray_tests[] {
  {{make_byte_array<12>("simple bytes")}, "\x0A\x0C\x73\x69\x6D\x70\x6C\x65\x20\x62\x79\x74\x65\x73"sv },
};

suite<"numeric"> numeric_suite = [] {
    add_tests(
        Bool__tests,
        SInt_32_tests,
        Int_32_tests,
        UInt_32_tests,
        Fixed_32_tests,
        SFixed_32_tests,
        SInt_64_tests,
        Int_64_tests,
        UInt_64_tests,
        Fixed_64_tests,
        SFixed_64_tests,
        Double__tests,
        Float__tests
    );
};
suite<"pairs"> pairs_suite = [] {
    add_tests(
        Bool_SInt_tests,
        SInt_String_tests,
        UInt_Fixed_tests,
        Double_SFixed_tests
    );
};
suite<"vectors"> vectors_suite = [] {
    add_tests(
        Bool_v_tests,
        Int_32v_tests,
        UInt_32v_tests,
        SInt_32v_tests,
        Fixed_32v_tests,
        SFixed_32v_tests,
        Int_64v_tests,
        UInt_64v_tests,
        SInt_64v_tests,
        Fixed_64v_tests,
        SFixed_64v_tests,
        Double_v_tests,
        Float_v_tests
    );
};
suite<"unpacked"> unpacked_suite = [] {
    add_tests(
        Bool_u_tests,
        Int_32u_tests,
        UInt_32u_tests,
        SInt_32u_tests,
        Fixed_32u_tests,
        SFixed_32u_tests,
        Int_64u_tests,
        UInt_64u_tests,
        SInt_64u_tests,
        Fixed_64u_tests,
        SFixed_64u_tests,
        Double_u_tests,
        Float_u_tests
    );
};

suite<"messages"> messages_suite = [] {
    add_tests(
        Outer_tests,
        Combined_tests
    );
};

suite<"strings"> strings_suite = [] {
    add_tests(
        String_tests,
        Chars_tests,
        Bytes_tests,
        CharArray_tests,
        ByteArray_tests
    );
};

suite<"oneof"> oneof_suite = [] {
    add_tests(
        Variant_tests
    );
};

suite<"oneof"> union_suite = [] {
    // union has no intrinsic discriminator, selecting which filed to serialize not possible
    add_test(Union_tests, direction::deserialize);
};

const Item<Pointer> Pointer_tests[] {
  {{std::make_unique<Inner>(Status::OK, 2),1}, "\x0A\x04\x08\x2A\x10\x04\x10\x02"sv },
  {{{},2}, "\x10\x04"sv },
  {{std::make_unique<Inner>(Status::NOTOK, 65535),8388607}, "\x0A\x04\x10\xFE\xFF\x07\x10\xFE\xFF\xFF\x07"sv },
  {{std::make_unique<Inner>(Status::NOTOK, 65535)}, "\x0A\x04\x10\xFE\xFF\x07"sv },
};

suite<"optional"> optional_suite = [] {
    add_tests(
        Optional_tests
    );
};

suite<"pointer"> pointer_suite = [] {
    add_pointer_tests(Pointer_tests);
};

suite<"map"> map_suite = [] {
    add_test(Map_tests);
};

suite<"map"> unorderd_map_suite = [] {
    // Order of items in unorderd map is not guaranteed, wire may data mismatch, making serialize tests not possible
    add_test(UnorderedMap_tests, direction::deserialize | direction::roundtrip);
};

suite<"deserialize_result"> deserialize_result_tests = [] {
    test("has<>() returns false if nothing is read") = [] {
        Inner cute {};
        std::ispanstream in {""sv};
        const auto result = deserialize(in, cute);
        expect(!result.has<&Inner::status>());
        expect(!result.has<&Inner::v>());
    };
    test("has<>() returns true if field has been read") = [] {
        Inner cute {};
        std::ispanstream in {"\x08\x2A"sv};
        const auto result = deserialize(in, cute);
        expect(result.has<&Inner::status>());
    };
    test("has<>() returns false if field has not been read") = [] {
        Inner cute {};
        std::ispanstream in {"\x10\x02"sv};
        const auto result = deserialize(in, cute);
        expect(!result.has<&Inner::status>());
    };
    test("has<>() returns true for union alternative that has been read") = [] {
        OneOfUnion cute{};
        std::ispanstream in {"\x18\x02"sv};
        const auto result = deserialize(in, cute);
        expect(result.has<&OneOfUnion::count>());
        expect(!result.has<&OneOfUnion::value>());
    };
    test("variant contains variants alternative that has been read") = [] {
        OneOfVariant cute{};
        std::ispanstream in {"\x18\x02"sv};
        const auto result = deserialize(in, cute);
        expect(result.has<&OneOfVariant::oneof>());
        expect(std::holds_alternative<int>(cute.oneof));
    };
};
} // namespace testing
