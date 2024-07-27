#ifndef LANGUAGEDB_H
#define LANGUAGEDB_H

#include <map>
#include <memory>
#include <string>

#include "onboardoskglobals.h"


class ISOCodes;

// Keeps track of languages selectable in the language menu.
class LanguageDB : public ContextBase
{
    public:
        using Super = ContextBase;
        LanguageDB(const ContextBase& context);
        virtual ~LanguageDB();

        static std::unique_ptr<LanguageDB> make(const ContextBase& context);

        static std::vector<std::string> get_main_languages();
        std::string  get_language_full_name_or_id(const std::string& lang_id);
        std::string get_language_full_name(const std::string& lang_id);
        std::string get_language_name(const std::string& lang_id);
        std::string get_language_code(const std::string& lang_id);

        // Return lang_id if there is an exact match, else return the;
        // default lang_id for its lanugage code.;
        std::string find_system_model_language_id(const std::string& lang_id);

        // List of language_ids of the available system language models.
        virtual void get_system_model_language_ids(std::vector<std::string>& ids);

        // List of language_ids (ll_CC) that can be selected by the user.
        std::vector<std::string> get_language_ids();

        // Complete given language code to the language id of the main language.
        std::string get_main_lang_id(const std::string& lang_code);

        std::vector<std::string> get_language_ids_in_locale();
        std::vector<std::string> get_locale_ids();
        std::vector<std::string> find_locale_ids();
        static std::string get_system_default_lang_id();

        static std::tuple<std::string, std::string, std::string>
                split_locale_id(const std::string& locale_id);

        static std::tuple<std::string, std::string>
                split_lang_id(const std::string& lang_id);

    private:
        static std::string get_system_default_locale_id();
        static std::string get_lang_id_from_locale_id(const std::string& locale_id);

    private:
        std::unique_ptr<ISOCodes> m_iso_codes;
        std::vector<std::string> m_locale_ids;
        static const std::map<std::string, std::string> m_main_languages;
};


// Load and translate ISO language and country codes.
class ISOCodes : public ContextBase
{
    public:
        using Super = ContextBase;
        ISOCodes(const ContextBase& context);

        virtual ~ISOCodes();

        std::string get_language_name(const std::string& lang_code);
        std::string get_country_name(const std::string& country_code);
        std::string get_translated_language_name(const std::string& lang_code);
        std::string get_translated_country_name(const std::string& country_code);

    private:
        void read_all();
        void read_languages();
        void read_countries();

    private:
         std::map<std::string, std::string> m_languages;  // lowercase lang_ids
         std::map<std::string, std::string> m_countries;  // uppercase country_ids
};


#endif // LANGUAGEDB_H
