# Sapphire: modules for VCV Rack 2

## Requirements
The Sapphire family of modules requires VCV Rack 2.

---

## Installation
The latest release of the Sapphire plugin is available in the
[VCV Rack Library](https://library.vcvrack.com/?brand=Sapphire).

---

## Moots

![Moots](images/moots.png)

The Sapphire Moots module is similar to the
[VCV Mutes](https://library.vcvrack.com/Fundamental/Mutes) module,
but differs in the following ways:

* Instead of toggling between the input signal or a zero volt
  signal like VCV Mutes does, Sapphire Moots toggles between
  the input signal and an unplugged cable.
  Sometimes the result is completely different, as explained
  in more detail below.

* Each instance of the Sapphire Moots module contains 5 independent
  controls instead of the 10 that VCV Mutes has.

* Each Moots control has a push-button like VCV Mutes does,
  but it also has a gate input that allows replacing the button with
  a control voltage. When a signal is connected to a control's gate,
  the control connects the input to the output when the
  gate voltage is +4 volts or higher. When the gate voltage
  drops below +4 volts, the output cable acts like it is
  unplugged. When there is no gate input connected,
  the manual push-button takes control.

I created this module after I discovered one day that the
[Audible Instruments Resonator](https://library.vcvrack.com/AudibleInstruments/Rings)
module didn't behave the way I expected when I placed a VCV Mutes
control in series with the Resonator's `IN` jack. When I muted
the input, I expected the Resonator to act as if the input were
unplugged. Instead, the sound faded to silence and stayed there.

I realized that a 0V signal is not the same thing as an unplugged cable.
I searched for a module that would simulate plugging and unplugging a
cable from a target jack, but could not find one. So I decided to create
one myself. Moots is the result.

The following video demonstrates how Sapphire Moots works,
and how its effects are different in some cases from VCV Mutes:


[![Sapphire Moots demo](https://img.youtube.com/vi/_E_QpehAGMw/0.jpg)](https://www.youtube.com/watch?v=_E_QpehAGMw)

Technical note: Moots simulates unplugging all cables connected
to its output jack by setting the output jack to have zero channels.
To simulate enabled (plugged-in) cables, the jack is set to the
same number of polyphonic channels as the input signal, and the
input signal is copied verbatim to the output jack.
Moots supports the full range of polyphonic channels allowed
by VCV Rack, namely 1 to 16.

When in the quasi-unplugged state, all cables coming out of the
output jack will appear slightly thinner than a normal cable.
When in the enabled state, all such cables will appear the
correct thickness for their polyphony.

Any other module's input jack that receives one of these thin-looking
"unplugged" cables will act as if it is not connected to anything,
because VCV Rack does not distinguish between an unplugged jack
and a jack connected to a zero-channel cable.

### Moots Configuration

The right-click context menu for Moots looks like this:

![Moots menu options](images/moots_menu.png)

The menu contains "Anti-click ramping" options for all five controls.
Anti-click ramping, when enabled, causes the corresponding control to
fade the polyphonic voltages down to zero before disconnecting a cable,
and to fade the voltages up from zero after reconnecting a cable.
The fading is a linear ramp that lasts for 1/400 of a second (2.5 ms).
When a Moots control is used to plug/unplug an audio signal, enabling
anti-click ramping can be useful to prevent clicking or popping sounds.

By default, anti-click is disabled, which is usually a better choice
when using a Moots control for plugging/unplugging control voltage (CV)
signals. When anti-click is disabled, the cable is connected or
disconnected instantly without any fading.

---

## Elastika

Elastika is a stereo synthesis filter based on a physics simulation.
There is a pair of left/right audio input jacks, and a pair of left/right
audio output jacks.

The model includes a network of balls and springs connected in a hexagonal
grid pattern.

### Physics model

There are three kinds of components in the Elastika physics model:

* Anchor: A point in space that stays locked in one location.
  However, two of the anchors are used for injecting input audio into the model.
  These anchors are moved back and forth in response to input voltages.
* Ball: A mobile point mass. A ball has a positive finite mass,
  a 3D position vector, and a 3D velocity vector. A special pair of
  balls determines audio output. The stereo output is based on the physical
  movement of these two output balls.
* Spring: An elastic rod that connects one ball with another, or one anchor with one ball.
  The springs have two parameters that control their behavior: stiffness and span.
  The stiffness parameter adjusts how much force it takes per unit change in the length of
  the spring. Span is the rest length at which the spring exerts zero net force.
  The force is applied equally to both balls, in opposite directions, in accordance
  with Newton's Third Law.

![Elastika model](./images/elastika_model.svg)

This diagram shows the structure of the Elastika physical model.
The magenta spheres around the perimeter are anchors.
The teal spheres on the interior are mobile balls.
The lines show the way anchors and balls are connected.
The two anchors that are used for left and right inputs are indicated by surrounding squares.
The input anchors are forced to move up and down, perpendicular to the plane
of the diagram, in response to applied input voltages.
Similarly, circles indicate the two balls that are used as audio outputs.

### Controls

The following controls have sliders for manual control, along with
attenuverters and control voltage (CV) inputs for automation.

* FRIC: the friction force that slows down vibration in the simulation.
  Low friction is similar to increased reverb.
* STIF: adjusts the stiffness of the springs, which is the amount of
  force per unit length of the spring when stretched or compressed away
  from its rest length. Higher stiffness generally creates higher pitched sounds.
* SPAN: adjusts the rest length of all the springs. It is possible for the span
  to be shorter than the initial distance between the connected balls, which
  results in a bell-like quality. Also, span can be made longer than the
  initial ball distance, in which case the network tends to "explode"
  and vibrate in a more chaotic manner as the surface becomes
  loose and convex.
* TONE: a mixture of how much the output audio is based on position
  of the output balls, and how much is based on velocity.
  Because velocity is the time derivative of position, the tone
  control affects the bass/treble mix of the resulting stereo output.
