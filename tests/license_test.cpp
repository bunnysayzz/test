#include "../src/app.h"
#include "../src/json.h"
#include "../src/util.h"
#include "test_server.h"

#include <cstdio>
#include <ctime>

static int failures = 0;

static void Check(bool ok, const char* what) {
    std::printf(ok ? "ok: %s\n" : "FAIL: %s\n", what);
    if (!ok) ++failures;
}

static Config Broker(int port, int grace) {
    Config cfg;
    cfg.mode = L"broker";
    cfg.license_url = L"http://127.0.0.1:" + std::to_wstring(port) + L"/activate";
    cfg.timeout_ms = 3000;
    cfg.max_retries = 0;
    cfg.offline_grace_hours = grace;
    return cfg;
}

static void RewriteCache(const std::string& hwid, const std::string& token,
                         long long expires) {
    const std::string sig = Sha256Hex(hwid + "|" + token + "|" +
                                      std::to_string(expires) + "|crowman-study-v1");
    const std::string plain = "{\"token\":" + json::Quote(token) +
                              ",\"expires\":" + std::to_string(expires) +
                              ",\"sig\":" + json::Quote(sig) + "}";
    WriteWholeFile(LicensePath(), DpapiSeal(plain));
}

int main() {
    ::DeleteFileW(LicensePath().c_str());

    Secrets secrets;
    secrets.license_key = L"GOOD-KEY";
    SecretsSetCurrent(secrets);

    const std::string hwid = ComputeHwid();
    Check(hwid.size() == 35, "hwid is 8-8-8-8");

    {  // activation
        TestServer server({200, "Content-Type: application/json\r\n",
                           "{\"token\":\"tok-1\",\"expires_in\":3600}", 0});
        const LicenseState state = CheckLicense(Broker(server.port(), 72), hwid, true);
        Check(state.ok && state.token == "tok-1", "activation succeeds");
        Check(::GetFileAttributesW(LicensePath().c_str()) != INVALID_FILE_ATTRIBUTES,
              "token cached");
    }

    {  // valid cache, server refused
        const LicenseState state = CheckLicense(Broker(1, 72), hwid);
        Check(state.ok && state.token == "tok-1", "valid cache reused");
    }

    {  // expired 60 s ago, grace 72 h -> still licensed, transient
        RewriteCache(hwid, "tok-1", (long long)std::time(nullptr) - 60);
        const LicenseState state = CheckLicense(Broker(1, 72), hwid);
        Check(state.ok && state.transient, "inside grace still licensed");
        Check(!state.hard_stop, "inside grace not a hard stop");
    }

    {  // expired 3 h ago, grace 1 h -> hard stop
        RewriteCache(hwid, "tok-1", (long long)std::time(nullptr) - 3 * 3600);
        const LicenseState state = CheckLicense(Broker(1, 1), hwid);
        Check(!state.ok && state.hard_stop, "outside grace hard stops");
    }

    {  // revoked
        TestServer server({403, "", "{\"message\":\"revoked\"}", 0});
        const LicenseState state = CheckLicense(Broker(server.port(), 72), hwid, true);
        Check(!state.ok && !state.hard_stop, "403 refuses without hard stop");
        Check(::GetFileAttributesW(LicensePath().c_str()) == INVALID_FILE_ATTRIBUTES,
              "403 clears the cache");
    }

    ::DeleteFileW(LicensePath().c_str());

    std::printf(failures ? "\n%d FAILURE(S)\n" : "\nALL PASS\n", failures);
    return failures ? 1 : 0;
}
