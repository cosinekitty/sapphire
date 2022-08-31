## Sapphire modules for VCV Rack 2 &mdash; change log

<table style="width:1000px;">

<tr valign="top">
<th align="center" style="width:140px;" width="140">Date</th>
<th align="center">Version</th>
<th align="left">Notes</th>
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
Initial release of Sapphire Moots.
</td>
</tr>

</table>