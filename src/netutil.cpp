#include "netutil.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>

namespace {

constexpr size_t kMaxBuffer = 1024 * 1024;

int MonthFromName(const std::string& name) {
    static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int i = 0; i < 12; ++i)
        if (name == months[i]) return i;
    return -1;
}

}  // namespace

void FeedSseLines(std::string& buffer, const char* data, size_t length,
                  std::vector<std::string>& out) {
    if (length) buffer.append(data, length);
    if (buffer.size() > kMaxBuffer) buffer.erase(0, buffer.size() - kMaxBuffer);

    size_t start = 0;
    for (;;) {
        const size_t nl = buffer.find('\n', start);
        if (nl == std::string::npos) break;
        std::string line = buffer.substr(start, nl - start);
        start = nl + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line.compare(0, 5, "data:") == 0) {
            size_t begin = 5;
            if (begin < line.size() && line[begin] == ' ') ++begin;
            out.push_back(line.substr(begin));
        }
    }
    if (start) buffer.erase(0, start);
}

long long ParseRetryAfter(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t");
    if (first == std::string::npos) return 0;
    const size_t last = value.find_last_not_of(" \t");
    const std::string trimmed = value.substr(first, last - first + 1);
    if (trimmed.empty()) return 0;

    bool digits = true;
    for (char c : trimmed)
        if (c < '0' || c > '9') {
            digits = false;
            break;
        }
    if (digits) {
        const long long seconds = std::strtoll(trimmed.c_str(), nullptr, 10);
        return seconds > 0 ? seconds : 0;
    }

    if (trimmed.size() < 29) return 0;
    const int month = MonthFromName(trimmed.substr(8, 3));
    if (month < 0) return 0;

    std::tm tm{};
    tm.tm_mday = std::atoi(trimmed.substr(5, 2).c_str());
    tm.tm_mon = month;
    tm.tm_year = std::atoi(trimmed.substr(12, 4).c_str()) - 1900;
    tm.tm_hour = std::atoi(trimmed.substr(17, 2).c_str());
    tm.tm_min = std::atoi(trimmed.substr(20, 2).c_str());
    tm.tm_sec = std::atoi(trimmed.substr(23, 2).c_str());

    const std::time_t when = _mkgmtime(&tm);
    if (when <= 0) return 0;
    const std::time_t now = std::time(nullptr);
    return when > now ? (long long)(when - now) : 0;
}

int BackoffMillis(int attempt, long long retry_after_seconds) {
    if (retry_after_seconds > 0)
        return (int)(std::min<long long>(retry_after_seconds, 120) * 1000);
    if (attempt < 0) attempt = 0;
    long long base = 500LL << std::min(attempt, 6);
    if (base > 30000) base = 30000;
    const int jitter = (int)(base / 4);
    return (int)base + (std::rand() % (jitter * 2 + 1)) - jitter;
}
