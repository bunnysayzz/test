#include "app.h"
#include "log.h"
#include "util.h"

#include <wincrypt.h>

#pragma comment(lib, "crypt32.lib")

namespace {

constexpr wchar_t kEntropy[] = L"crowman-study-secret-v1";

DATA_BLOB Entropy() {
    DATA_BLOB blob{};
    blob.pbData = (BYTE*)const_cast<wchar_t*>(kEntropy);
    blob.cbData = (DWORD)(sizeof(kEntropy) - sizeof(wchar_t));
    return blob;
}

}  // namespace

std::string DpapiSeal(const std::string& plain) {
    if (plain.empty()) return {};

    DATA_BLOB in{};
    in.pbData = (BYTE*)const_cast<char*>(plain.data());
    in.cbData = (DWORD)plain.size();
    DATA_BLOB entropy = Entropy();
    DATA_BLOB out{};

    if (!::CryptProtectData(&in, L"crowman", &entropy, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        LogLine("CryptProtectData failed (%lu)", ::GetLastError());
        return {};
    }

    const std::string sealed((char*)out.pbData, out.cbData);
    ::LocalFree(out.pbData);
    return Base64Encode(sealed);
}

bool DpapiUnseal(const std::string& sealed, std::string& out) {
    if (sealed.empty()) return false;

    const std::string raw = Base64Decode(sealed);
    if (raw.empty()) return false;

    DATA_BLOB in{};
    in.pbData = (BYTE*)const_cast<char*>(raw.data());
    in.cbData = (DWORD)raw.size();
    DATA_BLOB entropy = Entropy();
    DATA_BLOB plain{};

    if (!::CryptUnprotectData(&in, nullptr, &entropy, nullptr, nullptr,
                              CRYPTPROTECT_UI_FORBIDDEN, &plain))
        return false;

    out.assign((char*)plain.pbData, plain.cbData);
    ::LocalFree(plain.pbData);
    return true;
}
