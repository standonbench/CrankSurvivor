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
static PDSynth* synthKillNoise = NULL;  // Bitcrushed noise layer for kills
static PDSynth* synthBoomSub = NULL;    // Sub-bass layer for boom
static PDSynth* synthShrinkLFO = NULL;  // Second voice for shrink

// Envelopes for pitch modulation
static PDSynthEnvelope* envSharpDrop = NULL;   // Sharp transient for weapons/hits
static PDSynthEnvelope* envSubDrop = NULL;     // Deep sweep for explosions
static PDSynthEnvelope* envSquelch = NULL;     // Very fast for splats

// LFOs
static PDSynthLFO* lfoVibrato = NULL;
static PDSynthLFO* lfoSweep = NULL;

// Effects
static DelayLine* echoDelay = NULL;
static DelayLineTap* echoTap = NULL;
static TwoPoleFilter* warmFilter = NULL;
static Overdrive* gritDrive = NULL;
static BitCrusher* gritBitCrusher = NULL;

// Channels
static SoundChannel* fxChannel = NULL;    // Atmospheric: delay + warmth
static SoundChannel* gritChannel = NULL;  // Impacts: overdrive + warmth + grit

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

    // Hit crack layer: higher triangle transient (softer)
    synthHitCrack = make_synth(kWaveformTriangle);
    synth_adsr(synthHitCrack, 0.01f, 0.04f, 0.0f, 0.02f);

    // Kill: square with sharp snap
    synthKill = make_synth(kWaveformSquare);
    synth_adsr(synthKill, 0.0f, 0.1f, 0.0f, 0.05f);

    // Kill Noise Layer: Bitcrushed noise for gore
    synthKillNoise = make_synth(kWaveformNoise);
    synth_adsr(synthKillNoise, 0.0f, 0.15f, 0.0f, 0.1f);

    // XP: sine pluck, crystalline ping
    synthXP = make_synth(kWaveformSine);
    synth_adsr(synthXP, 0.0f, 0.05f, 0.0f, 0.02f);

    // Level-up: triangle with slow attack, sustain for echo
    synthLevelUp = make_synth(kWaveformTriangle);
    synth_adsr(synthLevelUp, 0.01f, 0.15f, 0.4f, 0.2f);

    // Weapon: triangle (smoother than square)
    synthWeapon = make_synth(kWaveformTriangle);
    synth_adsr(synthWeapon, 0.005f, 0.04f, 0.0f, 0.02f);

    // Boom: noise with body
    synthBoom = make_synth(kWaveformNoise);
    synth_adsr(synthBoom, 0.0f, 0.15f, 0.1f, 0.1f);

    // Boom sub-bass layer
    synthBoomSub = make_synth(kWaveformSawtooth);
    synth_adsr(synthBoomSub, 0.0f, 0.2f, 0.05f, 0.15f);

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

    // --- Pitch Envelopes ---
    
    // Sharp drop: for weapon pops and impacts (1 octave drop)
    envSharpDrop = pd->sound->envelope->newEnvelope(0.0f, 0.04f, 0.0f, 0.0f);
    if (envSharpDrop) {
        pd->sound->signal->setValueScale((PDSynthSignal*)envSharpDrop, 1.0f); // +1 octave
    }

    // Sub drop: deep sweep for explosions
    envSubDrop = pd->sound->envelope->newEnvelope(0.0f, 0.25f, 0.0f, 0.0f);
    if (envSubDrop) {
        pd->sound->signal->setValueScale((PDSynthSignal*)envSubDrop, 1.5f); // +1.5 octaves
    }

    // Squelch: very fast for hits/kills
    envSquelch = pd->sound->envelope->newEnvelope(0.0f, 0.08f, 0.0f, 0.0f);
    if (envSquelch) {
        pd->sound->signal->setValueScale((PDSynthSignal*)envSquelch, 0.8f);
    }

    // Apply pitch envelopes to synths
    if (synthWeapon && envSharpDrop) pd->sound->synth->setFrequencyModulator(synthWeapon, (PDSynthSignalValue*)envSharpDrop);
    if (synthHit && envSquelch)      pd->sound->synth->setFrequencyModulator(synthHit, (PDSynthSignalValue*)envSquelch);
    if (synthKill && envSquelch)     pd->sound->synth->setFrequencyModulator(synthKill, (PDSynthSignalValue*)envSquelch);
    if (synthBoomSub && envSubDrop)  pd->sound->synth->setFrequencyModulator(synthBoomSub, (PDSynthSignalValue*)envSubDrop);

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
        pd->sound->effect->twopolefilter->setFrequency(warmFilter, 1800.0f); // Lower freq for more "thud"
        pd->sound->effect->twopolefilter->setResonance(warmFilter, 0.5f);
    }

    // Overdrive for gritty impacts
    gritDrive = pd->sound->effect->overdrive->newOverdrive();
    if (gritDrive) {
        pd->sound->effect->overdrive->setGain(gritDrive, 1.8f);
        pd->sound->effect->overdrive->setLimit(gritDrive, 0.7f);
    }

    // BitCrusher for terrifying cosmic crunch - tone it down for comfort
    gritBitCrusher = pd->sound->effect->bitcrusher->newBitCrusher();
    if (gritBitCrusher) {
        pd->sound->effect->bitcrusher->setAmount(gritBitCrusher, 0.35f);
        pd->sound->effect->bitcrusher->setUndersampling(gritBitCrusher, 0.15f);
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
        if (synthXP)       pd->sound->channel->addSource(fxChannel, (SoundSource*)synthXP);
        if (synthWeapon)   pd->sound->channel->addSource(fxChannel, (SoundSource*)synthWeapon); // Moved to FX
    }

    // Grit channel: overdrive + bitcrush for impacts
    gritChannel = pd->sound->channel->newChannel();
    if (gritChannel) {
        if (gritBitCrusher) pd->sound->channel->addEffect(gritChannel, (SoundEffect*)gritBitCrusher);
        if (gritDrive)      pd->sound->channel->addEffect(gritChannel, (SoundEffect*)gritDrive);
        pd->sound->channel->setVolume(gritChannel, 0.75f);

        // Route impact synths to grit channel
        if (synthHit)       pd->sound->channel->addSource(gritChannel, (SoundSource*)synthHit);
        if (synthHitCrack)  pd->sound->channel->addSource(gritChannel, (SoundSource*)synthHitCrack);
        if (synthKill)      pd->sound->channel->addSource(gritChannel, (SoundSource*)synthKill);
        if (synthKillNoise) pd->sound->channel->addSource(gritChannel, (SoundSource*)synthKillNoise);
        if (synthBoom)      pd->sound->channel->addSource(gritChannel, (SoundSource*)synthBoom);
        if (synthBoomSub)   pd->sound->channel->addSource(gritChannel, (SoundSource*)synthBoomSub);
    }

    DLOG("Sound overhaul initialized: 15 synths, 3 envelopes, 4 effects, 2 channels");
}

void sound_play_hit(void)
{
    if (!synthHit) return;
    float r = 1.0f + (rng_float() - 0.5f) * 0.2f;
    // Softer thud: reduced noise volume, boosted triangle crack
    pd->sound->synth->playNote(synthHit, 80.0f * r, 0.4f, 0.08f, 0);
    uint32_t now = pd->sound->getCurrentTime();
    if (synthHitCrack)
        pd->sound->synth->playNote(synthHitCrack, 240.0f * r, 0.55f, 0.06f,
                                    now + (uint32_t)(0.005f * 44100));
}

void sound_play_kill(void)
{
    if (!synthKill) return;
    float r = 1.0f + (rng_float() - 0.5f) * 0.15f;
    // Descending splat: rapid notes down + bitcrushed noise gore
    uint32_t now = pd->sound->getCurrentTime();
    pd->sound->synth->playNote(synthKill, 550.0f * r, 0.5f, 0.05f, 0);
    pd->sound->synth->playNote(synthKill, 250.0f * r, 0.4f, 0.08f,
                                now + (uint32_t)(0.02f * 44100));
    
    if (synthKillNoise) {
        // Lower, crunchier noise burst
        pd->sound->synth->playNote(synthKillNoise, 60.0f * r, 0.65f, 0.15f, now);
    }
}

void sound_play_xp(void)
{
    if (!synthXP) return;
    // Crystalline chime: randomized base + high harmonic
    float r = 1.0f + (rng_float() - 0.5f) * 0.25f;
    pd->sound->synth->playNote(synthXP, 1200.0f * r, 0.2f, 0.08f, 0);
    
    // Harmonic sparkle layer
    uint32_t now = pd->sound->getCurrentTime();
    pd->sound->synth->playNote(synthXP, 2400.0f * r, 0.12f, 0.05f, 
                                now + (uint32_t)(0.035f * 44100));
    pd->sound->synth->playNote(synthXP, 1800.0f * r, 0.1f, 0.04f, 
                                now + (uint32_t)(0.07f * 44100));
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
    // Snappy fire sound with smooth triangle wave and softened mech-click
    float r = 1.0f + (rng_float() - 0.5f) * 0.12f;
    pd->sound->synth->playNote(synthWeapon, freq * r, 0.45f, 0.08f, 0);

    // Subtle mechanical "thump" instead of a sharp "click"
    if (synthHit) {
        pd->sound->synth->playNote(synthHit, 60.0f * r, 0.25f, 0.03f, 0);
    }
}

void sound_play_boom(float freq, float vol, float len)
{
    if (!synthBoom) return;
    // Deep explosion: noise body + sawtooth sub-bass sweep (via envSubDrop)
    float r = 1.0f + (rng_float() - 0.5f) * 0.1f;
    pd->sound->synth->playNote(synthBoom, freq * r, vol, len, 0);
    uint32_t now = pd->sound->getCurrentTime();
    if (synthBoomSub)
        pd->sound->synth->playNote(synthBoomSub, (freq * 0.35f) * r, vol * 0.65f, len * 2.0f,
                                    now + (uint32_t)(0.02f * 44100));
}

void sound_play_confirm(void)
{
    if (!synthConfirm) return;
    // Clean ascending chord with reverb tail: F4→A4→C5
    // Slightly randomized timing for a more "strummed" feel
    uint32_t now = pd->sound->getCurrentTime();
    float r = 1.0f + (rng_float() - 0.5f) * 0.05f;
    pd->sound->synth->playNote(synthConfirm, 349.0f * r, 0.22f, 0.12f, now);
    pd->sound->synth->playNote(synthConfirm, 440.0f * r, 0.22f, 0.12f,
                                now + (uint32_t)(0.07f * 44100));
    pd->sound->synth->playNote(synthConfirm, 523.0f * r, 0.28f, 0.15f,
                                now + (uint32_t)(0.14f * 44100));
}

void sound_play_menu(void)
{
    if (!synthMenu) return;
    // Clean click with very slight pitch variation
    float r = 1.0f + (rng_float() - 0.5f) * 0.1f;
    pd->sound->synth->playNote(synthMenu, 700.0f * r, 0.15f, 0.025f, 0);
}

void sound_play_shrink(void)
{
    if (!synthShrink) return;
    // Oppressive descending rumble: two detuned voices with sweep LFO
    // We use lower frequencies for a physical "throom" feeling
    pd->sound->synth->playNote(synthShrink, 45.0f, 0.7f, 0.8f, 0);
    uint32_t now = pd->sound->getCurrentTime();
    if (synthShrinkLFO)
        pd->sound->synth->playNote(synthShrinkLFO, 48.0f, 0.45f, 0.7f,
                                    now + (uint32_t)(0.04f * 44100));
}

void sound_play_crate(void)
{
    if (!synthCrate) return;
    // Magical sparkling arpeggio with echo: E5→G#5→B5→E6
    // Added a slight "swelling" velocity sequence
    uint32_t now = pd->sound->getCurrentTime();
    pd->sound->synth->playNote(synthCrate, 659.0f, 0.15f, 0.12f, now);
    pd->sound->synth->playNote(synthCrate, 831.0f, 0.20f, 0.12f,
                                now + (uint32_t)(0.07f * 44100));
    pd->sound->synth->playNote(synthCrate, 988.0f, 0.25f, 0.12f,
                                now + (uint32_t)(0.14f * 44100));
    pd->sound->synth->playNote(synthCrate, 1318.0f, 0.35f, 0.20f,
                                now + (uint32_t)(0.21f * 44100));
}

void sound_play_gameover(void)
{
    if (!synthGameOver) return;
    // Haunting foghorn: slow mournful descent with vibrato + echo
    // Dm → C → Bb → A (minor feel), lower frequencies for more doom
    uint32_t now = pd->sound->getCurrentTime();
    pd->sound->synth->playNote(synthGameOver, 220.0f, 0.4f, 0.4f, now);
    pd->sound->synth->playNote(synthGameOver, 196.0f, 0.4f, 0.4f,
                                now + (uint32_t)(0.4f * 44100));
    pd->sound->synth->playNote(synthGameOver, 174.6f, 0.35f, 0.5f,
                                now + (uint32_t)(0.8f * 44100));
    pd->sound->synth->playNote(synthGameOver, 164.8f, 0.45f, 0.7f,
                                now + (uint32_t)(1.2f * 44100));
}

void sound_play_streak(void)
{
    if (!synthKill) return;
    // Exciting rapid ascending triple-blip with pitch randomization
    float r = 1.0f + (rng_float() - 0.5f) * 0.1f;
    pd->sound->synth->playNote(synthKill, 500.0f * r, 0.3f, 0.03f, 0);
    uint32_t now = pd->sound->getCurrentTime();
    pd->sound->synth->playNote(synthKill, 800.0f * r, 0.35f, 0.03f,
                                now + (uint32_t)(0.03f * 44100));
    pd->sound->synth->playNote(synthKill, 1200.0f * r, 0.4f, 0.05f,
                                now + (uint32_t)(0.06f * 44100));
}

void sound_play_surge(void)
{
    if (!synthBoom) return;
    // Deep threatening pulse: noise swell + massive sub drop
    pd->sound->synth->playNote(synthBoom, 35.0f, 0.9f, 0.8f, 0);
    uint32_t now = pd->sound->getCurrentTime();
    if (synthBoomSub)
        pd->sound->synth->playNote(synthBoomSub, 30.0f, 0.6f, 1.2f,
                                    now + (uint32_t)(0.05f * 44100));
}

void sound_play_tier(void)
{
    if (!synthConfirm) return;
    // Ominous cathedral chord with echo: D3→A3→D4 (open fifth)
    // More dramatic timing and volume for tier shifts
    uint32_t now = pd->sound->getCurrentTime();
    pd->sound->synth->playNote(synthConfirm, 146.8f, 0.4f, 0.5f, now);
    pd->sound->synth->playNote(synthConfirm, 220.0f, 0.4f, 0.5f,
                                now + (uint32_t)(0.2f * 44100));
    pd->sound->synth->playNote(synthConfirm, 293.7f, 0.5f, 0.7f,
                                now + (uint32_t)(0.4f * 44100));
}
