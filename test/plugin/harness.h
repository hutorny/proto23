#pragma once
#include <proto23/proto23.h>

namespace testing::harness {
template<typename Message>
void one(std::istream& in, std::ostream& out) {
    Message msg{};
    proto23::serialize(out, msg);
    std::ignore = proto23::deserialize(in, msg);
}
template<typename ... Message>
void all(std::istream& in, std::ostream& out) {
    (one<Message>(in, out), ...);
}

}
