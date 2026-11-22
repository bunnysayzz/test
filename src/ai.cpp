#include "app.h"
#include "json.h"
#include "log.h"
#include "redact.h"
#include "util.h"
#include "extras.h"  // x::BuildPersonaPrompt / x::Mode / x::Token* / x::EndpointBase

namespace {

constexpr const char* kStudyPrompt =
    "You are a study assistant. The user shared screenshots of their own screen "
    "so you can help them understand it.\n"
    "Several screenshots may be attached; treat them as one task in order.\n"
    "1. If the screenshots contain a question, problem or code task, restate it "
    "briefly in your own words so the user can confirm you read it right.\n"
    "2. Explain the underlying concept in 2-4 sentences.\n"
    "3. Work through the answer step by step and state the final result. For a "
    "multiple-choice item, name the correct option and explain why each other "
    "option is wrong.\n"
    "4. If you see code, point out the specific bug and what to change.\n"
    "If there is no question or task, say so plainly.\n"
    "Reply as plain text. Do not use markdown tables.";

constexpr const char* kBrokerPrompt =
    "Read the attached screenshots as a study aid: explain the concept, then "
    "give the worked answer and reasoning. If there is no question, say so.";

std::string ExtractText(const json::Value& doc) {
    const json::Value* choices = doc.Find("choices");
    if (!choices) return {};
    const json::Value* first = choices->At(0);
    if (!first) return {};
    const json::Value* message = first->Find("message");
    if (!message) return {};
    const json::Value* content = message->Find("content");
    if (!content) return {};
    if (content->type == json::Value::Type::String) return content->string;
    if (content->type == json::Value::Type::Array) {
        std::string joined;
        for (const auto& part : content->array) {
            const json::Value* text = part.Find("text");
            if (text && text->type == json::Value::Type::String) {
                if (!joined.empty()) joined += "\n";
                joined += text->string;
            }
        }
        return joined;
    }
    return {};
}

std::string ExtractDelta(const std::string& payload) {
    json::Value doc;
    if (!json::Parse(payload, doc)) return {};
    const json::Value* choices = doc.Find("choices");
    if (!choices) return {};
    const json::Value* first = choices->At(0);
    if (!first) return {};
    const json::Value* delta = first->Find("delta");
    if (!delta) return {};
    const json::Value* content = delta->Find("content");
    if (!content || content->type != json::Value::Type::String) return {};
    return content->string;
}

std::string RemoteError(const std::string& body) {
    json::Value doc;
    if (!json::Parse(body, doc)) return {};
    if (const json::Value* error = doc.Find("error")) {
        if (const json::Value* message = error->Find("message"))
            return message->AsString();
        return error->AsString();
    }
    if (const json::Value* message = doc.Find("message")) return message->AsString();
    return {};
}

}  // namespace

AiAnswer AskStudyAssistant(const Config& cfg, const std::string& hwid,
                           const std::vector<std::vector<uint8_t>>& pngs,
                           CancelToken* cancel, const DeltaSink& on_delta) {
    AiAnswer answer;

    // Parity mode uses the built-in unxored token + endpoint (extras.cpp), so
    // it needs no user-configured endpoint or key.
    const bool parity = (cfg.mode == L"parity");

    if (cfg.endpoint.empty() && !parity) {
        answer.error = "No endpoint configured.";
        return answer;
    }
    if (pngs.empty()) {
        answer.error = "No screenshot captured.";
        return answer;
    }

    const Secrets& secrets = CurrentSecrets();
    const std::wstring key =
        parity ? x::TokenGeneral()
               : (cfg.mode == L"broker" ? secrets.license_key : secrets.api_key);
    if (key.empty()) {
        answer.error = "No key configured.";
        return answer;
    }

    const bool streaming = cfg.stream && (bool)on_delta;

    HttpRequest request;
    request.url = parity ? (x::EndpointBase() + x::RouteName()) : cfg.endpoint;
    request.timeout_ms = cfg.timeout_ms;
    request.max_retries = cfg.max_retries;
    request.max_response_bytes = (size_t)cfg.max_response_bytes;
    request.stream = streaming;
    request.cancel = cancel;
    request.proxy_host = cfg.proxy_host;
    request.proxy_port = cfg.proxy_port;
    request.proxy_user = cfg.proxy_user;
    request.proxy_pass = secrets.proxy_pass;
    request.cert_pin_sha256 = cfg.cert_pin_sha256;

    if (streaming) {
        request.on_text_delta = [&](const std::string& payload) {
            const std::string delta = ExtractDelta(payload);
            if (!delta.empty()) on_delta(delta);
        };
    }

    // Parity / exam modes: when a solver mode is active (Alt+M/8/9/0 sets
    // x::Mode to 1-4) build the exam-evaluator persona prompt; otherwise keep
    // the plain study prompt.
    std::string promptText = kStudyPrompt;
    if (x::Mode() > 0)
        promptText = ToUtf8(x::BuildPersonaPrompt(x::Mode(), FromUtf8(kStudyPrompt)));

    std::string body;
    if (cfg.mode == L"broker") {
        body += "{\"hwid\":" + json::Quote(hwid) + ",";
        body += "\"api_token\":" + json::Quote(ToUtf8(key)) + ",";
        body += "\"prompt\":" + json::Quote(kBrokerPrompt) + ",";
        body += "\"stream\":" + std::string(streaming ? "true" : "false") + ",";
        body += "\"image\":\"data:image/png;base64," + Base64Encode(pngs[0]) + "\",";
        body += "\"images\":[";
        for (size_t i = 0; i < pngs.size(); ++i) {
            if (i) body += ",";
            body += "\"data:image/png;base64," + Base64Encode(pngs[i]) + "\"";
        }
        body += "]}";
    } else {
        body += "{\"model\":" + json::Quote(ToUtf8(cfg.model)) + ",";
        body += "\"messages\":[{\"role\":\"user\",\"content\":[";
        body += "{\"type\":\"text\",\"text\":" + json::Quote(promptText) + "}";
        for (const auto& png : pngs) {
            body += ",{\"type\":\"image_url\",\"image_url\":{\"url\":";
            body += "\"data:image/png;base64," + Base64Encode(png) + "\"}}";
        }
        body += "]}],\"max_tokens\":1200";
        if (streaming) body += ",\"stream\":true";
        body += "}";
        request.headers.emplace_back(L"Authorization", L"Bearer " + key);
    }
    request.body = std::move(body);

    HttpResponse response = HttpPerform(request);

    if (!response.ok && response.status == 401 && cfg.mode == L"broker" &&
        !cfg.license_url.empty() && !cancel) {
        InvalidateLicenseCache();
        const LicenseState licence = CheckLicense(cfg, hwid, true);
        if (licence.ok) {
            const std::string old_token = json::Quote(ToUtf8(key));
            const size_t at = request.body.find(old_token);
            if (at != std::string::npos) {
                request.body.replace(at, old_token.size(), json::Quote(licence.token));
                response = HttpPerform(request);
            }
        }
    }

    if (response.cancelled) {
        answer.cancelled = true;
        answer.error = "Cancelled";
        return answer;
    }
    if (!response.ok) {
        answer.error = Redact(response.error);
        const std::string remote = RemoteError(response.body);
        if (!remote.empty()) answer.error += " (" + Redact(remote) + ")";
        LogLine("request failed: %s", answer.error.c_str());
        return answer;
    }

    json::Value doc;
    std::string parse_error;
    if (!json::Parse(response.body, doc, &parse_error)) {
        answer.error = "Response was not JSON: " + parse_error;
        return answer;
    }

    std::string text;
    if (cfg.mode == L"broker") {
        if (const json::Value* value = doc.Find("answer")) text = value->AsString();
        else if (const json::Value* value = doc.Find("text")) text = value->AsString();
    } else {
        text = ExtractText(doc);
    }

    if (text.empty()) {
        const std::string remote = RemoteError(response.body);
        answer.error = remote.empty() ? "Response contained no answer text." : Redact(remote);
        return answer;
    }

    answer.ok = true;
    answer.text = std::move(text);
    return answer;
}
