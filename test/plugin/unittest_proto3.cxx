#include "unittest_proto3.proto23.h"
#include <proto23/options.proto23.h>
static_assert(std::is_trivially_copyable_v<proto23::inplace_string<64>>);
static_assert(std::is_trivially_copyable_v<proto23::inplace_vector<long, 16>>);
