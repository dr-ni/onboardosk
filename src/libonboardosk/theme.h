#ifndef THEME_H
#define THEME_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "tools/noneable.h"
#include "tools/version.h"

#include "configdecls.h"
#include "layoutdecls.h"
#include "onboardoskglobals.h"


// Theme controls the visual appearance of Onboards keyboard window.
class Theme : public ContextBase, public std::enable_shared_from_this<Theme>
{
    public:
            // onboard 0.95
            static constexpr const Version THEME_FORMAT_INITIAL{1, 0};

            // onboard 0.97, added key_size, switch most int values to float,
            // changed range of key_gradient_direction
            static constexpr const Version THEME_FORMAT_1_1{1, 1};

            // onboard 0.98, added shadow keys
            static constexpr const Version THEME_FORMAT_1_2{1, 2};

            // onboard 0.99, added key_stroke_width
            static constexpr const Version THEME_FORMAT_1_3{1, 3};

            static constexpr const Version THEME_FORMAT = THEME_FORMAT_1_3;

            // core theme members
            std::string color_scheme_basename;
            double background_gradient{0.0};
            KeyStyle::Enum key_style{KeyStyle::GRADIENT};
            double roundrect_radius{0.0};
            double key_size{100.0};
            double key_stroke_width{100.0};
            double key_fill_gradient{0.0};
            double key_stroke_gradient{0.0};
            double key_gradient_direction{0.0};
            std::string key_label_font;
            LabelOverrideMap key_label_overrides;   // dict {name:(key:group)}
            double key_shadow_strength{0.0};
            double key_shadow_size{0.0};

    public:
        using Super = ContextBase;
        using This = Theme;
        using Ptr = std::shared_ptr<This>;
        Ptr getptr() { return shared_from_this(); }

        Theme(const ContextBase& context);
        static std::unique_ptr<Theme> make(const ContextBase& context);

        bool operator==(const Theme& other) const;

        void set_color_scheme_filename(const std::string& filename);
        std::string get_superkey_label();
        std::string get_superkey_size_group();
        // Sets or clears the override for left and right super key labels.
//        void set_superkey_label(const Noneable<std::string>& label, std::__cxx11::string& size_group);
void set_superkey_label(const Noneable<std::string>& label, std::string& size_group);
    private:
        std::string m_name;
        std::string m_filename;
        bool m_is_system{false};             // is this a system theme?
        bool m_system_theme_exists{false};   // true if there exists a system
                                             //  theme with the same basename

        friend class ThemeLoader;
};


typedef std::map<std::string, ThemePtr> ThemeMap;

class ThemeLoader : public ContextBase
{
    public:
        using Super = ContextBase;
        using This = ThemeLoader;

        ThemeLoader(const ContextBase& context);

    public:
        void get_merged_color_schemes(ThemeMap& themes);
        std::vector<ThemePtr> load_themes(bool is_system = false);
        std::vector<std::string> find_themes(const std::string& path);
        ThemePtr load(const std::string& filename, bool is_system = false);
};


#endif // THEME_H
