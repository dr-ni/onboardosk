
#include "tools/container_helpers.h"

#include "configuration.h"
#include "layoutwordlist.h"
#include "textrendererpangocairo.h"
#include "theme.h"


std::unique_ptr<LayoutPanelWordList> LayoutPanelWordList::make(const ContextBase& context)
{
    return std::make_unique<LayoutPanelWordList>(context);
}

void LayoutPanelWordList::dump(std::ostream& out) const
{
    out << "LayoutWordListPanel(";
    Super::dump(out);
    out << ")";
}

size_t LayoutPanelWordList::get_max_non_expanded_corrections()
{
    return 1;
}

void LayoutPanelWordList::expand_corrections(bool expand)
{
    m_correcions_expanded = expand;
}

bool LayoutPanelWordList::are_corrections_expanded()
{
    return m_correcions_expanded;
}

bool LayoutPanelWordList::were_more_predictions_requested()
{
    return m_more_predictions_requested;
}

size_t LayoutPanelWordList::get_first_prediction_index()
{
    return m_first_prediction;
}

void LayoutPanelWordList::goto_next_predictions()
{
    m_prev_predictions_stack.emplace_back(m_first_prediction);
    m_first_prediction += m_next_predictions_increment;
    m_more_predictions_requested = true;
}

void LayoutPanelWordList::goto_previous_predictions()
{
    if (!m_prev_predictions_stack.empty())
    {
        m_first_prediction = m_prev_predictions_stack.back();
        m_prev_predictions_stack.pop_back();
    }
}

void LayoutPanelWordList::goto_first_prediction()
{
    m_first_prediction = 0;
    m_prev_predictions_stack.clear();
    m_more_predictions_requested = false;
}

bool LayoutPanelWordList::can_goto_previous_predictions()
{
    return m_can_goto_previous_predictions;
}

bool LayoutPanelWordList::can_goto_next_predictions()
{
    return m_can_goto_next_predictions;
}

double LayoutPanelWordList::get_button_width(const LayoutItemPtr& button)
{
    return button->get_initial_border_rect().w *
            config()->current_theme()->key_size / 100.0;
}

double LayoutPanelWordList::get_button_spacing()
{
    return config()->wordlist_button_spacing.x * 2 *
            (2.0 - config()->current_theme()->key_size / 100.0);
}

double LayoutPanelWordList::get_entry_spacing()
{
    return config()->wordlist_entry_spacing.x * 2 *
            (2.0 - config()->current_theme()->key_size / 100.0);
}

void LayoutPanelWordList::create_keys(std::vector<LayoutKeyPtr>& keys,
                                      const std::vector<UString>& correction_choices,
                                      const std::vector<UString>& prediction_choices)
{
    auto fixed_background = find_ids({"wordlist",
                                      "prediction",
                                      "correction"});
    auto fixed_buttons    = find_ids({"expand-corrections"});
    auto hideable_buttons = find_ids({"previous-predictions",
                                      "next-predictions",
                                      "pause-learning",
                                      "language",
                                      "hide"});
    // scroll to current position
    int first = static_cast<int>(m_first_prediction);
    int max_choices = first == 0 ?
                      config()->word_suggestions->max_word_choices : 100;
    auto scrolled_prediction_choices =
            slice(prediction_choices, first, first + max_choices);

    // hide/show buttons
    std::vector<std::string> visible_button_ids;
    get_visible_buttons(visible_button_ids,
                        correction_choices, prediction_choices,
                        scrolled_prediction_choices);
    for (auto& button : hideable_buttons)
        button->set_visible(contains(visible_button_ids, button->get_id()));

    // create keys
    std::vector<LayoutKeyPtr> correction_keys, prediction_keys;
    do_create_keys(correction_keys, prediction_keys,
                   correction_choices,
                   scrolled_prediction_choices,
                   visible_button_ids);

    // add all keys to the panel
    keys = correction_keys + prediction_keys;
    if (!fixed_buttons.empty())
    {
        auto color_scheme = config()->current_color_scheme();
        for (auto key  : keys)
            key->m_color_scheme = color_scheme;
    }
    clear_children();
    append_children(fixed_background);
    append_children(keys);
    append_children(fixed_buttons);
    append_children(hideable_buttons);

    m_can_goto_previous_predictions =
            m_first_prediction > 0;
    m_can_goto_next_predictions =
            m_first_prediction + prediction_keys.size() <
            prediction_choices.size();
    m_next_predictions_increment = prediction_keys.size();
}

void LayoutPanelWordList::get_visible_buttons(
        std::vector<std::string>& visible_button_ids,
        const std::vector<UString>& correction_choices,
        const std::vector<UString>& prediction_choices,
        const std::vector<UString>& scrolled_prediction_choices)
{
    auto enabled_button_ids =
            config()->word_suggestions->get_shown_wordlist_button_ids();

    const char* previous_predictions_id = "previous-predictions";
    const char* next_predictions_id = "next-predictions";

    bool previous_predictions_enabled =
            contains(enabled_button_ids, previous_predictions_id);
    bool next_predictions_enabled =
            contains(enabled_button_ids, next_predictions_id);

    if (previous_predictions_enabled ||
        next_predictions_enabled)
    {
        // generate keys without scrolling buttons
        visible_button_ids = enabled_button_ids;
        if (previous_predictions_enabled)
            remove(visible_button_ids, previous_predictions_id);

        if (next_predictions_enabled)
            remove(visible_button_ids, next_predictions_id);

        std::vector<LayoutKeyPtr> correction_keys, prediction_keys;
        do_create_keys(correction_keys, prediction_keys,
                       correction_choices,
                       scrolled_prediction_choices,
                       visible_button_ids);

        // hide/show buttons as needed
        visible_button_ids = enabled_button_ids;
        bool enabled = prediction_keys.size() != prediction_choices.size() &&
                       !m_correcions_expanded;

        if (previous_predictions_enabled &&
            (!m_more_predictions_requested ||
             !enabled))
        {
            remove(visible_button_ids, previous_predictions_id);
        }

        if (next_predictions_enabled &&
            !enabled)
        {
            remove(visible_button_ids, next_predictions_id);
        }
    }
    else
    {
        visible_button_ids = enabled_button_ids;
    }
}

void LayoutPanelWordList::do_create_keys(std::vector<LayoutKeyPtr>& correction_keys,
        std::vector<LayoutKeyPtr>& prediction_keys,
        const std::vector<UString>& correction_choices,
        const std::vector<UString>& prediction_choices,
        const std::vector<std::string>& visible_button_ids)
{
    auto wordlist = std::dynamic_pointer_cast<LayoutWordListKey>(
                        get_child_button("wordlist"));
    if (wordlist)
    {
        auto key_context = wordlist->get_context();
        Rect wordlist_rect = wordlist->get_rect();
        Rect rect = wordlist_rect;

        // position visible buttons
        struct ButtonEntry {LayoutItemPtr button;
                            double button_width;
                            int align;};
        std::vector<ButtonEntry> buttons;
        double button_spacing = get_button_spacing();
        for (auto it=visible_button_ids.rbegin();
             it != visible_button_ids.rend(); ++it)
        {
            auto& button_id = *it;
            auto button = get_child_button(button_id);
            if (button && button->visible)
            {
                double button_width = get_button_width(button);
                rect.w -= button_width + button_spacing;
                buttons.emplace_back(ButtonEntry{button, button_width, 1});
            }
        }

        // font size is based on the height of the wordlist background
        double font_size = LayoutWordKey(*this).calc_font_size(key_context, rect.get_size());

        // hide the wordlist background when corrections create their own
        wordlist->set_visible(correction_choices.empty());

        // create correction keys
        Rect used_rect = create_correction_keys(correction_keys,
                                                correction_choices,
                                                rect, wordlist,
                                                key_context, font_size);
        rect.x += used_rect.w;
        rect.w -= used_rect.w;

        // create prediction keys
        if (!this->are_corrections_expanded())
        {
            create_prediction_keys(prediction_keys,
                                   prediction_choices,
                                   rect, key_context,
                                   font_size);
        }

        // move the buttons to the end of the bar
        if (!buttons.empty())
        {
            Rect rw = wordlist_rect;
            for (auto& e : buttons)
            {
                Rect r = rw;
                r.w = e.button_width;
                if (e.align == -1)  // left align
                {
                    r.x = rw.left() - e.button_width;
                    rw.x += e.button_width;
                    rw.w -= e.button_width + button_spacing;
                }
                else if (e.align == 1)  // right align
                {
                    r.x = rw.right() - e.button_width;
                    rw.w -= e.button_width + button_spacing;
                }
                e.button->set_border_rect(r);
            }
        }
    }
}

Rect LayoutPanelWordList::create_correction_keys(
        std::vector<LayoutKeyPtr>& correction_keys,
        const std::vector<UString>& correction_choices,
        const Rect& rect,
        const LayoutWordListKeyPtr& wordlist,
        const LayoutContext* key_context, double font_size)
{
    Rect choices_rect = rect;
    Rect wordlist_rect = wordlist->get_rect();
    double section_spacing = 1.0;
    if (!are_corrections_expanded())
    {
        section_spacing += wordlist->get_fullsize_rect().w - wordlist_rect.w;
        section_spacing = std::max(section_spacing, wordlist_rect.h * 0.1);
    }

    // get button to expand/close the corrections
    bool show_button = correction_choices.size() > 1;
    double button_width = 0;
    auto button = get_child_button("expand-corrections");
    if (button && show_button)
    {
        button_width = get_button_width(button);
        choices_rect.w -= button_width + section_spacing;
    }

    // get template key for tooltips
    auto template_button = std::dynamic_pointer_cast<LayoutKey>(
                               get_child_button("correction"));

    // partition choices
    int n = static_cast<int>(get_max_non_expanded_corrections());
    auto choices = slice(correction_choices, 0, n);
    std::vector<UString> expanded_choices;
    if (are_corrections_expanded())
        expanded_choices = slice(correction_choices, n);

    // create unexpanded correction keys
    std::vector<LayoutKeyPtr> keys;
    Rect used_rect = create_correction_choices(keys, choices, choices_rect,
                                               key_context, font_size,
                                               0, template_button);
    std::vector<LayoutKeyPtr> exp_keys;
    std::vector<LayoutKeyPtr> bg_keys;
    if (!keys.empty())
    {
        if (button)
        {
            if (show_button)
            {
                // Move the expand button to the end
                // of the unexpanded corrections.
                Rect r = used_rect;
                r.x = used_rect.right();
                r.w = button_width;
                button->set_border_rect(r);
                button->set_visible(true);

                used_rect.w += r.w;
            }
            else
            {
                button->set_visible(false);
            }
        }

        // create background keys
        double x_split = are_corrections_expanded() ?
                             wordlist_rect.right() : used_rect.right();

        Rect r = wordlist_rect;
        r.w = x_split - r.x;
        auto key = std::make_shared<LayoutFullSizeKey>(*this);
        key->set_id("correctionsbg");
        key->set_border_rect(r);
        key->sensitive = false;
        key->scannable = false;
        bg_keys.emplace_back(key);

        r = wordlist_rect;
        r.w = r.right() - x_split - section_spacing;
        r.x = x_split + section_spacing;
        key = std::make_shared<LayoutFullSizeKey>(*this);
        key->set_id("predictionsbg");
        key->set_border_rect(r);
        key->sensitive = false;
        key->scannable = false;
        bg_keys.emplace_back(key);

        used_rect.w += section_spacing;

        // create expanded correction keys
        if (!expanded_choices.empty())
        {
            Rect exp_rect = choices_rect;
            exp_rect.x += used_rect.w;
            exp_rect.w -= used_rect.w;
            Rect exp_used_rect = create_correction_choices(
                                     exp_keys,
                                     expanded_choices, exp_rect,
                                     key_context, font_size,
                                     choices.size());
            //keys += exp_keys;  // TODO: adding twice?
            used_rect.w += exp_used_rect.w;
        }
    }
    else
    {
        if (button)
            button->set_visible(false);
    }

    correction_keys = bg_keys + keys + exp_keys;

    return used_rect;
}

Rect LayoutPanelWordList::create_correction_choices(
        std::vector<LayoutKeyPtr>& keys,
        const std::vector<UString>& choices,
        const Rect& rect,
        const LayoutContext* key_context,
        double font_size,
        const size_t start_index,
        const LayoutKeyPtr& template_button)
{
    double spacing = get_entry_spacing();
    std::vector<ButtonInfo> button_infos;
    bool filled_up;
    double xend;
    std::tie(filled_up, xend) = fill_rect_with_choices(button_infos,
                                                       choices,
                                                       rect, key_context,
                                                       font_size);
    // create buttons
    double x{};
    double y{};
    for (size_t i=0; i<button_infos.size(); ++i)
    {
        auto& bi = button_infos[i];
        double w = bi.log_width;

        // create key
        Rect r = Rect(rect.x + x, rect.y + y, w, rect.h);
        auto key = std::make_shared<LayoutWordKey>(*this);
        key->set_id("correction" + std::to_string(i));
        key->set_border_rect(r);
        key->group = m_suggestions_group;
        key->labels = {{0, bi.label.to_utf8()}};
        key->font_size = font_size;
        key->type = KeyType::CORRECTION;
        key->keyindex = start_index + i;
        if (template_button)
            key->tooltip = template_button->tooltip;
        keys.emplace_back(key);

        x += w + spacing;  // move to begin of next button
    }

    // return the actually used rect for all correction keys
    if (!keys.empty())
        x -= spacing;
    Rect used_rect = rect;
    used_rect.w = x;

    return used_rect;
}

void LayoutPanelWordList::create_prediction_keys(
    std::vector<LayoutKeyPtr>& keys,
    const std::vector<UString>& choices,
    const Rect& wordlist_rect,
    const LayoutContext* key_context,
    double font_size)
{
    double spacing = get_entry_spacing();
    std::vector<ButtonInfo> button_infos;
    bool filled_up;
    double xend;
    std::tie(filled_up, xend) = fill_rect_with_choices(
                                    button_infos, choices, wordlist_rect, key_context, font_size);

    if (!button_infos.empty())
    {
        double scale;
        double all_spacings = (button_infos.size() - 1) * spacing;

        if (filled_up)
        {
            // Find a stretch factor that fills the remaining space
            // with only expandable items.
            double length_nonexpandables = 0.0;
            for (const auto& bi : button_infos)
                if (!bi.expand)
                    length_nonexpandables += bi.log_width;

            double length_expandables = 0.0;
            for (const auto& bi : button_infos)
                if (bi.expand)
                    length_nonexpandables += bi.log_width;

            double length_target = wordlist_rect.w -
                                   length_nonexpandables -
                                   all_spacings;
            scale = length_expandables != 0.0 ?
                                              length_target / length_expandables : 1.0;
        }
        else
        {
            // Find the stretch factor that fills the available
            // space with all items.
            scale = (wordlist_rect.w - all_spacings) /
                    (xend - all_spacings);
        }
        // scale = 1.0  // no stretching, left aligned

        // create buttons
        double x{};
        double y{};
        for (size_t i=0; i<button_infos.size(); ++i)
        {
            auto& bi = button_infos[i];
            double w = bi.log_width;

            // scale either all buttons || only the expandable ones
            if (!filled_up || bi.expand)
                w *= scale;

            // create the word key with the generic id "prediction<n>"
            auto key = std::make_shared<LayoutWordKey>(*this);
            key->set_id("prediction" + std::to_string(i));
            key->set_border_rect(Rect(wordlist_rect.x + x,
                                      wordlist_rect.y + y,
                                      w, wordlist_rect.h));

            key->group = m_suggestions_group;
            key->labels = {{0, bi.label.to_utf8()}};
            key->font_size = font_size;
            key->type = KeyType::PREDICTION;
            key->keyindex = i;
            keys.emplace_back(key);

            x += w + spacing;  // move to begin of next button
        }
    }
}

std::tuple<bool, double> LayoutPanelWordList::fill_rect_with_choices(
        std::vector<ButtonInfo>& button_infos,
        const std::vector<UString>& choices,
        const Rect& rect,
        const LayoutContext* key_context,
        double font_size)
{
    double spacing = get_entry_spacing();
    double x = 0.0;

    // context = Gdk.pango_context_get()
    auto text_renderer = get_text_renderer(TextRendererSlot::DEFAULT,
                                           static_cast<int>(font_size));
    text_renderer->set_font(config()->get_key_label_font(), font_size);
    bool filled_up = false;
    double margins = config()->wordlist_label_margin.x * 2;

    for (size_t i=0; i<choices.size(); i++)
    {
        const auto& choice = choices[i];

        // ellipsize in canvase units then go back to logical units
        double max_width = key_context->scale_log_to_canvas_x(rect.w - margins);
        UString label;
        double label_width = text_renderer->ellipsize(label, choice, max_width);
        double w = key_context->scale_canvas_to_log_x(label_width) + margins;

        // Long words are allowed to expand their widths into the
        // remaining space. Short ones stay short.
        bool expand = w >= rect.h;
        if (!expand)
            w = rect.h;

        // reached the end of the available space?
        if (x + w > rect.w)
        {
            filled_up = true;
            break;
        }

        button_infos.emplace_back();
        ButtonInfo& bi = button_infos.back();
        bi.label_canvas_width = label_width;
        bi.log_x = x;
        bi.log_width = w;
        bi.expand = expand;  // can stretch into available space?
        bi.label = label;

        x += w + spacing;  // move to begin of next button
    }

    if (!button_infos.empty())
    {
        x -= spacing;
    }

    return {filled_up, x};
}


std::unique_ptr<LayoutWordListKey> LayoutWordListKey::make(const ContextBase& context)
{
    return std::make_unique<LayoutWordListKey>(context);
}

