/*
 * Copyright (C) 2026 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * proto23/inplace_buffer.h - Fixed-size in-place streambuf for proto23
 * serialization/deserialization
 *
 * Licensed under MIT License, see full text in LICENSE
 * or visit page https://opensource.org/license/mit/
 */
#pragma once
// This file is optional, it imports 3rd party inplace_string and inplace_vector
#if __has_include(<mp/inplace_string.h>)
#include <mp/inplace_string.h>
namespace proto23 {
using mp::inplace_string;
}// namespace proto23
#endif
#if __has_include(<beman/inplace_vector/inplace_vector.hpp>)
#include <beman/inplace_vector/inplace_vector.hpp>
namespace proto23 {
using beman::inplace_vector::inplace_vector;
}// namespace proto23
#endif
