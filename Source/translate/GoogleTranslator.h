#pragma once
#include "ITranslator.h"
#include <juce_core/juce_core.h>

class GoogleTranslator : public ITranslator
{
public:
    GoogleTranslator() {}

    GoogleTranslator(const juce::String& apiKey)
        : key(apiKey)
    {
    }

    void setKey(const juce::String& apiKey)
    {
        key = apiKey;
    }

    // Blocking call for now (fast enough for short phrases),
    // You can swap to async thread later if needed.
    std::string translate(const TranslateRequest& r) override;

private:
    juce::String key;

    juce::String toGoogleLang(const std::string& lang) const
    {
        juce::String L = lang;

        if (L.equalsIgnoreCase("auto")) return "auto";
        if (L.equalsIgnoreCase("en") || L.containsIgnoreCase("english")) return "en";
        if (L.equalsIgnoreCase("de") || L.containsIgnoreCase("german")) return "de";
        if (L.equalsIgnoreCase("gsw")) return "de";
        if (L.equalsIgnoreCase("fr")) return "fr";
        if (L.equalsIgnoreCase("it")) return "it";

        return L.toLowerCase();
    }
};
