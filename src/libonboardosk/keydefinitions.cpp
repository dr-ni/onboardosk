
#include <assert.h>

#include "tools/container_helpers.h"
#include "tools/iostream_helpers.h"
#include "tools/string_helpers.h"

#include "exception.h"
#include "keydefinitions.h"

using std::string;
using std::vector;

Modifier::Enum Modifier::mod_name_to_modifier(const std::string s)
{
    static std::map<std::string, Modifier::Enum> m = {
        {"shift", SHIFT},
        {"caps", CAPS},
        {"control", CTRL},
        {"mod1", ALT},
        {"mod2", NUMLK},
        {"mod3", MOD3},
        {"mod4", SUPER},
        {"mod5", ALTGR},
    };
    return get_value(m, s);
}

static std::map<std::string, Modifier::Enum> map_key_id_to_modifier = {
    {"RTSH", Modifier::SHIFT},
    {"LFSH", Modifier::SHIFT},
    {"CAPS", Modifier::CAPS},
    {"RCTL", Modifier::CTRL},
    {"LCTL", Modifier::CTRL},
    {"RALT", Modifier::ALTGR},
    {"LALT", Modifier::ALT},
    {"NMLK", Modifier::NUMLK},
    {"LWIN", Modifier::SUPER},
};

Modifier::Enum Modifier::key_id_to_modifier(const std::string s)
{
    return get_value(map_key_id_to_modifier, s);
}

std::vector<std::string> Modifier::mod_group_to_key_ids(const std::string s)
{
    static std::map<string, vector<string>> m = {
        {"SHIFT", {"LFSH"}},
        {"CTRL", {"LCTL"}},
    };
    return get_value(m, s);
}

size_t Modifier::modifier_to_bit_index(Modifier::Enum modifier)
{
    switch (modifier)
    {
        case SHIFT: return 0;
        case CAPS: return 1;
        case CTRL: return 2;
        case ALT: return 3;
        case NUMLK: return 4;
        case MOD3: return 5;
        case SUPER: return 6;
        case ALTGR: return 7;
        default: return static_cast<size_t>(-1);
    }
}

Modifier::Enum Modifier::bit_index_to_modifier(size_t index)
{
    switch (index)
    {
        case 0: return SHIFT;
        case 1: return CAPS;
        case 2: return CTRL;
        case 3: return ALT;
        case 4: return NUMLK;
        case 5: return MOD3;
        case 6: return SUPER;
        case 7: return ALTGR;
        default: return NONE;
    }
}

// Return all permutations of the bits in mask.
// Doctests:
// >>> permute_mask(1)
// [0, 1]
// >>> permute_mask(5)
// [0, 1, 4, 5]
// >>> permute_mask(14)
// [0, 2, 4, 6, 8, 10, 12, 14]
std::vector<int> permute_modmask(ModMask mask)
{
    std::vector<int> bit_masks;
    for (int bit = 0; bit < 7; bit++)
    {
        int bit_mask = 1<<bit;
        if (mask & bit_mask)
            bit_masks.emplace_back(bit_mask);
    }

    int n = static_cast<int>(bit_masks.size());
    std::vector<int> permutations;
    for (int i = 0; i < (1<<n); i++)
    {
        int m = 0;
        for (int bit = 0; bit < n; bit++)
        {
            if (i & (1<<bit))
            {
                m |= bit_masks[static_cast<size_t>(bit)];
            }
        }
        permutations.emplace_back(m);
    }
    return permutations;
}



int& ModifierCounts::operator[](Modifier::Enum modifier)
{
    size_t index = Modifier::modifier_to_bit_index(modifier);
    assert(index < m_counts.size());
    if (index < m_counts.size())
        return m_counts[static_cast<size_t>(index)];

    static int null{0};
    return null;
}

int ModifierCounts::operator[](Modifier::Enum modifier) const
{
    size_t index = Modifier::modifier_to_bit_index(modifier);
    assert(index < m_counts.size());
    if (index < m_counts.size())
        return m_counts[static_cast<size_t>(index)];
    return 0;
}

void ModifierCounts::assign_filtered(const ModifierCounts& src, ModMask mask)
{
    ModifierCounts& dst = *this;
    for (size_t i=0; i<dst.size(); i++)
    {
        Modifier::Enum mod = dst.mod_at(i);
        if (mod & mask)
            dst[mod] = src[mod];
    }
}

ModMask ModifierCounts::get_mod_mask()
{
    ModMask mask = 0;
    for (size_t i=0; i<m_counts.size(); i++)
        if (m_counts[i])
            mask |= 1<<i;
    return mask;
}

Modifier::Enum ModifierCounts::mod_at(size_t index) const
{ return Modifier::bit_index_to_modifier(index);}

bool ModifierCounts::empty()
{
    for (auto n : m_counts)
        if (n)
            return false;
    return true;
}


std::ostream&operator<<(std::ostream& out, const ModifierCounts& m) {
    out << m.m_counts;
    return out;
}

// Build modifier mask from modifier strings.
static ModMask parse_modifier_strings(const vector<string>& modifiers)
{
    ModMask mod_mask = 0;
    for (auto& modifier : modifiers)
    {
        ModMask m = Modifier::key_id_to_modifier(modifier);
        if (m != Modifier::NONE)
        {
            mod_mask |= m;
        }
        else
        {
            auto group = Modifier::mod_group_to_key_ids(modifier);
            if (!group.empty())
            {
                for (auto& key_id : group)
                {
                    Modifier::Enum mod = Modifier::key_id_to_modifier(key_id);
                    mod_mask |= mod;
                }
            }
            else
            {
                throw KeyDefsException(sstr()
                    << "unrecognized modifier " << repr(modifier)
                    << "; try one of "
                    << join(get_keys(map_key_id_to_modifier), ","));
            }
        }
    }

    return mod_mask;
}

//
// Doctests:
//
// modifiers
// >>> parse_key_combination(["TAB"], ["TAB"])
// [('TAB', 0)]
// >>> parse_key_combination(["LALT", "TAB"], ["TAB"])
// [('TAB', 8)]
// >>> parse_key_combination(["LALT", "LFSH", "TAB"], ["TAB"])
// [('TAB', 9)]
// >>> parse_key_combination(["LWIN", "RTSH", "LFSH", "RALT", "LALT", "RCTL", "LCTL", "CAPS", "NMLK", "TAB"], ["TAB"])
// [('TAB', 223)]
//
// // modifier groups
// >>> parse_key_combination(["CTRL", "SHIFT", "TAB"], ["TAB"])
// [('TAB', 5)]
//
// // regex
// >>> parse_key_combination(["F\d+"], ["TAB", "F1", "F2", "F3", "F9"])
// [('F1', 0), ('F2', 0), ('F3', 0), ('F9', 0)]
std::vector<KeyIDModEntry> parse_key_combination(const std::vector<std::string>& combo,
                                                 const std::vector<std::string>& avaliable_key_ids)
{
    vector<string>  modifiers = slice(combo, 0, -1);
    string key_pattern = combo.back();

    // find modifier mask
    ModMask mod_mask = parse_modifier_strings(modifiers);
    if (!mod_mask)
        return {};

    // match regex key id with all available ids
    vector<KeyIDModEntry> results;
    for (auto key_id  : avaliable_key_ids)
    {
        auto fields = re_match(key_id, key_pattern);
        if (!fields.empty() && fields[0] == key_id)
            results.emplace_back(key_id, mod_mask);
    }

    return results;
}


KeySym get_keysym_from_name(const std::string& keysym_name)
{
    static std::map<std::string, KeySym> m = {
        {"space", 0x0020},
        {"kp_space", 65408},
        {"insert", 0xff63},
        {"home", 0xff50},
        {"page_up", 0xff55},
        {"page_down", 0xff56},
        {"end",0xff57},
        {"delete", 0xffff},
        {"return", 65293},
        {"backspace", 65288},
        {"left", 0xff51},
        {"up", 0xff52},
        {"right", 0xff53},
        {"down", 0xff54},
    };

    return get_value(m, keysym_name);
}
