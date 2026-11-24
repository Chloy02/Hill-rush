#pragma once

#include <raylib.h>

#include <array>
#include <cstddef>
#include <string>

namespace hr {

enum class Sfx {
    Coin,
    Fuel,
    Crash,
    Land,
    Flip,
    LowFuel,
    Click,
    Buy,
    GameOver,
    Fanfare,
    Denied,
    RollCageHit,
    RollCageBreak,
    LuckyFuel,
    Wheelie,
    Count
};

enum class EngineVoice { Standard, TwoStroke, Diesel, Race, Electric, Rally };

EngineVoice engineVoiceFor(const std::string& vehicleId);

// All sound is synthesised at start-up: effects into memory buffers, music
// into a looping track, and the engine live in an audio-thread callback.
class Audio {
public:
    Audio() = default;
    ~Audio() { shutdown(); }
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    bool init(bool muted);
    void shutdown();

    void setVolumes(float master, float sfx, float music);
    void play(Sfx sfx, float pitch = 1.0f, float volume = 1.0f);

    // Engine sound parameters (smoothed on the audio thread).
    void setEngine(bool running, float rpm, float throttle, EngineVoice voice = EngineVoice::Standard,
                   float boost = 0.0f, bool thrusting = false);

    void setMusicOn(bool on);
    void setMusicDuck(float factor);  // in-game the music sits lower
    void update();                    // feeds the music stream; call every frame

    bool ready() const { return ready_; }

private:
    bool ready_ = false;
    bool musicOn_ = false;
    float musicVolume_ = 0.5f;
    float musicDuck_ = 1.0f;
    float sfxVolume_ = 1.0f;
    std::array<Sound, static_cast<std::size_t>(Sfx::Count)> sounds_{};
    std::array<Sound, 4> coinVoices_{};  // aliases so rapid coins overlap
    int nextCoinVoice_ = 0;
    AudioStream engine_{};
    Music music_{};
    unsigned char* musicWav_ = nullptr;
};

// Writes every synthesised sound to WAV files (for offline inspection).
bool exportAudio(const std::string& dir);

} // namespace hr
