#ifndef WORDSUGGESTIONS_H
#define WORDSUGGESTIONS_H

#include <functional>
#include <set>
#include <string>
#include <vector>
#include <optional>

#include "tools/container_helpers.h"
#include "tools/noneable.h"
#include "tools/textdecls.h"
#include "tools/ustringmain.h"

#include "layoutdecls.h"
#include "keyboarddecls.h"
#include "onboardoskglobals.h"
#include "textchangesdecls.h"
#include "signalling.h"

using std::string;
using std::vector;

class LayoutView;
class LearnStrategy;
class LanguageDB;
class WPErrorRecovery;
class PendingSeparatorPopup;
class Punctuator;
class SpellChecker;
class TextDomain;
class TextContext;
class TextChanger;
class Timer;
class UString;
class WPEngine;


class WordSuggestions : public ContextBase
{
    public:
        WordSuggestions(const ContextBase& context);
        virtual ~WordSuggestions();

        LayoutItemPtr get_layout() const;
        TextContext* get_text_context() const;
        WPEngine* get_wp_engine() const;
        TextChanger* get_text_changer() const;
        SpellChecker* get_spell_checker();

        virtual TextDomain* get_text_domain();

        void reset();

        void enable_word_suggestions(bool enable);

        // Union of all system and user models
        void get_system_model_names(std::vector<std::string>& names);

        // Union of all system and user models
        std::vector<std::string> get_merged_model_names();

        // Current language id; never empty.
        std::string get_lang_id();

        // Current language id; Empty for system default language.
        std::string get_active_lang_id();
        void on_active_lang_id_changed();
        void set_active_language_id(const std::string& lang_id, bool add_to_mru);

        std::vector<std::string> get_spellchecker_dicts();

        // Config callback for word_suggestions.enabled changes.
        void on_word_suggestions_enabled();

        void apply_prediction_profile();

        void on_layout_loaded();
        void on_any_key_down();
        void on_all_keys_up();
        void on_before_key_press(const LayoutKeyPtr& key);
        void on_before_key_release(const LayoutKeyPtr& key);
        void on_after_key_release(const LayoutKeyPtr& key);
        void send_key_up(const LayoutKeyPtr& key,
                         LayoutView* view,
                         MouseButton::Enum button,
                         EventType::Enum event_type);

        void on_activity_detected();

        void on_punctuator_changed();
        void on_spell_checker_changed();


        void on_outside_click(MouseButton::Enum button);
        void on_cancel_outside_click();


        // Password entries may block feedback to prevent eavesdropping.
        bool can_give_keypress_feedback();
        bool is_capitalization_requested();
        bool can_direct_insert_text();

        // No spell checking for passwords, URLs, etc.
        bool can_spell_check(const TextSpanPtr& span_to_check);

        // No auto correct for passwords, URLs, etc.
        bool can_auto_correct(const TextSpanPtr& section_span);

        // No auto-punctuation in terminal when prompt was detected.
        bool can_auto_punctuate();

        void update_punctuator();

        void update_spell_checker();
        void invalidate_for_resize();

        void update_suggestions_ui(std::set<LayoutItemPtr>& keys_to_redraw);
        void update_wordlists(std::set<LayoutItemPtr>& keys_to_redraw);

        bool has_suggestions();
        void expand_corrections(bool expand);
        void goto_first_prediction();

        // wordlist:     WordListPanel that choice_index belongs to
        // choice_index: index of a visible prediction key
        UString get_prediction_choice(const LayoutPanelWordListPtr& wordlist,
                                      size_t choice_index);

        std::tuple<UString, std::vector<UString>>
            get_prediction_choice_and_history(const LayoutPanelWordListPtr& wordlist,
                                              size_t choice_index);

        void remove_prediction_context(const std::vector<UString>& context);

        // spelling correction clicked
        void insert_correction_choice(const LayoutKeyPtr& key,
                                      size_t choice_index);

        // prediction choice clicked
        void insert_prediction_choice(const LayoutKeyPtr& key,
                                      size_t choice_index,
                                      bool allow_separator);

        // Insert a word/word-remainder and add a separator string as needed.
        void replace_text_at_caret(const UString& deletion,
                                   const UString& insertion,
                                   const std::optional<UString>& auto_separator={});

        // Return the context up to caret_span with deletion
        // and insertion applied.
        TextSpanPtr simulate_insertion(const TextSpanPtr& caret_span,
                                       const UString& deletion,
                                       const UString& insertion);

        void update_correction_choices();

        // Find spelling suggestions for the word at || before the caret.;
        std::tuple<TextSpanPtr, UString> find_correction_choices(
                std::vector<UString>& correction_choices,
                const TextSpanPtr& word_span,
                bool auto_capitalize);

        // word prediction: find choices, only once per key press
        void update_prediction_choices();

        std::tuple<int, bool, bool, bool> get_prediction_options(
                std::vector<UString> tokens,
                bool shift,
                std::optional<UString> bot_marker={});

        // Set first letters to upper case and remove;
        // double entries created that way.
        std::vector<UString> capitalize_choices(const std::vector<UString>& choices);

        // Determines the text changes necessary when inserting
        // the prediction choice at choice_index.
        std::tuple<UString, UString> get_prediction_choice_changes(const UString& choice);

        // Get the word to be spell-checked.
        TextSpanPtr get_word_to_spell_check(const TextSpanPtr& caret_span, bool is_typing);

        // Auto-capitalize/correct a word_span.
        void auto_correct_at(const TextSpanPtr& word_span, TextPos caret_offset);

        std::tuple<TextSpanPtr, UString> find_auto_correction(const TextSpanPtr& word_span,
                                                              bool auto_capitalize,
                                                              bool auto_correct);

        TextSpanPtr get_word_to_auto_correct(const TextSpanPtr& insertion_span);

        // Get first whitespace-separated section that
        // begins before the given span.
        TextSpanPtr get_section_before_span(const TextSpanPtr& insertion_span);

        // Get the word at or before the span.
        TextSpanPtr get_word_before_span(const TextSpanPtr& span);

        // Delete the current selection.
        void delete_selected_text();

        // Replace text from <begin> to <end> with <new_text>,
        void replace_text(const Span& span, TextPos caret, const UString& new_text);

        // Replace text from <begin> to <end> with <new_text>,
        void replace_text_direct(const Span& span, TextPos caret, const UString& new_text);

        // Replace text from <begin> to <end> with <new_text>,
        // Fall-back for text entries without support for direct text insertion.
        void replace_text_key_strokes(const Span& span, TextPos caret, const UString& new_text);

        // The current accessible lost focus.
        void on_text_entry_deactivated();

        // A different target widget has been focused
        void on_text_entry_activated();

        // Synchronous callback for text insertion
        void on_text_inserted(const TextSpanPtr& insertion_span, TextPos caret_offset);

        // Asynchronous callback for generic context changes.
        // The text of the target widget changed or the caret moved.
        // Use this for low priority display stuff.
        void on_text_context_changed(bool change_detected);

        // Caret position changed.
        void on_text_caret_moved();

        // Turn off AT-SPI listeners while there is a dialog open.
        // Onboard and occasionally the whole desktop tend to lock up otherwise.
        // Call this before dialog/popop menus are opened by Onboard itthis->
        void on_focusable_gui_opening();

        // Call this after dialogs/menus have been closed.
        void on_focusable_gui_closed();

        bool has_focusable_gui();

        // Are there any text changes to learn?
        bool has_changes();

        // Learn all accumulated changes and clear them
        void commit_changes();

        // Discard all changes that have accumulated, don't learn them.
        void discard_changes();

        // Reset all contexts and clear all changes.
        void clear_changes();

        // Insert pending separator at the position of the separator.
        // This is only useful when text was inserted from outside of
        // Onboard, e.g. by middle clicking to insert something from
        // clipboard.
        void insert_pending_separator();
        bool update_pending_separator_popup();
        void hide_pending_separator_popup();

        // Levenshtein string distance
        int string_distance(const UString& s1, UString& s2);

        void show_language_selection(LayoutView* view,
                                     const LayoutKeyPtr& key,
                                     MouseButton::Enum button,
                                     std::function<void()> close_func);
        void on_language_selection_closed();

    private:
        vector<LayoutPanelWordListPtr>& get_wordlist_panels();
        void update_wp_engine();
        void load_models();

    private:
        std::unique_ptr<Timer> m_load_models_timer;
        std::unique_ptr<TextContext> m_atspi_text_context;
        TextContext* m_text_context{};
        std::unique_ptr<LearnStrategy> m_learn_strategy;
        std::unique_ptr<SpellChecker> m_spell_checker;
        std::unique_ptr<Punctuator> m_punctuator;
        std::unique_ptr<PendingSeparatorPopup> m_pending_separator_popup;
        std::unique_ptr<Timer> m_pending_separator_popup_timer;
        std::unique_ptr<WPErrorRecovery> m_load_error_recovery;
        bool m_load_errors_reported{false};

    protected:
        std::unique_ptr<WPEngine> m_wpengine;

    private:
        std::vector<UString> m_correction_choices;
        TextSpanPtr m_correction_span;
        std::vector<UString> m_prediction_choices;

        TextSpanPtr m_separator_before_key_press;

        bool m_hide_input_line{false};
        vector<LayoutPanelWordListPtr> m_word_list_bars;

        size_t m_focusable_count{};

        std::function<void()> m_language_selection_close_func;

        SignalConnections m_connections; // last to initialize, first to destruct
};

#endif // WORDSUGGESTIONS_H
