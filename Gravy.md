## Gravy

![Gravy](images/gravy.png)

### Controls

Gravy is a stereo filter that provides control over the following parameters:

* **FREQ**: the filter's corner frequency. The default setting is 0, which corresponds to the note C5 = 523.251&nbsp;Hz. Each unit on the FREQ dial represents an octave. The dial spans &pm;5 octaves around the note C5.
* **RES**: the filter's resonance, on a scale from 0 (the default) to 1. Higher resonance causes the filter to concentrate the passband closer in to the corner frequency. Toward the upper end of the resonance scale, you can even get sustained oscillations, also known as "ringing".
* **MIX**: A value from 0 to 1 that controls how much of the original, unfiltered stereo signal is included in the output. The default MIX value is 1, which means that the output represents 100% filtered audio. As the value is decreased toward 0, more of the original audio is included in the output. At zero, the output is identical to the input (except for a one-sample delay inherent to all VCV Rack modules).
* **GAIN**: Allows you to make the output louder or quieter, as a convenience. Consider it a built-in VCA.
* **MODE**: Gravy can operate in three modes, selected by clicking the MODE switch:
    * Lowpass: left switch position. Lower frequencies pass through the filter but higher frequencies are suppressed.
    * Bandpass: middle switch position. Frequencies near the corner frequency are passed, but higher and lower frequencies are suppressed.
    * Highpass: right switch position. Frequencies above the corner frequency are passed, but lower frequencies are suppressed.

Each of the controls except for MODE include a CV input port, a smaller attenuverter knob, and a larger manual control knob.

Gravy supports a [low-sensitivity](LowSensitivityAttenuverterKnobs.md) option for each attenuverter.

### Algorithm

Gravy is a state-variable filter as described in the following paper by Andrew Simper (Cytomic). See page 6 of that paper for the equations used in Gravy.

> https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
