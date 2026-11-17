#pragma once

#include <string>
#include <vector>

void FeedSseLines(std::string& buffer, const char* data, size_t length,
                  std::vector<std::string>& out);
long long ParseRetryAfter(const std::string& value);
int BackoffMillis(int attempt, long long retry_after_seconds);
