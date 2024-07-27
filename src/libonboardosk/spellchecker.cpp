#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include <hunspell/hunspell.h>

#include "tools/container_helpers.h"
#include "tools/logger.h"
#include "tools/string_helpers.h"
#include "tools/ustringregex.h"

#include "configuration.h"
#include "languagedb.h"
#include "spellchecker.h"


template<class T>
const std::string get_type_name(T* p)
{
    if (p)
        return typeid(*p).name();
    return "nullptr";
}


// Abstract base class of all spellchecker backends
class SCBackend : public ContextBase
{
    public:
        using Super = ContextBase;

        SCBackend(const ContextBase& context) :
            Super(context)
        {}
        virtual ~SCBackend()
        {
        }

        virtual void start(const std::vector<std::string>& dict_ids)
        {
            m_active_dicts = dict_ids;
            LOG_INFO << "starting " << repr(get_type_name(this))
                     << " " << dict_ids;
        }

        virtual void stop()
        {
            if (is_running())
            {
                LOG_INFO << "stopping " << repr(get_type_name(this));
                m_active_dicts.clear();
            }
        }

        virtual bool is_running() = 0;

        // Query for spelling suggestions.
        // Text may contain one or more words. Each word generates its own
        // list of suggestions. The spell checker backend decides about
        // word boundaries.
        virtual void query(std::vector<SpellChecker::Result>& results,
                           const UString& text) = 0;

        // Return raw supported dictionary ids.
        virtual std::vector<std::string> get_supported_dict_ids() = 0;

        // Return active dictionary ids.
        std::vector<std::string> get_active_dict_ids()
        {
            return m_active_dicts;
        }

    private:
       std::vector<std::string> m_active_dicts;
};


// Hunspell backend using the C API
class SCBackendHunspell : public SCBackend
{
    public:
        using Super = SCBackend;

        SCBackendHunspell(const ContextBase& context) :
            Super(context)
        {
        }
        virtual ~SCBackendHunspell()
        {
            stop();
        }

    private:
        std::unique_ptr<Hunhandle, decltype(&Hunspell_destroy)> m_hh{nullptr, Hunspell_destroy};

        virtual void start(const std::vector<std::string>& dict_ids) override
        {
            Super::start(dict_ids);

            if (!dict_ids.empty())
            {
                std::string dic, aff;
                std::tie(dic, aff) = search_dict_files(dict_ids[0]);
                LOG_INFO << "using hunspell files " << dic
                         << " " << aff;
                if (!dic.empty())
                {
                    m_hh = {Hunspell_create(aff.c_str(), dic.c_str()),
                            Hunspell_destroy};
                    if (!m_hh)
                        LOG_ERROR << "failed to create hunspell backend:"
                                  << " Hunspell_create failed";
                }
            }
        }

        virtual void stop() override
        {
            if (is_running())
            {
                Super::stop();
                m_hh = nullptr;
            }
        }

        virtual bool is_running() override
        {
            return m_hh != nullptr;
        }


        // Query for spelling suggestions.
        // Text may contain one or more words. Each word generates its own
        // list of suggestions. Word boundaries are chosen to match the
        // behavior of the hunspell command line tool, i.e. split at '-','_'
        // and whitespace.
        virtual void query(std::vector<SpellChecker::Result>& results,
                           const UString& text) override
        {
            static UStringPattern split_words_pattern(R"([^-_\s]+)",
                                                      UStringRegexFlag::DOTALL);
            if (m_hh)
            {
                UStringMatcher matcher(&split_words_pattern, text);
                while (matcher.find())
                {
                    UString word = matcher.group();
                    TextPos begin = matcher.begin();
                    TextPos end   = matcher.end();
                    USpan span = {begin, end, word};

                    try {
                        if (spell(word) == 0)
                        {
                            std::vector<UString> suggestions;
                            suggest(word, suggestions);
                            results.emplace_back(SpellChecker::Result{span, suggestions});
                        }
                    }
                    catch (const std::runtime_error& ex)
                    {
                        LOG_WARNING << ex.what();

                        // Assume the offending character isn't part of the
                        // target language and consider this word to be not
                        // in the dictionary, i.e. misspelled without suggestions.
                        results.emplace_back(SpellChecker::Result{span, {}});
                    }
                }
            }
        }

    private:
        int spell(const UString& word)
        {
            char* encoding = Hunspell_get_dic_encoding(m_hh.get());
            if (!encoding)
                throw std::runtime_error("SCBackendHunspell::spell: "
                                         "unknown dictionary encoding");

            std::string w = word.encode(encoding);
            return Hunspell_spell(m_hh.get(), w.c_str());
        }

        void suggest(const UString& word, std::vector<UString>& suggestions)
        {
            char* encoding = Hunspell_get_dic_encoding(m_hh.get());
            if (!encoding)
                throw std::runtime_error("SCBackendHunspell::suggest: "
                                         "unknown dictionary encoding");

            char** slst;
            struct SListDeleter
            {
                ~SListDeleter() { Hunspell_free_list(hh, slst, n); }

                Hunhandle* hh;
                int n;
                char*** slst;
            };

            std::string w = word.encode(encoding);
            int n = Hunspell_suggest(m_hh.get(), &slst, w.c_str());
            SListDeleter d{m_hh.get(), n, &slst};   // decode throws exceptions

            for (int i=0; i<n; i++)
            {
                UString us = UString::decode(slst[i], encoding);
                suggestions.emplace_back(us);
            }
        }

        // Return raw supported dictionary ids.;
        // They may !all be valid language ids, e.g. en-GB;
        // instead of en_GB for myspell dicts.;
        virtual std::vector<std::string> get_supported_dict_ids() override
        {
            std::vector<std::string> dict_ids;

            // search for dictionary files
            std::vector<std::string> filenames;
            auto dirs = get_dict_paths();
            for (const auto& dir : dirs)
            {
                if (fs::is_directory(dir))
                {
                    try
                    {
                        for (auto &p : fs::directory_iterator(dir))
                        {
                            fs::path path = p.path();
                            if (path.extension() == ".dic")
                                filenames.emplace_back(path);
                        }
                    }
                    catch (const fs::filesystem_error& ex)
                    {
                        LOG_ERROR << "directory_iterator failed: " << ex.what();
                    }
                }
            }

            // extract language ids
            for (const auto& fn : filenames)
            {
                std::string lang_id = fs::path(fn).filename().replace_extension("");
                if (!startswith(lower(lang_id), "hyph"))
                    dict_ids.emplace_back(lang_id);
            }

            return dict_ids;
        }

        // Locate dictionary and affix files for the given dict_id
        std::tuple<std::string, std::string> search_dict_files(const std::string& dict_id)
        {
            auto paths = get_dict_paths();
            auto aff = find_file_in_paths(paths, dict_id, "aff");
            auto dic = find_file_in_paths(paths, dict_id, "dic");
            return {dic, aff};
        }

        // Search for a file name in multiple paths.
        static std::string find_file_in_paths(const std::vector<std::string>& paths,
                                              const std::string& name,
                                              const std::string& extension)
        {
            for (const auto& path : paths)
            {
                auto file = fs::path(path) / (name + "." + extension);
                if (fs::exists(file))
                    return file;
            }
            return {};
        }

        // Return all paths where dictionaries may be located.
        // Path logic taken from the source of the hunspell command line tool.
        std::vector<std::string> get_dict_paths()
        {
            std::vector<std::string> paths;

            char pathsep = fs::path::preferred_separator;
            std::string usr_share_dir = config()->usr_share_dir;
            std::string usr_lib_dir = config()->usr_lib_dir;

            char* home = getenv("HOME");
            std::string home_dir = home ? home : "";

            std::vector<std::string> lib_dirs
            {
                fs::path(usr_share_dir) / "hunspell",
                fs::path(usr_share_dir) / "myspell",
                fs::path(usr_share_dir) / "myspell/dicts",
                "/Library/Spelling"
            };

            // user custom dictionaries
            std::vector<std::string> user_ooo_dirs
            {
                fs::path(home_dir) / ".openoffice.org/4/user/wordbook",
                fs::path(home_dir) / ".openoffice.org/3/user/wordbook",
                fs::path(home_dir) / ".openoffice.org2/user/wordbook",
                fs::path(home_dir) / ".openoffice.org2.0/user/wordbook",
                fs::path(home_dir) / "Library/Spelling"
            };

            // this is probably out-dated
            std::vector<std::string> ooo_dirs
            {
                "/opt/openoffice.org/basis3.0/share/dict/ooo",
                fs::path(usr_lib_dir) / "openoffice.org/basis3.0/share/dict/ooo",
                "/opt/openoffice.org2.4/share/dict/ooo",
                fs::path(usr_lib_dir) / "openoffice.org2.4/share/dict/ooo,",
                "/opt/openoffice.org2.3/share/dict/ooo",
                fs::path(usr_lib_dir) / "openoffice.org2.3/share/dict/ooo",
                "/opt/openoffice.org2.2/share/dict/ooo",
                fs::path(usr_lib_dir) / "openoffice.org2.2/share/dict/ooo",
                "/opt/openoffice.org2.1/share/dict/ooo",
                fs::path(usr_lib_dir) / "openoffice.org2.1/share/dict/ooo",
                "/opt/openoffice.org2.0/share/dict/ooo",
                fs::path(usr_lib_dir) / "openoffice.org2.0/share/dict/ooo"
            };

            paths.emplace_back(".");

            char* dicpath = getenv("DICPATH");
            if (dicpath)
                extend(paths, split(dicpath, pathsep));

            extend(paths, lib_dirs);

            if (!home_dir.empty())
            {
                paths.emplace_back(home_dir);
                extend(paths, user_ooo_dirs);
            }

            extend(paths, ooo_dirs);

            return paths;
        }

};

SpellChecker::SpellChecker(const ContextBase& context) :
    Super(context)
{}

SpellChecker::~SpellChecker()
{}

void SpellChecker::set_backend(SpellcheckBackend::Enum backend)
{
    if (backend == SpellcheckBackend::HUNSPELL)
    {
        std::unique_ptr<SCBackend> p(std::make_unique<SCBackendHunspell>(*this));
        m_backend = std::move(p);
    }
    else if (backend == SpellcheckBackend::ASPELL)
    {
        LOG_WARNING << "ASpell backend not implemented in libonboardosk, "
                    << "use the hunspell backend. Spell checking disabled.";
        m_backend = nullptr;
    }
    else
    {
        m_backend = nullptr;
    }

    this->invalidate_query_cache();
}

bool SpellChecker::set_dict_ids(const std::vector<std::string>& dict_ids)
{
    bool success = false;
    std::vector<std::string> ids = find_matching_dicts(dict_ids);
    if (m_backend &&
        ids != m_backend->get_active_dict_ids())
    {
        m_backend->stop();
        if (!ids.empty())
        {
            m_backend->start(ids);
            success = true;
        }
        else
        {
            LOG_INFO << "no matching dictionaries for " <<
                        repr(get_type_name(m_backend.get()))
                     << " " << dict_ids;
        }
    }
    invalidate_query_cache();

    return success;
}

std::vector<std::string> SpellChecker::find_matching_dicts(const std::vector<std::string>& dict_ids)
{
    std::vector<std::string> results;
    for (auto& dict_id : dict_ids)
    {
        std::string id = find_matching_dict(dict_id);
        if (!id.empty())
            results.emplace_back(id);
    }
    return results;
}

std::string SpellChecker::find_matching_dict(const std::string& dict_id)
{
    auto language_db = get_language_db();
    std::vector<std::string> available_dict_ids = get_supported_dict_ids();

    std::string result;

    // try an exact match
    if (contains(available_dict_ids, dict_id))
    {
        result = dict_id;
    }
    else
    {
        // try separator "-", common for myspell dicts
        std::string alt_id = replace_all(dict_id, "_", "-");
        if (contains(available_dict_ids, alt_id))
        {
            result = alt_id;
        }
        else if (language_db)
        {
            // try the language code alone
            std::string lang_code, country_code;
            std::tie(lang_code, country_code) = language_db->split_lang_id(dict_id);
            if (contains(available_dict_ids, lang_code))
            {
                result = lang_code;
            }
            else
            {
                // try adding the languages main country
                std::string lang_id = language_db->get_main_lang_id(lang_code);
                if (!lang_id.empty() &&
                    contains(available_dict_ids, lang_id))
                {
                    result = lang_id;
                }
            }
        }
    }

    return result;
}

USpan SpellChecker::find_corrections(std::vector<UString>& corrections,
                                     const UString& word, TextPos caret_offset)
{
    Result const* r{};

    if (m_backend)
    {
        const std::vector<Result>& results = query_cached(word);

        // hunspell splits words at underscores && then
        // returns results for multiple sub-words.
        // -> find the sub-word at the current caret offset.

        for (const auto& result : results)
        {
            if (result.span.begin > caret_offset)
                break;
            r = &result;
        }
    }

    if (r)
    {
        corrections = r->choices;
        return r->span;
    }

    return {};
}

void SpellChecker::find_incorrect_spans(const UString& word, std::vector<USpan>& spans)
{
    if (m_backend)
    {
        const std::vector<Result>& results = query_cached(word);

        // hunspell splits words at underscores && then
        // returns results for multiple sub-words.
        // -> find the sub-word at the current caret offset.
        for (const auto& result : results)
            spans.emplace_back(result.span);
    }
}

const std::vector<SpellChecker::Result>& SpellChecker::query_cached(const UString& word)
{
    auto it = std::find_if(m_cached_queries.begin(), m_cached_queries.end(),
                           [&](const CachedQuery& q){return q.first == word;});
    if (it == m_cached_queries.end())
    {
        // limit cache size
        while (m_cached_queries.size() >= m_max_query_cache_size)
            m_cached_queries.pop_front();

        // query backend
        it = m_cached_queries.emplace(m_cached_queries.end(), CachedQuery{word, {}});
        query(it->second, word);
    }

    return it->second;
}

void SpellChecker::query(std::vector<SpellChecker::Result>& results,
                         const UString& word)
{
    return m_backend->query(results, word);
}

void SpellChecker::invalidate_query_cache()
{
    m_cached_queries.clear();
}

std::vector<std::string> SpellChecker::get_supported_dict_ids()
{
    return m_backend->get_supported_dict_ids();
}

