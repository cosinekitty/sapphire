## Sapphire modules for VCV Rack 2 &mdash; change log

<table style="width:1000px;">

<tr valign="top">
<th align="center" style="width:140px;" width="140">Date</th>
<th align="center">Version</th>
<th align="left">Notes</th>
</tr>

<tr valign="top">
<td align="center">3 Mar 2023</td>
<td align="center">2.2.1</td>
<td align="left">
<ul>
    Initial release of Sapphire <a href="TubeUnit.md">Tube Unit</a>.
</ul>
</td>
</tr>

<tr valign="top">
<td align="center">18 Nov 2022</td>
<td align="center">2.1.3</td>
<td align="left">
<ul>
    <li>
        Fixed a bug in the output level knob.
        It was incorrectly scaling the output level far too sensitively.
        Now the decibel value displayed by the knob exactly matches its actual amplification.
    </li>
    <li>
        Elastika now includes an adjustable output limiter.
        Before this was added, output levels could get VERY hot.
        The limiter mostly prevents the output amplitude from exceeding an adjustable level,
        although there can be brief transients after sudden changes.
        By default, the limiter is enabled and set to a threshold of 8.5V.
        Using Elastika's right-click context menu, you can adjust the limiter
        threshold anywhere from 5V to 10V. If you slide the limiter all the way
        to the right, it turns the limiter completely off.
        Disabling the limiter like this can result in extreme output voltages in
        some cases, but it could make sense for patches where Elastika's output
        is attenuated externally.
    </li>
</ul>
</td>
</tr>

<tr valign="top">
<td align="center">13 Nov 2022</td>
<td align="center">2.1.2</td>
<td align="left">
Improvements based on community feedback.
<ul>
    <li>Added bypass support for Elastika and Moots.</li>
    <li>Sum polyphonic inputs (audio and CV) in Elastika and Moots.</li>
    <li>Elastika checks for NAN outputs every quarter of a second and auto-recovers if found. (One user reported NAN output, but was not able to reproduce it, and I haven't see it happen yet.)</li>
    <li>Added right-click menu slider in Elastika to adjust output DC reject corner frequency. Default is 20 Hz, but can go up to 400 Hz.</li>
    <li>Prevent randomization of Elastika's input and output level knobs.</li>
</ul>
</td>
</tr>

<tr valign="top">
<td align="center">8 Nov 2022</td>
<td align="center">2.1.1</td>
<td align="left">
Initial release of Sapphire <a href="Elastika.md">Elastika</a>.
</td>
</tr>

<tr valign="top">
<td align="center">30 Aug 2022</td>
<td align="center">2.0.1</td>
<td align="left">
Made the following fixes/improvements to Sapphire Moots:
<ul>
    <li>
        <a href="https://github.com/cosinekitty/sapphire/issues/1">Issue #1</a>:
        When a controller is turned off, no longer make the push-button
        completely dark. Instead, give it a very dim glow, so that it can still
        be seen when the room brightness is very dark.
    </li>
    <li>
        <a href="https://github.com/cosinekitty/sapphire/issues/2">Issue #2</a>:
        Added 5 options to the context menu, one for each controller, to
        enable/distable anti-click ramping. When enabled, the controller applies
        a 2.5 millisecond linear ramp before unplugging a cable and after plugging it back in.
        When disabled, the cable is plugged/unplugged instantly, which is usually
        a better choice for control voltages.
    </li>
    <li>
        <a href="https://github.com/cosinekitty/sapphire/issues/3">Issue #3</a>:
        Debounce gate voltages using hysteresis.
        Turn a controller on when its gate reaches 1.0V or higher.
        Turn it back off when the gate descends to 0.1V or lower.
    </li>
</ul>
</td>
</tr>

<tr valign="top">
<td align="center">19 Aug 2022</td>
<td align="center">2.0.0</td>
<td align="left">
Initial release of Sapphire <a href="Moots.md">Moots</a>.
</td>
</tr>

</table>