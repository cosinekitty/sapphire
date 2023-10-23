## Tricorder

![Tricorder](images/frolic_tricorder.png)

Tricorder is a 3D oscilloscope designed for displaying
animations of a triplet of low-frequency voltages.

Tricorder was designed specifically as an expander module
for [Frolic](Frolic.md), but it also allows graphing
any three input voltages you want, using [Tin](Tin.md)
instead of Frolic to the left of Tricorder.

The photo at the top is an example of Frolic placed to the immediate
left side of Tricorder, which causes them to "connect".
When connected, Frolic sends its (X, Y, Z) outputs
into Tricorder for graphing.

Here is an example of using [Tin](Tin.md) instead:

![Tin and Tricorder](images/tin_tricorder.png)

Three different sine waves are fed through Tin into Tricorder
to be graphed. The result is a 3D [Lissajous curve](https://en.wikipedia.org/wiki/Lissajous_curve).
