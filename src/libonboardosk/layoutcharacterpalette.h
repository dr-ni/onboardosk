#ifndef LAYOUTCHARACTERPALETTE_H
#define LAYOUTCHARACTERPALETTE_H

#include <memory>

#include "layoutdrawingitem.h"


class CharacterGridPanel;
class SymbolData;


class LayoutPalettePanel : public LayoutPanel
{
    public:
        using Super = LayoutPanel;
        using This = LayoutPalettePanel;
        using Ptr = std::shared_ptr<This>;

        LayoutPalettePanel(const ContextBase& context);

        virtual void dump(std::ostream& out) const override;

        void set_active_category_index(size_t index);
        void scroll_to_category_index(size_t index);

    protected:
        virtual void configure_header_key(const LayoutKeyPtr& key, const std::string& label);
        virtual bool is_favorites_index(size_t index, size_t num_keys);

    private:
        virtual void update_log_rect() override;

        void update_content();

        void create_header_content();

        void create_header_keys(std::vector<LayoutKeyPtr>& keys, Rect& header_rect,
                                const Rect& palette_rect, const Rect& header_key_border_rect,
                                const std::string& header_key_group, const ColorSchemePtr& color_scheme,
                                const std::vector<std::string>& labels);



    protected:
        std::string m_content_type;
        std::unique_ptr<SymbolData> m_symbol_data;
        std::vector<std::string> m_extra_labels;

    private:
        std::shared_ptr<CharacterGridPanel> m_character_grid;
        std::vector<LayoutKeyPtr> m_header_keys;
        size_t m_active_category_index{static_cast<size_t>(-1)};
};


class EmojiPalettePanel : public LayoutPalettePanel
{
    public:
        using Super = LayoutPalettePanel;
        using This = EmojiPalettePanel;
        using Ptr = std::shared_ptr<This>;

        EmojiPalettePanel(const ContextBase& context);

        static std::unique_ptr<This> make(const ContextBase& get_context);

        static constexpr const char* CLASS_NAME = "EmojiPalettePanel";
        virtual std::string get_class_name() const override {return CLASS_NAME;}

        virtual void dump(std::ostream& out) const override;

        virtual void configure_header_key(const LayoutKeyPtr& key,
                                          const std::string& label) override;

        virtual void on_visibility_changed(bool visible) override;

        virtual bool is_favorites_index(size_t index, size_t num_keys);
};


class SymbolPalettePanel : public LayoutPalettePanel
{
    public:
        using Super = LayoutPalettePanel;
        using This = SymbolPalettePanel;
        using Ptr = std::shared_ptr<This>;

        SymbolPalettePanel(const ContextBase& context);

        static std::unique_ptr<This> make(const ContextBase& get_context);

        static constexpr const char* CLASS_NAME = "SymbolPalettePanel";
        virtual std::string get_class_name() const override {return CLASS_NAME;}

        virtual void dump(std::ostream& out) const override;
};

#endif // LAYOUTCHARACTERPALETTE_H
