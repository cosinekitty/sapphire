# Sapphire - a collection of free modules for VCV Rack 2

## Requirements
The Sapphire family of modules requires VCV Rack 2.

---

## Installation
The latest stable release of the Sapphire modules is available in
the [VCV Rack Library](https://library.vcvrack.com/?query=&brand=Sapphire&tag=&license=).

---

## Moots

<img src="images/moots.png"/>

The Moots module is similar to the
[VCV Mutes](https://library.vcvrack.com/Fundamental/Mutes) module,
but differs in the following ways:

* Instead of toggling between the input signal or a zero volt
  signal like VCV Mutes does, Sapphire Moots toggles between
  the input signal and an unplugged cable.
  Sometimes the result is completely different, as explained
  in more detail below.

* Each Moots control has a push button like VCV Mutes does,
  but it also has a gate input that allows overriding the button with
  a control voltage. When a signal is connected to a gate,
  the moot control connects the input to the output when the
  gate voltage is greater than +4.0 volts; otherwise the output
  cable acts like it is unplugged.

