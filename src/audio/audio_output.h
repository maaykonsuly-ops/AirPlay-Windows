#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace ap::audio {

// Called by the SDL event loop when Windows reports an output device change.
void notify_audio_device_change();
std::vector<std::string> audio_output_device_names();
std::string preferred_audio_output_device();
void set_preferred_audio_output_device(const std::string& name);
int audio_output_delay_ms();
void set_audio_output_delay_ms(int delay_ms);

// Push-mode PCM sink backed by SDL2 Audio (WASAPI on Windows, ALSA /
// PulseAudio on Linux). The producer calls push() with interleaved int16
// samples; SDL handles pacing and delivers them to the OS mixer.
//
// We don't use SDL's callback pull mode because the producer cadence
// (decoder output) is already naturally paced by the network, and the
// queue lets us absorb short bursts cleanly without locking the
// decoder thread on audio hardware.
class SdlAudioOutput {
public:
    SdlAudioOutput();
    ~SdlAudioOutput();

    SdlAudioOutput(const SdlAudioOutput&)            = delete;
    SdlAudioOutput& operator=(const SdlAudioOutput&) = delete;

    bool start(int sample_rate, int channels);
    void stop();

    // Enqueue interleaved int16 samples. `count` is the total number of
    // int16 samples (NOT frames) — for stereo, that's 2 * frame_count.
    // Non-blocking. The current volume gain is applied in-place to an
    // internal scratch buffer before SDL_QueueAudio.
    void push(const int16_t* samples, int count);

    // Set playback gain from a dB value in AirPlay's convention:
    //   0 dB   = full volume
    //   -144 dB or below = mute
    // Internally converted to a linear multiplier. Thread-safe.
    void set_volume_db(float db);

    // Bytes currently queued (not yet played). Useful for back-pressure.
    uint32_t queued_bytes() const;
    void clear();

private:
    unsigned int       device_{0};   // SDL_AudioDeviceID is uint32_t
    int                channels_{0};
    int                sample_rate_{0};
    uint64_t           device_generation_{0};
    bool               initial_delay_applied_{false};
    int                initial_samples_to_skip_{0};
    std::atomic<float> gain_{1.0f};  // linear multiplier, 0.0 = muted
};

} // namespace ap::audio
