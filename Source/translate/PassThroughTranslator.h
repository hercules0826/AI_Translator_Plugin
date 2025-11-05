#pragma once
#include "ITranslator.h"

class PassThroughTranslator : public ITranslator {
public:
    std::string translate(const TranslateRequest& r) override {
        // Simple identity; replace with DeepL/Cloud call later
        return r.text;
    }
};
