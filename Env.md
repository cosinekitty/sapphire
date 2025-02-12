## Env

![Env](images/env.png)

Env is a combination pitch detector and envelope follower. Given an input audio signal,
Env tries to detect the loudness and musical pitch of a single tone in it.

Env is mostly based on the pitch detector and envelope follower in the Surge XT [TreeMonster](https://library.vcvrack.com/SurgeXTRack/SurgeXTFXTreeMonster) module. Both Env and TreeMonster "naively" measure pitch frequency based on time intervals when the filtered waveform's voltage passing through zero. In other words, both count samples between zero-crossings and smooth out the result over time. Env uses a different prefilter (Cytomic with FREQ / RES) than TreeMonster (LO CUT / HI CUT). But the THRESH and SPEED controls are lifted directly from TreeMonster.

Thanks to [BaconPaul](https://github.com/baconpaul/) of Surge XT for the idea for, and support of, this project!

## Controls

There are 4 controls for Env: THRESH, SPEED, FREQ, and RES.
These controls assist making the pitch detector work better on a variety of possible inputs.
From left to right, each control consists of a CV input port,
a smaller attenuverter knob, and a larger control knob.

* **THRESH**: The amplitude threshold of the input signal needed to trigger pitch detection. The range is &minus;94&nbsp;dB to 0&nbsp;dB, with a default of &minus;24&nbsp;dB. Adjust as needed to report pitch for valid notes while rejecting any low-level noise while notes are not playing.
* **SPEED**: How quickly to slew reported output pitches. Lower values result in more stable note detection, but with a trombone-like glide through note changes. Faster values track changes in notes more quickly, but are more susceptible to unwanted variations of pitch (which can sound like birds twittering).
* **FREQ**: Adjusts the center frequency of a bandpass prefilter that helps narrow in on the intended pitch range of the notes being detected. This can help reject unwanted harmonics from the input audio.
* **RES**: Adjusts the resonance of the bandpass prefilter. Higher resonance can help squeeze the passband closer to the expected range of notes in the input audio. Too high a value can cause erroneous detection of notes at or near the center frequency.

## Polyphony

Env is fully polyphonic, meaning all 5 of its input ports (AUDIO and the 4 CV input ports) allow
independent control of up to 16 channels in the ENV and V/OCT output ports.
Each channel of output represents a completely independent combined pitch detector and envelope follower.

Whichever of the 5 input ports has the highest number of channels (1..16) determines the
number of channels in the two output ports ENV and V/OCT.
Any of the remaining 4 input ports having fewer channels will "clone" their final channel's
voltage across all the required output channels.

For example, if a cable connected to AUDIO has 2 channels (stereo),
and a cable connected to the FREQ CV input port has 1 channel,
and there are no other input cables,
then the outputs ENV and V/OCT will each have 2 channels.
Because in this example the FREQ CV signal has only 1 channel, that one channel
will be used to satisfy the second channel also.

If instead you used a 2-channel FREQ CV input cable, each of the two output channels
in the ENV and V/OCT ports would set their respective prefilters using the two CV voltages,
one for each channel in the output.

This system of polyphony treats all the 5 input ports equally, using the rules explained above. As another example, you can put in 1-channel (mono) AUDIO
but perform up to 16 simultaneous pitch/env operations, all with different settings, so long as at least one of your CV input ports has a polyphonic cable attached to it.

## Audio Input

The AUDIO input port receives a cable with one or more channels of audio signal.
Each channel in a polyphonic cable is processed as a completely independent audio signal.

## Env Output

The ENV output port carries an amplitude signal that follows the overall amplitude of
the input audio. The ENV port has as many channels as the input port, with a separate
envelope factor for each channel of the input AUDIO port.

## V/OCT output

The V/OCT output reports the pitch of any detected signal. The zero volt level indicates
a C4 note (261.625&nbsp;Hz). Each unit volt indicates an octave. If no pitch can be detected,
this port may output &minus;10&nbsp;V as a placeholder. Like the ENV port, V/OCT is polyphonic;
there will be one channel of output for each channel of input on the AUDIO port.

## Attenuverters

Env supports [low-sensitivity mode](LowSensitivityAttenuverterKnobs.md) for all four attenuverter knobs.
