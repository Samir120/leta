#include "adapters/http/json_timestamp.hpp"

#include <string>

#include <chrono>
#include <cstddef>

#include "core/timestamp.hpp"

namespace leta::http {

namespace {

constexpr std::size_t YearDigits = 4;
constexpr std::size_t FieldDigits = 2;
constexpr std::size_t MillisecondDigits = 3;

void append_zero_padded(std::string& out, unsigned value, std::size_t width) {
    const std::string digits = std::to_string(value);
    if (digits.size() < width) {
        out.append(width - digits.size(), '0');
    }
    out.append(digits);
}

}  // namespace

void append_json_timestamp(std::string& out, core::Timestamp timestamp) {
    // floor, not a cast: for an instant before 1970 a cast rounds toward zero, into the wrong day.
    const auto day = std::chrono::floor<std::chrono::days>(timestamp);
    const std::chrono::year_month_day date{day};
    const std::chrono::hh_mm_ss time{timestamp - day};

    out.push_back('"');
    append_zero_padded(out, static_cast<unsigned>(static_cast<int>(date.year())), YearDigits);
    out.push_back('-');
    append_zero_padded(out, static_cast<unsigned>(date.month()), FieldDigits);
    out.push_back('-');
    append_zero_padded(out, static_cast<unsigned>(date.day()), FieldDigits);
    out.push_back('T');
    append_zero_padded(out, static_cast<unsigned>(time.hours().count()), FieldDigits);
    out.push_back(':');
    append_zero_padded(out, static_cast<unsigned>(time.minutes().count()), FieldDigits);
    out.push_back(':');
    append_zero_padded(out, static_cast<unsigned>(time.seconds().count()), FieldDigits);
    out.push_back('.');
    append_zero_padded(out, static_cast<unsigned>(time.subseconds().count()), MillisecondDigits);
    out.append("Z\"");
}

}  // namespace leta::http
