## Split/Add/Merge (SAM)

![Split/Add/Merge](images/splitAddMerge.png)

Splits, adds, and/or merges a stereo or 3D polyphonic signal with 2 or 3 monophonic signals.
This is like VCV Merge and VCV Split combined, but only for a maximum of 3 channels.
SplitAddMerge saves patch screen space because it is only 2 HP wide.

I created this module to save surface area in my patches.
Often I want to merge a pair of stereo signals into a single 2-channel polyphonic cable,
so that I can multiply both by the same envelope in a VCA, for example.
Usually I would use VCV Split and VCV Merge, but they are designed for up to 16 channels
and are larger than I need.

Split/Add/Merge is only 2 HP wide, and serves as both a split and a merge module.
However, it works for a maximum of only 3 channels. I chose 3 as the channel limit
to support 3D vectors produced by [Frolic](Frolic.md), [Polynucleus](Polynucleus.md), etc.

## Input Ports

The upper half of the panel includes 4 input ports.

The top 3 input ports are monophonic X, Y, and Z inputs.
Below it is a polyphonic P input.

Usually you plug monophonic cables into X, Y, or Z.
However, if any of these inputs has more than one channel, the channel voltages
are added to produce a single monophonic sum.

The polyphonic P input respects up to 3 channels in the input cable.
Any remaining channels are ignored.
Missing channels are treated as 0&nbsp;V.

## Output Ports

