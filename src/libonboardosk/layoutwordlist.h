#ifndef LAYOUTWORDLIST_H
#define LAYOUTWORDLIST_H

#include "tools/ustringmain.h"

#include "layoutdrawingitem.h"
#include "layoutkey.h"

class LayoutPanelWordList : public LayoutPanel
{
    private:
        struct ButtonInfo
        {
                double label_canvas_width;
                double log_x;
                double log_width;
                bool expand;   // can stretch into available space?
                UString label;
        };

    public:
        using Super = LayoutPanel;
        using Ptr = std::shared_ptr<LayoutPanelWordList>;

        LayoutPanelWordList(const ContextBase& context) :
            Super(context)
        {}

        static std::unique_ptr<LayoutPanelWordList> make(const ContextBase& get_context);

        static constexpr const char* CLASS_NAME = "WordListPanel";
        virtual std::string get_class_name() const override {return CLASS_NAME;}

        virtual void dump(std::ostream& out) const override;

        size_t get_max_non_expanded_corrections();

        void expand_corrections(bool expand);

        bool are_corrections_expanded();

        // Has the more arrow been clicked at least once?
        bool were_more_predictions_requested();

        // index of first visible prediction
        size_t get_first_prediction_index();

        // scroll suggestions to the right
        void goto_next_predictions();

        // scroll suggestions to the left
        void goto_previous_predictions();

        // reset scroll position
        void goto_first_prediction();

        bool can_goto_previous_predictions();

        bool can_goto_next_predictions();

        double get_button_width(const LayoutItemPtr& button);

        double get_button_spacing();

        double get_entry_spacing();

        void create_keys(std::vector<LayoutKeyPtr>& keys,
                         const std::vector<UString>& correction_choices,
                         const std::vector<UString>& prediction_choices);

        void get_visible_buttons(std::vector<std::string>& visible_button_ids,
                                 const std::vector<UString>& correction_choices,
                                 const std::vector<UString>& prediction_choices,
                                 const std::vector<UString>& scrolled_prediction_choices);

        // Dynamically create a variable number of buttons
        // for word correction and prediction.
        void do_create_keys(std::vector<LayoutKeyPtr>& correction_keys,
                            std::vector<LayoutKeyPtr>& prediction_keys,
                            const std::vector<UString>& correction_choices,
                            const std::vector<UString>& prediction_choices,
                            const std::vector<std::string>& visible_button_ids);

        // Create all correction keys.
        Rect create_correction_keys(std::vector<LayoutKeyPtr>& correction_keys,
                                    const std::vector<UString>& correction_choices,
                                    const Rect& rect,
                                    const LayoutWordListKeyPtr& wordlist,
                                    const LayoutContext* key_context,
                                    double font_size);

        // Dynamically create a variable number of buttons for word correction.
        Rect create_correction_choices(std::vector<LayoutKeyPtr>& keys,
                                       const std::vector<UString>& choices,
                                       const Rect& rect,
                                       const LayoutContext* key_context,
                                       double font_size,
                                       const size_t start_index=0,
                                       const LayoutKeyPtr& template_button={});

        // Dynamically create a variable number of buttons for word prediction.
        void create_prediction_keys(std::vector<LayoutKeyPtr>& keys,
                                    const std::vector<UString>& choices,
                                    const Rect& wordlist_rect,
                                    const LayoutContext* key_context,
                                    double font_size);

        std::tuple<bool, double> fill_rect_with_choices(
                std::vector<ButtonInfo>& button_infos,
                const std::vector<UString>& choices,
                const Rect& rect,
                const LayoutContext* key_context,
                double font_size);

        LayoutItemPtr get_child_button(const std::string& id_)
        {
            return find_id(id_);
        }

    private:
        // Place all dynamically created suggestions into their own
        // group to prevent font_size changes forced by unrelated keys.
        static constexpr const char* m_suggestions_group = "_suggestion_";

        bool m_correcions_expanded{false};
        size_t m_first_prediction{0};
        bool m_more_predictions_requested{false};
        bool m_can_goto_previous_predictions{false};
        bool m_can_goto_next_predictions{false};
        size_t m_next_predictions_increment{0};
        std::vector<size_t> m_prev_predictions_stack;
};

typedef LayoutPanelWordList::Ptr LayoutPanelWordListPtr;

#endif // LAYOUTWORDLIST_H
