
#include "tools/logger.h"
#include "tools/string_helpers.h"

#include "exception.h"
#include "audioplayer.h"


AudioBackend::AudioBackend(const ContextBase& context) :
    ContextBase(context)
{}

AudioBackend::~AudioBackend()
{

}

extern std::unique_ptr<AudioBackend> audio_backend_canberra_make(const ContextBase& context);

AudioPlayer::AudioPlayer(const ContextBase& context, std::unique_ptr<AudioBackend>& backend) :
    ContextBase(context),
    m_backend(std::move(backend))
{
}

AudioPlayer::~AudioPlayer()
{
}

std::unique_ptr<AudioPlayer> AudioPlayer::make_ap_canberra(const ContextBase& context)
{
    std::unique_ptr<AudioBackend> backend;
    try
    {
        backend = audio_backend_canberra_make(context);
        backend->enable(true);
        backend->cache_sample(key_feedback);
    }
    catch (const AudioException& ex)
    {
        throw AudioException(sstr()
            << "failed to create audio backend: " << ex.what());
    }

    return std::make_unique<AudioPlayer>(context, backend);
}

bool AudioPlayer::is_valid()
{
    return m_backend && m_backend->is_valid();
}

void AudioPlayer::enable(bool enable)
{
    try
    {
        if (is_valid())
            m_backend->enable(enable);
    }
    catch (const AudioException& ex)
    {
        if (enable)
            LOG_WARNING << "failed to enable audio backend: " << ex.what();
        else
            LOG_WARNING << "failed to disable audio backend: " << ex.what();
    }
}

void AudioPlayer::set_theme(const std::string& theme_name)
{
    try
    {
        if (is_valid())
            m_backend->set_theme(theme_name);
    }
    catch (const AudioException& ex)
    {
        LOG_WARNING << "failed to set sound theme: " << ex.what();
    }
}

void AudioPlayer::play(const std::string& event_id, const Point& accessibility_point, const Point& space_point)
{
    try
    {
        if (is_valid())
            m_backend->play(event_id, accessibility_point, space_point);
    }
    catch (const AudioException& ex)
    {
        LOG_WARNING << "failed to play sound: '" << event_id << "'" << ex.what();
    }
}

void AudioPlayer::cancel()
{
    try
    {
        if (is_valid())
            m_backend->cancel();
    }
    catch (const AudioException& ex)
    {
        LOG_WARNING << "failed to cancel sound: " << ex.what();
    }
}

void AudioPlayer::cache_sample(const std::string& event_id)
{
    try
    {
        if (is_valid())
            m_backend->cache_sample(event_id);
    }
    catch (const AudioException& ex)
    {
        LOG_WARNING << "failed to cache sample sound '" << event_id << "': " << ex.what();
    }
}
