#pragma once
#include <string>
#include <unordered_map>
#include <vector>

enum class Language {
    EN,
    ZH_CN,
    VI,
    TH,
    ES_MX,
    HU
};

class Localization {
public:
    static Localization& getInstance();

    void setLanguage(Language lang);
    Language getLanguage() const;
    std::string get(const std::string& key);
    
    struct LangInfo {
        Language code;
        std::string name;
        std::string font_file; // Suggested font file hint
    };
    
    const std::vector<LangInfo>& getAvailableLanguages() const;

private:
    Localization();
    
    Language m_current_lang;
    std::unordered_map<Language, std::unordered_map<std::string, std::string>> m_translations;
    std::vector<LangInfo> m_languages;
};
