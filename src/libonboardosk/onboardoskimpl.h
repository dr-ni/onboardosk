#ifndef ONBOARDOSKIMPL_H
#define ONBOARDOSKIMPL_H

#include "tools/loggerdecls.h"
#include "tools/rect_fwd.h"

#include "callonce.h"
#include "onboardosk.h"
#include "onboardoskglobals.h"
#include "signalling.h"


class OnboardOskGlobals;
class ViewBase;

class OnboardOskImpl : public OnboardOsk,
                       public ContextBase
{
    public:
        using Super = ContextBase;
        using This = OnboardOskImpl;

        OnboardOskImpl();
        virtual ~OnboardOskImpl();

        // init everything
        virtual bool startup(const std::vector<std::string>& args={}) override;

        // init bare essentials (for unit testing, mainly)
        bool init_test(const std::vector<std::string>& args={},
                       LogLevel log_level=LogLevel::WARNING)
        {
            return init_essentials(args, log_level, true);
        }

        bool init_essentials(const std::vector<std::string>& args={},
                             LogLevel log_level=LogLevel::WARNING,
                             bool test_mode=false);

        virtual void toggle_visible() override;
        virtual void show() override;
        virtual void hide() override;
        virtual bool is_visible() override;

        virtual void on_keymap_changed() override;
        virtual void on_group_changed() override;

        virtual std::vector<ViewBase*>& get_toplevel_views() override;

        virtual void draw(ViewBase* view, DrawingContext& dc) override;

        virtual void on_toplevel_view_geometry_changed(ViewBase* view) override;
        virtual void on_event(ViewBase* view, OnboardOskEvent* event) override;

        virtual std::string get_language_full_name(const std::string& lang_id) override;
        virtual void set_active_language_id(const std::string& lang_id, bool add_to_mru=false) override;
        virtual void on_language_selection_closed() override;

    private:
        void reload_layout(bool force_load=false);
        bool load_layout(const std::string& layout_filename);

        // determine which views are toplevels
        void update_view_graph();

        void add_toplevel_keyboard_views();
        void notify_toplevels_added();
        void notify_toplevels_remove();

        void update_input_event_source();

    private:
        std::unique_ptr<OnboardOskGlobals> m_globals_ptr;
        LayoutItemPtr m_layout;

        using KeyboardState = std::tuple<std::string, std::string>;
        KeyboardState m_last_keyboard_state;

        std::unique_ptr<CallOnce<>> m_call_reload_layout;

        SignalConnections m_connections;
};


#endif // ONBOARDOSKIMPL_H
