#include "emulator32bit/host_audio.h"

#include <fstream>

AudioRecorder::AudioRecorder(AudioSink *next) :
    m_next(next)
{
}

void AudioRecorder::play(const word rate, const S16 *samples, const size_t frames)
{
    if (m_rate == 0) m_rate = rate;
    m_samples.insert(m_samples.end(), samples, samples + 2 * frames);
    if (m_next != nullptr) m_next->play(rate, samples, frames);
}

/// Writes a number in little endian order.
///
/// @param[out] out The file.
/// @param[in] value The number.
/// @param[in] bytes Its size in the file.
static void put_le(std::ostream &out, const U32 value, const size_t bytes)
{
    for (size_t i = 0; i < bytes; i++) out.put(char((value >> (8 * i)) & 0xFF));
}

bool AudioRecorder::write_wav(const std::string &path) const
{
    // A RIFF file with a "fmt " chunk for PCM and a "data" chunk (the canonical WAV header).
    constexpr U32 kChannels = 2;
    constexpr U32 kBitsPerSample = 16;
    constexpr U32 kBlockAlign = kChannels * kBitsPerSample / 8;
    constexpr U32 kFmtSize = 16;
    constexpr U32 kFormatPcm = 1;
    constexpr U32 kHeaderAfterRiff = 4 + (8 + kFmtSize) + 8; // "WAVE", the fmt chunk, data's head.

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    const U32 data_size = U32(m_samples.size() * sizeof(S16));
    out.write("RIFF", 4);
    put_le(out, kHeaderAfterRiff + data_size, sizeof(U32));
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    put_le(out, kFmtSize, sizeof(U32));
    put_le(out, kFormatPcm, sizeof(U16));
    put_le(out, kChannels, sizeof(U16));
    put_le(out, m_rate, sizeof(U32));
    put_le(out, m_rate * kBlockAlign, sizeof(U32));
    put_le(out, kBlockAlign, sizeof(U16));
    put_le(out, kBitsPerSample, sizeof(U16));
    out.write("data", 4);
    put_le(out, data_size, sizeof(U32));
    for (const S16 sample : m_samples) put_le(out, U16(sample), sizeof(U16));
    return bool(out);
}
