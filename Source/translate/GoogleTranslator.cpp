#include "GoogleTranslator.h"
#include <juce_core/juce_core.h>

std::string GoogleTranslator::translate(const TranslateRequest& r)
{
    if (key.isEmpty() || r.text.empty())
        return r.text; // fallback

    auto src = toGoogleLang(r.srcLang);
    auto dst = toGoogleLang(r.dstLang);

    juce::URL url("https://translation.googleapis.com/language/translate/v2");

    url = url
        .withParameter("key", key)
        .withParameter("q",  juce::String(r.text))
        .withParameter("source", src)
        .withParameter("target", dst)
        .withParameter("format", "text");

    juce::String response = url.readEntireTextStream();

    if (response.isEmpty())
        return r.text;

    juce::var resVar = juce::JSON::parse(response);
    if (! resVar.isObject())
        return r.text;

    auto* root = resVar.getDynamicObject();
    if (!root)
        return r.text;

    auto data = root->getProperty("data");
    if (!data.isObject())
        return r.text;

    auto* dataObj = data.getDynamicObject();
    if (!dataObj)
        return r.text;

    auto translations = dataObj->getProperty("translations");
    if (!translations.isArray())
        return r.text;

    auto* arr = translations.getArray();
    if (!arr || arr->isEmpty())
        return r.text;

    auto entry = (*arr)[0];
    if (!entry.isObject())
        return r.text;

    juce::String translated = entry.getProperty("translatedText", juce::String(r.text));

    return translated.toStdString();
}
