

## Short answer

Use **BLEP** (band-limited step) residuals for the saw and square, and **BLAMP** (band-limited ramp) residuals for the triangle's corners. You never need to run a `sinc` convolution per sample — you only inject a small correction *at the sample times immediately surrounding a discontinuity*, which happens at most once or twice per cycle. That's the whole trick, and it's why it's cheap.

There are two tiers of quality/cost:

| Method | Cost | Aliasing suppression |
|---|---|---|
| PolyBLEP / PolyBLAMP (2-point polynomial) | Essentially free (~a few flops/sample) | ~−40 dB, degrades at high f |
| minBLEP table (VCV's `dsp::MinBlepGenerator`) | Small: one table lookup + ~32 adds per discontinuity | ~−80 to −100 dB |

For "audiophile quality" in VCV Rack, **minBLEP is the right answer**, and Rack already ships it. It's what the Fundamental VCO uses, and that module runs well under 5% CPU.

---

## Why BLEP works

An ideal band-limited step is $\mathrm{Si}$ — the integral of a sinc. Your naive waveform contains a *perfect* step at a fractional sample position $d \in [0,1)$. The band-limited version differs from it by a fixed shape:

$$r(t) = \text{BLEP}(t) - \text{step}(t)$$

This **residual** is nonzero only for a few samples around the discontinuity and decays to zero. So instead of synthesizing the band-limited waveform, you synthesize the trivially cheap naive waveform and *add the residual*, scaled by the jump height $h$ and sampled at the correct fractional offset $d$.

Same idea for slope discontinuities: integrate the BLEP residual once and you get the BLAMP residual, which you scale by the *change in slope per sample*.

---

## Tier 1: PolyBLEP (2-sample polynomial approximation)

The residual is approximated by two parabola halves spanning one sample either side of the discontinuity. Let $t$ be the normalized phase in $[0,1)$ and $dt = f_0 / f_s$ the per-sample phase increment.

```c
// Normalized for a jump of height 2. For jump h, scale by h/2.
inline float polyBlep(float t, float dt) {
    if (t < dt) {                    // just after the discontinuity
        t /= dt;
        return t + t - t*t - 1.f;
    }
    if (t > 1.f - dt) {              // just before it (wrapping)
        t = (t - 1.f) / dt;
        return t*t + t + t + 1.f;
    }
    return 0.f;
}

// PolyBLAMP = integral of polyBlep w.r.t. sample index.
// Normalized for a slope change of 2 per sample.
inline float polyBlamp(float t, float dt) {
    if (t < dt)        { t = t/dt - 1.f;       return -(1.f/3.f) * t*t*t; }
    if (t > 1.f - dt)  { t = (t - 1.f)/dt + 1.f; return  (1.f/3.f) * t*t*t; }
    return 0.f;
}
```

**Rule of thumb:** add $\tfrac{h}{2}\,r(t)$ where $h$ is the *signed* jump.

```c
float frac(float x) { return x - std::floor(x); }

// Sawtooth, rising, ±1. Jump at t=0 is -2.
saw  = 2.f*t - 1.f;
saw -= polyBlep(t, dt);

// Square with pulse width pw. Jump +2 at t=0, jump -2 at t=pw.
sq   = (t < pw) ? 1.f : -1.f;
sq  += polyBlep(t, dt);
sq  -= polyBlep(frac(t - pw), dt);

// Triangle, ±1. Slope is ±4 per unit phase = ±4*dt per sample.
// Corner at t=0:   slope change = +8*dt
// Corner at t=0.5: slope change = -8*dt
tri  = (t < 0.5f) ? (4.f*t - 1.f) : (3.f - 4.f*t);
tri += 4.f*dt * polyBlamp(t, dt);
tri -= 4.f*dt * polyBlamp(frac(t - 0.5f), dt);
```

This is maybe 15 flops per sample, mostly branch-predictable no-ops. It is genuinely good up to a few kHz fundamental and gets audibly worse above that. Fine for an LFO or a utility module; probably not what you want to advertise as audiophile.

---

## Tier 2: minBLEP with Rack's built-in generator (recommended)

`rack::dsp::MinBlepGenerator<Z, O>` stores a minimum-phase BLEP residual with `Z` zero crossings, oversampled by `O`, in a static table. You call:

- `insertDiscontinuity(p, x)` where `p ∈ [-1, 0]` is *when* the discontinuity happened relative to the current sample (in samples, negative = in the past), and `x` is the signed jump height.
- `process()` once per sample; it returns the accumulated correction and advances the internal ring buffer.

Cost: the insert does `2*Z` fused multiply-adds (32 for `Z=16`) but **only once per discontinuity** — i.e. once per waveform cycle, not once per sample. `process()` is a single read + pointer bump. At 440 Hz you're doing 32 MACs every 109 samples. It's noise.

### Computing the fractional crossing time

After incrementing the phase, if it wrapped past 1.0, the overshoot tells you how far *into* the current sample interval the crossing occurred:

$$p = -\frac{\phi_{\text{wrapped}}}{dt}$$

which lands in $[-1, 0]$. Careful: `insertDiscontinuity` expects `p` relative to the *end* of the current sample, so this convention is what Rack's Fundamental VCO uses directly.

---

## Full `process()` sketch

```c
struct MyOsc {
    float phase = 0.f;
    float triState = 0.f;                       // integrator for triangle
    dsp::MinBlepGenerator<16, 16, float> sawBlep;
    dsp::MinBlepGenerator<16, 16, float> sqrBlep;
    dsp::MinBlepGenerator<16, 16, float> triBlep; // used for BLAMP if desired

    void process(const ProcessArgs& args) {
        float freq = /* from V/oct + FM */;
        float pw   = clamp(pwParam, 0.01f, 0.99f);
        float dt   = freq * args.sampleTime;

        // ---- 1. advance phase, detect discontinuities -------------------
        float prevPhase = phase;
        phase += dt;

        // Saw reset / square rising edge at phase wrap
        if (phase >= 1.f) {
            phase -= 1.f;
            float p = -phase / dt;              // in [-1, 0]
            sawBlep.insertDiscontinuity(p, -2.f);   // saw drops +1 -> -1
            sqrBlep.insertDiscontinuity(p, +2.f);   // square -1 -> +1
        }

        // Square falling edge at phase == pw
        if (prevPhase < pw && phase >= pw) {         // no wrap this sample
            float p = -(phase - pw) / dt;
            sqrBlep.insertDiscontinuity(p, -2.f);
        } else if (phase < prevPhase && phase >= pw) {
            // wrapped and already passed pw within the same sample
            float p = -(phase - pw) / dt;
            sqrBlep.insertDiscontinuity(p, -2.f);
        }

        // ---- 2. naive waveforms + correction ---------------------------
        float saw = 2.f*phase - 1.f;
        saw += sawBlep.process();

        float sqr = (phase < pw) ? 1.f : -1.f;
        sqr += sqrBlep.process();

        // ---- 3. triangle by integrating the band-limited square --------
        // slope = 4*dt per sample gives ±1 amplitude
        triState += 4.f * dt * sqr;
        triState -= 0.0001f * triState;     // gentle leak, or use a DC blocker
        float tri = triState;

        outputs[SAW_OUTPUT].setVoltage(5.f * saw);
        outputs[SQR_OUTPUT].setVoltage(5.f * sqr);
        outputs[TRI_OUTPUT].setVoltage(5.f * tri);
    }
};
```

### Note on the triangle

Integrating the already-band-limited square is the cheapest correct route: the integrator *is* the BLEP→BLAMP conversion, done for free. You get band-limited corners with zero extra work, and it tracks PWM correctly if you ever want an asymmetric triangle.

The downsides are DC drift and the $1/f$ amplitude scaling error if `dt` changes mid-cycle (FM). Two fixes:

1. **Leaky integrator + DC blocker** at ~5–10 Hz — simple, tiny low-frequency droop.
2. **Phase-locked reset**: every cycle wrap, nudge `triState` back toward its ideal value with a one-pole. Keeps amplitude exact under FM.

If you prefer the explicit route, run a second `MinBlepGenerator` on a *derivative* signal, or just use `polyBlamp` from Tier 1 for the corners — slope discontinuities alias far less severely than step discontinuities (their spectrum falls off as $1/f^2$ instead of $1/f$), so PolyBLAMP on the triangle plus minBLEP on saw/square is a very defensible quality/cost split.

---

## CPU: where the real wins are

The BLEP math is not going to be your bottleneck. What will be:

1. **Use `simd::float_4` and process 4 polyphony channels at once.** This is the single biggest factor in VCV CPU meters. Rack's `MinBlepGenerator` is templated on the value type, so `MinBlepGenerator<16, 16, simd::float_4>` works. Branches become `simd::ifelse` / masks.
2. **Don't call `std::floor`, `fmod`, or transcendentals per sample.** For phase wrapping, `if (phase >= 1.f) phase -= 1.f;` is exact and branch-friendly since `dt < 1` always.
3. **Compute frequency with `dsp::approxExp2_taylor5`** rather than `std::pow(2, v)` for V/oct.
4. **Only compute the outputs that are patched.** `if (outputs[X].isConnected())`.
5. **Don't oversample.** 4× oversampling with a decent decimation filter costs ~10× what minBLEP does and buys you less.

`Z = 16, O = 16` gives you a residual table with 16 zero crossings — roughly 90–100 dB of aliasing rejection, which is well past transparent. If you're paranoid, `Z = 32` doubles the per-discontinuity cost and is still trivially cheap.

---

## What about hard sync?

Same machinery, and this is where BLEP really earns its keep. On a sync trigger, you compute the sub-sample position of the trigger edge (linear interpolation between the previous and current sync input value crossing the threshold), compute what the waveform value would be just before the reset and just after, and `insertDiscontinuity` with the difference. Naive hard sync is *brutally* aliased; BLEP'd hard sync is clean. Just make sure you use the same `p` for the phase reset itself so everything is consistent.

---

**Bottom line:** drop `dsp::MinBlepGenerator<16, 16>` in for saw and square, derive the triangle by integrating the corrected square, use SIMD for polyphony. You'll be under 2% CPU for 16 voices and the aliasing floor will be below the noise floor of anything downstream.

Do you want to support hard sync and through-zero FM? Those change the discontinuity bookkeeping quite a bit, and TZFM in particular needs care because the phase can run backwards.