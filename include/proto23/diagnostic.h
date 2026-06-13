/*
 * Copyright (C) 2026 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * proto23/diagnostic.h - proto23 diagnostic utility
 *
 * Licensed under MIT License, see full text in LICENSE
 * or visit page https://opensource.org/license/mit/
 */


#pragma once

#include <numeric>
#include <proto23/proto23.h>

namespace proto23::diagnostic {

struct explain {
    explicit explain(parse_error err) : err_{err} {}
    explain(parse_error err, std::string_view delimiter) : err_{err}, dlm_{delimiter} {}
    friend auto& operator<<(std::ostream& out, explain expl) {
        if (expl.err_ == parse_error::none) {
            return out << explain::explanations[0].second;
        }
        std::string_view dlm {};
        for(const auto& e : explain::explanations) {
            if((e.first & expl.err_) != parse_error::none) {
                out << dlm << e.second;
                dlm = expl.dlm_;
            }
        }
        return out;
    }
    operator std::string() const {
        if (err_ == parse_error::none) {
            return explanations[0].second;
        }
        std::string result{};
        result.reserve(errcount(err_) * max_explanation_length);
        std::string_view dlm {};
        for(const auto& e : explanations) {
            if((e.first & err_) != parse_error::none) {
                result += dlm;
                result += e.second;
                dlm = dlm_;
            }
        }
        return result;
    }

    static constexpr std::array<std::pair<parse_error, const char*>, 7> explanations {
        std::pair{ parse_error::none, "success" },
        std::pair{ parse_error::type_mismatch, "input data type mismatches model data type: skipped" },
        std::pair{ parse_error::integer_overflow, "integer value exceeded data type limit: clamped" },
        std::pair{ parse_error::array_overrun, "number of input elements exceed field capacity: skipped" },
        std::pair{ parse_error::string_overrun, "input string length exceed field capacity: skipped" },
        std::pair{ parse_error::invalid_length, "input string length out of range: stall" },
        std::pair{ parse_error::tellg_fail, "input stream does not support tellg(): rejected" },
    };
    static constexpr std::size_t max_explanation_length = std::accumulate(explanations.begin(), explanations.end(),
        std::size_t{0}, [](std::size_t v, auto& e) { return std::max(v, std::string_view{e.second}.size()); });
    static constexpr std::size_t errcount(parse_error err) noexcept {
      return static_cast<std::size_t>(std::popcount(static_cast<std::size_t>(err)));
    }
private:
    parse_error err_{};
    std::string_view dlm_{"\n"};
};
} // namespace proto23::diagnostic

