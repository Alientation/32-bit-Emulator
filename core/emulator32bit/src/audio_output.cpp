#include "emulator32bit/audio_output.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

/// The bytes of a frame: two 16 bit samples.
static constexpr size_t kFrameBytes = 2 * sizeof(S16);

/// The frames of the buffer of SDL's device, a power of two.
static constexpr Uint16 kDeviceFrames = 1024;

/// How long the waits sleep at a time, in milliseconds.
static constexpr Uint32 kWaitMs = 1;

/// The milliseconds in a second.
static constexpr U64 kMsPerSecond = 1000;

AudioOutput::AudioOutput(const word max_buffered_ms) :
    m_max_buffered_ms(max_buffered_ms)
{
}

AudioOutput::~AudioOutput()
{
    if (m_stream != nullptr) SDL_FreeAudioStream(m_stream);
    if (m_device != 0) SDL_CloseAudioDevice(m_device);
    if (m_initialized) SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool AudioOutput::open(std::string &error)
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    {
        error = SDL_GetError();
        return false;
    }
    m_initialized = true;

    SDL_AudioSpec want{};
    want.freq = int(kOutputRate);
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = kDeviceFrames;
    // No callback: the sound is queued (SDL_QueueAudio), and SDL converts it if the speakers want
    // another format.
    m_device = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (m_device == 0)
    {
        error = SDL_GetError();
        return false;
    }
    SDL_PauseAudioDevice(m_device, 0);
    return true;
}

void AudioOutput::play(const word rate, const S16 *samples, const size_t frames)
{
    if (m_device == 0) return;

    const size_t bytes = frames * kFrameBytes;
    if (rate == kOutputRate)
    {
        SDL_QueueAudio(m_device, samples, Uint32(bytes));
    }
    else
    {
        // A stream of SDL converts from one rate to another; it keeps what it needs of the
        // samples before for the next ones, so it is made again only when the rate changes.
        if (m_stream == nullptr || m_stream_rate != rate)
        {
            if (m_stream != nullptr) SDL_FreeAudioStream(m_stream);
            m_stream =
                SDL_NewAudioStream(AUDIO_S16SYS, 2, int(rate), AUDIO_S16SYS, 2, int(kOutputRate));
            m_stream_rate = rate;
            if (m_stream == nullptr) return;
        }
        SDL_AudioStreamPut(m_stream, samples, int(bytes));
        const int available = SDL_AudioStreamAvailable(m_stream);
        m_converted.resize(size_t(available) / sizeof(S16));
        const int converted = SDL_AudioStreamGet(m_stream, m_converted.data(), available);
        if (converted > 0) SDL_QueueAudio(m_device, m_converted.data(), Uint32(converted));
    }

    const Uint32 max_bytes =
        Uint32(U64(kOutputRate) * kFrameBytes * m_max_buffered_ms / kMsPerSecond);
    while (SDL_GetQueuedAudioSize(m_device) > max_bytes) SDL_Delay(kWaitMs);
}

void AudioOutput::drain()
{
    if (m_device == 0) return;

    const Uint32 start = SDL_GetTicks();
    while (SDL_GetQueuedAudioSize(m_device) != 0 && SDL_GetTicks() - start < kMaxDrainMs)
    {
        SDL_Delay(kWaitMs);
    }
}
