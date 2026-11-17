#include "app.h"
#include "log.h"
#include "netutil.h"
#include "util.h"

#include <wincrypt.h>

#include <algorithm>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")

namespace {

constexpr wchar_t kAgent[] = L"CrowmanStudy/1.2 (study assistant)";

#ifndef CERT_SHA256_HASH_PROP_ID
#define CERT_SHA256_HASH_PROP_ID 21
#endif

std::string LastErrorText(const char* where) {
    const DWORD code = ::GetLastError();
    std::string out = where;
    out += " failed (error ";
    out += std::to_string(code);
    out += ")";
    return out;
}

bool TransientWin32(DWORD code) {
    switch (code) {
        case ERROR_WINHTTP_TIMEOUT:
        case ERROR_WINHTTP_CONNECTION_ERROR:
        case ERROR_WINHTTP_CANNOT_CONNECT:
        case ERROR_WINHTTP_NAME_NOT_RESOLVED:
        case ERROR_WINHTTP_SECURE_FAILURE:
        case ERROR_WINHTTP_RESEND_REQUEST:
        case ERROR_WINHTTP_INTERNAL_ERROR:
            return true;
        default:
            return false;
    }
}

bool TransientStatus(DWORD status) {
    return status == 408 || status == 425 || status == 429 ||
           (status >= 500 && status <= 599);
}

void SleepCancel(int millis, const CancelToken* cancel) {
    for (int waited = 0; waited < millis; waited += 50) {
        if (cancel && cancel->IsCancelled()) return;
        ::Sleep((DWORD)std::min(50, millis - waited));
    }
}

std::string HeaderString(HINTERNET request, DWORD query) {
    DWORD size = 0;
    ::WinHttpQueryHeaders(request, query, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size,
                          WINHTTP_NO_HEADER_INDEX);
    if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER || !size) return {};
    std::wstring buffer(size / sizeof(wchar_t) + 1, L'\0');
    if (!::WinHttpQueryHeaders(request, query, WINHTTP_HEADER_NAME_BY_INDEX, buffer.data(),
                               &size, WINHTTP_NO_HEADER_INDEX))
        return {};
    buffer.resize(size / sizeof(wchar_t));
    return ToUtf8(buffer);
}

bool VerifyPin(HINTERNET request, const std::wstring& expected) {
    if (expected.empty()) return true;
    PCCERT_CONTEXT certificate = nullptr;
    DWORD size = sizeof(certificate);
    if (!::WinHttpQueryOption(request, WINHTTP_OPTION_SERVER_CERT_CONTEXT, &certificate,
                              &size) ||
        !certificate)
        return false;
    BYTE hash[32] = {};
    DWORD hash_size = sizeof(hash);
    const BOOL hashed = ::CertGetCertificateContextProperty(
        certificate, CERT_SHA256_HASH_PROP_ID, hash, &hash_size);
    ::CertFreeCertificateContext(certificate);
    if (!hashed) return false;
    return ConstantTimeEquals(HexUpper(std::string((char*)hash, hash_size)),
                              ToUtf8(expected));
}

bool Attempt(const HttpRequest& req, HttpResponse& response) {
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = parts.dwHostNameLength = parts.dwUrlPathLength =
        parts.dwExtraInfoLength = (DWORD)-1;

    if (!::WinHttpCrackUrl(req.url.c_str(), (DWORD)req.url.size(), 0, &parts)) {
        response.error = LastErrorText("WinHttpCrackUrl");
        return true;
    }
    if (!parts.lpszHostName || !parts.dwHostNameLength) {
        response.error = "URL has no host";
        return true;
    }

    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath ? parts.lpszUrlPath : L"",
                      parts.lpszUrlPath ? parts.dwUrlPathLength : 0);
    if (parts.lpszExtraInfo && parts.dwExtraInfoLength)
        path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    if (path.empty()) path = L"/";

    HINTERNET session = ::WinHttpOpen(kAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        response.error = LastErrorText("WinHttpOpen");
        return true;
    }
    ::WinHttpSetTimeouts(session, req.timeout_ms, req.timeout_ms, req.timeout_ms,
                         req.timeout_ms);

    if (!req.proxy_host.empty()) {
        const std::wstring server =
            req.proxy_host + L":" + std::to_wstring(req.proxy_port ? req.proxy_port : 8080);
        WINHTTP_PROXY_INFO info{};
        info.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
        info.lpszProxy = const_cast<wchar_t*>(server.c_str());
        ::WinHttpSetOption(session, WINHTTP_OPTION_PROXY, &info, sizeof(info));
    }
    if (!req.proxy_user.empty()) {
        ::WinHttpSetOption(session, WINHTTP_OPTION_PROXY_USERNAME,
                           const_cast<wchar_t*>(req.proxy_user.c_str()),
                           (DWORD)(req.proxy_user.size() * sizeof(wchar_t)));
        if (!req.proxy_pass.empty())
            ::WinHttpSetOption(session, WINHTTP_OPTION_PROXY_PASSWORD,
                               const_cast<wchar_t*>(req.proxy_pass.c_str()),
                               (DWORD)(req.proxy_pass.size() * sizeof(wchar_t)));
    }

    HINTERNET connection = ::WinHttpConnect(session, host.c_str(), parts.nPort, 0);
    if (!connection) {
        response.error = LastErrorText("WinHttpConnect");
        ::WinHttpCloseHandle(session);
        return true;
    }

    HINTERNET request = ::WinHttpOpenRequest(
        connection, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!request) {
        response.error = LastErrorText("WinHttpOpenRequest");
        ::WinHttpCloseHandle(connection);
        ::WinHttpCloseHandle(session);
        return true;
    }
    if (req.cancel) req.cancel->handle.store(request);

    std::wstring headers =
        L"Content-Type: application/json\r\nAccept: application/json\r\n";
    if (req.stream) headers += L"Accept: text/event-stream\r\n";
    for (const auto& header : req.headers) {
        headers += header.first;
        headers += L": ";
        headers += header.second;
        headers += L"\r\n";
    }
    ::WinHttpAddRequestHeaders(request, headers.c_str(), (DWORD)headers.size(),
                               WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

    bool stop = false;
    do {
        // Count every attempt, including the first one, so callers can report
        // attempts = retries + 1.
        ++response.attempts;
        if (!::WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                  const_cast<char*>(req.body.data()),
                                  (DWORD)req.body.size(), (DWORD)req.body.size(), 0)) {
            const DWORD code = ::GetLastError();
            response.error = LastErrorText("WinHttpSendRequest");
            if (req.cancel && req.cancel->IsCancelled()) {
                response.cancelled = true;
                stop = true;
            } else if (!TransientWin32(code) || response.attempts > req.max_retries + 1) {
                stop = true;
            }
            break;
        }

        if (!::WinHttpReceiveResponse(request, nullptr)) {
            const DWORD code = ::GetLastError();
            response.error = LastErrorText("WinHttpReceiveResponse");
            if (req.cancel && req.cancel->IsCancelled()) {
                response.cancelled = true;
                stop = true;
            } else if (!TransientWin32(code) || response.attempts > req.max_retries + 1) {
                stop = true;
            }
            break;
        }

        if (!VerifyPin(request, req.cert_pin_sha256)) {
            response.error = "Server certificate pin mismatch";
            stop = true;
            break;
        }

        DWORD status = 0, status_size = sizeof(status);
        if (!::WinHttpQueryHeaders(request,
                                   WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                   WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                                   WINHTTP_NO_HEADER_INDEX)) {
            response.error = LastErrorText("WinHttpQueryHeaders");
            stop = true;
            break;
        }
        response.status = status;

        if (TransientStatus(status) && response.attempts <= req.max_retries) {
            const int delay = BackoffMillis(
                response.attempts,
                ParseRetryAfter(HeaderString(request, WINHTTP_QUERY_RETRY_AFTER)));
            LogLine("HTTP %lu, retrying in %d ms", status, delay);
            ::WinHttpCloseHandle(request);
            if (req.cancel) req.cancel->handle.store(nullptr);
            ::WinHttpCloseHandle(connection);
            ::WinHttpCloseHandle(session);
            response.status = 0;
            SleepCancel(delay, req.cancel);
            if (req.cancel && req.cancel->IsCancelled()) {
                response.cancelled = true;
                stop = true;
                return stop;
            }
            return Attempt(req, response);
        }

        std::string line_buffer;
        for (;;) {
            if (req.cancel && req.cancel->IsCancelled()) {
                response.cancelled = true;
                stop = true;
                break;
            }
            DWORD available = 0;
            if (!::WinHttpQueryDataAvailable(request, &available)) {
                response.error = LastErrorText("WinHttpQueryDataAvailable");
                break;
            }
            if (!available) break;

            const size_t room = req.max_response_bytes > response.body.size()
                                    ? req.max_response_bytes - response.body.size()
                                    : 0;
            if (!room) {
                response.truncated = true;
                break;
            }
            bool capped = false;
            if (available > room) {
                available = (DWORD)room;
                capped = true;
            }

            std::vector<char> chunk(available);
            DWORD read = 0;
            if (!::WinHttpReadData(request, chunk.data(), available, &read)) {
                response.error = LastErrorText("WinHttpReadData");
                break;
            }
            if (!read) break;
            chunk.resize(read);

            if (req.stream && req.on_text_delta) {
                std::vector<std::string> payloads;
                FeedSseLines(line_buffer, chunk.data(), chunk.size(), payloads);
                for (const std::string& payload : payloads)
                    if (payload != "[DONE]") req.on_text_delta(payload);
            }

            response.body.append(chunk.data(), chunk.size());
            if (capped) {
                response.truncated = true;
                break;
            }
        }
        stop = true;
    } while (false);

    HINTERNET owned = req.cancel ? req.cancel->handle.exchange(nullptr) : request;
    if (owned) ::WinHttpCloseHandle(owned);
    else if (!req.cancel) ::WinHttpCloseHandle(request);
    ::WinHttpCloseHandle(connection);
    ::WinHttpCloseHandle(session);
    return stop;
}

}  // namespace

void CancelToken::Cancel() {
    cancelled.store(true);
    if (HINTERNET handle = this->handle.exchange(nullptr)) ::WinHttpCloseHandle(handle);
}

HttpResponse HttpPerform(const HttpRequest& req) {
    HttpResponse response;
    Attempt(req, response);

    if (response.cancelled) {
        response.error = "Cancelled";
        return response;
    }
    if (!response.error.empty()) return response;
    if (response.truncated) {
        response.error = "Response exceeded the 8 MB cap";
        return response;
    }
    if (response.status >= 200 && response.status < 300) {
        response.ok = true;
    } else {
        std::string snippet = TrimA(response.body);
        if (snippet.size() > 400) snippet = snippet.substr(0, 400) + "...";
        response.error = "HTTP " + std::to_string(response.status);
        if (!snippet.empty()) response.error += ": " + snippet;
    }
    return response;
}

bool InstallCaBundle(const std::wstring& path) {
    std::string raw;
    if (!ReadWholeFile(path, raw, 1024 * 1024) || raw.empty()) return false;

    HCERTSTORE store = ::CertOpenSystemStoreW(0, L"ROOT");
    if (!store) return false;

    bool added = false;
    const char* begin = raw.c_str();
    while (const char* marker = strstr(begin, "-----BEGIN CERTIFICATE-----")) {
        const char* body = strstr(marker, "-----") + 5;
        const char* end = strstr(body, "-----END CERTIFICATE-----");
        if (!end) break;
        const std::string der = Base64Decode(std::string(body, (size_t)(end - body)));
        if (!der.empty() &&
            ::CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                               (const BYTE*)der.data(), (DWORD)der.size(),
                                               CERT_STORE_ADD_USE_EXISTING, nullptr))
            added = true;
        begin = end + 1;
    }
    ::CertCloseStore(store, 0);
    return added;
}
