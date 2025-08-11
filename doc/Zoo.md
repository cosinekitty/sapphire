## Zoo

![Zoo](images/zoo.png)

Zoo is a low frequency (and ultra-low frequency) chaotic oscillator
that is programmable by the user with 3 configurable math formulas.

If you don't like algebra, Zoo also comes with plenty of example factory presets,
each of which generates a distinctive chaotic output:

![Zoo factory presets](images/zoo_factory_presets.png)

The default is the [Rössler attractor](https://en.wikipedia.org/wiki/R%C3%B6ssler_attractor).

### Demo video

### Mathematical basis

I created [Frolic](Frolic.md), [Glee](Glee.md), and [Lark](Lark.md) before Zoo.
They are all chaotic oscillators, each using a different attractor formula.
See the documentation links for the exact formulas used in each case.

All three attractor formulas follow the same pattern. In every case,
you model a particle in 3D space whose position vector is $(x, y, z)$
and whose velocity vector is $(\dot{x}, \dot{y}, \dot{z})$.
In every time step,

$$
\dot{x} = f(x,y,z)
$$

$$
\dot{y} = g(x,y,z)
$$

$$
\dot{z} = h(x,y,z)
$$

The functions $f$, $g$, and $h$ are particular to each kind of chaotic oscillator.

Zoo is a generalization of chaotic oscillators that use a particle's position to determine the direction and speed (the *velocity vector*) it must have at that moment in time. The velocity vector in turn causes a small update in position for each small time increment of the VCV Rack simulation.

### Entering formulas

Zoo's front panel looks just like Frolic, Glee, and Lark. All of the programmability options are in the right-click context menu:

![Zoo context menu](images/zoo_context_menu.png)

The items `vx` (for $\dot{x}$), `vy` (for $\dot{y}$), `vz` (for $\dot{z}$) are formula editors that allow you to enter in a simple algebra expression for each component of the velocity vector. You can use the operators `+`, `-`, `*` (multiply), and `/` (divide). You can also use `^` for exponentiation, but the exponent must be constant 1..4.

You are allowed to use the variables `x`, `y`, `z` for the particle's position, plus up to four adjustable parameters `a`, `b`, `c`, `d`.

To find formulas, try [this list](http://www.3d-meier.de/tut19/Seite0.html) (German language). For example, [here is where I found the Halvorsen attractor formulas](http://www.3d-meier.de/tut19/Seite13.html).

### Chaos mode

When you right-click on the CHAOS knob (or in the main context menu shown above), you will see the following list of choices:

* Alpha (a)
* Bravo (b)
* Charlie (c)
* Delta (d)

However, you will **NOT** see one of the above if you never use the associated parameter `a`, `b`, `c`, `d` in any of the 3 formulas `vx`, `vy`, `vz`.

When you select a chaos mode like Charlie, it means the CHAOS knob now is controlling the parameter `c`.

### Chaops on the left helps!

If you are programming a new chaotic oscillator in Zoo, I strongly recommend adding [Chaops](Chaops.md) to the left and [Tricorder](Tricorder.md) to the right:

![Zoo, Chaops, Tricorder combo](images/zoo_chaops_tricorder.png)

Chaops provides the following helpful functions for developing new Zoo presets:
* Chaops allows you to **freeze Zoo** while you are making changes.
* Chaops also allows you to **reset Zoo** if the numerical simulation goes out of control.
* As a detail, using Chaops to store a location in memory cell 0 causes that cell to be the starting location when a saved preset is later loaded.

And of course, Tricoder lets you see the behavior of your custom chaotic attractor in real time.
