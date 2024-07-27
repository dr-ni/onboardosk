#include <algorithm>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "tools/container_helpers.h"
#include "tools/path_helpers.h"
#include "tools/string_helpers.h"
#include "tools/ustringmain.h"
#include "tools/ustringregex.h"

#include "partialurlparser.h"


const std::vector<std::string> PartialURLParser::m_gTLDs =
{
    "aero", "asia", "biz", "cat", "com", "coop", "info", "int",
    "jobs", "mobi", "museum", "name", "net", "org", "pro", "tel",
    "travel", "xxx"
};
const std::vector<std::string> PartialURLParser::m_usTLDs = {"edu", "gov", "mil"};
const std::vector<std::string> PartialURLParser::m_ccTLDs =
{
    "ac", "ad", "ae", "af", "ag", "ai", "al", "am", "an", "ao",
    "aq", "ar", "as", "at", "au", "aw", "ax", "az", "ba", "bb",
    "bd", "be", "bf", "bg", "bh", "bi", "bj", "bm", "bn", "bo",
    "br", "bs", "bt", "bv", "bw", "by", "bz", "ca", "cc", "cd",
    "cf", "cg", "ch", "ci", "ck", "cl", "cm", "cn", "co", "cr",
    "cs", "cu", "cv", "cx", "cy", "cz", "dd", "de", "dj", "dk",
    "dm", "do", "dz", "ec", "ee", "eg", "eh", "er", "es", "et",
    "eu", "fi", "fj", "fk", "fm", "fo", "fr", "ga", "gb", "gd",
    "ge", "gf", "gg", "gh", "gi", "gl", "gm", "gn", "gp", "gq",
    "gr", "gs", "gt", "gu", "gw", "gy", "hk", "hm", "hn", "hr",
    "ht", "hu", "id", "ie", "il", "im", "in", "io", "iq", "ir",
    "is", "it", "je", "jm", "jo", "jp", "ke", "kg", "kh", "ki",
    "km", "kn", "kp", "kr", "kw", "ky", "kz", "la", "lb", "lc",
    "li", "lk", "lr", "ls", "lt", "lu", "lv", "ly", "ma", "mc",
    "md", "me", "mg", "mh", "mk", "ml", "mm", "mn", "mo", "mp",
    "mq", "mr", "ms", "mt", "mu", "mv", "mw", "mx", "my", "mz",
    "na", "nc", "ne", "nf", "ng", "ni", "nl", "no", "np", "nr",
    "nu", "nz", "om", "pa", "pe", "pf", "pg", "ph", "pk", "pl",
    "pm", "pn", "pr", "ps", "pt", "pw", "py", "qa", "re", "ro",
    "rs", "ru", "rw", "sa", "sb", "sc", "sd", "se", "sg", "sh",
    "si", "sj", "sk", "sl", "sm", "sn", "so", "sr", "ss", "st",
    "su", "sv", "sy", "sz", "tc", "td", "tf", "tg", "th", "tj",
    "tk", "tl", "tm", "tn", "to", "tp", "tr", "tt", "tv", "tw",
    "tz", "ua", "ug", "uk", "us", "uy", "uz", "va", "vc", "ve",
    "vg", "vi", "vn", "vu", "wf", "ws", "ye", "yt", "yu", "za",
    "zm", "zw"
};
const std::vector<std::string> PartialURLParser::m_TLDs = m_gTLDs + m_usTLDs + m_ccTLDs;
const std::vector<std::string> PartialURLParser::m_schemes = {"http", "https", "ftp", "file"};
const std::vector<std::string> PartialURLParser::m_protocols = {"mailto", "apt"};
const std::vector<std::string> PartialURLParser::m_all_schemes = m_schemes + m_protocols;


PartialURLParser::PartialURLParser() :
    m_url_pattern(std::make_unique<UStringPattern>(R"(([\w-]+)|(\W+))"))
{
}

PartialURLParser::~PartialURLParser()
{
}

std::vector<UString> PartialURLParser::tokenize_url(const UString& url)
{
    std::vector<UString> v;
    m_url_pattern->find_all(url, v);
    return v;
}

bool PartialURLParser::is_maybe_url(const UString& context)
{
    auto tokens = tokenize_url(context);

    // with scheme
    UString token;
    UString septok;
    if (tokens.size() >= 2)
    {
        token  = tokens[0];
        septok = tokens[1];
        if (contains(m_all_schemes, token.to_utf8()) &&
            septok.startswith(":"))
            return true;
    }

    // without scheme
    if (tokens.size() >= 5)
    {
        if (tokens[1] == "." && tokens[3] == ".")
        {
            std::vector<UString> hostname;
            size_t index = find_index(tokens, UString{"/"});
            if (index != NPOS && index >= 4)
                hostname = slice(tokens, 0, static_cast<int>(index));
            else
                hostname = tokens;

            if (hostname.size() && contains(m_TLDs, hostname.back().to_utf8()))
            {
                return true;
            }
        }
    }

    return false;
}

bool PartialURLParser::is_maybe_filename(const UString& s)
{
    return s.contains("/");
}

UString PartialURLParser::get_auto_separator(const UString& context)
{
    Noneable<UString> separator;

    enum {URL_SCHEME, URL_PROTOCOL, URL_DOMAIN, URL_PATH};
    int component = URL_SCHEME;
    UString last_septok;

    std::vector<UStringMatch> matches;
    m_url_pattern->find_all(context, matches);
    for (size_t index=0; index<matches.size(); index++)
    {
        const auto& match = matches[index];
        const UString& group0 = match.groups.at(0);
        const UString& group1 = match.groups.at(1);
        const UString& token  = group1.empty() ? "" : group0;
        const UString& septok = group1.empty() ? group0 : "";

        if (!septok.empty())
            last_septok = septok;

        UString next_septok;
        if (index < matches.size()-1)
        {
            const auto& next_match = matches.at(index+1);
            const UString& next_group0 = next_match.groups.at(0);
            const UString& next_group1 = next_match.groups.at(1);
            next_septok = next_group1.empty() ? next_group0 : "";
        }

        if (component == URL_SCHEME)
        {
            if (!token.empty())
            {
                if (token == "file")
                {
                    separator = ":///";
                    component = URL_PATH;
                    continue;
                }
                if (contains(m_schemes, token.to_utf8()))
                {
                    separator = "://";
                    component = URL_DOMAIN;
                    continue;
                }
                else if (contains(m_protocols, token.to_utf8()))
                {
                    separator = ":";
                    component = URL_PROTOCOL;
                    continue;
                }
                else
                {
                    component = URL_DOMAIN;
                }
            }
        }

        if (component == URL_DOMAIN)
        {
            if (!token.empty())
            {
                separator = ".";
                if (last_septok == "." &&
                    next_septok != "." &&
                    contains(m_TLDs, token.to_utf8()))
                {
                    separator = "/";
                    component = URL_PATH;
                    continue;
                }
            }
        }

        if (component == URL_PATH)
        {
            separator = "";
        }

        if (component == URL_PROTOCOL)
        {
            separator = "";
        }
    }

    if (component == URL_PATH && separator.value.empty())
    {
        UString file_scheme = "file://";
        if (context.startswith(file_scheme))
        {
            UString filename = context.slice(static_cast<int>(file_scheme.size()));
            separator = get_filename_separator(filename);
        }
        else
        {
            if (last_septok != ".")
                separator = "/";
        }
    }

    if (separator.is_none())
        separator = " ";  // may be entering search terms, keep space as default

    return separator;
}

UString PartialURLParser::get_filename_separator(const UString& filename)
{
    Noneable<UString> separator;

    auto path = fs::path(filename.to_utf8());
    if (path.is_absolute())
    {
        std::vector<UString> files;
        auto dir = path;
        dir.remove_filename();
        try
        {
            for (const auto &p : fs::directory_iterator(dir))
                if (startswith(p.path(), path))
                    files.emplace_back(p.path());

            // Add an entry for directories too.
            // No need to actually look inside this way.
            if (fs::is_directory(path))
                files.emplace_back(path / "something");
        }
        catch (const fs::filesystem_error& ex)
        {
            // LOG_ERROR << "directory_iterator failed: " << ex.what();
        }

        separator = get_separator_from_file_list(filename, files);
    }

    if (separator.is_none())
    {
        if (fs::path(filename.to_utf8()).has_extension())
            separator = " ";
        else
            separator = "";
    }

    return separator;
}

Noneable<UString> PartialURLParser::get_separator_from_file_list(const UString& filename,
                                                                 const std::vector<UString>& files)
{
    Noneable<UString> separator;

    int l = static_cast<int>(filename.size());
    std::vector<UString> separators;
    for (auto& f : files)
    {
        if (f.startswith(filename))
        {
            UString s = f.slice(l, l+1);
            if (!contains(separators, s))
                separators.emplace_back(s);
        }
    }

    // directory?
    if (separators.size() == 2 &&
        contains(separators, UString("/")) &&
        contains(separators, {}))
    {
        separator = "/";
    }
    // end of filename?
    else if (separators.size() == 1 && contains(separators, {}))
    {
        separator = " ";
    }
    // unambigous separator?
    else if (separators.size() == 1)
    {
        separator = separators[0];
    }
    // multiple separators
    else if (!separators.empty())
    {
        separator = "";
    }

    return separator;
}

