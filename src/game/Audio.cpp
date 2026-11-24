#include "game/Audio.hpp"

#include "core/Log.hpp"
#include "core/Math.hpp"
#include "core/Rng.hpp"

#include <atomic>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <vector>

namespace hr {

namespace {

constexpr int kRate = 44100;

// --- small synthesis toolkit -----------------------------------------------------

float sine(float freq, float t) { return std::sin(kTwoPi * freq * t); }

// Adds `s` into `buf` at time `t` seconds.
void mixAt(std::vector<float>& buf, float t, float s)
{
    const size_t i = static_cast<size_t>(t * kRate);
    if (i < buf.size()) buf[i] += s;
}

float decayEnv(float t, float attack, float decay)
{
    if (t < 0.0f) return 0.0f;
    if (t < attack) return t / attack;
    return std::exp(-(t - attack) / decay);
}

std::vector<float> render(float seconds)
{
    return std::vector<float>(static_cast<size_t>(seconds * kRate), 0.0f);
}

// Scales to the given peak and fades the last few samples to avoid clicks.
std::vector<short> toPcm(const std::vector<float>& samples, float peak)
{
    float maxAbs = 1e-6f;
    for (float s : samples) maxAbs = std::fmax(maxAbs, std::fabs(s));
    std::vector<short> pcm(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        const float fade = std::fmin(1.0f, static_cast<float>(samples.size() - i) / 200.0f);
        pcm[i] = static_cast<short>(clampf(samples[i] / maxAbs * peak * fade, -1.0f, 1.0f) * 32000.0f);
    }
    return pcm;
}

Sound toSound(const std::vector<float>& samples, float peak)
{
    std::vector<short> pcm = toPcm(samples, peak);
    Wave wave{};
    wave.frameCount = static_cast<unsigned int>(pcm.size());
    wave.sampleRate = kRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = pcm.data();
    return LoadSoundFromWave(wave);  // copies the samples
}

// Karplus-Strong plucked string: banjo/guitar-like notes for the music.
void pluck(std::vector<float>& buf, float start, float freq, float seconds, float amp, float brightness, Rng& rng)
{
    const int period = std::max(2, static_cast<int>(std::lround(kRate / freq)));
    std::vector<float> line(static_cast<size_t>(period));
    float prev = 0.0f;
    for (float& v : line) {
        const float noise = rng.range(-1.0f, 1.0f);
        prev = prev + brightness * (noise - prev);  // darker excitation for lower brightness
        v = prev;
    }
    const size_t begin = static_cast<size_t>(start * kRate);
    const size_t length = static_cast<size_t>(seconds * kRate);
    const float decay = 0.9965f + 0.003f * (1.0f - brightness);
    size_t idx = 0;
    for (size_t n = 0; n < length && begin + n < buf.size(); ++n) {
        const size_t next = (idx + 1) % line.size();
        const float out = line[idx];
        line[idx] = 0.5f * (line[idx] + line[next]) * decay;
        idx = next;
        const float release = n + 400 > length ? static_cast<float>(length - n) / 400.0f : 1.0f;
        const float attack = std::fmin(1.0f, static_cast<float>(n) / 44.0f);  // 1 ms: no click at onset
        buf[begin + n] += out * amp * release * attack;
    }
}

float noteFreq(const char* name)
{
    // "C4", "F#5", "Bb3"
    static const int offsets[] = {9, 11, 0, 2, 4, 5, 7};  // A B C D E F G relative to C
    int semis = offsets[name[0] - 'A'];
    int i = 1;
    if (name[i] == '#') {
        semis++;
        i++;
    } else if (name[i] == 'b') {
        semis--;
        i++;
    }
    const int octave = name[i] - '0';
    const int midi = 12 * (octave + 1) + semis;
    return 440.0f * std::pow(2.0f, static_cast<float>(midi - 69) / 12.0f);
}

// --- effects -----------------------------------------------------------------------

std::vector<float> makeCoin()
{
    auto buf = render(0.32f);
    for (size_t i = 0; i < buf.size(); ++i) {
        const float t = static_cast<float>(i) / kRate;
        float s = 0.0f;
        if (t < 0.09f) s += (sine(1318.5f, t) + 0.25f * sine(2637.0f, t)) * decayEnv(t, 0.002f, 0.05f);
        s += (sine(1975.5f, t) + 0.2f * sine(3951.0f, t)) * decayEnv(t - 0.065f, 0.002f, 0.09f);
        buf[i] = s;
    }
    return buf;
}

std::vector<float> makeFuel()
{
    auto buf = render(0.55f);
    float phase = 0.0f;
    for (size_t i = 0; i < buf.size(); ++i) {
        const float t = static_cast<float>(i) / kRate;
        const float f = 260.0f + 900.0f * (t / 0.55f) * (t / 0.55f);
        phase += f / kRate;
        const float trem = 0.65f + 0.35f * std::sin(kTwoPi * 22.0f * t);
        buf[i] = (std::sin(kTwoPi * phase) + 0.3f * std::sin(2.0f * kTwoPi * phase)) * trem * decayEnv(t, 0.01f, 0.35f);
    }
    return buf;
}

std::vector<float> makeCrash()
{
    auto buf = render(0.9f);
    Rng rng(11);
    float lp = 0.0f;
    float phase = 0.0f;
    for (size_t i = 0; i < buf.size(); ++i) {
        const float t = static_cast<float>(i) / kRate;
        lp += 0.18f * (rng.range(-1.0f, 1.0f) - lp);
        phase += (75.0f - 45.0f * t) / kRate;
        const float thump = std::sin(kTwoPi * phase) * std::exp(-t * 9.0f);
        const float clank = (sine(523.0f, t) + 0.7f * sine(787.0f, t) + 0.5f * sine(1131.0f, t)) * std::exp(-t * 11.0f);
        buf[i] = 1.4f * lp * std::exp(-t * 5.0f) + 1.2f * thump + 0.22f * clank;
    }
    return buf;
}

std::vector<float> makeLand()
{
    auto buf = render(0.22f);
    Rng rng(12);
    float lp = 0.0f;
    float phase = 0.0f;
    for (size_t i = 0; i < buf.size(); ++i) {
        const float t = static_cast<float>(i) / kRate;
        lp += 0.1f * (rng.range(-1.0f, 1.0f) - lp);
        phase += (95.0f - 120.0f * t) / kRate;
        buf[i] = std::sin(kTwoPi * phase) * std::exp(-t * 18.0f) + 0.8f * lp * std::exp(-t * 25.0f);
    }
    return buf;
}

std::vector<float> makeArpeggio(std::initializer_list<const char*> notes, float step, float tail, float bright)
{
    auto buf = render(step * static_cast<float>(notes.size()) + tail);
    float start = 0.0f;
    for (const char* n : notes) {
        const float f = noteFreq(n);
        for (size_t i = static_cast<size_t>(start * kRate); i < buf.size(); ++i) {
            const float t = static_cast<float>(i) / kRate - start;
            const float wave = sine(f, t) + bright * 0.33f * sine(3.0f * f, t) + bright * 0.2f * sine(5.0f * f, t);
            buf[i] += wave * decayEnv(t, 0.004f, 0.16f);
        }
        start += step;
    }
    return buf;
}

std::vector<float> makeLowFuel()
{
    auto buf = render(0.42f);
    for (size_t i = 0; i < buf.size(); ++i) {
        const float t = static_cast<float>(i) / kRate;
        const bool on = t < 0.11f || (t > 0.2f && t < 0.31f);
        if (!on) continue;
        const float w = sine(880.0f, t) + 0.3f * sine(2640.0f, t);
        buf[i] = w * 0.8f;
    }
    return buf;
}

std::vector<float> makeClick()
{
    auto buf = render(0.05f);
    Rng rng(13);
    for (size_t i = 0; i < buf.size(); ++i) {
        const float t = static_cast<float>(i) / kRate;
        buf[i] = (sine(1700.0f, t) + 0.3f * rng.range(-1.0f, 1.0f)) * std::exp(-t * 110.0f);
    }
    return buf;
}

std::vector<float> makeBuy()
{
    auto buf = render(0.8f);
    Rng rng(14);
    for (size_t i = 0; i < buf.size(); ++i) {
        const float t = static_cast<float>(i) / kRate;
        float s = 0.6f * rng.range(-1.0f, 1.0f) * std::exp(-t * 60.0f);
        for (float f : {1046.5f, 1318.5f, 1568.0f, 2093.0f}) s += sine(f, t) * decayEnv(t - 0.05f, 0.003f, 0.28f) * 0.5f;
        s += sine(3141.0f, t) * decayEnv(t - 0.05f, 0.003f, 0.08f) * 0.3f;
        buf[i] = s;
    }
    return buf;
}

std::vector<float> makeGameOver()
{
    auto buf = render(1.35f);
    const float notes[] = {392.0f, 329.6f, 261.6f};
    const float starts[] = {0.0f, 0.28f, 0.56f};
    for (int k = 0; k < 3; ++k) {
        float phase = 0.0f;
        const float len = k == 2 ? 0.75f : 0.3f;
        for (size_t i = static_cast<size_t>(starts[k] * kRate); i < buf.size(); ++i) {
            const float t = static_cast<float>(i) / kRate - starts[k];
            if (t > len) break;
            const float vib = k == 2 ? 1.0f + 0.012f * std::sin(kTwoPi * 6.0f * t) : 1.0f;
            phase += notes[k] * vib / kRate;
            const float tri = 2.0f * std::fabs(2.0f * (phase - std::floor(phase + 0.5f))) - 1.0f;
            const float env = std::fmin(1.0f, t / 0.01f) * std::fmin(1.0f, (len - t) / 0.08f);
            buf[i] += (tri + 0.4f * std::sin(kTwoPi * phase)) * env;
        }
    }
    return buf;
}

std::vector<float> makeFanfare()
{
    auto buf = render(1.2f);
    const char* notes[] = {"C5", "E5", "G5", "C6"};
    for (int k = 0; k < 4; ++k) {
        const float start = 0.1f * static_cast<float>(k);
        const float f = noteFreq(notes[k]);
        for (size_t i = static_cast<size_t>(start * kRate); i < buf.size(); ++i) {
            const float t = static_cast<float>(i) / kRate - start;
            float saw = 0.0f;
            for (int h = 1; h <= 6; ++h) saw += sine(f * static_cast<float>(h), t) / static_cast<float>(h);
            buf[i] += saw * decayEnv(t, 0.02f, k == 3 ? 0.5f : 0.25f) * 0.5f;
        }
    }
    return buf;
}

std::vector<float> makeDenied()
{
    auto buf = render(0.3f);
    for (size_t i = 0; i < buf.size(); ++i) {
        const float t = static_cast<float>(i) / kRate;
        const bool on = t < 0.11f || (t > 0.15f && t < 0.26f);
        if (!on) continue;
        buf[i] = (sine(140.0f, t) > 0.0f ? 1.0f : -1.0f) * 0.5f + 0.5f * sine(280.0f, t);
    }
    return buf;
}

std::vector<float> makeRollCageHit()
{
    auto buf = render(0.38f);
    Rng rng(15);
    float hp = 0.0f;
    float prev = 0.0f;
    float thudPhase = 0.0f;
    for (size_t i = 0; i < buf.size(); ++i) {
        const float t = static_cast<float>(i) / kRate;
        const float n = rng.range(-1.0f, 1.0f);
        hp = 0.8f * (hp + n - prev);
        prev = n;
        thudPhase += (160.0f - 180.0f * std::fmin(t, 0.15f)) / kRate;
        const float impact = 0.9f * hp * std::exp(-t * 85.0f) + 0.65f * std::sin(kTwoPi * thudPhase) * std::exp(-t * 28.0f);
        const float ring = (sine(690.0f, t) + 0.75f * sine(1085.0f, t) + 0.55f * sine(1690.0f, t) +
                            0.35f * sine(2440.0f, t) + 0.2f * sine(3270.0f, t)) *
                           std::exp(-t * 15.0f);
        buf[i] = impact + 0.45f * ring;
    }
    return buf;
}

std::vector<float> makeRollCageBreak()
{
    auto buf = render(0.65f);
    Rng rng(16);
    float lp = 0.0f;
    float crunchPhase = 0.0f;
    const float hits[] = {0.0f, 0.085f, 0.19f, 0.31f};
    const float freqs[] = {980.0f, 760.0f, 610.0f, 470.0f};
    for (size_t i = 0; i < buf.size(); ++i) {
        const float t = static_cast<float>(i) / kRate;
        const float n = rng.range(-1.0f, 1.0f);
        lp += 0.32f * (n - lp);
        crunchPhase += (130.0f - 95.0f * std::fmin(t, 0.4f)) / kRate;
        float s = 1.1f * n * std::exp(-t * 42.0f) + 0.9f * lp * std::exp(-t * 10.0f) +
                  0.85f * std::sin(kTwoPi * crunchPhase) * std::exp(-t * 12.0f);
        for (int k = 0; k < 4; ++k) {
            const float lt = t - hits[k];
            if (lt < 0.0f) continue;
            const float f = freqs[k] * (1.0f - 0.12f * std::fmin(lt, 0.25f));
            const float amp = 0.42f * std::pow(0.75f, static_cast<float>(k));
            s += amp * (sine(f, lt) + 0.7f * sine(f * 1.57f, lt) + 0.45f * sine(f * 2.41f, lt)) * std::exp(-lt * 20.0f);
        }
        buf[i] = s;
    }
    return buf;
}

std::vector<float> makeLuckyFuel()
{
    auto buf = render(0.62f);
    const char* notes[] = {"G5", "B5", "D6", "G6", "B6"};
    for (int k = 0; k < 5; ++k) {
        const float start = 0.055f * static_cast<float>(k);
        const float f = noteFreq(notes[k]);
        const float tail = k == 4 ? 0.32f : 0.16f;
        for (size_t i = static_cast<size_t>(start * kRate); i < buf.size(); ++i) {
            const float t = static_cast<float>(i) / kRate - start;
            const float wave = sine(f, t) + 0.4f * sine(2.0f * f, t) + 0.2f * sine(3.0f * f, t);
            buf[i] += wave * decayEnv(t, 0.003f, tail);
        }
    }
    return buf;
}

std::vector<float> makeWheelie()
{
    auto buf = render(0.38f);
    const char* notes[] = {"G5", "D6"};
    for (int k = 0; k < 2; ++k) {
        const float start = 0.085f * static_cast<float>(k);
        const float f = noteFreq(notes[k]);
        float phase = 0.0f;
        for (size_t i = static_cast<size_t>(start * kRate); i < buf.size(); ++i) {
            const float t = static_cast<float>(i) / kRate - start;
            const float pitchDip = 1.0f - 0.04f * std::exp(-t * 45.0f);
            phase += f * pitchDip / kRate;
            const float wave = std::sin(kTwoPi * phase) + 0.3f * std::sin(2.0f * kTwoPi * phase);
            buf[i] += wave * decayEnv(t, 0.003f, k == 1 ? 0.18f : 0.11f);
        }
    }
    return buf;
}

struct SfxRecipe {
    Sfx id;
    const char* name;
    float peak;
};

const SfxRecipe kRecipes[] = {
    {Sfx::Coin, "coin", 0.55f},
    {Sfx::Fuel, "fuel", 0.6f},
    {Sfx::Crash, "crash", 0.9f},
    {Sfx::Land, "land", 0.8f},
    {Sfx::Flip, "flip", 0.55f},
    {Sfx::LowFuel, "lowfuel", 0.45f},
    {Sfx::Click, "click", 0.5f},
    {Sfx::Buy, "buy", 0.6f},
    {Sfx::GameOver, "gameover", 0.55f},
    {Sfx::Fanfare, "fanfare", 0.55f},
    {Sfx::Denied, "denied", 0.35f},
    {Sfx::RollCageHit, "rollcagehit", 0.7f},
    {Sfx::RollCageBreak, "rollcagebreak", 0.85f},
    {Sfx::LuckyFuel, "luckyfuel", 0.55f},
    {Sfx::Wheelie, "wheelie", 0.55f},
};

std::vector<float> renderSfx(Sfx id)
{
    switch (id) {
    case Sfx::Coin: return makeCoin();
    case Sfx::Fuel: return makeFuel();
    case Sfx::Crash: return makeCrash();
    case Sfx::Land: return makeLand();
    case Sfx::Flip: return makeArpeggio({"C6", "E6", "G6", "C7"}, 0.07f, 0.25f, 1.0f);
    case Sfx::LowFuel: return makeLowFuel();
    case Sfx::Click: return makeClick();
    case Sfx::Buy: return makeBuy();
    case Sfx::GameOver: return makeGameOver();
    case Sfx::Fanfare: return makeFanfare();
    case Sfx::Denied: return makeDenied();
    case Sfx::RollCageHit: return makeRollCageHit();
    case Sfx::RollCageBreak: return makeRollCageBreak();
    case Sfx::LuckyFuel: return makeLuckyFuel();
    case Sfx::Wheelie: return makeWheelie();
    case Sfx::Count: break;
    }
    return {};
}

// --- music ---------------------------------------------------------------------------

// A cheerful country-blues loop: plucked bass, strummed chords, banjo-ish lead.
std::vector<float> renderMusic(float& loopSeconds)
{
    const float beat = 60.0f / 116.0f;
    const int bars = 8;
    loopSeconds = beat * 4.0f * static_cast<float>(bars);
    std::vector<float> buf = render(loopSeconds + 3.0f);
    Rng rng(2024);

    struct Chord {
        const char* root;
        const char* fifth;
        const char* tones[3];
    };
    const Chord G{"G2", "D3", {"G3", "B3", "D4"}};
    const Chord C{"C3", "G2", {"C4", "E4", "G4"}};
    const Chord D{"D3", "A2", {"D4", "F#4", "A4"}};
    const Chord progression[bars] = {G, C, G, D, G, C, D, G};
    const char* melody[bars][8] = {
        {"D5", "B4", "G4", "B4", "D5", nullptr, "E5", "D5"},
        {"E5", "G5", "E5", "C5", "E5", nullptr, "D5", "C5"},
        {"B4", "D5", "B4", "G4", "A4", "B4", "D5", nullptr},
        {"A4", "F#4", "A4", "D5", "C5", "A4", "F#4", "A4"},
        {"G4", "B4", "D5", "G5", "F#5", nullptr, "E5", "D5"},
        {"E5", "C5", "E5", "G5", "E5", "D5", "C5", nullptr},
        {"D5", "E5", "F#5", "A5", "F#5", "E5", "D5", "C5"},
        {"B4", nullptr, "G4", nullptr, "G4", nullptr, nullptr, nullptr},
    };

    // Swung eighths: long-short.
    auto eighthTime = [&](int bar, int slot) {
        const float beatStart = (static_cast<float>(bar) * 4.0f + static_cast<float>(slot / 2)) * beat;
        return beatStart + (slot % 2 == 1 ? beat * 0.6f : 0.0f);
    };

    for (int bar = 0; bar < bars; ++bar) {
        const Chord& chord = progression[bar];
        // Bass on 1 and 3.
        pluck(buf, eighthTime(bar, 0), noteFreq(chord.root), beat * 1.8f, 0.9f, 0.35f, rng);
        pluck(buf, eighthTime(bar, 4), noteFreq(chord.fifth), beat * 1.8f, 0.8f, 0.35f, rng);
        // Strummed chord "chick" on 2 and 4.
        for (int b : {2, 6}) {
            for (int k = 0; k < 3; ++k)
                pluck(buf, eighthTime(bar, b) + 0.012f * static_cast<float>(k), noteFreq(chord.tones[k]), beat * 0.5f,
                      0.32f, 0.6f, rng);
        }
        // Lead.
        for (int slot = 0; slot < 8; ++slot) {
            if (!melody[bar][slot]) continue;
            pluck(buf, eighthTime(bar, slot), noteFreq(melody[bar][slot]), beat * 0.9f, 0.42f, 0.7f, rng);
        }
        // Percussion: soft kick on 1 and 3, brush on 2 and 4, shaker on every eighth.
        for (int slot = 0; slot < 8; ++slot) {
            const float t0 = eighthTime(bar, slot);
            const bool kick = slot == 0 || slot == 4;
            const bool brush = slot == 2 || slot == 6;
            float lp = 0.0f;
            float phase = 0.0f;
            for (int n = 0; n < static_cast<int>(0.16f * kRate); ++n) {
                const float t = static_cast<float>(n) / kRate;
                float s = 0.0f;
                if (kick) {
                    phase += (110.0f - 280.0f * t) / kRate;
                    s += 0.8f * std::sin(kTwoPi * phase) * std::exp(-t * 22.0f);
                }
                const float noise = rng.range(-1.0f, 1.0f);
                if (brush) {
                    lp += 0.35f * (noise - lp);
                    s += 0.22f * lp * std::exp(-t * 26.0f);
                }
                s += 0.025f * (noise - lp) * std::exp(-t * 90.0f);
                mixAt(buf, t0 + t, s);
            }
        }
    }

    // Soften the top end of the whole mix (one-pole low-pass around 6 kHz).
    float lpState = 0.0f;
    const float lpCoef = 1.0f - std::exp(-kTwoPi * 6000.0f / kRate);
    for (float& v : buf) {
        lpState += lpCoef * (v - lpState);
        v = lpState;
    }

    // Wrap the ringing tail onto the start so the loop is seamless.
    const size_t loopFrames = static_cast<size_t>(loopSeconds * kRate);
    for (size_t i = loopFrames; i < buf.size(); ++i) buf[i - loopFrames] += buf[i];
    buf.resize(loopFrames);
    return buf;
}

unsigned char* encodeWav(const std::vector<float>& samples, int& size)
{
    float maxAbs = 1e-6f;
    for (float s : samples) maxAbs = std::fmax(maxAbs, std::fabs(s));
    const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * 2);
    size = static_cast<int>(44 + dataBytes);
    auto* out = static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(size)));
    auto put32 = [&](int at, uint32_t v) { std::memcpy(out + at, &v, 4); };
    auto put16 = [&](int at, uint16_t v) { std::memcpy(out + at, &v, 2); };
    std::memcpy(out, "RIFF", 4);
    put32(4, 36 + dataBytes);
    std::memcpy(out + 8, "WAVEfmt ", 8);
    put32(16, 16);
    put16(20, 1);   // PCM
    put16(22, 1);   // mono
    put32(24, kRate);
    put32(28, kRate * 2);
    put16(32, 2);
    put16(34, 16);
    std::memcpy(out + 36, "data", 4);
    put32(40, dataBytes);
    for (size_t i = 0; i < samples.size(); ++i) {
        const short v = static_cast<short>(std::tanh(samples[i] / maxAbs * 1.1f) * 0.85f * 32000.0f);
        std::memcpy(out + 44 + i * 2, &v, 2);
    }
    return out;
}

// --- engine (audio thread) -------------------------------------------------------------

struct EngineParams {
    std::atomic<float> rpm{0.0f};
    std::atomic<float> throttle{0.0f};
    std::atomic<float> volume{0.0f};
    std::atomic<int> voice{static_cast<int>(EngineVoice::Standard)};
    std::atomic<float> boost{0.0f};
    std::atomic<float> thrust{0.0f};
};
EngineParams gEngine;

struct EngineState {
    float rpm = 0.0f;
    float throttle = 0.0f;
    float volume = 0.0f;
    float boost = 0.0f;
    float thrust = 0.0f;
    float phase = 0.0f;
    float halfPhase = 0.0f;
    float turboPhase = 0.0f;
    float elecPhase1 = 0.0f;
    float elecPhase2 = 0.0f;
    float shimmerPhase = 0.0f;
    float pulseAmp = 1.0f;
    float lp = 0.0f;
    float lp2 = 0.0f;
    float thrustLp = 0.0f;
    float thrustLp2 = 0.0f;
    float thrustMid = 0.0f;
    float nitroLp = 0.0f;
    float nitroHpLp = 0.0f;
    float dcIn = 0.0f;
    float dcOut = 0.0f;
    std::vector<float> comb = std::vector<float>(512, 0.0f);
    size_t combPos = 0;
    Rng rng{99};
};
EngineState gState;

void engineCallback(void* bufferData, unsigned int frames)
{
    auto* out = static_cast<float*>(bufferData);
    EngineState& e = gState;
    const float targetRpm = gEngine.rpm.load(std::memory_order_relaxed);
    const float targetThrottle = gEngine.throttle.load(std::memory_order_relaxed);
    const float targetVolume = gEngine.volume.load(std::memory_order_relaxed);
    const auto voice = static_cast<EngineVoice>(gEngine.voice.load(std::memory_order_relaxed));
    const float targetBoost = gEngine.boost.load(std::memory_order_relaxed);
    const float targetThrust = gEngine.thrust.load(std::memory_order_relaxed);
    const size_t combDelay = static_cast<size_t>(kRate / 95);
    const float thrustMidCoef = 1.0f - std::exp(-kTwoPi * 1650.0f / kRate);
    const float nitroLpCoef = 1.0f - std::exp(-kTwoPi * 2600.0f / kRate);
    const float nitroHpCoef = 1.0f - std::exp(-kTwoPi * 380.0f / kRate);

    for (unsigned int i = 0; i < frames; ++i) {
        e.rpm += (targetRpm - e.rpm) * 0.0012f;
        e.throttle += (targetThrottle - e.throttle) * 0.002f;
        e.volume += (targetVolume - e.volume) * 0.001f;
        e.boost += (targetBoost - e.boost) * 0.0025f;
        e.thrust += (targetThrust - e.thrust) * 0.0025f;

        const float baseFreq = 26.0f + 124.0f * e.rpm;
        const float baseCutoff = 220.0f + 600.0f * e.rpm + 2200.0f * e.throttle * e.rpm;

        float y = 0.0f;
        if (voice == EngineVoice::Electric) {
            const float f1 = 140.0f + 380.0f * e.rpm;
            const float f2 = f1 * 1.505f;
            e.elecPhase1 += f1 / kRate;
            if (e.elecPhase1 >= 1.0f) e.elecPhase1 -= 1.0f;
            e.elecPhase2 += f2 / kRate;
            if (e.elecPhase2 >= 1.0f) e.elecPhase2 -= 1.0f;
            const float hum = 0.45f * std::sin(kTwoPi * e.elecPhase1) + 0.18f * std::sin(kTwoPi * e.elecPhase2) +
                              0.015f * e.rng.range(-1.0f, 1.0f);
            const float eCoef = 1.0f - std::exp(-kTwoPi * (900.0f + 1600.0f * e.rpm) / kRate);
            e.lp += eCoef * (hum - e.lp);
            y = e.lp;
        } else {
            float freqMul = 1.0f;
            float pulseDecay = 7.0f;
            float pulseGain = 1.0f;
            float subGain = 0.35f;
            float secondGain = 0.12f;
            float thirdGain = 0.0f;
            float noiseScale = 1.0f;
            float cutoff = baseCutoff;

            switch (voice) {
            case EngineVoice::TwoStroke:
                freqMul = 1.75f;
                pulseDecay = 11.5f;
                subGain = 0.16f;
                secondGain = 0.28f;
                thirdGain = 0.12f;
                cutoff = baseCutoff * 1.6f + 260.0f;
                break;
            case EngineVoice::Diesel:
                freqMul = 0.62f;
                pulseDecay = 5.5f;
                subGain = 0.6f;
                secondGain = 0.10f;
                noiseScale = 1.15f;
                cutoff = baseCutoff * 0.68f;
                break;
            case EngineVoice::Race:
                freqMul = 1.85f;
                pulseDecay = 8.5f;
                pulseGain = 0.85f;
                subGain = 0.22f;
                secondGain = 0.38f;
                thirdGain = 0.24f;
                cutoff = baseCutoff * 1.7f + 220.0f;
                break;
            case EngineVoice::Rally:
                freqMul = 1.15f;
                pulseDecay = 7.8f;
                subGain = 0.48f;
                secondGain = 0.24f;
                thirdGain = 0.10f;
                cutoff = baseCutoff * 1.25f;
                break;
            case EngineVoice::Standard:
            case EngineVoice::Electric:
                break;
            }

            // Firing frequency adjusted per vehicle voice.
            const float freq = baseFreq * freqMul;
            e.phase += freq / kRate;
            if (e.phase >= 1.0f) {
                e.phase -= 1.0f;
                e.pulseAmp = 1.0f + 0.08f * e.rng.range(-1.0f, 1.0f);
            }
            e.halfPhase += 0.5f * freq / kRate;
            if (e.halfPhase >= 1.0f) e.halfPhase -= 1.0f;

            const float pulse = e.pulseAmp * std::exp(-pulseDecay * e.phase);
            float s = pulseGain * pulse + subGain * std::sin(kTwoPi * e.halfPhase) +
                      secondGain * std::sin(2.0f * kTwoPi * e.phase);
            if (thirdGain > 0.0f) s += thirdGain * std::sin(3.0f * kTwoPi * e.phase);
            s += e.rng.range(-1.0f, 1.0f) * (0.05f + 0.3f * e.throttle) * noiseScale * pulse;

            // Two one-pole low-passes whose cutoff opens with throttle and revs.
            const float a = 1.0f - std::exp(-kTwoPi * cutoff / kRate);
            e.lp += a * (s - e.lp);
            e.lp2 += a * (e.lp - e.lp2);
            y = e.lp2;

            // Short comb: a hint of exhaust resonance.
            const float delayed = e.comb[(e.combPos + e.comb.size() - combDelay) % e.comb.size()];
            y += 0.4f * delayed;
            e.comb[e.combPos] = y;
            e.combPos = (e.combPos + 1) % e.comb.size();

            if (voice == EngineVoice::Rally && e.throttle > 0.5f) {
                const float spool = clampf((e.throttle - 0.5f) * 2.0f, 0.0f, 1.0f);
                e.turboPhase += (1600.0f + 1800.0f * e.rpm) / kRate;
                if (e.turboPhase >= 1.0f) e.turboPhase -= 1.0f;
                y += 0.09f * spool * std::sin(kTwoPi * e.turboPhase);
            }
        }

        // DC blocker: the firing pulses are all positive, which would otherwise
        // waste headroom and pop when the engine starts or stops.
        const float hp = y - e.dcIn + 0.995f * e.dcOut;
        e.dcIn = y;
        e.dcOut = hp;

        float fx = 0.0f;
        if (e.thrust > 0.01f) {
            const float n = e.rng.range(-1.0f, 1.0f);
            const float tCoef = 1.0f - std::exp(-kTwoPi * (420.0f + 240.0f * e.rpm) / kRate);
            e.thrustLp += tCoef * (n - e.thrustLp);
            e.thrustLp2 += tCoef * (e.thrustLp - e.thrustLp2);
            e.thrustMid += thrustMidCoef * (n - e.thrustMid);
            const float roar = 1.7f * e.thrustLp2 + 0.38f * e.thrustMid;
            fx += roar * e.thrust;
        }
        if (e.boost > 0.01f) {
            const float n = e.rng.range(-1.0f, 1.0f);
            e.nitroLp += nitroLpCoef * (n - e.nitroLp);
            e.nitroHpLp += nitroHpCoef * (e.nitroLp - e.nitroHpLp);
            const float whoosh = (e.nitroLp - e.nitroHpLp) * 1.5f;
            e.shimmerPhase += (820.0f + 980.0f * e.rpm + 320.0f * e.boost) / kRate;
            if (e.shimmerPhase >= 1.0f) e.shimmerPhase -= 1.0f;
            const float shimmer = 0.16f * std::sin(kTwoPi * e.shimmerPhase) +
                                  0.09f * std::sin(1.5f * kTwoPi * e.shimmerPhase);
            fx += (whoosh + shimmer) * e.boost;
        }

        out[i] = std::tanh(1.6f * hp + 1.25f * fx) * 0.42f * e.volume;
    }
}

} // namespace

EngineVoice engineVoiceFor(const std::string& vehicleId)
{
    if (vehicleId == "bike") return EngineVoice::TwoStroke;
    if (vehicleId == "tractor" || vehicleId == "monster" || vehicleId == "tank") return EngineVoice::Diesel;
    if (vehicleId == "racecar") return EngineVoice::Race;
    if (vehicleId == "moonlander") return EngineVoice::Electric;
    if (vehicleId == "rally") return EngineVoice::Rally;
    return EngineVoice::Standard;
}

bool Audio::init(bool muted)
{
    if (muted) {
        LOG_INFO("Audio disabled (--mute)");
        return false;
    }
    SetAudioStreamBufferSizeDefault(2048);
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        LOG_WARN("No audio device available; continuing without sound");
        return false;
    }

    for (const SfxRecipe& r : kRecipes) sounds_[static_cast<size_t>(r.id)] = toSound(renderSfx(r.id), r.peak);
    for (Sound& voice : coinVoices_) voice = LoadSoundAlias(sounds_[static_cast<size_t>(Sfx::Coin)]);

    engine_ = LoadAudioStream(kRate, 32, 1);
    SetAudioStreamCallback(engine_, engineCallback);
    PlayAudioStream(engine_);

    float loopSeconds = 0.0f;
    const std::vector<float> track = renderMusic(loopSeconds);
    int size = 0;
    musicWav_ = encodeWav(track, size);
    music_ = LoadMusicStreamFromMemory(".wav", musicWav_, size);
    music_.looping = true;

    ready_ = true;
    LOG_INFO("Audio ready (music loop %.1fs)", loopSeconds);
    return true;
}

void Audio::shutdown()
{
    if (!ready_) return;
    StopMusicStream(music_);
    UnloadMusicStream(music_);
    MemFree(musicWav_);
    musicWav_ = nullptr;
    StopAudioStream(engine_);
    UnloadAudioStream(engine_);
    for (Sound& voice : coinVoices_) UnloadSoundAlias(voice);
    for (Sound& s : sounds_) UnloadSound(s);
    CloseAudioDevice();
    ready_ = false;
}

void Audio::setVolumes(float master, float sfx, float music)
{
    sfxVolume_ = sfx;
    musicVolume_ = music;
    if (!ready_) return;
    SetMasterVolume(master);
    SetMusicVolume(music_, musicVolume_ * musicDuck_);
}

void Audio::play(Sfx sfx, float pitch, float volume)
{
    if (!ready_) return;
    Sound s = sounds_[static_cast<size_t>(sfx)];
    if (sfx == Sfx::Coin) {
        s = coinVoices_[static_cast<size_t>(nextCoinVoice_)];
        nextCoinVoice_ = (nextCoinVoice_ + 1) % static_cast<int>(coinVoices_.size());
    }
    SetSoundPitch(s, pitch);
    SetSoundVolume(s, clampf(volume * sfxVolume_, 0.0f, 1.0f));
    PlaySound(s);
}

void Audio::setEngine(bool running, float rpm, float throttle, EngineVoice voice, float boost, bool thrusting)
{
    gEngine.rpm.store(clampf(rpm, 0.0f, 1.2f), std::memory_order_relaxed);
    gEngine.throttle.store(clampf(throttle, 0.0f, 1.0f), std::memory_order_relaxed);
    gEngine.voice.store(static_cast<int>(voice), std::memory_order_relaxed);
    gEngine.boost.store(running ? clampf(boost, 0.0f, 1.0f) : 0.0f, std::memory_order_relaxed);
    gEngine.thrust.store(running && thrusting ? 1.0f : 0.0f, std::memory_order_relaxed);
    const float baseVol = 0.55f + 0.3f * clampf(throttle, 0.0f, 1.0f);
    const float activeBoost = running ? std::fmax(clampf(boost, 0.0f, 1.0f), thrusting ? 1.0f : 0.0f) : 0.0f;
    const float vol = activeBoost > 0.0f ? std::fmax(baseVol, 0.75f + 0.15f * activeBoost) : baseVol;
    gEngine.volume.store(running ? vol * sfxVolume_ : 0.0f, std::memory_order_relaxed);
}

void Audio::setMusicOn(bool on)
{
    if (!ready_ || on == musicOn_) return;
    musicOn_ = on;
    if (on) PlayMusicStream(music_);
    else PauseMusicStream(music_);
}

void Audio::setMusicDuck(float factor)
{
    musicDuck_ = factor;
    if (ready_) SetMusicVolume(music_, musicVolume_ * musicDuck_);
}

void Audio::update()
{
    if (ready_ && musicOn_) UpdateMusicStream(music_);
}

bool exportAudio(const std::string& dir)
{
    auto writeWav = [&](const std::string& name, const std::vector<float>& samples) {
        int size = 0;
        unsigned char* data = encodeWav(samples, size);
        const bool ok = SaveFileData((dir + "/" + name + ".wav").c_str(), data, size);
        MemFree(data);
        return ok;
    };
    bool ok = true;
    for (const SfxRecipe& r : kRecipes) {
        std::vector<float> samples = renderSfx(r.id);
        const std::vector<short> pcm = toPcm(samples, r.peak);
        for (size_t i = 0; i < samples.size(); ++i) samples[i] = static_cast<float>(pcm[i]) / 32000.0f;
        ok = writeWav("sfx-" + std::string(r.name), samples) && ok;
    }
    float loopSeconds = 0.0f;
    ok = writeWav("music", renderMusic(loopSeconds)) && ok;
    // Engine at a few operating points, 2 s each, through the real callback.
    const float points[][2] = {{0.1f, 0.0f}, {0.5f, 0.5f}, {1.0f, 1.0f}};
    for (const auto& pt : points) {
        gState = EngineState{};
        gEngine.rpm.store(pt[0]);
        gEngine.throttle.store(pt[1]);
        gEngine.volume.store(0.8f);
        gEngine.voice.store(static_cast<int>(EngineVoice::Standard));
        gEngine.boost.store(0.0f);
        gEngine.thrust.store(0.0f);
        gState.rpm = pt[0];
        gState.throttle = pt[1];
        gState.volume = 0.8f;
        std::vector<float> buf(static_cast<size_t>(2 * kRate));
        engineCallback(buf.data(), static_cast<unsigned int>(buf.size()));
        char name[64];
        std::snprintf(name, sizeof(name), "engine-rpm%.1f-thr%.1f", pt[0], pt[1]);
        ok = writeWav(name, buf) && ok;
    }
    return ok;
}

} // namespace hr
