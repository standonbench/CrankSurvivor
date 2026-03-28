#include "game.h"

// ---------------------------------------------------------------------------
// Sound synths
// ---------------------------------------------------------------------------
static PDSynth* synthHit = NULL;
static PDSynth* synthKill = NULL;
static PDSynth* synthXP = NULL;
static PDSynth* synthLevelUp = NULL;
static PDSynth* synthWeapon = NULL;
static PDSynth* synthBoom = NULL;
static PDSynth* synthConfirm = NULL;
static PDSynth* synthMenu = NULL;
static PDSynth* synthShrink = NULL;
static PDSynth* synthCrate = NULL;
static PDSynth* synthGameOver = NULL;

// Additional synths for richer sounds
static PDSynth* synthHitCrack = NULL;   // Layered crack on hit
static PDSynth* synthBoomSub = NULL;    // Sub-bass layer for boom
static PDSynth* synthShrinkLFO = NULL;  // Second voice for shrink

// LFOs
static PDSynthLFO* lfoVibrato = NULL;
static PDSynthLFO* lfoSweep = NULL;

// Effects
static DelayLine* echoDelay = NULL;
static DelayLineTap* echoTap = NULL;
static TwoPoleFilter* warmFilter = NULL;
static Overdrive* gritDrive = NULL;

// Channels
static SoundChannel* fxChannel = NULL;    // Atmospheric: delay + warmth
static SoundChannel* gritChannel = NULL;  // Impacts: overdrive + warmth

static PDSynth* make_synth(SoundWaveform wave)
{
    PDSynth* s = pd->sound->synth->newSynth();
    if (!s) { DLOG("Failed to create synth (wave=%d)", wave); return NULL; }
    pd->sound->synth->setWaveform(s, wave);
    return s;
}

// Configure ADSR envelope directly on a synth
static void synth_adsr(PDSynth* s, float a, float d, float sus, float r)
{
    if (!s) return;
    pd->sound->synth->setAttackTime(s, a);
    pd->sound->synth->setDecayTime(s, d);
    pd->sound->synth->setSustainLevel(s, sus);
    pd->sound->synth->setReleaseTime(s, r);
}

void sound_init(void)
{
    // --- Create synths with shaped envelopes ---

    // Hit: noise crunch, sharp attack, fast decay
    synthHit = make_synth(kWaveformNoise);
    synth_adsr(synthHit, 0.0f, 0.06f, 0.0f, 0.03f);

    // Hit crack layer: higher square transient
    synthHitCrack = make_synth(kWaveformSquare);
    synth_adsr(synthHitCrack, 0.0f, 0.04f, 0.0f, 0.02f);

    // Kill: square with sharp snap
    synthKill = make_synth(kWaveformSquare);
    synth_adsr(synthKill, 0.0f, 0.05f, 0.0f, 0.03f);

    // XP: sine pluck, crystalline ping
    synthXP = make_synth(kWaveformSine);
    synth_adsr(synthXP, 0.0f, 0.02f, 0.0f, 0.01f);

    // Level-up: triangle with slow attack, sustain for echo
    synthLevelUp = make_synth(kWaveformTriangle);
    synth_adsr(synthLevelUp, 0.01f, 0.15f, 0.4f, 0.2f);

    // Weapon: square snap
    synthWeapon = make_synth(kWaveformSquare);
    synth_adsr(synthWeapon, 0.0f, 0.03f, 0.0f, 0.02f);

    // Boom: noise with body
    synthBoom = make_synth(kWaveformNoise);
    synth_adsr(synthBoom, 0.0f, 0.12f, 0.1f, 0.08f);

    // Boom sub-bass layer
    synthBoomSub = make_synth(kWaveformSawtooth);
    synth_adsr(synthBoomSub, 0.0f, 0.15f, 0.05f, 0.1f);

    // Confirm: triangle, clean sustain for reverb tail
    synthConfirm = make_synth(kWaveformTriangle);
    synth_adsr(synthConfirm, 0.005f, 0.1f, 0.3f, 0.15f);

    // Menu: square pluck, clean click
    synthMenu = make_synth(kWaveformSquare);
    synth_adsr(synthMenu, 0.0f, 0.02f, 0.0f, 0.01f);

    // Shrink: sawtooth swell, oppressive
    synthShrink = make_synth(kWaveformSawtooth);
    synth_adsr(synthShrink, 0.08f, 0.3f, 0.3f, 0.15f);

    // Shrink second voice for depth
    synthShrinkLFO = make_synth(kWaveformSquare);
    synth_adsr(synthShrinkLFO, 0.1f, 0.25f, 0.2f, 0.1f);

    // Crate: sine with gentle sustain for sparkle
    synthCrate = make_synth(kWaveformSine);
    synth_adsr(synthCrate, 0.005f, 0.08f, 0.3f, 0.15f);

    // Game Over: triangle, haunting swell
    synthGameOver = make_synth(kWaveformTriangle);
    synth_adsr(synthGameOver, 0.05f, 0.2f, 0.4f, 0.25f);

    // --- LFOs ---

    // Subtle vibrato for atmospheric sounds
    lfoVibrato = pd->sound->lfo->newLFO(kLFOTypeSine);
    if (lfoVibrato) {
        pd->sound->lfo->setRate(lfoVibrato, 5.0f);
        pd->sound->lfo->setDepth(lfoVibrato, 0.015f);
    }

    // Sweep for shrink/surge (slow downward)
    lfoSweep = pd->sound->lfo->newLFO(kLFOTypeSawtoothDown);
    if (lfoSweep) {
        pd->sound->lfo->setRate(lfoSweep, 1.5f);
        pd->sound->lfo->setDepth(lfoSweep, 0.3f);
    }

    // Apply vibrato to atmospheric synths
    if (lfoVibrato) {
        if (synthGameOver) pd->sound->synth->setFrequencyModulator(synthGameOver, (PDSynthSignalValue*)lfoVibrato);
        if (synthLevelUp)  pd->sound->synth->setFrequencyModulator(synthLevelUp, (PDSynthSignalValue*)lfoVibrato);
        if (synthCrate)    pd->sound->synth->setFrequencyModulator(synthCrate, (PDSynthSignalValue*)lfoVibrato);
    }

    // Apply sweep to shrink
    if (lfoSweep && synthShrink)
        pd->sound->synth->setFrequencyModulator(synthShrink, (PDSynthSignalValue*)lfoSweep);

    // --- Effects ---

    // Delay line for echo/reverb (100ms at 44.1kHz)
    echoDelay = pd->sound->effect->delayline->newDelayLine(4410, 0);
    if (echoDelay) {
        pd->sound->effect->delayline->setFeedback(echoDelay, 0.25f);
        echoTap = pd->sound->effect->delayline->addTap(echoDelay, 3300); // ~75ms tap
    }

    // Low-pass filter for warmth
    warmFilter = pd->sound->effect->twopolefilter->newFilter();
    if (warmFilter) {
        pd->sound->effect->twopolefilter->setType(warmFilter, kFilterTypeLowPass);
        pd->sound->effect->twopolefilter->setFrequency(warmFilter, 2500.0f);
        pd->sound->effect->twopolefilter->setResonance(warmFilter, 0.4f);
    }

    // Overdrive for gritty impacts
    gritDrive = pd->sound->effect->overdrive->newOverdrive();
    if (gritDrive) {
        pd->sound->effect->overdrive->setGain(gritDrive, 1.8f);
        pd->sound->effect->overdrive->setLimit(gritDrive, 0.7f);
    }

    // --- Channels ---

    // FX channel: warmth + echo for atmospheric sounds
    fxChannel = pd->sound->channel->newChannel();
    if (fxChannel) {
        if (warmFilter) pd->sound->channel->addEffect(fxChannel, (SoundEffect*)warmFilter);
        if (echoDelay)  pd->sound->channel->addEffect(fxChannel, (SoundEffect*)echoDelay);
        pd->sound->channel->setVolume(fxChannel, 0.85f);

        // Route atmospheric synths to FX channel
        if (synthLevelUp)  pd->sound->channel->addSource(fxChannel, (SoundSource*)synthLevelUp);
        if (synthGameOver) pd->sound->channel->addSource(fxChannel, (SoundSource*)synthGameOver);
        if (synthCrate)    pd->sound->channel->addSource(fxChannel, (SoundSource*)synthCrate);
        if (synthConfirm)  pd->sound->channel->addSource(fxChannel, (SoundSource*)synthConfirm);
    }

    // Grit channel: overdrive for impacts
    gritChannel = pd->sound->channel->newChannel();
    if (gritChannel) {
        if (gritDrive) pd->sound->channel->addEffect(gritChannel, (SoundEffect*)gritDrive);
        pd->sound->channel->setVolume(gritChannel, 0.8f);

        // Route impact synths to grit channel
        if (synthHit)      pd->sound->channel->addSource(gritChannel, (SoundSource*)synthHit);
        if (synthHitCrack) pd->sound->channel->addSource(gritChannel, (SoundSource*)synthHitCrack);
        if (synthBoom)     pd->sound->channel->addSource(gritChannel, (SoundSource*)synthBoom);
        if (synthBoomSub)  pd->sound->channel->addSource(gritChannel, (SoundSource*)synthBoomSub);
    }

    DLOG("Sound initialized: 14 synths, 2 LFOs, 3 effects, 2 channels");
}

void sound_play_hit(void)
{
    if (!synthHit) return;
    // Crunchy impact: noise body + square transient crack
    pd->sound->synth->playNote(synthHit, 120.0f, 0.5f, 0.1f, 0);
    uint32_t now = pd->sound->getCurrentTime();
    if (synthHitCrack)
        pd->sound->synth->playNote(synthHitCrack, 280.0f, 0.35f, 0.05f,
                                    now + (uint32_t)(0.01f * 44100));
}

void sound_play_kill(void)
{
    if (!synthKill) return;
    // Descending splat: three rapid notes down
    pd->sound->synth->playNote(synthKill, 700.0f, 0.25f, 0.03f, 0);
    uint32_t now = pd->sound->getCurrentTime();
    pd->sound->synth->playNote(synthKill, 450.0f, 0.22f, 0.04f,
                                now + (uint32_t)(0.025f * 44100));
    pd->sound->synth->playNote(synthKill, 250.0f, 0.18f, 0.05f,
                                now + (uint32_t)(0.05f * 44100));
}

void sound_play_xp(void)
{
    if (!synthXP) return;
    // Crystalline ping — single bright note
    pd->sound->synth->playNote(synthXP, 1200.0f, 0.12f, 0.04f, 0);
}

void sound_play_levelup(void)
{
    if (!synthLevelUp) return;
    // Triumphant arpeggio with sustain into echo: C5→E5→G5→C6
    uint32_t now = pd->sound->getCurrentTime();
    pd->sound->synth->playNote(synthLevelUp, 523.0f, 0.35f, 0.2f, now);
    pd->sound->synth->playNote(synthLevelUp, 659.0f, 0.35f, 0.2f,
                                now + (uint32_t)(0.15f * 44100));
    pd->sound->synth->playNote(synthLevelUp, 784.0f, 0.35f, 0.25f,
                                now + (uint32_t)(0.30f * 44100));
    pd->sound->synth->playNote(synthLevelUp, 1046.0f, 0.4f, 0.35f,
                                now + (uint32_t)(0.48f * 44100));
}

void sound_play_weapon(float freq)
{
    if (!synthWeapon) return;
    // Snappy fire sound
    pd->sound->synth->playNote(synthWeapon, freq, 0.18f, 0.06f, 0);
}

void sound_play_boom(float freq, float vol, float len)
{
    if (!synthBoom) return;
    // Deep explosion: noise body + sawtooth sub-bass rumble
    pd->sound->synth->playNote(synthBoom, freq, vol, len, 0);
    uint32_t now = pd->sound->getCurrentTime();
    if (synthBoomSub)
        pd->sound->synth->playNote(synthBoomSub, freq * 0.4f, vol * 0.5f, len * 1.8f,
                                    now + (uint32_t)(0.03f * 44100));
}

void sound_play_confirm(void)
{
    if (!synthConfirm) return;
    // Clean ascending chord with reverb tail: F4→A4→C5
    uint32_t now = pd->sound->getCurrentTime();
    pd->sound->synth->playNote(synthConfirm, 349.0f, 0.22f, 0.12f, now);
    pd->sound->synth->playNote(synthConfirm, 440.0f, 0.22f, 0.12f,
                                now + (uint32_t)(0.08f * 44100));
    pd->sound->synth->playNote(synthConfirm, 523.0f, 0.28f, 0.15f,
                                now + (uint32_t)(0.16f * 44100));
}

void sound_play_menu(void)
{
    if (!synthMenu) return;
    // Clean click
    pd->sound->synth->playNote(synthMenu, 700.0f, 0.12f, 0.025f, 0);
}

void sound_play_shrink(void)
{
    if (!synthShrink) return;
    // Oppressive descending rumble: two detuned voices with sweep LFO
    pd->sound->synth->playNote(synthShrink, 50.0f, 0.65f, 0.7f, 0);
    uint32_t now = pd->sound->getCurrentTime();
    if (synthShrinkLFO)
        pd->sound->synth->playNote(synthShrinkLFO, 55.0f, 0.4f, 0.6f,
                                    now + (uint32_t)(0.05f * 44100));
}

void sound_play_crate(void)
{
    if (!synthCrate) return;
    // Magical sparkling arpeggio with echo: E5→G#5→B5→E6
    uint32_t now = pd->sound->getCurrentTime();
    pd->sound->synth->playNote(synthCrate, 659.0f, 0.22f, 0.12f, now);
    pd->sound->synth->playNote(synthCrate, 831.0f, 0.22f, 0.12f,
                                now + (uint32_t)(0.08f * 44100));
    pd->sound->synth->playNote(synthCrate, 988.0f, 0.22f, 0.12f,
                                now + (uint32_t)(0.16f * 44100));
    pd->sound->synth->playNote(synthCrate, 1318.0f, 0.28f, 0.18f,
                                now + (uint32_t)(0.24f * 44100));
}

void sound_play_gameover(void)
{
    if (!synthGameOver) return;
    // Haunting foghorn: slow mournful descent with vibrato + echo
    // Dm → C → Bb → A (minor feel)
    uint32_t now = pd->sound->getCurrentTime();
    pd->sound->synth->playNote(synthGameOver, 294.0f, 0.35f, 0.35f, now);
    pd->sound->synth->playNote(synthGameOver, 262.0f, 0.35f, 0.35f,
                                now + (uint32_t)(0.30f * 44100));
    pd->sound->synth->playNote(synthGameOver, 233.0f, 0.32f, 0.35f,
                                now + (uint32_t)(0.60f * 44100));
    pd->sound->synth->playNote(synthGameOver, 220.0f, 0.38f, 0.5f,
                                now + (uint32_t)(0.90f * 44100));
}

void sound_play_streak(void)
{
    if (!synthKill) return;
    // Exciting rapid ascending triple-blip
    pd->sound->synth->playNote(synthKill, 500.0f, 0.2f, 0.025f, 0);
    uint32_t now = pd->sound->getCurrentTime();
    pd->sound->synth->playNote(synthKill, 700.0f, 0.22f, 0.025f,
                                now + (uint32_t)(0.025f * 44100));
    pd->sound->synth->playNote(synthKill, 1000.0f, 0.28f, 0.04f,
                                now + (uint32_t)(0.05f * 44100));
}

void sound_play_surge(void)
{
    if (!synthBoom) return;
    // Deep threatening pulse: noise swell + low sawtooth
    pd->sound->synth->playNote(synthBoom, 35.0f, 0.8f, 0.6f, 0);
    uint32_t now = pd->sound->getCurrentTime();
    if (synthBoomSub)
        pd->sound->synth->playNote(synthBoomSub, 28.0f, 0.5f, 0.8f,
                                    now + (uint32_t)(0.05f * 44100));
}

void sound_play_tier(void)
{
    if (!synthConfirm) return;
    // Ominous cathedral chord with echo: D3→A3→D4 (open fifth)
    uint32_t now = pd->sound->getCurrentTime();
    pd->sound->synth->playNote(synthConfirm, 147.0f, 0.3f, 0.4f, now);
    pd->sound->synth->playNote(synthConfirm, 220.0f, 0.3f, 0.4f,
                                now + (uint32_t)(0.25f * 44100));
    pd->sound->synth->playNote(synthConfirm, 294.0f, 0.35f, 0.5f,
                                now + (uint32_t)(0.50f * 44100));
}
