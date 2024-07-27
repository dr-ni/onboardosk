#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include <lm.h>

#include "tools/logger.h"
#include "tools/string_helpers.h"

#include "keyboard.h"
#include "onboardoskcallbacks.h"
#include "wperrorrecovery.h"


WPErrorRecovery::WPErrorRecovery(const ContextBase& context) :
    Super(context)
{
}

WPErrorRecovery::~WPErrorRecovery()
{
}

ModelCache*WPErrorRecovery::get_model_cache()
{
    if (auto wpengine = get_wp_engine())
        return wpengine->get_model_cache();
    return {};
}

void WPErrorRecovery::report_errors()
{
    auto wpengine = get_wp_engine();
    auto cache = get_model_cache();
    if (!wpengine)
        return;

    bool retry = false;
    for (auto& lmid : wpengine->get_models_descriptions())
    {
        if (cache->is_user_lmid(lmid))
        {
            auto model = cache->get_model(lmid);
            std::string filename = cache->get_filename(lmid);
            if (model->get_load_error())
            {
                retry = retry ||
                        report_load_error(filename, model->get_load_error_msg());
            }
        }
    }

    // clear the model cache and have models reloaded lazily
    if (retry)
        cache->clear();
}

bool WPErrorRecovery::show_dialog(const std::string& markup,
                                  const std::string& message_type,
                                  const std::string& buttons)
{
    bool ret = false;
    auto callbacks = get_global_callbacks();
    if (callbacks->show_dialog_wp_error_recovery)
    {
        auto keyboard = get_keyboard();
        keyboard->on_focusable_gui_opening();

        ret = callbacks->show_dialog_wp_error_recovery(get_cinstance(),
                                                       markup.c_str(),
                                                       message_type.c_str(),
                                                       buttons.c_str()) != 0;
        keyboard->on_focusable_gui_closed();
    }
    return ret;
}

bool WPErrorRecovery::report_load_error(const std::string& filename,
                                        const std::string& msg, bool test_mode_)
{
    bool retry = false;
    auto backup_filename = get_backup_filename(filename);
    auto broken_filename = get_broken_filename(filename);

    if (fs::exists(backup_filename))
    {
        std::string markup = sstr()
            << "<big>Onboard failed to open learned "
            << "word suggestions.</big>\n\n"
            << "The error was:\n<tt>" << msg << "</tt>\n\n"
            << "Revert word suggestions to the last backup (recommended)?";
        markup = escape_markup(markup, true);
        bool reply = test_mode_ ? true : show_dialog(markup, "yes_no");

        if (reply)
        {
            try
            {
                fs::rename(filename, broken_filename);
                fs::copy(backup_filename, filename);
            }
            catch (const fs::filesystem_error&)
            {
                LOG_ERROR
                        << "failed to revert to backup model: "
                        << repr(filename)
                        << ": " << strerror(errno)
                        << " (" << errno << ")";
                if (test_mode_)
                    throw;
            }

            retry = true;
        }
    }
    else
    {
        std::string markup = sstr()
            << "<big>Onboard failed to open learned "
            << "word suggestions.</big>\n\n"
            << "The error was:\n<tt>" << msg << "</tt>\n\n"
            << "The suggestions have to be reset, but "
            << "the erroneous file will remain at \n'"
            << broken_filename << "'";
        markup = escape_markup(markup, true);
        if (!test_mode_)
        {
            show_dialog(markup);
        }

        try
        {
            fs::rename(filename, broken_filename);
        }
        catch (const fs::filesystem_error&)
        {
            LOG_ERROR
                    << "failed to rename broken model: "
                    << repr(filename)
                    << " -> " << broken_filename
                    << ": " << strerror(errno)
                    << " (" << errno << ")";
            if (test_mode_)
                throw;
        }
    }
    return retry;
}
