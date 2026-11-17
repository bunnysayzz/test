#include "app.h"
#include "json.h"
#include "log.h"
#include "redact.h"
#include "util.h"

#include <cctype>
#include <ctime>

namespace {

constexpr const char* kSalt = "crowman-study-v1";
constexpr long long kDefaultTtl = 86400;

struct Cache {
    bool present = false;
    bool valid = false;
    std::string token;
    long long expires = 0;
};

std::string MachineGuid() {
    std::string guid;
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0,
                        KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
        return guid;
    wchar_t buffer[256] = {};
    DWORD size = sizeof(buffer), type = 0;
    if (::RegQueryValueExW(key, L"MachineGuid", nullptr, &type, (LPBYTE)buffer, &size) ==
            ERROR_SUCCESS &&
        type == REG_SZ)
        guid = ToUtf8(buffer);
    ::RegCloseKey(key);
    return guid;
}

std::string Group(const std::string& hex) {
    std::string out;
    for (size_t i = 0; i < hex.size(); ++i) {
        if (i && i % 8 == 0) out.push_back('-');
        out.push_back((char)::toupper((unsigned char)hex[i]));
    }
    return out;
}

std::string Signature(const std::string& hwid, const std::string& token,
                      long long expires) {
    return Sha256Hex(hwid + "|" + token + "|" + std::to_string(expires) + "|" + kSalt);
}

Cache ReadCache(const std::string& hwid) {
    Cache cache;
    std::string raw;
    if (!ReadWholeFile(LicensePath(), raw, 256 * 1024)) return cache;
    cache.present = true;

    std::string opened;
    if (!DpapiUnseal(TrimA(raw), opened)) return cache;

    json::Value doc;
    if (!json::Parse(opened, doc)) return cache;
    const json::Value* token = doc.Find("token");
    const json::Value* expires = doc.Find("expires");
    const json::Value* signature = doc.Find("sig");
    if (!token || !expires || !signature) return cache;

    cache.token = token->AsString();
    cache.expires = (long long)expires->AsNumber(0);
    if (cache.token.empty() || cache.expires <= 0) return cache;
    if (!ConstantTimeEquals(signature->AsString(),
                            Signature(hwid, cache.token, cache.expires)))
        return cache;

    cache.valid = (long long)std::time(nullptr) < cache.expires;
    return cache;
}

}  // namespace

std::string ComputeHwid() {
    DWORD serial = 0;
    wchar_t name[MAX_PATH + 1] = {}, fs[MAX_PATH + 1] = {};
    ::GetVolumeInformationW(L"C:\\", name, MAX_PATH, &serial, nullptr, nullptr, fs,
                            MAX_PATH);
    const std::string digest = Sha256Hex(std::string(kSalt) + "|" +
                                         std::to_string(serial) + "|" + MachineGuid());
    if (digest.size() < 32) return "UNAVAILABLE";
    return Group(digest.substr(0, 32));
}

void InvalidateLicenseCache() { ::DeleteFileW(LicensePath().c_str()); }

LicenseState CheckLicense(const Config& cfg, const std::string& hwid,
                          bool force_refresh) {
    LicenseState state;

    if (cfg.license_url.empty()) {
        state.ok = true;
        state.message = "Trial mode - no licence server configured";
        return state;
    }

    const Cache cache = ReadCache(hwid);
    if (!force_refresh) {
        if (cache.valid) {
            state.ok = true;
            state.token = cache.token;
            state.message = "Licensed (cached)";
            return state;
        }
        if (cache.present && !cache.token.empty() && cfg.offline_grace_hours > 0) {
            const long long grace_end =
                cache.expires + (long long)cfg.offline_grace_hours * 3600;
            if ((long long)std::time(nullptr) < grace_end) {
                state.ok = true;
                state.transient = true;
                state.token = cache.token;
                state.message = "Licensed (offline grace)";
                return state;
            }
            state.hard_stop = true;
            state.message =
                "Offline grace expired - connect to the network to re-activate.";
            return state;
        }
    }

    const std::wstring key = CurrentSecrets().license_key;
    if (key.empty()) {
        state.message = "Set a licence key in Settings";
        return state;
    }

    std::string body = "{\"hwid\":" + json::Quote(hwid) + ",\"key\":" +
                       json::Quote(ToUtf8(key)) + ",\"app_version\":" +
                       json::Quote(ToUtf8(CROWMAN_VERSION)) + "}";

    HttpRequest request;
    request.url = cfg.license_url;
    request.body = std::move(body);
    request.timeout_ms = cfg.timeout_ms;
    request.max_retries = 1;
    request.cert_pin_sha256 = cfg.cert_pin_sha256;
    request.proxy_host = cfg.proxy_host;
    request.proxy_port = cfg.proxy_port;
    request.proxy_user = cfg.proxy_user;
    request.proxy_pass = CurrentSecrets().proxy_pass;

    const HttpResponse response = HttpPerform(request);

    if (response.status == 403) {
        InvalidateLicenseCache();
        state.message = "Activation refused: " + Redact(response.error);
        return state;
    }

    if (!response.ok) {
        if (cache.present && !cache.token.empty() && cfg.offline_grace_hours > 0) {
            state.ok = true;
            state.transient = true;
            state.token = cache.token;
            state.message = "Licensed (server unreachable)";
            return state;
        }
        state.message = "Activation failed: " + Redact(response.error);
        return state;
    }

    json::Value doc;
    std::string parse_error;
    if (!json::Parse(response.body, doc, &parse_error)) {
        state.message = "Activation response was not JSON: " + parse_error;
        return state;
    }

    const json::Value* token = doc.Find("token");
    if (!token || token->AsString().empty()) {
        state.message = "Activation response had no token";
        return state;
    }

    const long long ttl = (long long)(doc.Find("expires_in")
                                          ? doc.Find("expires_in")->AsNumber((double)kDefaultTtl)
                                          : (double)kDefaultTtl);
    const long long expires_at = (long long)std::time(nullptr) + (ttl > 0 ? ttl : 3600);

    std::string plain = "{\"token\":" + json::Quote(token->AsString()) +
                        ",\"expires\":" + std::to_string(expires_at) +
                        ",\"sig\":" +
                        json::Quote(Signature(hwid, token->AsString(), expires_at)) + "}";
    const std::string sealed = DpapiSeal(plain);
    if (!sealed.empty()) WriteWholeFile(LicensePath(), sealed);

    state.ok = true;
    state.token = token->AsString();
    state.message = "Activated";
    return state;
}
