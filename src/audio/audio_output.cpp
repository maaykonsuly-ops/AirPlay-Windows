#include "audio/audio_output.h"
#include "log.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <vector>
#include <atomic>
#include <mutex>

namespace ap::audio {

namespace {
std::atomic<uint64_t> g_device_generation{0};
std::mutex g_device_name_mutex;
std::string g_preferred_device;
std::atomic<int> g_delay_ms{0};
}

void notify_audio_device_change() {
    g_device_generation.fetch_add(1, std::memory_order_relaxed);
}

std::vector<std::string> audio_output_device_names() {
    std::vector<std::string> names{"Padrão do sistema"};
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return names;
    const int count = SDL_GetNumAudioDevices(0);
    for (int i = 0; i < count; ++i) {
        const char* name = SDL_GetAudioDeviceName(i, 0);
        if (name && *name) names.emplace_back(name);
    }
    return names;
}

std::string preferred_audio_output_device() {
    std::lock_guard<std::mutex> lock(g_device_name_mutex);
    return g_preferred_device;
}

void set_preferred_audio_output_device(const std::string& name) {
    {
        std::lock_guard<std::mutex> lock(g_device_name_mutex);
        if (g_preferred_device == name) return;
        g_preferred_device = name;
    }
    notify_audio_device_change();
}

int audio_output_delay_ms() { return g_delay_ms.load(std::memory_order_relaxed); }

void set_audio_output_delay_ms(int delay_ms) {
    delay_ms = std::clamp(delay_ms, -500, 500);
    if (g_delay_ms.exchange(delay_ms, std::memory_order_relaxed) != delay_ms)
        notify_audio_device_change();
}

SdlAudioOutput::SdlAudioOutput()  = default;
SdlAudioOutput::~SdlAudioOutput() { stop(); }

bool SdlAudioOutput::start(int sample_rate, int channels) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        LOG_ERROR << "SDL_InitSubSystem(AUDIO) failed: " << SDL_GetError();
        return false;
    }

    SDL_AudioSpec want{};
    want.freq     = sample_rate;
    want.format   = AUDIO_S16SYS;     // interleaved int16, host byte order
    want.channels = static_cast<Uint8>(channels);
    want.samples  = 1024;             // buffer granularity (~23 ms @ 44.1 kHz)
    want.callback = nullptr;          // push mode via SDL_QueueAudio

    SDL_AudioSpec got{};
    // The producer queues PCM in the decoder's format. Let SDL perform any
    // backend conversion internally instead of changing the queue format
    // behind the producer's back (which changes playback rate/layout).
    const std::string preferred = preferred_audio_output_device();
    const char* device_name = preferred.empty() ? nullptr : preferred.c_str();
    device_ = SDL_OpenAudioDevice(device_name, 0, &want, &got, 0);
    if (!device_ && device_name) {
        LOG_WARN << "Selected audio device unavailable; using system default: "
                 << SDL_GetError();
        device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
    }
    if (device_ == 0) {
        LOG_ERROR << "SDL_OpenAudioDevice failed: " << SDL_GetError();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    sample_rate_ = got.freq;
    channels_    = got.channels;
    device_generation_ = g_device_generation.load(std::memory_order_relaxed);
    initial_delay_applied_ = false;
    initial_samples_to_skip_ = audio_output_delay_ms() < 0
        ? (sample_rate_ * -audio_output_delay_ms() / 1000) * channels_ : 0;
    SDL_PauseAudioDevice(device_, 0);
    LOG_INFO << "SdlAudioOutput playing at " << sample_rate_ << " Hz x "
             << channels_ << "ch (format int16)";
    return true;
}

void SdlAudioOutput::stop() {
    if (device_) {
        SDL_PauseAudioDevice(device_, 1);
        SDL_ClearQueuedAudio(device_);
        SDL_CloseAudioDevice(device_);
        device_ = 0;
        // Deliberately NOT calling SDL_QuitSubSystem(SDL_INIT_AUDIO):
        // stop() runs on the RTSP server thread during session
        // teardown, and on Windows shutting down the SDL audio
        // subsystem cross-thread can knock out the video event pump
        // (spontaneous SDL_QUIT / window close). SDL_Quit() in the
        // renderer's shutdown path will clean up the subsystem when
        // the app actually exits.
    }
}

void SdlAudioOutput::push(const int16_t* samples, int count) {
    if (!samples || count <= 0) return;

    // HDMI/DisplayPort audio endpoints may appear only after monitor hot-plug.
    // Reopen SDL's default output on the next packet so the stream follows the
    // endpoint Windows selected after the display change.
    const auto generation = g_device_generation.load(std::memory_order_relaxed);
    if (generation != device_generation_) {
        stop();
        if (!start(sample_rate_, channels_)) return;
    }
    if (!device_) return;

    if (!initial_delay_applied_) {
        initial_delay_applied_ = true;
        const int delay_ms = audio_output_delay_ms();
        const int delay_frames = (sample_rate_ * std::abs(delay_ms)) / 1000;
        if (delay_ms > 0 && delay_frames > 0) {
            std::vector<int16_t> silence(
                static_cast<std::size_t>(delay_frames) * channels_, 0);
            SDL_QueueAudio(device_, silence.data(),
                static_cast<Uint32>(silence.size() * sizeof(int16_t)));
        }
    }
    if (initial_samples_to_skip_ > 0) {
            const int skip = std::min(count, initial_samples_to_skip_);
            samples += skip;
            count -= skip;
            initial_samples_to_skip_ -= skip;
            if (count <= 0) return;
    }

    const float gain = gain_.load(std::memory_order_relaxed);

    // Fast path: unity gain → send as-is.
    if (gain >= 0.9999f && gain <= 1.0001f) {
        const uint32_t bytes = static_cast<uint32_t>(count) * sizeof(int16_t);
        if (SDL_QueueAudio(device_, samples, bytes) != 0) {
            LOG_WARN << "SDL_QueueAudio failed: " << SDL_GetError();
        }
        return;
    }

    // Apply gain into a scratch buffer, saturate to int16 range.
    static thread_local std::vector<int16_t> scratch;
    scratch.resize(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        float s = static_cast<float>(samples[i]) * gain;
        if (s >  32767.f) s =  32767.f;
        if (s < -32768.f) s = -32768.f;
        scratch[i] = static_cast<int16_t>(s);
    }
    const uint32_t bytes = static_cast<uint32_t>(count) * sizeof(int16_t);
    if (SDL_QueueAudio(device_, scratch.data(), bytes) != 0) {
        LOG_WARN << "SDL_QueueAudio failed: " << SDL_GetError();
    }
}

void SdlAudioOutput::set_volume_db(float db) {
    // AirPlay: 0 dB = full, -144 dB = mute. Anything below -100 treat as 0.
    float lin = (db <= -100.f) ? 0.f
              : (db >=    0.f) ? 1.f
              : std::pow(10.f, db / 20.f);
    gain_.store(lin, std::memory_order_relaxed);
    LOG_INFO << "audio volume: " << db << " dB (gain=" << lin << ')';
}

uint32_t SdlAudioOutput::queued_bytes() const {
    return device_ ? SDL_GetQueuedAudioSize(device_) : 0u;
}

void SdlAudioOutput::clear() {
    if (device_) SDL_ClearQueuedAudio(device_);
    initial_delay_applied_ = false;
}

} // namespace ap::audio
