#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <string>

#include "tools/point_fwd.h"
#include "onboardoskglobals.h"


class AudioBackend;

class AudioPlayer : public ContextBase
{
    public:
        static constexpr const char* key_feedback = "onboard-key-feedback";

        AudioPlayer(const ContextBase& context, std::unique_ptr<AudioBackend>& backend);
        virtual ~AudioPlayer();

        static std::unique_ptr<AudioPlayer> make_ap_canberra(const ContextBase& context);

        // Check initialization of the audio backend
        bool is_valid();

        // Enable canberra audio output
        void enable(bool enable);

        // Set the XDG sound theme
        void set_theme(const std::string& theme_name);


        // Play a sound
        void play(const std::string& event_id,
                  const Point& accessibility_point, const Point& space_point);

        // Cancel all playing sounds
        void cancel();

        // Upload sample to the sound server. Blocking call.
        void cache_sample(const std::string& event_id);

    private:
        std::unique_ptr<AudioBackend> m_backend;
};


class AudioBackend : public ContextBase
{
    public:
        AudioBackend(const ContextBase& context);
        virtual ~AudioBackend();

        virtual bool is_valid() = 0;

        virtual void enable(bool enable) = 0;
        virtual void set_theme(const std::string& name) = 0;

        virtual void play(const std::string& event_id, const Point& accessibility_point, const Point& space_point) = 0;
        virtual void cancel() = 0;

        virtual void cache_sample(const std::string& event_id) = 0;
};

#endif // AUDIOPLAYER_H
