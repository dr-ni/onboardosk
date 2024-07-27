#include <numeric>

#include "tools/container_helpers.h"
#include "tools/logger.h"
#include "tools/ustringregex.h"

#include "atspistatetracker.h"
#include "configuration.h"
#include "keyboard.h"
#include "keyboardkeylogic.h"
#include "keyboardview.h"
#include "languagedb.h"
#include "layoutkey.h"
#include "layoutroot.h"
#include "layoutview.h"
#include "layoutwordlist.h"
#include "learnstrategy.h"
#include "punctuator.h"
#include "spellchecker.h"
#include "textchanger.h"
#include "textchanges.h"
#include "textcontext.h"
#include "textdomain.h"
#include "timer.h"
#include "wordsuggestions.h"
#include "wpengine.h"
#include "wperrorrecovery.h"



class PendingSeparatorPopup : public ContextBase
{
    public:
        using Super = ContextBase;
        PendingSeparatorPopup(const ContextBase& context) :
            Super(context)
        {}
};



WordSuggestions::WordSuggestions(const ContextBase& context) :
    ContextBase(context),
    m_load_models_timer(std::make_unique<Timer>(context)),
    m_atspi_text_context(TextContext::make_atspi(context)),
    m_text_context(m_atspi_text_context.get()),
    m_learn_strategy(std::make_unique<LearnStrategyLRU>(context)),
    m_spell_checker(std::make_unique<SpellChecker>(context)),
    m_punctuator(std::make_unique<PunctuatorImmediateSeparators>(context)),
    m_pending_separator_popup(std::make_unique<PendingSeparatorPopup>(context)),
    m_pending_separator_popup_timer(std::make_unique<Timer>(context)),
    m_load_error_recovery(std::make_unique<WPErrorRecovery>(context))
{
    m_connections.connect(config()->word_suggestions->enabled.changed,
                         [this]{on_word_suggestions_enabled();});
    m_connections.connect(config()->typing_assistance->active_language.changed,
                         [this]{on_active_lang_id_changed();});
}

WordSuggestions::~WordSuggestions()
{}

LayoutItemPtr WordSuggestions::get_layout() const
{
    return get_keyboard()->get_layout();
}

TextContext* WordSuggestions::get_text_context() const
{
    return m_text_context;
}

WPEngine* WordSuggestions::get_wp_engine() const
{
    return m_wpengine.get();
}

TextChanger* WordSuggestions::get_text_changer() const
{
    auto key_logic = get_key_logic();
    return key_logic->get_text_changer();
}

TextDomain*WordSuggestions::get_text_domain()
{
    auto text_context = get_text_context();
    if (text_context)
        return text_context->get_text_domain();
    return {};
}

SpellChecker* WordSuggestions::get_spell_checker()
{
    return m_spell_checker.get();
}

void WordSuggestions::reset()
{
    commit_changes();
}

void WordSuggestions::enable_word_suggestions(bool enable)
{
    auto layout = get_layout_root(get_layout());
    if (!layout)
        return;

    // show/hide word-prediction buttons
    for (auto item  : get_wordlist_panels())
        layout->set_item_visible(item, enable);

    // show/hide other wordlist-dependent layout items
    layout->for_each_global_item([&](const LayoutItemPtr& item) -> void
    {
        if (item->group == "wordlist")
            layout->set_item_visible(item, enable);
        else if (item->group == "nowordlist")
            layout->set_item_visible(item, !enable);
    });

    update_wp_engine();
    update_spell_checker();
    update_punctuator();
}

void WordSuggestions::update_wp_engine()
{
    bool enable = config()->is_typing_assistance_enabled();

    if (enable)
    {
        // only enable if there is a wordlist in the layout
        if (!get_wordlist_panels().empty())
        {
            std::unique_ptr<WPEngine> p(std::make_unique<WPEngine>(*this));
            m_wpengine = std::move(p);
            apply_prediction_profile();
        }
    }
    else
    {
        if (m_wpengine)
            m_wpengine = nullptr;
    }

    // Init text context tracking.
    // Keep track in && write to both contexts in parallel,
    // but read only from the active one.
    m_text_context = m_atspi_text_context.get();
    if (m_text_context)
        m_text_context->enable(enable);  // register AT-SPI listerners
}

void WordSuggestions::on_word_suggestions_enabled()
{
    auto keyboard = get_keyboard();
    enable_word_suggestions(config()->are_word_suggestions_enabled());
    keyboard->invalidate_ui();
    keyboard->commit_ui_updates();
}

void WordSuggestions::apply_prediction_profile()
{
    if (m_wpengine)
    {
        auto language_db = get_language_db();

        auto lang_id = get_lang_id();
        std::string system_lang_id = language_db ?
                language_db->find_system_model_language_id(lang_id) : lang_id;

        std::vector<std::string> system_models{"lm:system:" + system_lang_id};
        std::vector<std::string> user_models{"lm:user:" + lang_id};
        std::vector<std::string> scratch_models{"lm:mem"};

        std::vector<std::string> persistent_models = system_models + user_models;
        std::vector<std::string> auto_learn_models = user_models;

        LOG_INFO << "selecting language models:"
                 << " system=" << system_models
                 << " user=" << user_models
                 << " auto_learn=" << auto_learn_models;

        // auto-learn language model must be part of the user models
        for (auto& model : auto_learn_models)
        {
            if (!contains(user_models, model))
            {
                auto_learn_models.clear();
                LOG_WARNING << "No auto learn model selected. "
                               "Please setup learning first.";
                break;
            }
        }

        m_wpengine->set_model_ids(persistent_models,
                               auto_learn_models,
                               scratch_models);

        // Make sure to load the language models, so there is no
        // delay on first key press. Don't burden the startup
        // with this either, run it a little delayed.
        m_load_models_timer->start(std::chrono::seconds(1),
                                   [this]{load_models(); return false;});
    }
}

void WordSuggestions::load_models()
{
    if (m_wpengine)
    {
        m_wpengine->load_models();
        if (!m_load_errors_reported)
        {
            m_load_errors_reported = true;
            if(m_load_error_recovery)
                m_load_error_recovery->report_errors();
        }
    }
}

void WordSuggestions::get_system_model_names(std::vector<std::string>& names)
{
    m_wpengine->get_model_names(names, "system");
}

std::vector<std::string> WordSuggestions::get_merged_model_names()
{
    std::vector<std::string> names;
    if (m_wpengine)
    {
        std::vector<std::string> system_models;
        m_wpengine->get_model_names(system_models, "system");
        std::vector<std::string> user_models;
        m_wpengine->get_model_names(user_models, "user");
        names = union_sort_inplace(system_models, user_models);
    }
    return names;
}

std::string WordSuggestions::get_lang_id()
{
    auto lang_id = get_active_lang_id();
    if (lang_id.empty())
        lang_id = LanguageDB::get_system_default_lang_id();
    return lang_id;
}

std::string  WordSuggestions::get_active_lang_id()
{
    return config()->typing_assistance->active_language;
}

void WordSuggestions::on_active_lang_id_changed()
{
    LOG_WARNING << config()->typing_assistance->active_language.get();
    if (!m_wpengine)
        return;

    m_wpengine->save_models("active lang_id changed");

    update_spell_checker();
    apply_prediction_profile();

    auto keyboard = get_keyboard();
    keyboard->invalidate_context_ui();
    keyboard->commit_ui_updates();
}

void WordSuggestions::set_active_language_id(const std::string& lang_id,
                                             bool add_to_mru)
{
    config()->set_active_language_id(lang_id, add_to_mru);
    on_active_lang_id_changed();
}

std::vector<std::string> WordSuggestions::get_spellchecker_dicts()
{
    return m_spell_checker->get_supported_dict_ids();
}

void WordSuggestions::on_layout_loaded()
{
    auto items = get_keyboard()->find_items_from_classes({"WordListPanel"});
    m_word_list_bars.clear();
    for (auto& item : items)
    {
        LayoutPanelWordListPtr p = std::dynamic_pointer_cast<LayoutPanelWordList>(item);
        assert(p);
        if (p)
            m_word_list_bars.emplace_back(p);
    }

    enable_word_suggestions(config()->are_word_suggestions_enabled());
}

void WordSuggestions::on_any_key_down()
{
    this->on_activity_detected();

    // Don't auto-save while keys are being pressed,
    // or the resulting delay may cause key repeats.
    if (m_wpengine)
        m_wpengine->pause_autosave();
}

void WordSuggestions::on_all_keys_up()
{
    if (m_wpengine)
        m_wpengine->resume_autosave();
}

void WordSuggestions::on_before_key_press(const LayoutKeyPtr& key)
{
    auto key_logic = get_key_logic();

    m_separator_before_key_press =
        m_text_context->get_pending_separator();

    if (!key->is_modifier() && !key->is_button())
        m_punctuator->on_before_press(key);

    m_text_context->on_onboard_typing(key, key_logic->get_mod_mask());
}

void WordSuggestions::on_before_key_release(const LayoutKeyPtr& key)
{
    m_punctuator->on_before_release(key);
}

void WordSuggestions::on_after_key_release(const LayoutKeyPtr& key)
{
    auto keyboard = get_keyboard();

    m_punctuator->on_after_release(key);

    // DEL and BKSP don't always change text and generate
    // AT-SPI events. Be sure to update the separator popup here.
    auto separator_after = m_text_context->get_pending_separator();
    if (!is_equal(m_separator_before_key_press, separator_after))
        keyboard->invalidate_context_ui();

    if (!key->is_correction_key())
        expand_corrections(false);

    if (!key->is_button())
        goto_first_prediction();
}

void WordSuggestions::send_key_up(const LayoutKeyPtr& key,
                                  LayoutView* view,
                                  MouseButton::Enum button,
                                  EventType::Enum event_type)
{
    (void) view;
    (void) event_type;

    auto key_logic = get_key_logic();

    if (key->is_correction_key())
        insert_correction_choice(key, key->keyindex);

    else if (key->is_prediction_key())
    {
        // no punctuation assistance on right click
        insert_prediction_choice(key, key->keyindex, button != 3);
    }

    if (key->type == KeyType::PREDICTION ||
        key->type == KeyType::CORRECTION ||
        key->type == KeyType::MACRO)
    {
        m_text_context->on_onboard_typing(key, key_logic->get_mod_mask());
    }
}

// User interacted with the keyboard.
void WordSuggestions::on_activity_detected()
{
    if (m_wpengine)
        m_wpengine->postpone_autosave();
}

void WordSuggestions::on_punctuator_changed()
{
    auto keyboard = get_keyboard();

    update_punctuator();
    keyboard->invalidate_context_ui();
    keyboard->commit_ui_updates();
}

void WordSuggestions::on_spell_checker_changed()
{
    auto keyboard = get_keyboard();

    update_spell_checker();
    keyboard->commit_ui_updates();
}

void WordSuggestions::on_outside_click(MouseButton::Enum button)
{
    if (button == MouseButton::MIDDLE)
    {
        insert_pending_separator();
        get_keyboard()->invalidate_context_ui();
    }
}

void WordSuggestions::on_cancel_outside_click()
{
    auto keyboard = get_keyboard();

    m_text_context->set_pending_separator({});
    keyboard->invalidate_context_ui();
    keyboard->commit_ui_updates();
}

bool WordSuggestions::can_give_keypress_feedback()
{
    auto domain = get_text_domain();
    if (domain && !domain->can_give_keypress_feedback())
        return false;
    return true;
}

bool WordSuggestions::is_capitalization_requested()
{
    return false;  // FIXME
}

bool WordSuggestions::can_direct_insert_text()
{
    auto text_context = get_text_context();
    return text_context && text_context->can_insert_text();
}

bool WordSuggestions::can_spell_check(const TextSpanPtr& span_to_check)
{
    auto domain = get_text_domain();
    return domain && domain->can_spell_check(span_to_check);
}

bool WordSuggestions::can_auto_correct(const TextSpanPtr& section_span)
{
    auto domain = this->get_text_domain();
    return domain && domain->can_auto_correct(section_span);
}

bool WordSuggestions::can_auto_punctuate()
{
    auto text_context = get_text_context();
    return text_context && text_context->can_auto_punctuate();
}

void WordSuggestions::update_punctuator()
{
    if (config()->word_suggestions->delayed_word_separators_enabled)
        m_punctuator = std::make_unique<PunctuatorDelayedSeparators>(*this);
    else
        m_punctuator = std::make_unique<PunctuatorImmediateSeparators>(*this);
}

void WordSuggestions::update_spell_checker()
{
    auto keyboard = get_keyboard();

    // select the backend
    SpellcheckBackend::Enum backend = SpellcheckBackend::NONE;
    if (config()->is_spell_checker_enabled())
        backend = config()->typing_assistance->spell_check_backend;
    m_spell_checker->set_backend(backend);

    if (backend != SpellcheckBackend::NONE)
    {
        // chose dicts
        auto lang_id = get_lang_id();
        std::vector<std::string> dict_ids;
        if (!lang_id.empty())
            dict_ids.emplace_back(lang_id);

        m_spell_checker->set_dict_ids(dict_ids);
    }

    keyboard->invalidate_context_ui();
}

void WordSuggestions::invalidate_for_resize()
{
    goto_first_prediction();
}

void WordSuggestions::update_suggestions_ui(std::set<LayoutItemPtr>& keys_to_redraw)
{
    update_correction_choices();
    update_prediction_choices();
    update_pending_separator_popup();
    update_wordlists(keys_to_redraw);
}

void WordSuggestions::update_wordlists(std::set<LayoutItemPtr>& items_to_redraw)
{
    auto keyboard = get_keyboard();

    auto& items = get_wordlist_panels();
    for (auto& item : items)
    {
        std::vector<LayoutKeyPtr> keys;
        item->create_keys(keys, m_correction_choices, m_prediction_choices);
        for (auto& key : keys)
            items_to_redraw.emplace(key);
    }

    // layout changed, but doesn't know it yet
    // -> invalidate all item caches
    keyboard->invalidate_layout();

    for (auto& item : items_to_redraw)
    {
        if (item->is_key())  // should always be the case
        {
            LayoutKeyPtr key = std::dynamic_pointer_cast<LayoutKey>(item);
            key->configure_labels(0);
        }
    }
}

bool WordSuggestions::has_suggestions()
{
    return !m_correction_choices.empty() ||
            !m_prediction_choices.empty();
}

void WordSuggestions::expand_corrections(bool expand)
{
    auto keyboard = get_keyboard();

    // collapse all expanded corrections
    for (auto& item : get_wordlist_panels())
    {
        if (item->are_corrections_expanded())
        {
            item->expand_corrections(expand);
            keyboard->invalidate_item(item);
        }
    }
}

void WordSuggestions::goto_first_prediction()
{
    for (auto& item : get_wordlist_panels())
        item->goto_first_prediction();
}

UString WordSuggestions::get_prediction_choice(const LayoutPanelWordListPtr& wordlist, size_t choice_index)
{
    if (wordlist)
    {
        size_t index = choice_index + wordlist->get_first_prediction_index();
        if (index < m_prediction_choices.size())
            return m_prediction_choices[index];
    }
    return {};
}

std::tuple<UString, std::vector<UString> > WordSuggestions::get_prediction_choice_and_history(const LayoutPanelWordListPtr& wordlist, size_t choice_index)
{
    if (!m_wpengine)
        return {};

    UString word = get_prediction_choice(wordlist, choice_index);
    UString context = m_text_context->get_bot_context();

    std::vector<UString> tokens;
    std::vector<Span> spans;
    m_wpengine->tokenize_context(tokens, spans, context);

    std::vector<UString> history = slice(tokens, 0, -1);
    return {word, history};
}

void WordSuggestions::remove_prediction_context(const std::vector<UString>& context)
{
    if (m_wpengine)
        m_wpengine->remove_context(context);
}

void WordSuggestions::insert_correction_choice(const LayoutKeyPtr& key,
                                               size_t choice_index)
{
    (void) key;

    const auto& span = m_correction_span;  // span to correct
    const auto& span_at_caret = m_text_context->get_span_at_caret();

    if (span && span_at_caret &&
        choice_index < m_correction_choices.size())
    {
        SuppressModifiers sm(get_key_logic());
        delete_selected_text();
        replace_text(span->span(),
                     span_at_caret->begin(),
                     m_correction_choices[choice_index]);
    }
}

void WordSuggestions::insert_prediction_choice(const LayoutKeyPtr& key,
                                               size_t choice_index,
                                               bool allow_separator)
{
    auto text_context = get_text_context();
    if (!text_context ||
        !text_context->has_focus())  // else various getters return nullptr
        return;

    LayoutPanelWordListPtr panel =
            std::dynamic_pointer_cast<LayoutPanelWordList>(key->get_parent());
    if (!panel)
        return;

    UString choice = get_prediction_choice(panel, choice_index);
    UString deletion, insertion;
    std::tie(deletion, insertion) =
            get_prediction_choice_changes(choice);

    // Get the auto separator
    std::optional<UString> separator;
    TextSpanPtr simulated_result;
    if (allow_separator)
    {
        // Simulate the change and determine the final text
        // before the caret.
        auto caret_span = m_text_context->get_span_at_caret();
        simulated_result =
                simulate_insertion(caret_span, deletion, insertion);

        auto domain = get_text_domain();
        separator =
                domain->get_auto_separator(simulated_result->get_text());
    }

    {
        SuppressModifiers sm(get_key_logic());

        // Type remainder/replace word && possibly add separator.
        if (config()->word_suggestions->delayed_word_separators_enabled)
        {
            replace_text_at_caret(deletion, insertion);
        }
        else
        {
            // Insert separator now, so we suppress_modifiers only once.
            replace_text_at_caret(deletion, insertion, separator);
            if (separator)
                get_key_logic()->set_last_typed_was_separator(true);
        }
    }

    // Separator becomes the new pending separator
    TextSpanPtr separator_span;
    if (separator)
    {
        separator_span = std::make_shared<TextSpan>(
            simulated_result->begin(),
            static_cast<TextLength>(separator.value().size()),
            separator.value(),
            simulated_result->begin());
    }

   m_punctuator->set_separator(separator_span);
}

void WordSuggestions::replace_text_at_caret(const UString& deletion,
                                            const UString& insertion,
                                            const std::optional<UString>& auto_separator)
{
    bool delete_existing_separator = false;

    // Check if we have to delete an existing separator
    auto selection_span = m_text_context->get_selection_span();
    if (!selection_span)
        return;

    if (auto_separator &&
        !auto_separator.value().empty())
    {
        UString remaining_text = selection_span->get_text_after_span();
        UString remaining_line = m_text_context->get_line_past_caret();

        // Insert separator if the separator does !exist at
        // the caret yet. For space characters also check if the
        // caret is at the end of the line. The end of the line
        // in the terminal (e.g. in vim) may mean lots of spaces
        // until the final new line.
        delete_existing_separator = true;
        if (!remaining_text.startswith(auto_separator.value()) ||
            (auto_separator->isspace() && remaining_line.isspace()))
        {
            delete_existing_separator = false;
        }
    }

    if (!insertion.empty())
    {
        delete_selected_text();
        TextPos b = selection_span->begin() -
                    static_cast<TextLength>(deletion.size());
        TextPos e = selection_span->begin();
        replace_text(Span{b, e - b}, e, insertion);
    }
    if (auto_separator &&
        !auto_separator->empty())
    {
        get_text_changer()->insert_string_at_caret(auto_separator.value());
        if (delete_existing_separator)
        {
            TextLength l = static_cast<TextLength>(auto_separator->size());
            TextPos b = selection_span->begin() -
                          static_cast<TextLength>(deletion.size()) +
                          static_cast<TextLength>(insertion.size()) + l;
            replace_text(Span{b, l}, b, {});  // delete, replace with nothing
        }
    }
}

TextSpanPtr WordSuggestions::simulate_insertion(
        const TextSpanPtr& caret_span,
        const UString& deletion,
        const UString& insertion)
{
    assert(caret_span);

    auto context = caret_span->get_text_until_span();
    if (!deletion.empty())
        context = context.slice(0, -static_cast<int>(deletion.size()));
    context += insertion;
    TextPos text_pos = caret_span->text_begin();
    return std::make_shared<TextSpan>(text_pos + static_cast<int>(context.size()),
                                      0, context, text_pos);
}

void WordSuggestions::update_correction_choices()
{
    auto key_logic = get_key_logic();

    m_correction_choices.clear();
    m_correction_span = nullptr;
    if (m_spell_checker and
        config()->are_spelling_suggestions_enabled())
    {
        auto caret_span = m_text_context->get_span_at_caret();
        if (caret_span)
        {
            auto word_span = get_word_to_spell_check(caret_span,
                                                     key_logic->is_typing());
            if (word_span)
            {
                UString auto_capitalization;
                m_correction_choices.clear();

                std::tie(m_correction_span,
                         auto_capitalization) =
                    find_correction_choices(m_correction_choices,
                                            word_span, false);
            }
        }
    }
}

std::tuple<TextSpanPtr, UString> WordSuggestions::find_correction_choices(
        std::vector<UString>& correction_choices,
        const TextSpanPtr& word_span, bool auto_capitalize)
{
    TextSpanPtr correction_span;
    UString auto_capitalization;

    TextPos text_begin = word_span->text_begin();
    UString word = word_span->get_span_text();
    TextPos caret = m_text_context->get_caret_pos();
    TextPos offset = caret - text_begin;  // caret offset into the word

    std::vector<UString> choices;
    USpan span = m_spell_checker->find_corrections(choices, word, offset);
    if (!choices.empty())
    {
        correction_choices = choices;
        correction_span = std::make_shared<TextSpan>(
                span.begin + text_begin,
                span.length,
                span.text,
                span.begin + text_begin);

        // See if there is a valid upper caps variant for
        // auto-capitalization.
        if (auto_capitalize)
        {
            UString choice = correction_choices[0];
            if (word.upper() == choice.upper())
            {
                auto_capitalization = choice;
            }
            else if (!word.empty() && word.slice(0, 1).islower())
            {
                UString caps_word = word.capitalize();
                std::vector<UString> caps_choices;
                m_spell_checker->find_corrections(caps_choices,
                                                  caps_word, offset);
                if (caps_choices.empty())
                {
                    auto_capitalization = caps_word;
                    correction_span = word_span;
                }
            }
        }
    }

    return {correction_span, auto_capitalization};
}

void WordSuggestions::update_prediction_choices()
{
    auto key_logic = get_key_logic();
    auto text_context = get_text_context();

    auto& choices = m_prediction_choices;
    choices.clear();

    if (m_wpengine)
    {
        auto context = text_context->get_context();
        if (!context.empty())  // don't load models on startup
        {
            UString bot_marker = text_context->get_bot_marker();
            UString bot_context = text_context->get_pending_bot_context();

            std::vector<UString> tokens;
            std::vector<Span> spans;
            m_wpengine->tokenize_context( tokens, spans, bot_context);

            int case_insensitive_mode;
            bool ignore_non_caps, capitalize, drop_capitalized;
            std::tie(case_insensitive_mode, ignore_non_caps,
                     capitalize, drop_capitalized) =
                get_prediction_options(
                        tokens,
                        key_logic->get_mod_mask() & Modifier::SHIFT,
                        bot_marker);

            lm::PredictOptions options{};
            if (case_insensitive_mode == 1)
                options |= lm::PredictOptions::CASE_INSENSITIVE;
            if (case_insensitive_mode == 2)
                options |= lm::PredictOptions::CASE_INSENSITIVE_SMART;
            if (config()->word_suggestions->accent_insensitive)
                options |= lm::PredictOptions::ACCENT_INSENSITIVE_SMART;
            if (ignore_non_caps)
                options |= lm::PredictOptions::IGNORE_NON_CAPITALIZED;

            std::vector<UString> choices_;
            m_wpengine->predict(choices_,
                bot_context,
                static_cast<size_t>(
                    config()->word_suggestions->max_word_choices * 8),
                options);

            for (const auto& choice : choices_)
            {
                // Filter out begin-of-text-markers that sneak in as
                // high frequency unigrams.
                if (choice.startswith("<bot:"))
                    continue;

                // Drop upper caps spelling in favor of a lower caps one.
                // Auto-capitalization may later elect to upper caps on insertion.
                if (drop_capitalized)
                {
                    UString choice_lower = choice.lower();
                    if (choice != choice_lower &&
                        m_wpengine->word_exists(choice_lower))
                        continue;
                }

                choices.emplace_back(choice);
            }

            // Make all words start upper case
            if (capitalize)
                choices = capitalize_choices(choices);
        }

        // update word information for the input line display
        // this->word_infos =
        //    this->get_word_infos(m_text_context->get_line())
    }
}

std::tuple<int, bool, bool, bool> WordSuggestions::get_prediction_options(
        std::vector<UString> tokens,
        bool shift, std::optional<UString> bot_marker)
{
    int case_insensitive_mode{0};
    bool ignore_non_caps{false};
    bool capitalize{false};

    if (!tokens.empty())
    {
        UString prefix = tokens.back();
        size_t n = tokens.size();
        bool sentence_begin = (n >= 2 &&
                               (tokens[n-2] == "<s>" ||
                                (bot_marker && tokens[n-2] == bot_marker)));
        bool capitalized  = !prefix.empty() && prefix.slice(0, 1).isupper();
        bool empty_prefix = prefix.empty();

        ignore_non_caps  = (!sentence_begin &&
                            (capitalized || (empty_prefix && shift)));

        capitalize       = sentence_begin && (capitalized || shift);

        bool case_insensitive = sentence_begin || !(capitalized || shift);
        if (!case_insensitive)
        {
            case_insensitive_mode = 0;      // case sensitive
        }
        else
        {
            if (capitalize)
            {
                case_insensitive_mode = 1;  // simple
            }
            else
            {
                case_insensitive_mode = 2;  // smart
            }
        }
    }

    bool drop_capitalized = false;

    return {case_insensitive_mode, ignore_non_caps,
            capitalize, drop_capitalized};
}

std::vector<UString> WordSuggestions::capitalize_choices(const std::vector<UString>& choices)
{
    std::vector<UString> results;
    std::set<UString> seen;

    for (auto& choice : choices)
    {
        if (!choice.empty())
        {
            UString s = choice.capitalize();
            if (!contains(seen, s))
            {
                results.emplace_back(s);
                seen.emplace(s);
            }
        }
    }
    return results;
}

std::tuple<UString, UString> WordSuggestions::get_prediction_choice_changes(const UString& choice)
{
    UString deletion;
    UString insertion;

    if (m_wpengine)
    {
        UString context = get_text_context()->get_context();
        UString word_prefix = m_wpengine->get_last_context_fragment(context);
        UString remainder = choice.slice(static_cast<int>(word_prefix.size()));

        // no case change?
        if (word_prefix + remainder == choice)
        {
            insertion = remainder;

            // Force update of word suggestions, even if there is
            // no change necessary. Else, with lazy word separators,
            // predictions won't take the pending separator into account.
            if (deletion.empty() &&
                insertion.empty())
            {
                deletion = choice.slice(-1);
                insertion = deletion;
            }
        }
        else
        {
            deletion = word_prefix;
            insertion = choice;
        }
    }

    return {deletion, insertion};
}

TextSpanPtr WordSuggestions::get_word_to_spell_check(const TextSpanPtr& caret_span,
                                                     bool is_typing)
{
    TextSpanPtr section_span = get_section_before_span(caret_span);
    if (section_span &&
        can_spell_check(section_span))
    {
        TextSpanPtr word_span = get_word_before_span(caret_span);

        // Don't pop up spelling corrections if we're
        // currently typing the word.
        TextPos caret = caret_span->begin();
        if (is_typing &&
            word_span &&
            word_span->end() == caret)
        {
            word_span = {};
        }

        return word_span;
    }
    return {};
}

void WordSuggestions::auto_correct_at(const TextSpanPtr& word_span, TextPos caret_offset)
{
    auto key_logic = get_key_logic();

    TextSpanPtr correction_span;
    UString replacement;
    std::tie(correction_span, replacement) =  find_auto_correction(
        word_span, true, config()->typing_assistance->auto_correction);
    if (!replacement.empty())
    {
        SuppressModifiers sm(key_logic);

        replace_text(correction_span->span(),
                     caret_offset,
                     replacement);
    }
}

std::tuple<TextSpanPtr, UString> WordSuggestions::find_auto_correction(const TextSpanPtr& word_span,
                                                                       bool auto_capitalize,
                                                                       bool auto_correct)
{
    TextSpanPtr correction_span;
    UString auto_capitalization;
    std::vector<UString> correction_choices;
    std::tie(correction_span, auto_capitalization) =
            find_correction_choices(correction_choices,
                                    word_span, auto_capitalize);

    auto replacement = auto_capitalization;
    if (replacement.empty() &&
        !correction_choices.empty() &&
        auto_correct)
    {
        const size_t min_auto_correct_length = 2;
        const int max_string_distance = 2;

        auto word = word_span->get_span_text();
        if (word.size() > min_auto_correct_length)
        {
            auto choice = correction_choices[0];  // rely on spell checker for now
            int distance = string_distance(word, choice);
            if (distance <= max_string_distance)
                replacement = choice;
        }
    }

    return {correction_span, replacement};
}

TextSpanPtr WordSuggestions::get_word_to_auto_correct(const TextSpanPtr& insertion_span)
{
    UString c = insertion_span->get_last_char_in_span();
    if (c.isspace())
    {
        auto section_span = get_section_before_span(insertion_span);
        if (section_span &&
            can_auto_correct(section_span))
        {
            auto word_span = get_word_before_span(insertion_span);
            return word_span;
        }
    }
    return {};
}

TextSpanPtr WordSuggestions::get_section_before_span(const TextSpanPtr& insertion_span)
{
    static UStringPattern section_begin_pattern{R"(\S*\s*$)"};
    static UStringPattern section_end_pattern{R"(\S*(?=\s*))"};

    TextPos begin;

    {
        auto text = insertion_span->get_text_until_span();
        UStringMatcher matcher(&section_begin_pattern, text);
        if (!matcher.find())
            return {};
        begin = matcher.begin();
    }

    {
        auto text = insertion_span->text.slice(begin);
        UStringMatcher matcher(&section_end_pattern, text);
        if (!matcher.find())
            return {};

        auto span = insertion_span->clone();
        span->length = matcher.end() - matcher.begin();
        span->pos = span->text_pos + begin + matcher.begin();
        return span;
    }
}

TextSpanPtr WordSuggestions::get_word_before_span(const TextSpanPtr& span)
{
    TextSpanPtr word_span;
    if (span && m_wpengine)
    {
        std::vector<UString> tokens;
        std::vector<Span> spans;
        m_wpengine->tokenize_text(tokens, spans, span->get_text());

        TextPos text_begin = span->text_begin();
        TextPos local_caret = span->begin() - text_begin;

        std::optional<size_t> itoken;

        for (size_t i=0; i<spans.size(); i++)
        {
            if (spans[i].begin > local_caret)
                break;
            itoken = i;
        }

        if (itoken)
        {
            auto& token = tokens.at(*itoken);

            // We're only looking for actual words
            if (!contains(array_of<UString>("<unk>", "<num>", "<s>"), token))
            {
                TextPos b = spans[*itoken].begin + text_begin;
                TextPos e = spans[*itoken].end() + text_begin;
                word_span = std::make_shared<TextSpan>(b, e - b, token, b);
            }
        }
    }

    return word_span;
}

void WordSuggestions::delete_selected_text()
{
    auto text_context = get_text_context();
    if (!text_context)
        return;

    if (text_context->can_insert_text())
    {
        // delete any selected text first
        auto selection_span = text_context->get_selection_span();
        if (!selection_span->empty())
        {
            text_context->delete_text(selection_span->pos,
                                      selection_span->length);
        }
    }
    else
    {
        // keystrokes delete the selection on their own.
    }
}

void WordSuggestions::replace_text(const Span& span, TextPos caret,
                                   const UString& new_text)
{
    auto text_context = get_text_context();
    if (!text_context)
        return;

    if (text_context->can_insert_text())
    {
        replace_text_direct(span, caret, new_text);
    }
    else
    {
        replace_text_key_strokes(span, caret, new_text);
    }
}

void WordSuggestions::replace_text_direct(const Span& span,
                                          TextPos caret,
                                          const UString& new_text)
{
    (void) caret;

    auto text_context = get_text_context();
    if (!text_context)
        return;

    if (span.length >= 0)
        text_context->delete_text(span.begin, span.length);
    text_context->insert_text(span.begin, new_text);
}

void WordSuggestions::replace_text_key_strokes(const Span& span, TextPos caret, const UString& new_text)
{
    auto key_logic = get_key_logic();
    TextChanger* tc = key_logic->get_text_changer_keystroke();

    SuppressModifiers sm(key_logic);

    TextLength length = span.length;
    TextPos offset = caret - span.end();  // offset of caret to word end

    // delete the old word
    if (offset >= 0)
    {
        tc->press_keysyms("left",
                          static_cast<size_t>(offset));
        tc->press_keysyms("backspace",
                          static_cast<size_t>(length));
    }
    else
    {
        tc->press_keysyms("delete",
                          static_cast<size_t>(std::abs(offset)));
        tc->press_keysyms("backspace",
                          static_cast<size_t>(length - std::abs(offset)));
    }

    // insert the new word
    tc->press_key_string(new_text);

    // move caret back
    if (offset >= 0)
        tc->press_keysyms("right",
                          static_cast<size_t>(offset));
}

void WordSuggestions::on_text_entry_activated()
{
    m_learn_strategy->on_text_entry_activated();
}

void WordSuggestions::on_text_entry_deactivated()
{
    auto key_logic = get_key_logic();
    auto* tc = key_logic->get_text_changer_direct_insert();

    commit_changes();

    // deactivate the pause button
    if (config()->word_suggestions->
                        get_pause_learning() == PauseLearning::LATCHED)
        config()->word_suggestions->set_pause_learning(PauseLearning::OFF);

    m_text_context->set_pending_separator({});

    tc->stop_auto_repeat();
}

void WordSuggestions::on_text_inserted(const TextSpanPtr& insertion_span,
                                       TextPos caret_offset)
{
    // auto-correction
    if (config()->is_auto_capitalization_enabled())
    {
        auto word_span = get_word_to_auto_correct(insertion_span);
        if (word_span)
            auto_correct_at(word_span, caret_offset);
    }
}

// Asynchronous callback for generic context changes.
// The text of the target widget changed or the caret moved.
// Use this for low priority display stuff.
void WordSuggestions::on_text_context_changed(bool change_detected)
{
    LOG_DEBUG << change_detected;
    if (change_detected)
    {
        expand_corrections(false);
        goto_first_prediction();

        auto keyboard = get_keyboard();
        keyboard->invalidate_context_ui();
        keyboard->commit_ui_updates();

        m_learn_strategy->on_text_context_changed();
    }
}

void WordSuggestions::on_text_caret_moved()
{
}

void WordSuggestions::on_focusable_gui_opening()
{
    if (m_focusable_count == 0)
    {
        if (auto atspi_state_tracker = get_atspi_state_tracker())
            atspi_state_tracker->freeze();

        // disable keep_windows_on_top
        // TODO: not needed in gnome-shell.
        // Sort this out for a later stand-alone application.
        /*
        app = this->get_application();
        if (app)
        {
            app.on_focusable_gui_opening();
        }
        this->stop_raise_attempts();
        */

    }

    m_focusable_count += 1;

    LOG_WARNING << m_focusable_count;
}

void WordSuggestions::on_focusable_gui_closed()
{    
    m_focusable_count -= 1;
    if (m_focusable_count <= 0)
    {
        m_focusable_count = 0;

        // Re-enable AT-SPI listeners
        if (auto atspi_state_tracker = get_atspi_state_tracker())
            atspi_state_tracker->thaw();

        /*
        // re-enable keep_windows_on_top
        app = this->get_application();
        if (app)
        {
            app.on_focusable_gui_closed();
        }
        */
    }

    LOG_WARNING << m_focusable_count;
}

bool WordSuggestions::has_focusable_gui()
{
    return m_focusable_count > 0;
}

bool WordSuggestions::has_changes()
{
    auto text_context = get_text_context();
    return text_context &&
            m_text_context->has_changes();
}

void WordSuggestions::commit_changes()
{
    bool has_changes = this->has_changes();

    LOG_DEBUG << "has_changes=" << repr(has_changes);

    if (has_changes)
        m_learn_strategy->commit_changes();

    clear_changes();  // clear input line too
}

void WordSuggestions::discard_changes()
{
    LOG_INFO << "discarding changes";
    m_learn_strategy->discard_changes();
    clear_changes();
}

void WordSuggestions::clear_changes()
{
    auto text_context = get_text_context();
    if (text_context)
    {
        text_context->clear_changes();
        text_context->reset();
    }

    // Clear the spell checker cache, new words may have
    // been added from somewhere.
    if (m_spell_checker)
        m_spell_checker->invalidate_query_cache();

    get_key_logic()->set_last_typed_was_separator(false);
}

void WordSuggestions::insert_pending_separator()
{
    auto separator_span = m_text_context->get_pending_separator();
    if (separator_span)
    {
        m_punctuator->insert_separator_at_distance(separator_span);
        m_text_context->set_pending_separator({});
    }
}

bool WordSuggestions::update_pending_separator_popup()
{
#if 0
    if (config.xid_mode)
    {
        return;
    }

    show = false;
    rect = None;

    span = m_text_context->get_pending_separator();
    if (span)
    {
        offset = span.begin() - 1;
        if (offset >= 0)
        {
            rect = m_text_context->get_character_extents(offset);
            if (rect)
            {
                rect.x += rect.w;
                // Set width to a compromise between narrowest, e.g. "i",
                // && widest, e.g. "M", proportional characters.
                // This still does OK for fixed Terminal fonts.
                rect.w = max(min(rect.w, rect.h / 2), rect.h / 3);
                if (!rect.empty())
                {
                    show = true;
                }
            }
        }
    }

    if (show)
    {
        view = this->get_main_view();
        if (view)
        {
            m_pending_separator_popup.show_at(view, rect);
        }
    }
    else
    {
        this->hide_pending_separator_popup();
    }
#endif
    return false;
}

void WordSuggestions::hide_pending_separator_popup()
{
#if 0
    if (m_pending_separator_popup->is_visible())
    {
        m_pending_separator_popup->hide();
    }
#endif
}

int WordSuggestions::string_distance(const UString& s1, UString& s2)
{
    size_t len1 = s1.size();
    size_t len2 = s2.size();
    std::vector<int> last_row;

    std::vector<int> row(len2);
    std::iota(row.begin(), row.end(), 1);
    row.emplace_back(0);

    for (size_t ri=0; ri<len1; ++ri)
    {
        last_row = row;

        std::fill(row.begin(), row.end(), 0);
        row.back() = static_cast<int>(ri + 1);

        for (size_t ci=0; ci<len2; ++ci)
        {
            int d = s1[ri] == s2[ci] ? 0 : 1;
            row[ci] = std::min(row[ci - 1] + 1, std::min(
                               last_row[ci] + 1,
                               last_row[ci - 1] + d));
        }
    }

    return row[len2 - 1];
}

vector<LayoutPanelWordListPtr>& WordSuggestions::get_wordlist_panels()
{
    return m_word_list_bars;
}

void WordSuggestions::show_language_selection(LayoutView* view,
                                              const LayoutKeyPtr& key,
                                              MouseButton::Enum button,
                                              std::function<void()> close_func)
{
    (void)button;

    auto keyboard_view = get_keyboard_view();
    auto language_db = get_language_db();

    keyboard_view->hide_touch_feedback();

    if (keyboard_view && language_db && view)
    {
        ViewBase* toplevel = view->find_toplevel_view_from_leaf();
        if (toplevel)
        {
            Rect r = key->get_canvas_border_rect();
            std::array<double, 4> rect{r.x, r.y, r.w, r.h};

            std::string active_lang_id = config()->typing_assistance->active_language;
            std::string system_lang_id = language_db->get_system_default_lang_id();

            // all language ids
            std::vector<std::string> lang_ids = language_db->get_language_ids();

            // remove system language id, we'll send it separately
            remove(lang_ids, system_lang_id);

            // most recently used language ids
            size_t max_mru_langs = config()->typing_assistance->max_recent_languages;
            std::vector<std::string> mru_lang_ids =
                    slice(config()->typing_assistance->recent_languages.get(),
                          0, static_cast<int>(max_mru_langs));

            std::vector<const char*> cmru_lang_ids(mru_lang_ids.size());
            std::transform(mru_lang_ids.begin(), mru_lang_ids.end(), cmru_lang_ids.begin(),
                           [](const std::string& s){return s.data();});

            // remainging language ids, sorted by translated language name
            std::vector<std::string> other_lang_ids =
                    difference(lang_ids, mru_lang_ids);
            std::vector<std::pair<std::string, std::string>> other_langs;
            for (auto& lang_id : other_lang_ids)
            {
                auto name = language_db->get_language_full_name(lang_id);
                if (!name.empty())  // there might be junk in the models folder
                    other_langs.emplace_back(name, lang_id);
            }
            std::sort(other_langs.begin(), other_langs.end());

            std::vector<const char*> cother_lang_ids(other_langs.size());
            std::transform(other_langs.begin(), other_langs.end(), cother_lang_ids.begin(),
                           [](const auto& l){return l.second.data();});


            // let toolkit dependent code handle the actual ui
            auto callbacks = get_global_callbacks();
            if (callbacks->show_language_selection)
            {
                m_language_selection_close_func = close_func;

                on_focusable_gui_opening();

                callbacks->show_language_selection(
                        get_cinstance(), toplevel,
                        rect.data(), rect.size(),
                        active_lang_id.c_str(), system_lang_id.c_str(),
                        cmru_lang_ids.data(), cmru_lang_ids.size(),
                        cother_lang_ids.data(), cother_lang_ids.size());
            }
        }
    }
}

void WordSuggestions::on_language_selection_closed()
{
    if (m_language_selection_close_func)
    {
        m_language_selection_close_func();
        m_language_selection_close_func = nullptr;

        on_focusable_gui_closed();
    }
}
