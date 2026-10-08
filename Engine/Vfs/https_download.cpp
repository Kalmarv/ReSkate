#include "https_download.h"
#include <Windows.h>
#include <winhttp.h>
#include <array>
#include <atomic>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace dingosdk::https {
namespace {
struct Handle {
    HANDLE value{INVALID_HANDLE_VALUE};
    ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
struct Internet {
    HINTERNET value{};
    ~Internet() { if (value) WinHttpCloseHandle(value); }
};
// A GET (or HEAD) sent and answered with 200: the handles stay open for reading the body.
struct Exchange {
    Internet session, connection, request;
    std::uint64_t started{};
};
// Sends the request; false with result.error set when it could not be sent or was not answered with 200.
bool send(Exchange& exchange, std::wstring_view url, const wchar_t* verb, const wchar_t* agent, Download& result) {
    const auto fail = [&](DWORD error) { result.error = error; return false; };
    if (url.empty() || url.size() >= 4096 || url.find_first_of(L"\r\n\t #") != std::wstring_view::npos)
        return fail(ERROR_INVALID_PARAMETER);
    const std::wstring address(url);
    URL_COMPONENTS parts{}; parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength =
        parts.dwUserNameLength = parts.dwPasswordLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(address.c_str(), static_cast<DWORD>(address.size()), 0, &parts) ||
        parts.nScheme != INTERNET_SCHEME_HTTPS || !parts.dwHostNameLength || parts.dwUserNameLength || parts.dwPasswordLength)
        return fail(ERROR_INVALID_PARAMETER);
    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring resource = parts.dwUrlPathLength ? std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength) : L"/";
    if (parts.dwExtraInfoLength) resource.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    exchange.session.value = WinHttpOpen(agent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    if (!exchange.session.value) return fail(GetLastError());
    if (!WinHttpSetTimeouts(exchange.session.value, 5000, 5000, 10000, 10000)) return fail(GetLastError());
    exchange.connection.value = WinHttpConnect(exchange.session.value, host.c_str(), parts.nPort, 0);
    if (!exchange.connection.value) return fail(GetLastError());
    exchange.request.value = WinHttpOpenRequest(exchange.connection.value, verb, resource.c_str(), nullptr, nullptr,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!exchange.request.value) return fail(GetLastError());
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    DWORD logon = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
    if (!WinHttpSetOption(exchange.request.value, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect)) ||
        !WinHttpSetOption(exchange.request.value, WINHTTP_OPTION_AUTOLOGON_POLICY, &logon, sizeof(logon))) return fail(GetLastError());
    exchange.started = GetTickCount64();
    if (!WinHttpSendRequest(exchange.request.value, nullptr, 0, nullptr, 0, 0, 0) || !WinHttpReceiveResponse(exchange.request.value, nullptr))
        return fail(GetLastError());
    DWORD size = sizeof(result.http_status);
    if (!WinHttpQueryHeaders(exchange.request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            nullptr, &result.http_status, &size, nullptr)) return fail(GetLastError());
    if (result.http_status != 200) return fail(ERROR_BAD_NET_RESP);
    return true;
}
// The Content-Length of an answered request, 0 when it names none. Read as text: a 32-bit
// number would wrap past 4 GB, and not every WinHTTP takes WINHTTP_QUERY_FLAG_NUMBER64.
std::uint64_t content_length(const Exchange& exchange) {
    std::array<wchar_t, 32> text{};
    DWORD size = sizeof(text);
    if (!WinHttpQueryHeaders(exchange.request.value, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX,
                             text.data(), &size, WINHTTP_NO_HEADER_INDEX)) return 0;
    std::uint64_t length{};
    for (const auto ch : std::wstring_view(text.data())) {
        if (ch < L'0' || ch > L'9' || length > (UINT64_MAX - 9) / 10) return 0;
        length = length * 10 + static_cast<std::uint64_t>(ch - L'0');
    }
    return length;
}
}

Download get(std::wstring_view url, const std::filesystem::path& destination,
    std::uint64_t max_bytes, std::uint32_t timeout_seconds, const wchar_t* agent) {
    Download result;
    const auto fail = [&](DWORD error) { result.error = error; return result; };
    Exchange exchange;
    if (!send(exchange, url, L"GET", agent, result)) return result;
    if (content_length(exchange) > max_bytes) return fail(ERROR_FILE_TOO_LARGE);
    Handle output{CreateFileW(destination.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (output.value == INVALID_HANDLE_VALUE) return fail(GetLastError());
    std::array<char, 65536> buffer{};
    std::uint64_t total{};
    for (;;) {
        if (GetTickCount64() - exchange.started >= std::uint64_t{timeout_seconds} * 1000) return fail(ERROR_TIMEOUT);
        DWORD read{}, written{};
        if (!WinHttpReadData(exchange.request.value, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) return fail(GetLastError());
        if (!read) break;
        if (total + read > max_bytes) return fail(ERROR_FILE_TOO_LARGE);
        if (!WriteFile(output.value, buffer.data(), read, &written, nullptr) || written != read) return fail(ERROR_WRITE_FAULT);
        total += read;
    }
    if (!total) return fail(ERROR_BAD_LENGTH);
    if (!FlushFileBuffers(output.value)) return fail(GetLastError());
    result.ok = true;
    return result;
}

Download stream(std::wstring_view url, std::uint64_t max_bytes, std::uint32_t timeout_seconds, const wchar_t* agent,
    const Sink& sink) {
    Download result;
    const auto fail = [&](DWORD error) { result.error = error; return result; };
    Exchange exchange;
    if (!send(exchange, url, L"GET", agent, result)) return result;
    const auto announced = content_length(exchange);
    if (announced > max_bytes) return fail(ERROR_FILE_TOO_LARGE);
    std::array<char, 65536> buffer{};
    std::uint64_t total{};
    for (;;) {
        if (GetTickCount64() - exchange.started >= std::uint64_t{timeout_seconds} * 1000) return fail(ERROR_TIMEOUT);
        DWORD read{};
        if (!WinHttpReadData(exchange.request.value, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) return fail(GetLastError());
        if (!read) break;
        if (total + read > max_bytes) return fail(ERROR_FILE_TOO_LARGE);
        total += read;
        if (!sink(buffer.data(), read, announced)) return fail(ERROR_CANCELLED);
    }
    if (!total) return fail(ERROR_BAD_LENGTH);
    if (announced && total != announced) return fail(ERROR_BAD_LENGTH);
    result.ok = true;
    return result;
}

std::optional<std::uint64_t> download_size(std::wstring_view url, const wchar_t* agent) {
    Download result;
    Exchange exchange;
    if (!send(exchange, url, L"HEAD", agent, result)) return std::nullopt;
    if (const auto length = content_length(exchange)) return length;
    return std::nullopt;
}

std::optional<std::string> get_text(std::wstring_view url, std::uint64_t max_bytes,
    std::uint32_t timeout_seconds, const wchar_t* agent, Download* result) {
    static std::atomic<unsigned> serial{};
    wchar_t folder[MAX_PATH + 1]{};
    const auto length = GetTempPathW(MAX_PATH, folder);
    const auto path = std::filesystem::path(length ? std::wstring(folder, length) : L".") /
        (L"reskate-get-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++serial) + L".tmp");
    std::error_code error;
    std::filesystem::remove(path, error); // get needs a fresh destination
    const auto download = get(url, path, max_bytes, timeout_seconds, agent);
    if (result) *result = download;
    std::optional<std::string> text;
    if (download.ok) {
        std::ifstream in(path, std::ios::binary);
        text.emplace(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    std::filesystem::remove(path, error);
    return text;
}
}
