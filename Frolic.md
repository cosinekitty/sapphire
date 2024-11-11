## Frolic

![Frolic](images/frolic.png)

Frolic is a low frequency (and ultra-low frequency) chaotic oscillator.

### Demo video

[Omri Cohen](https://omricohen-music.com/) gives a tutorial of using Frolic and Glee to generate chaotic modulation:

[![Composing with Chaos video](https://img.youtube.com/vi/OxAhUkqFE5c/0.jpg)](https://www.youtube.com/watch?v=OxAhUkqFE5c)

### Mathematical basis

Frolic is based on the [Rucklidge Attractor](http://www.3d-meier.de/tut19/Seite17.html),
a system of three differential equations

$$
\frac{\mathrm{d}x}{\mathrm{d}t} = -2 x + A y - y z
$$

$$
\frac{\mathrm{d}y}{\mathrm{d}t} = x
$$

$$
\frac{\mathrm{d}z}{\mathrm{d}t} = -z + y^2
$$

The right sides of these equations are all linear functions
of $x$, $y$, and $z$. The left sides are all rates of change
of these variables with time.

The equations above convert the current location
of a particle into a velocity vector that points in the direction
the particle must move. No matter where the particle is, Frolic calculates
its velocity vector and uses it to update the particle's position vector.

### Outputs

The outputs are available in two different forms, for convenience:

1. Three separate monophonic output ports for X, Y, and Z respectively.
   These ports support [voltage flipping](VoltageFlipping.md).
2. A polyphonic port P that represents the vector (X, Y, Z) using 3 channels.

### Knobs

**SPEED**: This knob allows varying how fast the simulation runs.
The default speed is 0, but speed may be set anywhere
from &minus;7 to +7. Each unit on this knob's scale represents a factor
of 2. That means when you change SPEED from 0 to +3, it will be $2^3=8$
times faster. Thus the speed can made $2^7=128$ times slower than the default
by turning the SPEED knob all the way to the left, or 128 times faster than
the default by turning it all the way to the right.

However, at the risk of using a **LOT** more CPU time, you can right-click
on the SPEED knob and toggle *Turbo mode*, which adds a +5 bonus to the SPEED value.
This is the same as multiplying the effective speed by 32.

![SPEED button context menu](images/chaos_speed_menu.png)

**CHAOS**: This knob controls the value of the $A$ parameter in the first equation.
The value of $A$ ranges from 3.8 to 6.7, which was experimentally determined
to give a wide range of orbital behavior: from highly ordered to highly chaotic.
In other words, when the CHOAS knob is set all the way down (&minus;1), the
value of $A$ is set to 3.8, resulting in a simple repeated loop.
If CHOAS is all the way right (+1), the value of $A$ is set to 6.7 and the
output of Frolic will be more chaotic.

### CV input
The SPEED and CHOAS knobs have associated attenuverters and CV input ports.
Both can be operated over the full knob range using a wide enough attenuverter setting.

### Graphing with Tricorder

[Tricorder](Tricorder.md) is a 3D oscilloscope designed for Frolic.
If you place a Frolic immediately to the left of a Tricorder, Frolic
will start to feed data into Tricorder, which will plot a 3D graph:

![Frolic and Tricorder](images/frolic_tricorder.png)
