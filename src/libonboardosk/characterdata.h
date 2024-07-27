#ifndef CHARACTERDATA_H
#define CHARACTERDATA_H

#include <memory>
#include <string>
#include <vector>


struct CharacterCategory
{
    int level;
    std::string label;
    std::vector<const char*> sequences;
};


class SymbolData
{
    public:
        SymbolData(const std::vector<CharacterCategory>& symbol_data) :
            m_symbol_data(symbol_data)
        {
        }

        std::vector<std::string> get_category_labels()
        {
            std::vector<std::string> v;
            for (auto& category : m_symbol_data)
                if (category.level == 0)
                    v.emplace_back(category.label);
            return v;
        }

        // Walk along all subcategories.
        const std::vector<CharacterCategory>& get_subcategories()
        {
            return m_symbol_data;
        }

    private:
        const std::vector<CharacterCategory>& m_symbol_data;
};


// Singleton class providing emoji data && general Unicode information.
class CharacterData
{
    private:
    public:
        enum ContentType {EMOJI, SYMBOLS};

        CharacterData();
        ~CharacterData();

        static std::unique_ptr<SymbolData> get_symbol_data(ContentType content_type);

    private:
        static const std::vector<CharacterCategory> m_symbol_data;
        static const std::vector<CharacterCategory> m_short_emoji_data;
};

std::string emoji_filename_from_sequence(const std::string& sequence);

#endif // CHARACTERDATA_H
