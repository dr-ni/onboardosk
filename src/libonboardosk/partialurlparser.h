#ifndef PARTIALURLPARSER_H
#define PARTIALURLPARSER_H

#include <memory>
#include <string>
#include <vector>

#include "tools/noneable.h"

class UString;
class UStringPattern;

// Parse partial URLs and predict separators.
// Parsing is neither complete nor RFC prove but probably doesn't
// have to be either. The goal is to save key strokes for the
// most common cases.
class PartialURLParser
{
    public:
        PartialURLParser();
        ~PartialURLParser();

        // Is this maybe something looking like an URL?;
        bool is_maybe_url(const UString& context);

        bool is_maybe_filename(const UString& string);

        // Get word separator to add after inserting a prediction choice.
        UString get_auto_separator(const UString& context);

        // Get auto separator for a partial filename.
        UString get_filename_separator(const UString& filename);

        std::vector<UString> tokenize_url(const UString& url);

        // Extract separator from a list of matching filenames.;
        Noneable<UString> get_separator_from_file_list(const UString& filename,
                                                       const std::vector<UString>& files);
    private:
        static const std::vector<std::string> m_gTLDs;
        static const std::vector<std::string> m_usTLDs;
        static const std::vector<std::string> m_ccTLDs;
        static const std::vector<std::string> m_TLDs;

        static const std::vector<std::string> m_schemes;
        static const std::vector<std::string> m_protocols;
        static const std::vector<std::string> m_all_schemes;

        std::unique_ptr<UStringPattern> m_url_pattern;
};

#endif // PARTIALURLPARSER_H
