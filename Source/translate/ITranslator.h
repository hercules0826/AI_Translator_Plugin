#pragma once
#include <string>

struct TranslateRequest {
    std::string text;
    std::string srcLang; // "auto" allowed
    std::string dstLang; // e.g., "de", "en", "fr"
};

class ITranslator {
public:
    virtual ~ITranslator() = default;
    virtual std::string translate(const TranslateRequest& r) = 0;
};
