#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace dingosdk::https {
struct Download {
    bool ok{};
    unsigned long http_status{}, error{};
};
// Streams an HTTPS GET into `destination`, which must not exist yet. Follows
// redirects that stay on HTTPS; refuses bodies over `max_bytes` or transfers
// that take longer than `timeout_seconds` in total.
Download get(std::wstring_view url, const std::filesystem::path& destination,
    std::uint64_t max_bytes, std::uint32_t timeout_seconds, const wchar_t* agent);
// The same GET handed to `sink` piece by piece as it arrives, with the size the server
// announced (0 when it named none); `sink` returning false stops it (error ERROR_CANCELLED).
// A body shorter than announced is a failure.
using Sink = std::function<bool(const char* data, std::size_t size, std::uint64_t announced)>;
Download stream(std::wstring_view url, std::uint64_t max_bytes, std::uint32_t timeout_seconds,
    const wchar_t* agent, const Sink& sink);
// What a GET of `url` would download, from a HEAD request (redirects followed); nothing when
// it fails or the server names no size.
std::optional<std::uint64_t> download_size(std::wstring_view url, const wchar_t* agent);
// The body of the same GET, for an answer small enough to hold: nothing when it
// fails, and `result`, when given, says how.
std::optional<std::string> get_text(std::wstring_view url, std::uint64_t max_bytes,
    std::uint32_t timeout_seconds, const wchar_t* agent, Download* result = nullptr);
}
