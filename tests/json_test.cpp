#include "../src/json.h"

#include <cstdio>
#include <string>

static int failures = 0;

static void Check(bool ok, const char* what) {
    std::printf(ok ? "ok: %s\n" : "FAIL: %s\n", what);
    if (!ok) ++failures;
}

int main() {
    json::Value doc;
    Check(json::Parse("{\"a\":1,\"b\":[true,null,\"x\"]}", doc), "parses mixed document");
    const json::Value* a = doc.Find("a");
    Check(a && a->AsNumber(0) == 1, "reads a number");
    const json::Value* b = doc.Find("b");
    Check(b && b->At(2) && b->At(2)->AsString() == "x", "reads nested string");
    Check(json::Parse("\"\\u00e4\\u00f6\"", doc) && doc.AsString() == "\xc3\xa4\xc3\xb6",
          "decodes escapes to UTF-8");
    Check(json::Parse("\"\\ud83d\\ude00\"", doc) && doc.string.size() == 4,
          "decodes a surrogate pair");
    Check(!json::Parse("{\"a\":1} junk", doc), "rejects trailing data");
    Check(!json::Parse("{\"a\":", doc), "rejects truncated object");
    Check(!json::Parse("[1,2", doc), "rejects unterminated array");

    std::string deep;
    for (int i = 0; i < 100; ++i) deep += "[";
    for (int i = 0; i < 100; ++i) deep += "]";
    Check(!json::Parse(deep, doc), "rejects over-deep nesting");
    Check(json::Quote("a\"b\\c\n") == "\"a\\\"b\\\\c\\n\"", "escapes on write");
    Check(json::Quote("\x01") == "\"\\u0001\"", "escapes control bytes");

    std::printf(failures ? "\n%d FAILURE(S)\n" : "\nALL PASS\n", failures);
    return failures ? 1 : 0;
}
