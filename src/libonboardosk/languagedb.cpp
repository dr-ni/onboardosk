#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "tools/container_helpers.h"
#include "tools/logger.h"
#include "tools/process_helpers.h"
#include "tools/string_helpers.h"

#include "configuration.h"
#include "exception.h"
#include "languagedb.h"
#include "translation.h"
#include "wordsuggestions.h"
#include "xmlparser.h"



const std::map<std::string, std::string> LanguageDB::m_main_languages =
{
    {"aa", "ET"},
    {"ar", "EG"},
    {"bn", "BD"},
    {"ca", "ES"},
    {"cs", "CZ"},
    {"da", "DK"},  // Danish
    {"de", "DE"},
    {"el", "GR"},
    {"en", "US"},
    {"eo", "XX"},  // Esperanto
    {"es", "ES"},
    {"eu", "ES"},
    {"fr", "FR"},
    {"fy", "BL"},
    {"ga", "IE"},  // Irish
    {"gd", "GB"},  // Scottish Gaelic
    {"it", "IT"},
    {"li", "NL"},
    {"nl", "NL"},
    {"om", "ET"},
    {"pl", "PL"},  // Polish
    {"pa", "PK"},
    {"pt", "PT"},
    {"ro", "RO"},  // Romanian
    {"ru", "RU"},
    {"so", "SO"},
    {"sr", "RS"},
    {"sv", "SE"},
    {"ti", "ER"},
    {"tr", "TR"},  // Turkish
};


LanguageDB::LanguageDB(const ContextBase& context) :
    Super(context),
    m_iso_codes(std::make_unique<ISOCodes>(context))
{}

LanguageDB::~LanguageDB()
{}

std::unique_ptr<LanguageDB> LanguageDB::make(const ContextBase& context)
{
    return std::make_unique<LanguageDB>(context);
}

std::vector<std::string> LanguageDB::get_main_languages()
{
    std::vector<std::string> v;
    for (auto it : m_main_languages)
        v.emplace_back(it.first);
    return v;
}

std::string LanguageDB::get_language_full_name_or_id(const std::string& lang_id)
{
    auto full_name = get_language_full_name(lang_id);
    if (!full_name.empty())
        return full_name;
    return lang_id;
}

std::string LanguageDB::get_language_full_name(const std::string& lang_id)
{
    std::string lang_code, country_code;
    std::tie(lang_code, country_code) = split_lang_id(lang_id);
    auto name = m_iso_codes->get_translated_language_name(lang_code);
    if (!country_code.empty())
    {
        std::string country = m_iso_codes->get_translated_country_name(country_code);
        if (country.empty())
            country = country_code;

        if (!country.empty() &&
            country != "XX")  // Esperanto has no native country
        {
            name += " (" + country + ")";
        }
    }
    return name;
}

std::string LanguageDB::get_language_name(const std::string& lang_id)
{
    std::string lang_code, country_code;
    std::tie(lang_code, country_code) = split_lang_id(lang_id);
    return m_iso_codes->get_translated_language_name(lang_code);
}

std::string LanguageDB::get_language_code(const std::string& lang_id)
{
    std::string lang_code, country_code;
    std::tie(lang_code, country_code) = split_lang_id(lang_id);
    return lang_code;
}

std::string LanguageDB::find_system_model_language_id(const std::string& lang_id)
{
    std::string lang_id_ = lang_id;
    std::vector<std::string> known_ids;
    get_system_model_language_ids(known_ids);
    if (!contains(known_ids, lang_id))
    {
        std::string lang_code, country_code;
        std::tie(lang_code, country_code) = split_lang_id(lang_id);
        lang_id_ = get_main_lang_id(lang_code);
    }
    return lang_id_;
}

void LanguageDB::get_system_model_language_ids(std::vector<std::string>& ids)
{
    auto ws = get_word_suggestions();
    if (ws)
        return ws->get_system_model_names(ids);
}

std::vector<std::string> LanguageDB::get_language_ids()
{
    auto ws = get_word_suggestions();
    if (ws)
        return ws->get_merged_model_names();
    return {};
}

std::string LanguageDB::get_main_lang_id(const std::string& lang_code)
{
    std::string lang_id;
    std::string country_code = get_value(m_main_languages, lang_code);
    if (!country_code.empty())
        lang_id = lang_code + "_" + country_code;
    return lang_id;
}

std::vector<std::string> LanguageDB::get_language_ids_in_locale()
{
    std::vector<std::string> lang_ids;
    for (auto& locale_id : get_locale_ids())
    {
        std::string lang_id = get_lang_id_from_locale_id(locale_id);
        if (!lang_id.empty())
            lang_ids.emplace_back(lang_id);
    }
    return lang_ids;
}

std::string LanguageDB::get_lang_id_from_locale_id(const std::string& locale_id)
{
    std::string lang_id;

    std::string lang_code, country_code, encoding;
    std::tie(lang_code, country_code, encoding) = split_locale_id(locale_id);
    if (!lang_code.empty() &&
        lang_code != "POSIX" &&
        lang_code != "C")
    {
        lang_id = lang_code;
        if (!country_code.empty())
            lang_id += "_" + country_code;
    }

    return lang_id;
}

std::vector<std::string> LanguageDB::get_locale_ids()
{
    if (m_locale_ids.empty())
        m_locale_ids = find_locale_ids();
    return m_locale_ids;
}

std::vector<std::string> LanguageDB::find_locale_ids()
{
    std::vector<std::string> locale_ids;
    std::string output;
    std::string cmd = "locale -a";

    try
    {
        output = exec_cmd(cmd);
    }
    catch (const ProcessException& ex)
    {
        LOG_ERROR << "failed to execute " << repr(cmd)
                  << ": " << ex.what();
    }

    for (auto& id : split(output, '\n'))
        if (!id.empty())
            locale_ids.emplace_back(id);
    return locale_ids;
}

std::string LanguageDB::get_system_default_lang_id()
{
    std::string locale_id = get_system_default_locale_id();
    return  get_lang_id_from_locale_id(locale_id);
}

std::string LanguageDB::get_system_default_locale_id()
{
    static auto vars = array_of<std::string>("LC_ALL",
                                             "LC_CTYPE",
                                             "LANG",
                                             "LANGUAGE");
    for (const auto& var : vars)
    {
        const char* c = getenv(var.c_str());
        if (c)
        {
            std::string value{c};
            if (var == "LANGUAGE")
            {
                auto fields = split(value, ':');
                if (!fields.empty())
                    return fields[0];
            }
            return value;
        }
    }
    return {};
}

std::tuple<std::string, std::string, std::string> LanguageDB::split_locale_id(const std::string& locale_id)
{
    auto tokens = split(locale_id, '.');
    auto lang_id = tokens.size() >= 1 ? tokens[0] : "";
    auto encoding = tokens.size() >= 2 ? tokens[1] : "";
    std::string lang_code, country_code;
    std::tie(lang_code, country_code) = split_lang_id(lang_id);
    return {lang_code, country_code, encoding};
}

std::tuple<std::string, std::string> LanguageDB::split_lang_id(const std::string& lang_id)
{
    auto tokens = split(lang_id, '_');
    auto lang_code = tokens.size() >= 1 ? tokens[0] : "";
    auto country_code = tokens.size() >= 2 ? tokens[1] : "";
    return {lang_code, country_code};
}




ISOCodes::ISOCodes(const ContextBase& context) :
    Super(context)
{
    read_all();
}

ISOCodes::~ISOCodes()
{}

std::string ISOCodes::get_language_name(const std::string& lang_code)
{
    return get_value(m_languages, lang_code);
}

std::string ISOCodes::get_country_name(const std::string& country_code)
{
    return get_value(m_countries, country_code);
}

std::string ISOCodes::get_translated_language_name(const std::string& lang_code)
{
    return dgettext("iso_639", this->get_language_name(lang_code).c_str());
}

std::string ISOCodes::get_translated_country_name(const std::string& country_code)
{
    std::string country_name = this->get_country_name(country_code);
    if (country_name.empty())
        country_name = country_code;
    return dgettext("iso_3166", country_name.c_str());
}

void ISOCodes::read_all()
{
    read_languages();
    read_countries();
}

void ISOCodes::read_languages()
{
    std::string fn = fs::path(config()->isocodes_dir) / "iso_639.xml";
    std::unique_ptr<XMLParser> parser = XMLParser::make_xml2(*this);
    auto xml_node = parser->parse_file_utf8(fn);
    if (!xml_node)
    {
        LOG_ERROR << "failed to read '" << fn << "'";
        return;
    }

    xml_node->for_each_element_by_tag_name("iso_639_entry", [&](const XMLNodePtr& node)
    {
        auto lang_code = node->get_attribute("iso_639_1_code");
        if (lang_code.empty())
            lang_code = node->get_attribute("iso_639_2T_code");

        auto lang_name = node->get_attribute("name");
        if (!lang_code.empty() && !lang_name.empty())
            m_languages[lang_code] = lang_name;
    });
}

void ISOCodes::read_countries()
{
    std::string fn = fs::path(config()->isocodes_dir) / "iso_3166.xml";
    std::unique_ptr<XMLParser> parser = XMLParser::make_xml2(*this);
    auto xml_node = parser->parse_file_utf8(fn);
    if (!xml_node)
    {
        LOG_ERROR << "failed to read '" << fn << "'";
        return;
    }

    xml_node->for_each_element_by_tag_name("iso_3166_entry", [&](const XMLNodePtr& node)
    {
        auto country_code = node->get_attribute("alpha_2_code");
        auto country_name = node->get_attribute("name");

        if (!country_code.empty() && !country_name.empty())
            m_countries[upper(country_code)] = country_name;
    });
}
