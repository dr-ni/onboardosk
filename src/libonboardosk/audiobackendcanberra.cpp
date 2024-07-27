/*
 * Copyright © 2013 Gerd Kohlberger <lowfi@chello.at>
 * Copyright © 2013, 2016-2017 marmuta <marmvta@gmail.com>
 *
 * This file is part of Onboard.
 *
 * Onboard is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Onboard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <string>
#include <memory>

//#include <gdk/gdkx.h>
#include <canberra.h>

#include "tools/point.h"
#include "tools/string_helpers.h"

#include "audioplayer.h"
#include "exception.h"
#include "onboardoskcallbacks.h"


#define DEFAULT_SOUND_ID 0

class AudioBackendCanberra : public AudioBackend
{
    public:
        using Super = AudioBackend;
        AudioBackendCanberra(const ContextBase& context) :
            Super(context)
        {
            auto ret = ca_context_create(&m_ca);
            if (ret != CA_SUCCESS)
                throw AudioException(sstr()
                    << "ca_context_create failed (" << ret << ")");

            //auto screen = gdk_screen_get_default();
            //int nr = gdk_screen_get_number(screen);
            //const char* name = gdk_display_get_name(gdk_screen_get_display(screen));

            /* Set default application properties */
            ca_proplist* props;
            ca_proplist_create(&props);
            ca_proplist_sets(props, CA_PROP_APPLICATION_NAME, "Onboard");
            ca_proplist_sets(props, CA_PROP_APPLICATION_ID, "org.onboard.Onboard");
            ca_proplist_sets(props, CA_PROP_APPLICATION_ICON_NAME, "onboard");
            //ca_proplist_sets(props, CA_PROP_WINDOW_X11_DISPLAY, name);
            //ca_proplist_setf(props, CA_PROP_WINDOW_X11_SCREEN, "%i", nr);
            ca_context_change_props_full(m_ca, props);
            ca_proplist_destroy(props);

        }
        virtual ~AudioBackendCanberra()
        {
            if (m_ca)
                ca_context_destroy(m_ca);
        }

        virtual bool is_valid() override
        {
            return m_ca != nullptr;
        }

        virtual void enable(bool enable) override
        {
            if (enable)
                ca_context_change_props(m_ca, CA_PROP_CANBERRA_ENABLE, "1", NULL);
            else
                ca_context_change_props(m_ca, CA_PROP_CANBERRA_ENABLE, "0", NULL);
        }

        virtual void set_theme(const std::string& theme_name) override
        {
            int ret = ca_context_change_props(m_ca,
                CA_PROP_CANBERRA_XDG_THEME_NAME, theme_name.c_str(),
                NULL);
            if (ret < 0)
                throw AudioException(sstr()
                    << "ca_context_change_props failed (" << ret << ")");
        }

        virtual void play(const std::string& event_id, const Point& accessibility_point, const Point& space_point) override
        {
            double x = accessibility_point.x;
            double y = accessibility_point.y;
            double xs = space_point.x;
            double ys = space_point.y;

            int sw = 0;
            int sh = 0;
            auto callbacks = get_global_callbacks();
            if (callbacks->get_screen_geometry)
            {
                double x_, y_, w_, h_;
                callbacks->get_screen_geometry(get_cinstance(), &x_, &y_, &w_, &h_);
                sw = static_cast<int>(w_);
                sw = static_cast<int>(h_);
            }
            ca_proplist* props;
            ca_proplist_create(&props);
            ca_proplist_sets(props, CA_PROP_EVENT_ID, event_id.c_str());

            /* report mouse position for accessibility */
            if (x != -1.0 && y != -1.0)
            {
                ca_proplist_setf(props, CA_PROP_EVENT_MOUSE_X, "%0.0f", x);
                ca_proplist_setf(props, CA_PROP_EVENT_MOUSE_Y, "%0.0f", y);
            }

            /* place in space between speakers */
            if (xs != -1.0 && ys != -1.0)
            {
                /* comment from canberra-gtk.c:
                 * We use these strange format strings here to avoid that libc
                 * applies locale information on the formatting of floating numbers. */
                ca_proplist_setf(props, CA_PROP_EVENT_MOUSE_HPOS, "%i.%03i",
                                 (int) x / (sw - 1), (int) (1000.0 * x / (sw - 1)) % 1000);
                ca_proplist_setf(props, CA_PROP_EVENT_MOUSE_VPOS, "%i.%03i",
                                 (int) y / (sh - 1), (int) (1000.0 * y / (sh - 1)) % 1000);
            }

            int ret = ca_context_play_full(m_ca, DEFAULT_SOUND_ID, props, NULL, NULL);

            ca_proplist_destroy(props);

            if (ret < 0)
                throw AudioException(sstr()
                    << "ca_context_play failed (" << ret << ")");
        }

        virtual void cancel() override
        {
            int ret = ca_context_cancel(m_ca, DEFAULT_SOUND_ID);
            if (ret < 0)
                throw AudioException(sstr()
                    << "ca_context_cancel failed (" << ret << ")");
        }

        virtual void cache_sample(const std::string& event_id) override
        {
            ca_proplist* props;
            ca_proplist_create(&props);
            ca_proplist_sets(props, CA_PROP_EVENT_ID, event_id.c_str());
            int ret = ca_context_cache_full(m_ca, props);
            ca_proplist_destroy(props);

            if (ret < 0)
                throw AudioException(sstr()
                    << "ca_context_cache_full failed (" << ret << ")");
        }

    private:
        ca_context* m_ca{};
};


std::unique_ptr<AudioBackend> audio_backend_canberra_make(const ContextBase& context)
{
    return std::make_unique<AudioBackendCanberra>(context);
}
