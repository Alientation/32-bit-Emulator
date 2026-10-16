#pragma once

#include "util/types.h"

#include <string>
#include <vector>

/// Where the sound of the audio device goes (docs/devices.md): a recording, the speakers of the
/// host. The device calls it on the thread of the emulator, when a period starts, with the samples
/// of that period.
class AudioSink
{
  public:
    virtual ~AudioSink() = default;

    /// @param[in] rate The samples a second.
    /// @param[in] samples The samples, two for each frame: left, then right.
    /// @param[in] frames The number of frames.
    virtual void play(word rate, const S16 *samples, size_t frames) = 0;
};

/// Keeps all the sound, for `emu32 --audio-dump`, and passes it on to another sink if there is
/// one.
class AudioRecorder : public AudioSink
{
  public:
    /// @param[in] next Where the sound also goes, which is not owned, or null.
    explicit AudioRecorder(AudioSink *next = nullptr);

    void play(word rate, const S16 *samples, size_t frames) override;

    /// @return The rate of the first period, 0 if nothing was played.
    word rate() const
    {
        return m_rate;
    }

    /// @return The samples so far, two for each frame: left, then right.
    const std::vector<S16> &samples() const
    {
        return m_samples;
    }

    /// Writes the sound as a WAV file: 16 bit stereo at the rate of the first period. A period at
    /// another rate is written as it is, at that rate.
    ///
    /// @param[in] path The file.
    /// @return `false` if the file could not be written.
    bool write_wav(const std::string &path) const;

  private:
    AudioSink *m_next;
    word m_rate = 0;
    std::vector<S16> m_samples;
};
