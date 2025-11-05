#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

struct Language
{
    int id;
    juce::String code;   // ISO-ish for Whisper/Translate
    juce::String label;
};

static inline const std::vector<Language> kLanguages = {
    { 1, "auto", "Auto Detect" },
    { 2, "en",   "English" },
    { 3, "de",   "German" },
    { 4, "gsw",  "Swiss German" }, // map to de for Whisper, special-case in translation
    { 5, "fr",   "French" },
    { 6, "it",   "Italian" }
};
