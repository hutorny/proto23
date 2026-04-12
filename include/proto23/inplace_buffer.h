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

#include <algorithm>
#include <array>
#include <cstring>
#include <streambuf>

namespace proto23 {

// Fixed-capacity streambuf with internal storage of N bytes.
//
// Output: bytes up to N are stored; excess bytes are counted but discarded.
// size() always returns the total bytes written (including overflow).
// overflowed() indicates whether the written data exceeded capacity.
//
// Roundtrip use: after serialization, call flip() to set up the get area
// so the same buffer can be passed to an istream for deserialization.
//
// Usage with ostream:
//   inplace_buffer<1024> buf;
//   std::ostream out(&buf);
//   proto23::serialize(out, msg);
//   if (!buf.overflowed()) {
//       // buf.data() / buf.size() — ready to send
//   }
//
// Usage for roundtrip:
//   inplace_buffer<1024> buf;
//   std::ostream out(&buf);
//   proto23::serialize(out, original);
//   buf.flip();
//   std::istream in(&buf);
//   proto23::deserialize(in, copy);
template<std::size_t N>
class inplace_buffer : public std::streambuf {
public:
    inplace_buffer() noexcept {
        // Zero-size put area forces all writes through overflow()/xsputn().
        setp(storage_.data(), storage_.data());
    }

    // Total bytes written, including any that exceeded capacity.
    std::size_t size() const noexcept { return total_size_; }

    // True when more bytes were written than the buffer can hold.
    bool overflowed() const noexcept { return total_size_ > N; }

    // Pointer to the internal storage (valid for min(size(), capacity()) bytes).
    const char* data() const noexcept { return storage_.data(); }

    // Compile-time buffer capacity.
    static constexpr std::size_t capacity() noexcept { return N; }

    // Transition from write mode to read mode.
    // Sets up the get area over the bytes that were actually stored (up to N).
    // Call this after serialization, before passing the buffer to an istream.
    void flip() noexcept {
        char* base = storage_.data();
        auto readable = static_cast<std::ptrdiff_t>(std::min(total_size_, N));
        setg(base, base, base + readable);
    }

    // Reset to initial write state, discarding all written data.
    void reset() noexcept {
        total_size_ = 0;
        setp(storage_.data(), storage_.data());
        setg(nullptr, nullptr, nullptr);
    }

protected:
    // Handles single-character writes when the put area is exhausted (always).
    int_type overflow(int_type ch) override {
        if (!traits_type::eq_int_type(ch, traits_type::eof())) {
            if (total_size_ < N)
                storage_[total_size_] = traits_type::to_char_type(ch);
            ++total_size_;
        }
        return traits_type::not_eof(ch);
    }

    // Supports tellg() and seekg() on the read-mode buffer.
    // Required because the deserialize loop uses in.tellg() to track position.
    pos_type seekoff(off_type off, std::ios_base::seekdir dir,
                     std::ios_base::openmode which) override {
        if ((which & std::ios_base::in) != std::ios_base::in || eback() == nullptr)
            return pos_type{off_type{-1}};
        char* target{};
        switch (dir) {
        case std::ios_base::beg: target = eback() + off; break;
        case std::ios_base::cur: target = gptr()  + off; break;
        case std::ios_base::end: target = egptr() + off; break;
        default: return pos_type{off_type{-1}};
        }
        if (target < eback() || target > egptr())
            return pos_type{off_type{-1}};
        setg(eback(), target, egptr());
        return pos_type{static_cast<off_type>(target - eback())};
    }

    pos_type seekpos(pos_type sp, std::ios_base::openmode which) override {
        return seekoff(off_type(sp), std::ios_base::beg, which);
    }

    // Handles bulk writes from ostream::write().
    // Stores as many bytes as fit; counts all bytes regardless.
    std::streamsize xsputn(const char_type* s, std::streamsize count) override {
        if (count <= 0) return 0;
        if (total_size_ < N) {
            auto to_copy = std::min(static_cast<std::size_t>(count), N - total_size_);
            std::memcpy(storage_.data() + total_size_, s, to_copy);
        }
        total_size_ += static_cast<std::size_t>(count);
        return count;
    }

private:
    std::array<char, N> storage_{};
    std::size_t total_size_{0};
};

} // namespace proto23
