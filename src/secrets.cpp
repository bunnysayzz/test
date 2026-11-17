#include "secrets.h"

#include "app.h"
#include "json.h"
#include "util.h"

#include <mutex>

namespace {

Secrets g_current;
std::mutex g_lock;

}  // namespace

void SecretsSetCurrent(const Secrets& secrets) {
    std::lock_guard<std::mutex> guard(g_lock);
    g_current = secrets;
}

const Secrets& CurrentSecrets() { return g_current; }

Secrets LoadSecrets() {
    Secrets secrets;
    std::string raw;
    if (!ReadWholeFile(SecretsPath(), raw, 256 * 1024) || raw.empty()) return secrets;

    std::string opened;
    if (!DpapiUnseal(TrimA(raw), opened)) {
        LogLine("secrets.bin did not unseal");
        return secrets;
    }

    json::Value doc;
    if (!json::Parse(opened, doc)) return secrets;

    if (const json::Value* value = doc.Find("api_key"))
        secrets.api_key = FromUtf8(value->AsString());
    if (const json::Value* value = doc.Find("license_key"))
        secrets.license_key = FromUtf8(value->AsString());
    if (const json::Value* value = doc.Find("proxy_pass"))
        secrets.proxy_pass = FromUtf8(value->AsString());
    return secrets;
}

bool SaveSecrets(const Secrets& secrets) {
    std::string plain = "{";
    plain += "\"api_key\":" + json::Quote(ToUtf8(secrets.api_key)) + ",";
    plain += "\"license_key\":" + json::Quote(ToUtf8(secrets.license_key)) + ",";
    plain += "\"proxy_pass\":" + json::Quote(ToUtf8(secrets.proxy_pass));
    plain += "}";

    const std::string sealed = DpapiSeal(plain);
    if (sealed.empty()) return false;

    const std::wstring temporary = SecretsPath() + L".tmp";
    if (!WriteWholeFile(temporary, sealed)) return false;
    return ::MoveFileExW(temporary.c_str(), SecretsPath().c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}
