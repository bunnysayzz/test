#pragma once

#include <string>

struct Secrets {
    std::wstring api_key;
    std::wstring license_key;
    std::wstring proxy_pass;
};

std::wstring SecretsPath();
Secrets LoadSecrets();
bool SaveSecrets(const Secrets& secrets);
void SecretsSetCurrent(const Secrets& secrets);
const Secrets& CurrentSecrets();
