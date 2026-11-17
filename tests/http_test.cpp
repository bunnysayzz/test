#include "../src/app.h"
#include "test_server.h"

#include <cstdio>

static int failures = 0;

static void Check(bool ok, const char* what) {
    std::printf(ok ? "ok: %s\n" : "FAIL: %s\n", what);
    if (!ok) ++failures;
}

static HttpResponse Run(int port, int timeout, int retries, size_t cap) {
    HttpRequest request;
    request.url = L"http://127.0.0.1:" + std::to_wstring(port) + L"/v1/chat";
    request.body = "{\"probe\":1}";
    request.timeout_ms = timeout;
    request.max_retries = retries;
    request.max_response_bytes = cap;
    return HttpPerform(request);
}

int main() {
    {
        TestServer s({200, "Content-Type: application/json\r\n", "{\"ok\":true}", 0});
        const HttpResponse r = Run(s.port(), 5000, 0, 1 << 20);
        Check(r.ok && r.body.find("ok") != std::string::npos, "200 succeeds");
    }
    {
        TestServer s({401, "", "{\"error\":{\"message\":\"bad key\"}}", 0});
        const HttpResponse r = Run(s.port(), 5000, 0, 1 << 20);
        Check(!r.ok && r.status == 401, "401 reported");
        Check(r.error.find("bad key") != std::string::npos, "provider message surfaced");
    }
    {
        TestServer s({429, "Retry-After: 1\r\n", "{}", 0});
        const HttpResponse r = Run(s.port(), 5000, 2, 1 << 20);
        Check(!r.ok && r.attempts >= 2, "429 retried");
    }
    {
        TestServer s({500, "", "{}", 0});
        const HttpResponse r = Run(s.port(), 5000, 3, 1 << 20);
        Check(r.attempts == 4, "500 stops after max_retries");
    }
    {
        TestServer s({200, "", "{}", 4000});
        const HttpResponse r = Run(s.port(), 1500, 0, 1 << 20);
        Check(!r.ok, "timeout fails");
    }
    {
        TestServer s({200, "", std::string(4096, 'x'), 0});
        const HttpResponse r = Run(s.port(), 5000, 0, 1024);
        Check(!r.ok && r.truncated, "oversize body capped");
    }

    std::printf(failures ? "\n%d FAILURE(S)\n" : "\nALL PASS\n", failures);
    return failures ? 1 : 0;
}
