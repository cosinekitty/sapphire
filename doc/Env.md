## Env

![Env](images/env.png)

Env is a combination pitch detector and envelope follower. Given an input audio signal,
Env tries to detect the loudness and musical pitch of a single tone in it.

Env is mostly based on the pitch detector and envelope follower inside the Surge XT
[TreeMonster](https://library.vcvrack.com/SurgeXTRack/SurgeXTFXTreeMonster) module.
Both Env and TreeMonster "naively" measure pitch frequency based on time intervals
when the filtered waveform's voltage passes through zero.
In other words, both count samples between zero-crossings and smooth out the result over time to estimate pitch.
Env uses a different prefilter (based on [Sauce](Sauce.md) with FREQ and RES controls) than TreeMonster's LO CUT and HI CUT controls.
But the THRESH and SPEED controls are lifted directly from TreeMonster.

Thanks to [BaconPaul](https://github.com/baconpaul/) of Surge XT for the idea for, and support of, this project!

### Demo video

Here I create a drone with [Tube Unit](TubeUnit.md), then use Env to extract a pitch and envelope CV signal from both stereo audio channels. I reconstruct a sawtooth wave from that pitch and envelope. Mix it all together and add some reverb! (Note: this video was made with a pre-release version of Env with a different panel design.)

[![Sapphire Env demo](https://img.youtube.com/vi/P8HinJX07t4/0.jpg)](https://www.youtube.com/watch?v=P8HinJX07t4)

### Controls

There are 5 controls for Env: THRESH, SPEED, FREQ, RES, and GAIN.
These controls assist making the pitch detector work better on a variety of possible inputs.
From left to right, each control consists of a CV input port,
a smaller attenuverter knob, and a larger control knob.

* **THRESH**: The amplitude threshold of the input signal needed to trigger pitch detection. The range is &minus;94&nbsp;dB to 0&nbsp;dB, with a default of &minus;30&nbsp;dB. Adjust as needed to report pitch for valid notes while rejecting any low-level noise while notes are not playing.
* **SPEED**: How quickly to slew reported output pitches. Lower values result in more stable note detection, but with a trombone-like glide through note changes. Faster values track changes in notes more quickly, but are more susceptible to unwanted variations of pitch (which can sound like birds twittering).
* **FREQ**: Adjusts the center frequency of a bandpass prefilter that helps narrow in on the intended pitch range of the notes being detected. This can help reject unwanted harmonics from the input audio.
* **RES**: Adjusts the resonance of the bandpass prefilter. Higher resonance can help squeeze the passband closer to the expected range of notes in the input audio. Too high a value can cause erroneous detection of notes at or near the center frequency.
* **GAIN**: The GAIN control adjusts the output level of the ENV port by a factor anywhere from 0 to 16. This knob displays this range in decibels as &minus;&infin;&nbsp;dB to +24&nbsp;dB. The default factor is 1, represented as 0&nbsp;dB on the knob.

### Polyphony

Env is fully polyphonic, meaning all 6 of its input ports (AUDIO and the 5 CV input ports) allow
independent control of up to 16 channels in the output ports ENV, GATE, and V/OCT.
Each channel of output represents a completely independent combined pitch detector and envelope follower.

Whichever of the 6 input ports has the highest number of channels (1..16) determines the
number of channels in the output ports ENV, GATE, and V/OCT.
Any of the remaining 5 input ports having fewer channels will "clone" their final channel's
voltage across all the required output channels.

For example, if a cable connected to AUDIO has 2 channels (stereo),
and a cable connected to the FREQ CV input port has 1 channel,
and there are no other input cables,
then the outputs ENV, GATE, and V/OCT will each have 2 channels.
Because in this example the FREQ CV signal has only 1 channel, that one channel
will be used to satisfy the second channel also.

If instead you used a 2-channel FREQ CV input cable, each of the two output channels
in would set their respective prefilters using the two CV voltages,
one for each channel in the output.

This system of polyphony treats all of the input ports equally, using the rules explained above.
As another example, you can put in 1-channel (mono) AUDIO
but perform up to 16 simultaneous pitch/env operations, all with different settings, so long as at least one of your CV input ports has a polyphonic cable attached to it.

### Audio Input

The AUDIO input port receives a cable with 1..16 channels of audio signal.

### Env Output

The ENV output port is polyphonic, and each of its channels represents
the overall loudness (peak amplitude) of the input signal for that same channel.
It can be used to control the amplitude of a VCA, or for any other desired control
based on the input loudness.

### Gate Output

The GATE output port is polyphonic. GATE is related to the ENV port,
but instead of a linear envelope signal, each channel of the output is a gate
voltage that is either 0&nbsp;V or +10&nbsp;V.
You can use GATE to signal either when a pitch is detected or when a pitch is NOT detected.

When you right-click on the GATE port, you will see the following context menu:

![ENV port context menu](images/env_output_modes.png)

The option "GATE port output mode" provides the following two options:

* **Gate when pitch detected**: Send a high gate of +10&nbsp;V when a pitch is being detected. Otherwise it sends 0&nbsp;V. THRESH controls the minimum input audio level at which we send a high gate.

* **Gate when quiet**: Just like the previous option, only with the voltages reversed: sends 0&nbsp;V when a pitch is detected, otherwise +10&nbsp;V.

### V/OCT output

The V/OCT output is polyphonic, and reports the pitch of any detected signal on each input audio channel.
The zero volt level indicates a C4 note (261.625&nbsp;Hz).
Each unit volt indicates an octave away from C4.
If no pitch has yet been detected, this port will output &minus;10&nbsp;V as a placeholder.

### Attenuverters

Env supports [low-sensitivity mode](LowSensitivityAttenuverterKnobs.md) for all of its attenuverter knobs.
