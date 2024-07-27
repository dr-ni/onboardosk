
#include <fstream>
#include <iomanip>      // put_time, setw
#include <map>
#include <string>
#include <vector>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "lm_unigram.h"
#include "lm_dynamic_cached.h"
#include "lm_merged.h"
#include "lm_tokenize.h"

#include "tools/container_helpers.h"
#include "tools/logger.h"
#include "tools/file_helpers.h"
#include "tools/path_helpers.h"
#include "tools/string_helpers.h"
#include "tools/time_helpers.h"
#include "tools/ustringmain.h"
#include "tools/xdgdirs.h"

#include "configuration.h"
#include "timer.h"
#include "wpengine.h"


using lm::LanguageModel;



ModelCache::ModelCache(const ContextBase& context) :
    Super(context)
{}

ModelCache::~ModelCache()
{}

void ModelCache::clear()
{
    m_language_models.clear();
}

std::vector<lm::LanguageModel*> ModelCache::get_models(const LMIDs& lmids)
{
    std::vector<LanguageModel*> models;
    for (auto lmid : lmids)
    {
        auto model = get_model(lmid);
        if (model)
            models.emplace_back(model);
    }
    return models;
}

lm::LanguageModel* ModelCache::get_model(const LMID& lmid)
{
    LanguageModel* model{};
    LMID lmid_ = canonicalize_lmid(lmid);
    if (contains(m_language_models, lmid_))
    {
        model = m_language_models[lmid_].get();
    }
    else
    {
        auto m = load_model(lmid_);
        if (m)
        {
            model = m.get();
            m_language_models[lmid_] = std::move(m);
        }
    }
    return model;
}

void ModelCache::find_available_model_names(std::vector<std::string>& names,
                                            const std::string& class_)
{
    auto fns = find_models(class_);
    for (const auto& fn : fns)
    {
        auto basename = get_basename(fn);
        names.emplace_back(basename);
    }
}

std::vector<std::string> ModelCache::find_models(const std::string& class_)
{
    std::vector<std::string> fns;

    std::string dir;
    if (class_ == "system")
        dir = config()->get_system_model_dir();
    else
        dir = config()->get_user_model_dir();

    try
    {
        const std::string extension = ".lm";
        for (auto &p : fs::directory_iterator(dir))
            if (p.path().extension() == extension)
                fns.emplace_back(p.path());
    }
    catch (const fs::filesystem_error& ex)
    {
        LOG_WARNING << "Failed to find language models in " << repr(dir)
                    << ": " << ex.what();
    }
    return fns;
}

void ModelCache::parse_lmdesc(std::vector<LMID>& lmids,
                              std::vector<double>& weights,
                              const std::vector<std::string>& lmdesc)
{
    lmids.clear();
    weights.clear();

    for (const auto& entry : lmdesc)
    {
        auto fields = split(entry, ',');
        if (!fields.empty())
        {
            lmids.emplace_back(fields[0]);

            double weight = 1.0;
            if (fields.size() >= 2)
                weight = to_double(fields[1]);
            weights.emplace_back(weight);
        }
    }
}

LMID ModelCache::canonicalize_lmid(const LMID& lmid)
{
    // default values
    std::vector<std::string> result{"lm", "system", "en"};
    auto fields = slice(split(lmid, ':'),
                        0, static_cast<int>(result.size()));
    for (size_t i=0; i<result.size() && i<fields.size(); i++)
        result[i] = fields[i];
    return join(result, ":");
}

std::tuple<std::string, std::string, std::string> ModelCache::split_lmid(const LMID& lmid)
{
    auto lmid_ = ModelCache::canonicalize_lmid(lmid);
    auto fields = split(lmid_, ':');
    return {fields.size() > 0 ? fields[0] : "",
            fields.size() > 1 ? fields[1] : "",
            fields.size() > 2 ? fields[2] : ""};
    }

    bool ModelCache::is_user_lmid(const LMID& lmid)
    {
    std::string type_, class_, name;
    std::tie(type_, class_, name) = ModelCache::split_lmid(lmid);
    return class_ == "user";
}

std::unique_ptr<lm::LanguageModel> ModelCache::load_model(const LMID& lmid)
{
    std::unique_ptr<lm::LanguageModel> model;

    std::string type_, class_, name;
    std::tie(type_, class_, name) = ModelCache::split_lmid(lmid);

    std::string filename = get_filename(lmid);

    if (type_ == "lm")
    {
        if (class_ == "system")
        {
            if (lm::read_order(filename) == 1)
                model = std::make_unique<lm::UnigramModel>();
            else
                model = std::make_unique<lm::DynamicModel>();
        }
        else if (class_ == "user")
        {
            model = std::make_unique<lm::CachedDynamicModel>();
        }
        else if (class_ == "mem")
        {
            model = std::make_unique<lm::DynamicModel>();
        }
        else
        {
            LOG_ERROR << "Unknown class component " << repr(class_)
                      << " in lmid " << repr(lmid);
            return {};
        }
    }
    else
    {
        LOG_ERROR << "Unknown type component " << repr(type_)
                  << " in lmid " << repr(lmid);
        return {};
    }

    if (!filename.empty())
        do_load_model(model.get(), filename, class_);

    return model;
}

void ModelCache::do_load_model(lm::LanguageModel* model, const std::string& filename, const std::string& class_)
{
    LOG_INFO << "Loading language model " << repr(filename);

    if (!fs::exists(filename))
    {
        if (class_ == "system")
            LOG_WARNING << "System language model " << repr(filename)
                        << " doesn't exist, skipping.";
    }
    else
    {
        try
        {
            model->load(filename);
            if (class_ == "user")
                if (!backup_file(filename, "/tmp/models"))
                    LOG_ERROR << "backup_file failed: " << repr(filename);
        }
        catch (const lm::Exception& ex)
        {
            std::string msg = sstr()
                              << "Failed to load language model " << repr(filename)
                              << ": " << ex.what();
            model->set_load_error_msg(msg);
            LOG_ERROR << msg;

            if (class_ == "user")
            {
                LOG_ERROR << "Saving word suggestions disabled "
                          << "to prevent further data loss.";
            }
        }
    }
}

void ModelCache::save_models()
{
    for (auto& it : m_language_models)
    {
        const LMID& lmid = it.first;
        LanguageModel* model = it.second.get();
        if (can_save(lmid))
            save_model(model, lmid);
    }
}

bool ModelCache::can_save(const LMID& lmid)
{
    std::string type_, class_, name;
    std::tie(type_, class_, name) = ModelCache::split_lmid(lmid);
    return class_ == "user";
}

void ModelCache::save_model(lm::LanguageModel* model, const LMID& lmid)
{
    std::string type_, class_, name;
    std::tie(type_, class_, name) = ModelCache::split_lmid(lmid);
    std::string filename = get_filename(lmid);

    std::string backup_filename = get_backup_filename(filename);

    if (!filename.empty() &&
        model->is_modified())
    {

        if (model->get_load_error())
        {
            LOG_WARNING << "Not saving modified language model "
                        << repr(filename)
                        << " due to previous error on load.";
        }
        else
        {
            LOG_INFO << "Saving language model " << repr(filename);
            try
            {
                // create the path
                auto path = fs::path(filename).remove_filename();
                get_xdg_dirs()->assure_user_dir_exists(path);

                if (true)
                {
                    // save to temp file
                    std::string basename = get_basename(filename);
                    std::string tempfile = basename + ".tmp";
                    model->save(tempfile);

                    // rename to final file
                    if (fs::exists(filename))
                        fs::rename(filename, backup_filename);
                    fs::rename(tempfile, filename);
                }

                model->set_modified(false);
            }
            catch (const lm::Exception& ex)
            {
                LOG_ERROR
                        << "failed to save language model "
                        << repr(filename)
                        << ": " << ex.what();
            }
            catch (const fs::filesystem_error&)
            {
                LOG_ERROR
                        << "failed to save language model "
                        << repr(filename)
                        << ": " << strerror(errno)
                        << " (" << errno << ")";
            }
        }
    }
}

std::string ModelCache::get_filename(const LMID& lmid)
{
    std::string type_, class_, name;
    std::tie(type_, class_, name) = ModelCache::split_lmid(lmid);
    std::string filename;
    if (class_ == "mem")
    {
        filename = {};
    }
    else
    {
        std::string path;
        if (class_ == "system")
            path = config()->get_system_model_dir();
        else  // if (class_ == "user")
            path = config()->get_user_model_dir();
        std::string ext = type_;
        filename = fs::path(path) / (name + "." + ext);
    }

    return filename;
}

std::string ModelCache::get_backup_filename(const std::string& filename)
{
    return filename + ".bak";
}

std::string ModelCache::get_broken_filename(const std::string& filename)
{
    std::string fn;
    size_t count = 1;

    while (true)
    {
        fn = sstr()
             << filename << ".broken-"
             << format_time("%Y-%m-%d")
             << "_" << std::setw(3) << std::setfill('0') << count;

        if (!fs::exists(fn))
            break;
        count += 1;
    }
    return fn;
}


// Auto-save modified language models periodically
class AutoSaveTimer : public Timer
{
    public:
        using Super = Timer;
        using Clock = std::chrono::steady_clock;
        using TimePoint = std::chrono::time_point<Clock>;

        AutoSaveTimer(const ContextBase& context, WPEngine* wpengine) :
            Super(context),
            m_wpengine(wpengine)
        {
            start(m_timer_interval, [this]{return on_timer();});
        }
        virtual ~AutoSaveTimer();

        // No auto-saving while paused, e.g. during key-press.
        void pause(std::chrono::seconds duration={})
        {
            m_pause = duration;
        }

        // Allow auto-saving again.
        void resume()
        {
            m_pause = {};
        }

        // Postpone saving a little while the user is still typing.
        // Helps to mask the delay when saving large models, during which
        // Onboard briefly becomes unresponsive.
        void postpone()
        {
            auto elapsed = Clock::now() - m_last_save_time;
            if (m_interval < elapsed + m_postpone_delay)
            {
                m_interval = elapsed + m_postpone_delay;
                if (m_interval > m_interval_max)
                    m_interval = m_interval_max;
            }
            LOG_DEBUG << "postponing autosave: current interval " << m_interval.count()
                      << ", elapsed since last save " << elapsed.count();
        }

        bool on_timer()
        {
            auto now = Clock::now();
            auto elapsed = now - m_last_save_time;
            if (m_interval < elapsed &&
                m_pause == std::chrono::seconds{})
            {
                m_last_save_time = now;
                m_interval = m_interval_min;
                LOG_DEBUG << "auto-saving language models,"
                          << " interval " << m_interval.count()
                          << ", elapsed time " << elapsed.count();
                m_wpengine->save_models("AutoSaveTimer", true);
            }

            if (m_pause != std::chrono::seconds{})
                m_pause = std::max(std::chrono::seconds{},
                                   m_pause - m_timer_interval);

            return true;  // run again
        }

    private:
        WPEngine* m_wpengine{};
        std::chrono::minutes m_interval_min{10};
        std::chrono::minutes m_interval_max{30};
        std::chrono::seconds m_postpone_delay{10};
        std::chrono::duration<double> m_interval{m_interval_min};
        TimePoint m_last_save_time;
        std::chrono::seconds m_pause{};
        std::chrono::seconds m_timer_interval{5};
};

AutoSaveTimer::~AutoSaveTimer()
{
}



WPEngine::WPEngine(const ContextBase& context) :
    ContextBase(context),
    m_model_cache(std::make_unique<ModelCache>(context)),
    m_auto_save_timer(std::make_unique<AutoSaveTimer>(context,
                                                      this))
{
}

WPEngine::~WPEngine()
{
    save_models("WPEngine destructor");
}

ModelCache* WPEngine::get_model_cache()
{
    return m_model_cache.get();
}

void WPEngine::set_model_ids(const std::vector<std::string>& persistent_models,
                             const std::vector<std::string>& auto_learn_models,
                             const std::vector<std::string>& scratch_models)
{
    std::lock_guard<std::recursive_mutex> locker(m_save_mutex);

    m_models = persistent_models + scratch_models;
    m_persistent_models = persistent_models;
    m_auto_learn_models = auto_learn_models;
    m_scratch_models = scratch_models;
}

void WPEngine::load_models()
{
    std::lock_guard<std::recursive_mutex> locker(m_save_mutex);

    m_model_cache->get_models(m_models);
}

void WPEngine::save_models(const std::string& reason, bool concurrent)
{
    LOG_WARNING << "saving models: " + reason;

    if (config()->can_log_learning())
        log_learning(format_time_stamp() +
                     " saving models: " + reason + "\n");

    if (m_save_thread.joinable())
        m_save_thread.join();

    if (concurrent)
    {
        // Saving may take a few seconds with user language models sizes in
        // the megabytes. Don't block the GUI, hide it in a worker thread
        std::thread thread([&]{do_save_models();});
        m_save_thread = std::move(thread);
    }
    else
    {
        do_save_models();
    }
}

void WPEngine::do_save_models()
{
    LOG_WARNING << "saving begin";
    std::lock_guard<std::recursive_mutex> locker(m_save_mutex);
    m_model_cache->save_models();
    LOG_WARNING << "saving end";
}

void WPEngine::postpone_autosave()
{
    m_auto_save_timer->postpone();
}

void WPEngine::pause_autosave()
{
    m_auto_save_timer->pause(std::chrono::seconds(60));
}

void WPEngine::resume_autosave()
{
    m_auto_save_timer->resume();
}

void WPEngine::predict(std::vector<UString>& choices,
                       const UString& context_line, size_t limit,
                       lm::PredictOptions options)
{
    std::vector<UString> context;
    std::vector<Span> spans;
    lm::tokenize_context(context, spans, context_line);

    std::vector<lm::UPredictResult> predictions;
    get_prediction(predictions, m_models, context, limit, options);

    for (auto& p : predictions)
        choices.emplace_back(std::move(p.word));

    LOG_DEBUG << "context=" << context;
    LOG_DEBUG << "choices=" <<  slice(choices, 0, 5);
}

void WPEngine::learn_text(const UString& text, bool allow_new_words)
{
    LOG_WARNING << "learn_text1("
              << "text=" << repr(text)
              << ", allow_new_words=" << repr(allow_new_words)
              << ": " << m_auto_learn_models;

    std::lock_guard<std::recursive_mutex> locker(m_save_mutex);

    LOG_WARNING << "learn_text2";

    if (!m_auto_learn_models.empty())
    {
        std::vector<UString> tokens;
        std::vector<Span> spans;
        lm::tokenize_text(tokens, spans, text);

        // There are too many false positives with trailing
        // single quotes: remove them.
        // Do this here, because we still want "it's", etc. to
        // incrementally provide completions.
        for (size_t i=0; i<tokens.size(); i++)
        {
            const auto& token = tokens[i];
            if (token.endswith("'"))
            {
                UString tok = token.slice(0, -1);
                if (tok.empty())  // shouldn't happen
                    tok = "<unk>";
                tokens[i] = tok;
            }
        }

        // if requested, drop unknown words
        std::vector<std::vector<UString>> token_sections;
        if (allow_new_words)
            token_sections = {tokens};
        else
            drop_new_words(token_sections, tokens, spans,
                           m_persistent_models);

        auto models = m_model_cache->get_models(m_auto_learn_models);
        for (auto model : models)
        {
            auto dm = dynamic_cast<lm::DynamicModelBase*>(model);
            if (dm)
                for (const auto& tokens_ : token_sections)
                    dm->learn_tokens(tokens_);
        }

        LOG_INFO << "learn_text: tokens=" << token_sections;

        // debug: save all learned text for later parameter optimization
        if (config()->can_log_learning())
        {
            std::stringstream ss;
            //ss << text << std::endl;
            for (const auto& tokens_ : token_sections)
                ss << tokens_ << std::endl;
            ss << std::endl;
            log_learning(ss.str());
        }
    }
}

void WPEngine::log_learning(const std::string& s)
{
    std::string fn = fs::path(config()->get_user_dir()) / "learned_text.txt";
    std::ofstream ofs(fn, std::ios_base::app);
    if (ofs.good())
        ofs << s;
}

void WPEngine::drop_new_words(std::vector<std::vector<UString> >& token_sections,
                              const std::vector<UString>& tokens,
                              const std::vector<Span>& spans,
                              const LMIDs& lmids)
{
    std::vector<USpan> tokspans;
    std::vector<std::vector<int>> counts;
    lookup_tokens(tokspans, counts,
                  tokens, spans, lmids);

    std::vector<size_t> split_indices;
    for (size_t i=0; i<counts.size(); i++)
        if (std::all_of(counts[i].begin(), counts[i].end(),
                        [](lm::CountType n) {return n != 1;}))
            split_indices.emplace_back(i);

    return lm::split_tokens_at(token_sections,
                               tokens, split_indices);
}

void WPEngine::learn_scratch_text(const UString& text)
{
    std::vector<UString> tokens;
    std::vector<Span> spans;
    lm::tokenize_text(tokens, spans, text);
    const auto& models = m_model_cache->get_models(m_scratch_models);
    for (auto model : models)
    {
        auto dm = dynamic_cast<lm::DynamicModelBase*>(model);
        if (dm)
        {
            LOG_DEBUG << "scratch learn " << model << " " << tokens;
            dm->learn_tokens(tokens, true);
        }
    }
}

void WPEngine::clear_scratch_models()
{
    const auto& models = m_model_cache->get_models(m_scratch_models);
    for (auto model : models)
        model->clear();
}

void WPEngine::lookup_text(std::vector<USpan>& tokspans_out,
                           std::vector<std::vector<int> >& counts_out,
                           const UString& text, const LMIDs& lmids)
{
    std::vector<UString> tokens;
    std::vector<Span> spans;
    lm::tokenize_sentence(tokens, spans, text);
    lookup_tokens(tokspans_out, counts_out, tokens, spans, lmids);
}

void WPEngine::lookup_tokens(std::vector<USpan>& tokspans_out,
                             std::vector<std::vector<int> >& counts_out,
                             const std::vector<UString>& tokens,
                             const std::vector<Span>& spans,
                             const LMIDs& lmids)
{
    tokspans_out.clear();
    for (size_t i=0; i<tokens.size(); i++)
        tokspans_out.emplace_back(spans[i].begin, spans[i].length, tokens[i]);

    counts_out = {tokspans_out.size(), std::vector<int>(lmids.size(), 0)};

    for (size_t i=0; i<lmids.size(); i++)
    {
        auto model = m_model_cache->get_model(lmids[i]);
        if (model)
        {
            for (size_t j=0; j<tokspans_out.size(); j++)
            {
                const auto& t = tokspans_out[j];
                counts_out[j][i] = model->lookup_word(t.text);
            }
        }
    }

    LOG_DEBUG << "tokens=" << tokspans_out
              << " counts=" << counts_out;
}

bool WPEngine::word_exists(const UString& word)
{
    bool exists = false;
    const auto& lmids = m_persistent_models;
    for (size_t i=0; i<lmids.size(); i++)
    {
        auto model = m_model_cache->get_model(lmids[i]);
        if (model)
        {
            int count = model->lookup_word(word);
            if (count > 0)
            {
                exists = true;
                break;
            }
        }
    }
    return exists;
}

void WPEngine::tokenize_text(std::vector<UString>& tokens,
                             std::vector<Span>& spans,
                             const UString& text)
{
    lm::tokenize_text(tokens, spans, text);
}

void WPEngine::tokenize_context(std::vector<UString>& tokens, std::vector<Span>& spans, const UString& text)
{
    lm::tokenize_context(tokens, spans, text);
}

void WPEngine::get_model_names(std::vector<std::string>& names,
                               const std::string& _class)
{
    m_model_cache->find_available_model_names(names, _class);
}

UString WPEngine::get_last_context_fragment(const UString& text)
{
    auto text_ = text.slice(-1024);
    std::vector<UString> tokens;
    std::vector<Span> spans;
    tokenize_context(tokens, spans, text_);
    if (!spans.empty())
    {
        // Don't return the token itself as it won't include
        // trailing dashes. Catch the text until its very end.
        TextPos begin = spans.back().begin;
        return text.slice(begin);
    }
    else
    {
        return {};
    }
}

void WPEngine::get_prediction(std::vector<lm::UPredictResult>& predictions,
                              const LMDESCRs& lmdescrs,
                              const std::vector<UString>& context,
                              std::optional<size_t> limit, lm::PredictOptions options)
{
    LMIDs lmids;
    std::vector<double> weights;
    m_model_cache->parse_lmdesc(lmids, weights, lmdescrs);
    const auto& models = m_model_cache->get_models(lmids);

    for (auto model : models)
    {
        auto dm = dynamic_cast<lm::DynamicModelBase*>(model);
        if (dm)
        {
            // Kneser-ney perfomes best in entropy and ksr measures, but
            // is too unpredictable in practice for anything but natural
            // language, e.g. shell commands.
            // -> use the second best available: absolute discounting
            // model->set_smoothing(lm::Smoothing::KNESER_NEY_I);
            dm->set_smoothing(lm::Smoothing::ABS_DISC_I);
        }

        // setup recency caching
        auto cdm = dynamic_cast<lm::CachedDynamicModel*>(model);
        if (cdm)
        {
            // Values found with
            // $ pypredict/optimize caching models/en.lm learned_text.txt
            // based on multilingual text actually typed (--log-learning)
            // with onboard over ~3 months.
            // How valid those settings are under different conditions
            // remains to be seen, but for now this is the best I have.
            cdm->set_recency_ratio(0.811);
            cdm->set_recency_halflife(96);
            cdm->set_recency_smoothing(lm::Smoothing::JELINEK_MERCER_I);
            cdm->set_recency_lambdas({0.404, 0.831, 0.444});
        }
    }

    lm::OverlayModel model;
    model.set_models(models);
    // model = pypredict.linint(models, weights)
    // model = pypredict.loglinint(models, weights)

    model.predict(predictions, context, limit, options);
}

void WPEngine::remove_context(const std::vector<UString>& context)
{
    LMIDs lmids;
    std::vector<double> weights;
    m_model_cache->parse_lmdesc(lmids, weights, m_auto_learn_models);
    const auto& models = m_model_cache->get_models(lmids);

    for (size_t i=0; i<models.size(); i++)
    {
        auto model = dynamic_cast<lm::DynamicModelBase*>(models[i]);
        if (model)
        {
            std::map<std::vector<std::string>, int> changes;
            model->remove_context(changes, context);

            // debug output
            if (logger()->can_log(LogLevel::DEBUG))
            {
                LOG_DEBUG << "removing " << context
                          << " from " << repr(lmids[i])
                          << ": " << changes.size() << " n-grams affected";

                using Element = std::pair<UString, int>;
                std::vector<Element> v;
                std::sort(v.begin(), v.end(),
                          [](const Element& a, const Element& b)
                          {  return a.first.size() < b.first.size(); });
                for (const auto& e : v)
                {
                    LOG_DEBUG << "    remove: " << repr(e.first)
                              << ", count=" << e.second;
                }
            }
        }
    }
}

