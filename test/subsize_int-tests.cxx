/*
 * Copyright (C) 2026 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * subsize_int-tests.cxx - 
 *
 * Licensed under MIT License, see full text in LICENSE
 * or visit page https://opensource.org/license/mit/
 */

#include "functional-tests.h"
#include "data.h"

#include <iostream>
#include <spanstream>

namespace testing {

static_assert(proto23::detail::zigzag_integral<std::int8_t>);
static_assert(proto23::detail::zigzag_integral<std::int16_t>);
static_assert(proto23::detail::variant_integral<std::uint8_t>);
static_assert(proto23::detail::variant_integral<std::uint16_t>);

struct StdInt_16   { std::int16_t  v{};  bool operator==(const StdInt_16&)  const = default; using Model = Fields<Field<&StdInt_16::v,  1>>; };
struct StdUInt_16   { std::uint16_t  v{};  bool operator==(const StdUInt_16&)  const = default; using Model = Fields<Field<&StdUInt_16::v,  1>>; };
struct StdInt_8   { std::int8_t  v{};  bool operator==(const StdInt_8&)  const = default; using Model = Fields<Field<&StdInt_8::v,  1>>; };
struct StdUInt_8   { std::uint8_t  v{};  bool operator==(const StdUInt_8&)  const = default; using Model = Fields<Field<&StdUInt_8::v,  1>>; };

const Item<StdInt_16> StdInt_16_tests[] {
    {{-32767}, "\x08\xFD\xFF\x03"sv },
    {{-257}, "\x08\x81\x04"sv },
    {{-256}, "\x08\xFF\x03"sv },
    {{-129}, "\x08\x81\x02"sv },
    {{-128}, "\x08\xFF\x01"sv },
    {{-17}, "\x08\x21"sv },
    {{-16}, "\x08\x1F"sv },
    {{-2}, "\x08\x03"sv },
    {{-1}, "\x08\x01"sv },
    {{0}, ""sv },
    {{1}, "\x08\x02"sv },
    {{2}, "\x08\x04"sv },
    {{15}, "\x08\x1E"sv },
    {{16}, "\x08\x20"sv },
    {{127}, "\x08\xFE\x01"sv },
    {{128}, "\x08\x80\x02"sv },
    {{255}, "\x08\xFE\x03"sv },
    {{256}, "\x08\x80\x04"sv },
    {{32767}, "\x08\xFE\xFF\x03"sv },
};
const Item<StdInt_8> StdInt_8_tests[] {
    {{-128}, "\x08\xFF\x01"sv },
    {{-17}, "\x08\x21"sv },
    {{-16}, "\x08\x1F"sv },
    {{-2}, "\x08\x03"sv },
    {{-1}, "\x08\x01"sv },
    {{0}, ""sv },
    {{1}, "\x08\x02"sv },
    {{2}, "\x08\x04"sv },
    {{15}, "\x08\x1E"sv },
    {{16}, "\x08\x20"sv },
    {{127}, "\x08\xFE\x01"sv },
};

const Item<StdInt_16> StdInt_16_overflow_tests[] {
    {{-32768}, "\x08\xFF\xFF\xFF\xFF\xFF\x3F"sv }, // -1099511627776
    {{-32768}, "\x08\xFD\xFF\xFF\xFF\x0F"sv }, // -2147483647
    {{-32768}, "\x08\xFF\xFF\xFF\x07"sv }, // -8388608
    {{-32768}, "\x08\xFD\xFF\xFF\x07"sv }, // -8388607
    {{-32768}, "\x08\xFF\xFF\x07"sv }, // -65536
    {{-32768}, "\x08\xFD\xFF\x07"sv }, // -65535
    {{32767}, "\x08\x80\x80\x04"sv }, // 32768
    {{32767}, "\x08\xFE\xFF\x07"sv }, // 65535
    {{32767}, "\x08\x80\x80\x08"sv }, // 65536
    {{32767}, "\x08\xFE\xFF\xFF\x07"sv }, // 8388607
    {{32767}, "\x08\x80\x80\x80\x08"sv }, // 8388608
    {{32767}, "\x08\xFE\xFF\xFF\xFF\x0F"sv }, // 2147483647
    {{32767}, "\x08\x80\x80\x80\x80\x80\x40"sv }, // 1099511627776
};

const Item<StdInt_8> StdInt_8_overflow_tests[] {
    {{-128}, "\x08\xFF\xFF\xFF\xFF\xFF\x3F"sv }, // -1099511627776
    {{-128}, "\x08\xFD\xFF\xFF\xFF\x0F"sv }, // -2147483647
    {{-128}, "\x08\xFF\xFF\xFF\x07"sv }, // -8388608
    {{-128}, "\x08\xFD\xFF\xFF\x07"sv }, // -8388607
    {{-128}, "\x08\xFF\xFF\x07"sv }, // -65536
    {{-128}, "\x08\xFD\xFF\x07"sv }, // -65535
    {{-128}, "\x08\xFF\xFF\x03"sv }, // -32768
    {{-128}, "\x08\xFD\xFF\x03"sv }, // -32767
    {{-128}, "\x08\x81\x04"sv }, // -257
    {{-128}, "\x08\xFF\x03"sv }, // -256
    {{-128}, "\x08\x81\x02"sv }, // -129
    {{127}, "\x08\x80\x02"sv }, // 128
    {{127}, "\x08\xFE\x03"sv }, // 255
    {{127}, "\x08\x80\x04"sv }, // 256
    {{127}, "\x08\xFE\xFF\x03"sv }, // 32767
    {{127}, "\x08\x80\x80\x04"sv }, // 32768
    {{127}, "\x08\xFE\xFF\x07"sv }, // 65535
    {{127}, "\x08\x80\x80\x08"sv }, // 65536
    {{127}, "\x08\xFE\xFF\xFF\x07"sv }, // 8388607
    {{127}, "\x08\x80\x80\x80\x08"sv }, // 8388608
    {{127}, "\x08\xFE\xFF\xFF\xFF\x0F"sv }, // 2147483647
    {{127}, "\x08\x80\x80\x80\x80\x80\x40"sv }, // 1099511627776
};

const Item<StdUInt_16> StdUInt_16_tests[] {
  {{0}, ""sv },
  {{1}, "\x08\x01"sv },
  {{2}, "\x08\x02"sv },
  {{15}, "\x08\x0F"sv },
  {{16}, "\x08\x10"sv },
  {{127}, "\x08\x7F"sv },
  {{128}, "\x08\x80\x01"sv },
  {{255}, "\x08\xFF\x01"sv },
  {{32767}, "\x08\xFF\xFF\x01"sv },
  {{32768}, "\x08\x80\x80\x02"sv },
  {{65535}, "\x08\xFF\xFF\x03"sv },
};

const Item<StdUInt_16> StdUInt_16_overflow_tests[] {
  {{65535}, "\x08\x80\x80\x04"sv }, // 65536
  {{65535}, "\x08\xFF\xFF\xFF\x03"sv }, // 8388607
  {{65535}, "\x08\x80\x80\x80\x04"sv }, // 8388608
  {{65535}, "\x08\xFF\xFF\xFF\xFF\x07"sv }, // 2147483647
  {{65535}, "\x08\x80\x80\x80\x80\x08"sv }, // 2147483648
  {{65535}, "\x08\xFF\xFF\xFF\xFF\x0F"sv }, // 4294967295
  {{65535}, "\x08\x80\x80\x80\x80\x80\x20"sv }, // 1099511627776
};

const Item<StdUInt_8> StdUInt_8_tests[] {
  {{0_u64}, ""sv },
  {{1_u64}, "\x08\x01"sv },
  {{2_u64}, "\x08\x02"sv },
  {{15_u64}, "\x08\x0F"sv },
  {{16_u64}, "\x08\x10"sv },
  {{127_u64}, "\x08\x7F"sv },
  {{128_u64}, "\x08\x80\x01"sv },
  {{255_u64}, "\x08\xFF\x01"sv },
};

const Item<StdUInt_8> StdUInt_8_overflow_tests[] {
  {{255}, "\x08\xFF\xFF\x01"sv }, // 32767
  {{255}, "\x08\x80\x80\x02"sv }, // 32768
  {{255}, "\x08\xFF\xFF\x03"sv }, // 65535
  {{255}, "\x08\x80\x80\x04"sv }, // 65536
  {{255}, "\x08\xFF\xFF\xFF\x03"sv }, // 8388607
  {{255}, "\x08\x80\x80\x80\x04"sv }, // 8388608
  {{255}, "\x08\xFF\xFF\xFF\xFF\x07"sv }, // 2147483647
  {{255}, "\x08\x80\x80\x80\x80\x08"sv }, // 2147483648
  {{255}, "\x08\xFF\xFF\xFF\xFF\x0F"sv }, // 4294967295
  {{255}, "\x08\x80\x80\x80\x80\x80\x20"sv }, // 1099511627776
};


suite<"subsized"> subsized_suite = [] {
    add_tests(StdInt_16_tests);
    add_tests(StdUInt_16_tests);
    add_tests(StdInt_8_tests);
    add_tests(StdUInt_8_tests);
    add_overflow_tests(StdInt_16_overflow_tests);
    add_overflow_tests(StdUInt_16_overflow_tests);
    add_overflow_tests(StdInt_8_overflow_tests);
    add_overflow_tests(StdUInt_8_overflow_tests);
};
}

