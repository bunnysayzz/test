#include "app.h"
#include "json.h"
#include "util.h"

#include <ctime>
#include <vector>

namespace {

constexpr int kHardCap = 200;

std::vector<HistoryEntry> Read() {
    std::vector<HistoryEntry> entries;
    std::string raw;
    if (!ReadWholeFile(HistoryPath(), raw, 8 * 1024 * 1024) || raw.empty()) return entries;

    json::Value doc;
    if (!json::Parse(raw, doc) || doc.type != json::Value::Type::Array) return entries;

    for (const auto& item : doc.array) {
        HistoryEntry entry;
        if (const json::Value* ts = item.Find("ts")) entry.ts = (long long)ts->AsNumber(0);
        if (const json::Value* text = item.Find("text")) entry.text = text->AsString();
        if (!entry.text.empty()) entries.push_back(std::move(entry));
    }
    return entries;
}

void Write(const std::vector<HistoryEntry>& entries) {
    std::string doc = "[";
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i) doc += ",";
        doc += "{\"ts\":" + std::to_string(entries[i].ts) +
               ",\"text\":" + json::Quote(entries[i].text) + "}";
    }
    doc += "]";
    WriteWholeFile(HistoryPath(), doc);
}

}  // namespace

void HistoryLoad(std::vector<HistoryEntry>& out) { out = Read(); }

void HistoryAppend(const std::string& text, int limit) {
    if (text.empty()) return;
    std::vector<HistoryEntry> entries = Read();

    HistoryEntry entry;
    entry.ts = (long long)std::time(nullptr);
    entry.text = text;
    entries.insert(entries.begin(), std::move(entry));

    size_t cap = limit > 0 ? (size_t)limit : 20;
    if (cap > kHardCap) cap = kHardCap;
    if (entries.size() > cap) entries.resize(cap);
    Write(entries);
}

void HistoryClear() { ::DeleteFileW(HistoryPath().c_str()); }
