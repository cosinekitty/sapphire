## Env

![Env](images/env.png)

Env is a combination pitch detector and envelope follower. Given an input audio signal,
Env tries to detect the loudness and musical pitch of a single tone in it.

Env is based on the pitch detector and envelope follower in the Surge XT [TreeMonster](https://library.vcvrack.com/SurgeXTRack/SurgeXTFXTreeMonster) module. Thanks to [BaconPaul](https://github.com/baconpaul/) for ideas and encouragement in this project!

## Controls

There are 4 controls for Env: THRESH, SPEED, FREQ, and RES.
These controls assist making the pitch detector work better on a variety of possible inputs.
From left to right, each control consists of a CV input port,
a smaller attenuverter knob, and a larger control knob.

* **THRESH**: The amplitude threshold of the input signal needed to trigger pitch detection. The range is &minus;94&nbsp;dB to 0&nbsp;dB, with a default of &minus;24&nbsp;dB. Adjust as needed to report pitch for valid notes while rejecting any low-level noise while notes are not playing.
* **SPEED**: How quickly to slew reported output pitches. Lower values result in more stable note detection, but with a trombone-like glide through note changes. Faster values track changes in notes more quickly, but is more susceptible to unwanted variations of pitch.
* **FREQ**: Adjusts the center frequency of a bandpass prefilter that helps narrow in on the intended pitch range of the notes being detected. This can help reject unwanted harmonics from the input audio.
* **RES**: Adjusts the resonance of the prefilter. Higher resonance can help squeeze the passband closer to the expected range of notes in the input audio. Too high a value can cause erroneous detection of notes at or near the center frequency.

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
