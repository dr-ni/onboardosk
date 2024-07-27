#ifndef WPENGINE_H
#define WPENGINE_H

#include <mutex>
#include <thread>

#include "lm_decls.h"

#include "tools/textdecls.h"
#include "tools/ustringmain.h"

#include "onboardoskglobals.h"


class AutoSaveTimer;
class ModelCache;
class UString;

typedef TSpan<UString> USpan;

// colon separated language model desctiption
using LMDESCR = std::string;
using LMDESCRs = std::vector<LMDESCR>;

// language model id of the form lc_CC (language-code, country-code)
using LMID = std::string;
using LMIDs = std::vector<LMID>;

namespace lm {
    struct PredictResult;
}

// Singleton for interfacing with low-level word prediction.
// Local in-process engine.
class WPEngine : public ContextBase
{
    public:
        using Super = ContextBase;

        WPEngine(const ContextBase& context);
        ~WPEngine();

        ModelCache* get_model_cache();
        LMDESCRs& get_models_descriptions() {return m_models;}

        void set_model_ids(const LMDESCRs& persistent_models,
                           const LMDESCRs& auto_learn_models,
                           const LMDESCRs& scratch_models);

        // Pre-load models set with set_models. If this isn't called,
        // language models are lazy-loaded on demand.
        void load_models();

        void save_models(const std::string& reason,
                         bool concurrent=false);

        void postpone_autosave();

        // Pause for a minute max, because resume_autosave isn't
        // reliable called, e.g. when dragging and leaving the window.
        void pause_autosave();

        void resume_autosave();

        // Find completion/prediction choices.
        void predict(std::vector<UString>& choices,
                     const UString& context_line, size_t limit=20,
                     lm::PredictOptions options=lm::DEFAULT_OPTIONS);

        // Count n-grams and add words to the auto-learn models.
        void learn_text(const UString& text, bool allow_new_words);

        // Remove tokens that don't already exist in any active model.
        void drop_new_words(std::vector<std::vector<UString>>& token_sections,
                            const std::vector<UString>& tokens,
                            const std::vector<Span>& spans,
                            const LMIDs& lmids);

        // Count n-grams and add words to the scratch models.
        void learn_scratch_text(const UString& text);
        void clear_scratch_models();

        // Split <text> into tokens and lookup the individual tokens in each
        // of the given language models. See lookup_tokens() for more information.
        void lookup_text(std::vector<USpan>& tokspans_out,
                         std::vector<std::vector<int> >& counts_out,
                         const UString& text, const LMIDs& lmids);

        // Lookup the individual tokens in each of the given language models.
        // This method is meant to be a basis for highlighting (partially)
        // unknown words in a display for recently typed text.
        // The return value is a tuple of two arrays. First an array of tuples
        // (start, end, token), one per token, with start and end index pointing
        // into <text> and second a two dimensional array of lookup results.
        // There is one lookup result per token and language model. Each lookup
        // result is either 0 for no match, 1 for an exact match or -n for
        // count n partial (prefix) matches.
        void lookup_tokens(std::vector<USpan>& tokspans_out,
                           std::vector<std::vector<int>>& counts_out,
                           const std::vector<UString>& tokens,
                           const std::vector<Span>& spans,
                           const LMIDs& lmids);

        // Does word exist in any of the non-scratch models?
        bool word_exists(const UString& word);

        // Let the service find the words in text.
        void tokenize_text(std::vector<UString>& tokens,
                           std::vector<Span>& spans,
                           const UString& text);
        void tokenize_context(std::vector<UString>& tokens,
                           std::vector<Span>& spans,
                           const UString& text);

        // Return the names of the available models.
        void get_model_names(std::vector<std::string>& names,
                             const std::string& _class);

        // Return the very last (partial) word in text.
        UString get_last_context_fragment(const UString& text);

        void get_prediction(std::vector<lm::UPredictResult>& predictions,
                            const LMDESCRs& lmdescrs,
                            const std::vector<UString>& context,
                            std::optional<size_t> limit, lm::PredictOptions options);

        // Remove the last word of context in the given context.
        // If len(context) == 1 then all occurences of the word will be removed.
        void remove_context(const std::vector<UString>& context);

        void log_learning(const std::string& s);

    private:
        void do_save_models();

    private:
        std::unique_ptr<ModelCache> m_model_cache;
        std::unique_ptr<AutoSaveTimer> m_auto_save_timer;

        LMDESCRs m_models;
        LMDESCRs m_persistent_models;
        LMDESCRs m_auto_learn_models;
        LMDESCRs m_scratch_models;

        std::thread m_save_thread;
        std::recursive_mutex m_save_mutex;
};


// Load, save and cache language models
class ModelCache : public ContextBase
{
    public:
        using Super = ContextBase;

        ModelCache(const ContextBase& context);
        ~ModelCache();

        void clear();

        // Extract language model ids and interpolation weights from
        // the language model description.
        static void parse_lmdesc(std::vector<LMID>& lmids,
                                 std::vector<double>& weights,
                                 const LMDESCRs& lmdesc);

        void find_available_model_names(std::vector<std::string>& names,
                                        const std::string& class_);

        // get language model from cache or load it from disk
        lm::LanguageModel* get_model(const LMID& lmid);

        std::vector<lm::LanguageModel*> get_models(const LMIDs& lmids);

        void save_models();

        static bool is_user_lmid(const LMID& lmid);

        std::string get_filename(const LMID& lmid);

        static std::string get_backup_filename(const std::string& filename);

        // Return filename for renamed broken files.
        static std::string get_broken_filename(const std::string& filename);

    private:

        std::vector<std::string> find_models(const std::string& class_);

        // Fully qualifies and unifies language model ids.
        // Fills in missing fields with default values.
        // The result is of the format "type:class:name".
        static LMID canonicalize_lmid(const LMID& lmid);

        static std::tuple<std::string, std::string, std::string>
            split_lmid(const LMID& lmid);


        std::unique_ptr<lm::LanguageModel> load_model(const LMID& lmid);

        void do_load_model(lm::LanguageModel* model,
                           const std::string& filename,
                           const std::string& class_);

        static bool can_save(const LMID& lmid);

        void save_model(lm::LanguageModel* model, const LMID& lmid);

    private:
        std::map<LMID, std::unique_ptr<lm::LanguageModel>> m_language_models;
};

#endif // WPENGINE_H
