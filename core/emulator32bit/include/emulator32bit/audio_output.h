#pragma once

#include "emulator32bit/host_audio.h"

#include <string>
#include <vector>

struct _SDL_AudioStream;

/// The sound of the audio device on the speakers of the host (`emu32 --audio`, docs/devices.md),
/// through SDL. It is only built with the CMake option AEMU_GUI. The device of SDL plays
/// kOutputRate frames a second of 16 bit stereo, and a period at another rate is converted to it.
class AudioOutput : public AudioSink
{
  public:
    /// The rate the speakers are opened with.
    static constexpr word kOutputRate = 48000;

    /// @param[in] max_buffered_ms The most sound, in milliseconds, that waits to be heard: play
    ///                            waits while there is more, so that a machine that makes sound
    ///                            runs at the speed of its sound.
    explicit AudioOutput(word max_buffered_ms);

    ~AudioOutput() override;

    AudioOutput(const AudioOutput &) = delete;
    AudioOutput &operator=(const AudioOutput &) = delete;

    /// Opens the speakers.
    ///
    /// @param[out] error Why they could not be opened.
    /// @return Whether they are open.
    bool open(std::string &error);

    /// Queues the sound, and waits while more than the most is queued.
    void play(word rate, const S16 *samples, size_t frames) override;

    /// Waits until what is queued has been heard (at most kMaxDrainMs), for the end of a run.
    void drain();

  private:
    /// The longest drain waits, in milliseconds.
    static constexpr U32 kMaxDrainMs = 2000;

    word m_max_buffered_ms;
    bool m_initialized = false;
    U32 m_device = 0;                     /// SDL_AudioDeviceID, 0 when not open.
    _SDL_AudioStream *m_stream = nullptr; /// Converts from m_stream_rate.
    word m_stream_rate = 0;
    std::vector<S16> m_converted;
};
