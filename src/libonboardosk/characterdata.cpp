#include <iomanip>
#include <string>
#include <vector>

#include "tools/string_helpers.h"
#include "tools/ustringmain.h"

#include "characterdata.h"
#include "emojidata.h"


std::string emoji_filename_from_codepoints(const std::vector<int32_t>& codepoints)
{
    std::string fn;
    for (auto cp : codepoints)
    {
        if (cp != 0x200D &&
            cp != 0xfe0f)
        {
            if (!fn.empty())
            {
                fn += "-";
            }
            std::stringstream ss;
            ss << std::setfill('0') << std::setw(4)  << std::hex << cp;
            fn += ss.str();
        }
    }
    return fn + ".svg";
}

std::string emoji_filename_from_sequence(const std::string& sequence)
{
    UString us{sequence};
    return emoji_filename_from_codepoints(us.get_code_points());
}



const std::vector<CharacterCategory> CharacterData::m_symbol_data =
{{
    {0, "Αβγ", {"α","β","γ","δ","ε","ζ","η","θ","ι","κ","λ","μ","ν","ξ","ο","π","ρ","σ","ς","τ","υ","φ","χ","ψ","ω"}},        // Greek
    {1, "Α",   {"Α","Β","Γ","Δ","Ε","Ζ","Η","Θ","Ι","Κ","Λ","Μ","Ν","Ξ","Ο","Π","Ρ","Σ","Σ","Τ","Υ","Φ","Χ","Ψ","Ω", "z", "y"}},

    // math & physics
    {0, "ℝ", {"ℝ","ℂ","ℕ","ℙ","ℚ","ℤ",
             "∅","∃","∄","∈","∉","∀","∑","∥","∦","∡","⊾","∞",
             "∩","∪","⊂","⊃","⊄","⊅","⊈","⊉","⊆","⊇","…",
             "≤","≥","≦","≧","≨","≩",
             "≁","≂","≃","≄","≅","≆","≇","≈","≉","≊","≋","≌","≍",
             "√","∛","∜",
             "∫","∬","∭",
             "℃","℉","№"}
     },

    // super- && subscript
    {0, "²₂", {"⁰","¹","²","³","⁴","⁵","⁶","⁷","⁸","⁹","⁺","⁻","⁼","⁽","⁾",
               "ⁱ"}
    },
    {1, "₂", {"₀","₁","₂","₃","₄","₅","₆","₇","₈","₉","₊","₋","₌","₍","₎",
              "ₐ","ₑ","ₒ","ₓ","ₔ","ₕ","ₖ","ₗ","ₘ","ₙ","ₚ","ₛ","ₜ"}
    },

    // currency
    {0, "€", {"$","₠","₡","₢","₣","₤","₥","₦","₧","₨","₩","₪","₫","€",
              "₭","₮","₯","₰","₱","₲","₳","₴","₵","₶","₷","₸","₹","₺",
              "₽","₿"}
    },
}};

const std::vector<CharacterCategory> CharacterData::m_short_emoji_data
{{
    {0, "", {"♥", "😂", "", ""}},
}};


CharacterData::CharacterData()
{
}

CharacterData::~CharacterData()
{
}

std::unique_ptr<SymbolData> CharacterData::get_symbol_data(CharacterData::ContentType content_type)
{
    if (content_type == EMOJI)
    {
        return std::make_unique<SymbolData>(s_emoji_data);
    }
    else if (content_type == SYMBOLS)
    {
        return std::make_unique<SymbolData>(m_symbol_data);
    }
    return {};
}

