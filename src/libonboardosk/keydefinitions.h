#ifndef KEYDEFINITIONS_H
#define KEYDEFINITIONS_H

#include <array>
#include <ostream>
#include <string>
#include <map>
#include <vector>

#include "tools/keydecls.h"

class KeyCodeSymbols
{
    public:
        enum Enum {
            Return   = 36,
            KP_Enter = 104,
            C        = 54,
        };
};

class Modifier
{
    public:
        enum Enum {
            NONE = 0,
            SHIFT = 1<<0,
            CAPS = 1<<1,
            CTRL = 1<<2,
            ALT = 1<<3,
            NUMLK = 1<<4,
            MOD3 = 1<<5,
            SUPER = 1<<6,
            ALTGR = 1<<7,

            // modifiers affecting labels
            LABEL_MODIFIERS = SHIFT | CAPS | NUMLK | ALTGR,
        };

        // translate modifier names traditionally used by Onboards's layouts
        static Enum mod_name_to_modifier(const std::string s);
        static Enum key_id_to_modifier(const std::string s);
        static std::vector<std::string> mod_group_to_key_ids(const std::string s);
        static size_t modifier_to_bit_index(Enum modifier);
        static Enum bit_index_to_modifier(size_t index);
};

class ModifierCounts
{
    public:
        using Container = std::array<int, 8>;

        int& operator[](Modifier::Enum modifier);
        int operator[](Modifier::Enum modifier) const;

        void assign_filtered(const ModifierCounts& src, ModMask mask);

        // get bit mask of current modifiers
        ModMask get_mod_mask();

        Modifier::Enum mod_at(size_t index) const;

        bool empty();

        size_t size() const
        { return m_counts.size();}

        Container::iterator begin()
        { return m_counts.begin();}

        Container::iterator end()
        { return m_counts.end();}

    private:
        friend std::ostream&operator<<(std::ostream& out, const ModifierCounts& m);

        Container m_counts{};
};
std::ostream& operator<< (std::ostream& out, const ModifierCounts& m);


std::vector<int> permute_modmask(ModMask mask);

// Parses a key combination into a list of modifier masks and key_ids.
// The key-id part of the combo may contain a regex pattern.
using KeyIDModEntry = std::pair<std::string, ModMask>;
std::vector<KeyIDModEntry> parse_key_combination(const std::vector<std::string>& combo,
                                                 const std::vector<std::string>& avaliable_key_ids={});

KeySym get_keysym_from_name(const std::string& keysym_name);

#endif // KEYDEFINITIONS_H
