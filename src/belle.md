# Sapphire Belle developement

### Overview

Sapphire Belle is a polyphonic-stereo VCO with
a variety of voice engines.

One of its most important features: **all you need are
polyphonic GATE and V/OCT inputs to play it**.

Belle comes with its own internal envelope behavior,
so you don't need to provide an external envelope.

It also features built-in chaotic LFO modulation of parameters.

### Model selector

Belle has a quantized rotary knob that allows the user to cycle
through the available voice engines. I considered a CV input, but
the problem is that fast changes in CV will cause very annoying UI changes.
This is because changing the engine in turn changes the text labels displayed
above some controls.

Changing the engine also affects what the various parameter control groups mean,
not just their text labels. Therefore, cables that make sense with one
engine may not make any sense with another. This is another reason to keep
the change manual.

The user may need to move cables after changing the engine.
I predict users will prefer to select an engine before wiring cables,
unless they are using built-in chaotic modulation where cables are not needed.

### Input ports

The inputs are polyphonic, allowing up to 16 independent
and overlapping note articulations:

* PITCH (V/OCT)
* GATE

### Output ports

Belle is a polyphonic-stereo module.
It produces up to 16 independent stereo (left, right) voices.
There is left audio output port `L` and a right output audio port `R`.
Each port can produce up to 16 polyphonic channels.

For convenience, there will be a toggle button to "flatten" the output to simple stereo:
a single channel on the `L` output port, and a single channel on the `R`
output port, with the original polyphonic channels summed together.

This will be handy if there is no per-note shaping downstream of Belle.

### Built-in chaotic oscillator

Copy the CHAOS design and logic from Empath.
This requires a fairly large square region of panel space
for the CHAOS controls. I may decide to rearrange these
controls to be as skinny as possible (2 or 3 HP).
I might also omit the CHAOS/LEVEL control.

### Uniform parameters

Uniform parameters have the same meaning across all voices.

* **FREQ** - Just like VCV VCO, a frequency knob centered at C4 with CV input and attenuverter. Partially redundant with the PITCH V/OCT input, it does allow for vibrato or other FM effects. It could also be a way to transpose a melody from one key to another, just by entering a different note like `G#3`.
* **OCT** - One of my favorite techniques is to occasionally shift a generative melody up or down by an octave. This will be a quantized control group.
* **DETUNE** - Allow left and right channel to operate at a slightly different frequency for stereo fun.
* **LEVEL** - Make sure the output gain can go far beyond unity. I want to be able to output especially loud voices, because Sapphire Galaxy can attenuate signals so much.

### Generic parameters

Each voice engine will have 8 control groups available
for tuning and modulating its behavior.

We will put 4 in a column on the left, and 4 in a column on the right.

```text

                        |    FREQ      |   LABEL(0)   |    LABEL(4)
    --- CHAOS ---       |   *--o--O    |   *--o--O    |    *--o--O
    |           |       |              |              |
    |           |       |     OCT      |   LABEL(1)   |    LABEL(5)
    |           |       |   *--o--O    |   *--o--O    |    *--o--O
    |           |       |              |              |
    |           |       |    GAIN      |   LABEL(2)   |    LABEL(6)
    |           |       |   *--o--O    |   *--o--O    |    *--o--O
    |           |       |              |              |
    -------------       |    MODEL     |   LABEL(3)   |    LABEL(7)
                        |     -O-      |   *--o--O    |    *--o--O

```

Each engine will provide names for the generic parameters,
based on the parameter index 0..7.
These will render as strings on the panel display.
The engine **does not** need to know about VCV Rack knobs.
It just needs to answer questions on behalf of a VCV Rack module,
like "What text label should control group 3 have?"

### Fixed numeric range on parameter knobs

In general, the parameter knob units will have generic and uniform ranges.

I'm leaning toward 0 being the default and center position of each knob.
Then the knob's value will range from -1 to +1.

The voltage range [-1, +1] may be useful for having two different
mutually exclusive effects being applied, depending on whether the
parameter ends up with a positive or negative value.

Or zero may represent a reasonable default value inside the
operating range of a single numeric parameter.

Each engine is free to remap and/or interpret the range [-1, +1]
as it sees fit.

### Math function voices

Belle will start with a combination of traditional synth voices based on mathematical functions:

* sine
* triangle
* sawtooth
* square

The last 3 will require additional band-limiting logic to avoid aliasing near discontinuities.

**Envelope voices should maintain consistent positions for anything that acts like ADSR.**

The general principle: whenever you switch from one model to another, keep as many parameter settings consistent as possible. I will reserve the first four parameters for envelope control:

* Attack
* Sustain
* Decay
* Release

Because each engine might have a different concept of, say, "release",
I don't want to have to change the units or numbers on the knobs when
the engine changes.

### Physics voices and beyond

I want to add some innovative, or at least more physical sounding, instruments.
I would like to emulate the physics of one or more of the following:

* bell
* flute
* piano
* cello

### Internal sample and hold

I am often frustrated by the need to add sample-and-hold (S&H) logic to a patch
to keep a pitch signal stable while the entire note plays.
Even when I do use S&H, there can be problems with the pitch
voltage arriving before the sample is latched into memory.
I have had multiple problems with gate delays.

To reduce gate delay problems, I can wait maybe 1 millisecond
after the gate's rising edge before sampling the pitch signal
and beginning the note playback.

The tiny delay in the beginning of playback should not be perceptible,
but it will make using the module much simpler.

Sample-and-hold should be optional, because the original pitch V/OCT
signal might be intentionally modulated during note articulation.

**Conclusion**: add a tiny toggle button near the pitch input port
that designates whether polyphonic sample-and-hold is enabled.

**Optional**: add a right-click menu slider to adjust the sample-and-hold
delay time expressed in integer sample counts.
One millisecond is typically 48 samples, so I could just have
a slider that goes from 0..50 samples.
If I do that, I might as well represent the delay in integer samples internally.
