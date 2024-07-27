#ifndef WPERRORRECOVERY_H
#define WPERRORRECOVERY_H

#include "onboardoskglobals.h"
#include "wpengine.h"


// Notify of and recover from errors when loading user models.
class WPErrorRecovery : public ContextBase
{
    public:
        using Super = ContextBase;
        WPErrorRecovery(const ContextBase& context);
        ~WPErrorRecovery();

        ModelCache* get_model_cache();

        void report_errors();

        // Handle model load failure.
        bool report_load_error(const std::string& filename,
                               const std::string& msg, bool test_mode_=false);

        bool show_dialog(const std::string& markup,
                         const std::string& message_type="error",
                         const std::string& buttons="ok");

        std::string get_backup_filename(const std::string& filename)
        {
            return ModelCache::get_backup_filename(filename);
        }

        std::string get_broken_filename(const std::string& filename)
        {
            return ModelCache::get_broken_filename(filename);
        }
};


#endif // WPERRORRECOVERY_H
