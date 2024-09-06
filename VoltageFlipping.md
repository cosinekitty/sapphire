## Voltage flipping X, Y, Z output ports

The following Sapphire modules allow you to toggle the polarity
of the voltage on the X, Y, and/or Z output ports:

* [Frolic](Frolic.md)
* [Glee](Glee.md)
* [Pivot](Pivot.md)
* [Rotini](Rotini.md)

All of these modules have XYZP output ports, where the XYZ ports are monophonic
and the P port has 3 channels.

In each case, the X, Y, and Z output ports of these modules allow right-clicking
to toggle an option labeled "Flip voltage polarity".

![Flip voltage polarity menu option](images/voltage_flip.png)

When you right-click on any X, Y, Z output port and enable
voltage flipping, the voltage coming out of that port will be inverted.
For example, if X was sending +5.3V, it would now immediately become -5.3V.

Toggled voltage values are reflected in the polyphonic output port P.
In the example above, if P = [+5.3, -1.7, +0.2], after toggling the X polarity,
P would be [-5.3, -1.7, +0.2].

Toggled voltage values are also reflected in the vector that these modules
send to any [Tricorder](Tricorder.md)-like module immediately to their right.

The purpose of voltage flipping is to allow the patch designer to select from
$2^3=8$ different variations on curves produced by these modules.
This greatly multiplies the number of ways that chaotic vector signals
can be created and combined with each other.
