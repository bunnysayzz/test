#include "../src/netutil.h"

#include <cstdio>
#include <string>
#include <vector>

static int failures = 0;

static void Check(bool ok, const char* what) {
    std::printf(ok ? "ok: %s\n" : "FAIL: %s\n", what);
    if (!ok) ++failures;
}

int main() {
    {
        std::string buffer;
        std::vector<std::string> out;
        FeedSseLines(buffer, "data: one\n", 10, out);
        FeedSseLines(buffer, "data: two\n\ndata: thr", 20, out);
        FeedSseLines(buffer, "ee\n", 3, out);
        Check(out.size() == 3, "splits lines across chunks");
        Check(out.size() == 3 && out[0] == "one" && out[2] == "three",
              "payloads intact");
        Check(buffer.empty(), "buffer drains");
    }
    {
        std::string buffer;
        std::vector<std::string> out;
        FeedSseLines(buffer, "data:partial", 12, out);
        Check(out.empty(), "partial stays buffered");
    }

    Check(ParseRetryAfter("17") == 17, "reads seconds");
    Check(ParseRetryAfter(" 3 ") == 3, "trims");
    Check(ParseRetryAfter("") == 0, "empty is zero");
    Check(ParseRetryAfter("soon") == 0, "garbage is zero");

    const int first = BackoffMillis(0, 0);
    Check(first >= 375 && first <= 625, "first backoff near 500 ms");
    Check(BackoffMillis(0, 9) == 9000, "Retry-After overrides");
    Check(BackoffMillis(20, 0) <= 37500, "backoff capped");

    std::printf(failures ? "\n%d FAILURE(S)\n" : "\nALL PASS\n", failures);
    return failures ? 1 : 0;
}
