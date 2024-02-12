## Glee

![Glee](images/glee.png)

Glee is a low frequency (and ultra-low frequency) chaotic oscillator.

### Mathematical basis

Glee is based on the [Aizawa Attractor](http://www.3d-meier.de/tut19/Seite3.html),
a system of three differential equations

$$
\frac{\mathrm{d}x}{\mathrm{d}t} = (z - b)x - d y
$$

$$
\frac{\mathrm{d}y}{\mathrm{d}t} = d x + (z - b)y
$$

$$
\frac{\mathrm{d}z}{\mathrm{d}t} = c + a z - \frac{z^3}{3} - (x^2+y^2)(1 + e z) + f z x^3
$$

The right sides of these equations are all linear functions
of $x$, $y$, and $z$. The left sides are all rates of change
of these variables with time.

The equations above convert the current location
of a particle into a velocity vector that points in the direction
the particle must move. No matter where the particle is, Glee calculates
its velocity vector and uses it to update the particle's position vector.

### Outputs

The outputs are available in two different forms, for convenience:

1. Three separate monophonic output ports for X, Y, and Z respectively.
2. A polyphonic port P that represents the vector (X, Y, Z) using 3 channels.

### Knobs

**SPEED**: This knob allows varying how fast the simulation runs.
The default speed is 0, but speed may be set anywhere
from &minus;7 to +7. Each unit on this knob's scale represents a factor
of 2. That means when you change SPEED from 0 to +3, it will be $2^3=8$
times faster. Thus the speed can made $2^7=128$ times slower than the default
by turning the SPEED knob all the way to the left, or 128 times faster than
the default by turning it all the way to the right.

**CHAOS**: This knob controls the value of the $a$ parameter in the first equation.
The value of $c$ ranges from 0.5941 to 0.6117, which was experimentally determined
to give a wide range of orbital behavior while remaining stable.

### CV input
The SPEED and CHOAS knobs have associated attenuverters and CV input ports.
Both can be operated over the full knob range using a wide enough attenuverter setting.

### Graphing with Tricorder

[Tricorder](Tricorder.md) is a 3D oscilloscope designed for compatibility with Glee.
If you place a Glee immediately to the left of a Tricorder, Glee
will start to feed data into Tricorder, which will plot a 3D graph:

![Glee and Tricorder](images/glee_tricorder.png)
