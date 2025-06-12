## Sapphire modules for VCV Rack 2 &mdash; change log

<table style="width:1000px;">

<tr valign="top">
<th align="center" style="width:140px;" width="140">Date</th>
<th align="center">Version</th>
<th align="left">Notes</th>
</tr>

<tr valign="top">
<td align="center">11 Jun 2025</td>
<td align="center">2.6.000</td>
<td align="left">
    <ul>
        <li>Now you can undo/redo more actions across Sapphire modules. The goal is to support undo/redo for any module state that is saved/loaded with a patch.</li>
        <li>Now when a Sapphire module is initialized, any low-sensitivity attenuverters are reset to normal sensitivity.</li>
        <li><a href="doc/Gravy.md">Gravy</a> no longer makes a click/pop sound when you press the 3-way band mode switch (LP, BP, HP). Gravy now uses a smooth ramp over 50&nbsp;ms to fade out one mode, switches modes, then fades in the new mode for 50&nbsp;ms.</li>
        <li><a href="doc/Pop.md">Pop</a> has two new buttons:
            <ul>
                <li>Pulse mode: triggers/gates.</li>
                <li>Sync polyphonic channels.</li>
            </ul>
        </li>
        <li>Sapphire modules that include output limiters (Elastika, Nucleus, Polynucleus, Sauce, Gravy) have an option in the main menu to enable/disable the warning light. Now, that option also appears in the right-click menu for the output level knob itself.</li>
        <li>Bug fix in <a href="doc/Echo.md">Echo</a>: when you redo creating an Echo module, it no longer automatically creates the Echo Out, because creating Echo Out is already in the future list of redo-able actions. Before this fix, automatically creating Echo Out destroyed the future actions. Now you can just manually redo the creation of Echo Out that was automatic the first time, and keep on redoing future actions.</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">3 Jun 2025</td>
<td align="center">2.5.9</td>
<td align="left">
    <ul>
        <li>Added a module: <a href="doc/Echo.md">Echo</a>: a polyphonic multi-stage delay looper.</li>
        <li>Chaops did not reset the FREEZE button on initialize. This has been fixed.</li>
        <li>Fixed minor polyphony bug in Env: the number of channels in the GAIN control's CV input did not affect the number of output channels. This has been fixed.</li>
        <li>Gravy and Sauce had lowercase hover-text for their control groups, e.g. "resonance CV input". Now they follow the VCV Rack guidelines, e.g. "Resonance CV input".</li>
        <li>Added "neon mode" menu options to make the Sapphire panel glow. This effect is most visible when the room brightness is dim.</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">11 Feb 2025</td>
<td align="center">2.5.8</td>
<td align="left">
    <ul>
        <li>Added new module <a href="doc/Env.md">Env</a>: a polyphonic pitch detector and envelope follower.</li>
        <li><a href="doc/Tin.md">Tin</a> was sending an initial vector (0,0,0) after reset, causing a "spike" to appear from that point. The problem is that there can be a 2-sample delay before we start getting the real vector stream. Eliminated the spike by resetting Tricorder on the first 2 samples we receive.</li>
        <li>Elastika now supports polyphonic stereo using 2-channel input to the L input port, or output from the L output port. These are separate options. See the new checkbox menu options "Enable input stereo splitter" and "Send polyphonic stereo to L output".</li>
        <li>Module documentation is migrating to a new "doc" folder. Copies of those files will remain in the repo root folder for a while, so that older versions of Sapphire can still link to them. When enough people have upgraded, only the "doc" copies will be kept. The goal is to clean up the root directory.</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">5 Jan 2025</td>
<td align="center">2.5.7</td>
<td align="left">
    <ul>
        <li><a href="doc/Elastika.md">Elastika</a> uses 58% of the CPU time it did in earlier versions, thanks to a complete overhaul of its physics engine.</li>
        <li>Elastika now provides a context menu option to run its physics model at a different sample rate from the VCV Rack engine. This feature adds extra CPU overhead when enabled, so it is disabled by default.</li>
        <li>The channel count sliders in Hiss, Pop, and Split/Add/Merge now move in a "snapped" manner instead of smoothly, to help visually emphasize their quantized nature.</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">21 Dec 2024</td>
<td align="center">2.5.6</td>
<td align="left">
    <ul>
        <li><a href="doc/Chaops.md">Chaops</a> bug fix: when MORPH was not zero, the Frolic/Glee/Lark to the right was not using it for the monophonic outputs X, Y, Z.</li>
        <li>Chaops: Added missing help text for the MORPH CV input port.</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">8 Dec 2024</td>
<td align="center">2.5.5</td>
<td align="left">
    <ul>
        <li>Added left-expander module <a href="doc/Chaops.md">Chaops</a> to provide additional functionality to the chaos modules <a href="doc/Frolic.md">Frolic</a>, <a href="doc/Glee.md">Glee</a>, and <a href="doc/Lark.md">Lark</a>.</li>
        <li>Frolic, Glee, and Lark are now 30 times as CPU efficient when Turbo Mode is enabled. The CPU usage warning label on Turbo Mode has been removed.</li>
        <li>Glee and Lark now display the first letter of selected chaos mode on top of the CHAOS knob.</li>
        <li>Frolic, Glee, and Lark now display the letter T on the SPEED knob when Turbo Mode is enabled.</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">13 Nov 2024</td>
<td align="center">2.5.4</td>
<td align="left">
    <ul>
        <li>Added a new chaotic oscillator: <a href="doc/Lark.md">Lark</a>.</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">8 Nov 2024</td>
<td align="center">2.5.3</td>
<td align="left">
    <ul>
        <li>Fixed inaccurate frequency tuning of <a href="doc/Pop.md">Pop</a> when CHAOS=0.</li>
        <li><a href="doc/Pop.md">Pop</a>, <a href="doc/Gravy.md">Gravy</a>, <a href="doc/Sauce.md">Sauce</a>: SPEED/FREQ control voltages operate as V/OCT when the attenuverter is set to +100%. Before this change, V/OCT behavior required the attenuverter set to a lower value.</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">25 Oct 2024</td>
<td align="center">2.5.2</td>
<td align="left">
    <ul>
        <li>Added new filter module <a href="doc/Sauce.md">Sauce</a>. This uses the same filter algorithm as <a href="doc/Gravy.md">Gravy</a> but with an interface optimized for general polyphonic use.</li>
        <li><a href="doc/Gravy.md">Gravy</a> now includes an automatic gain control (AGC) limiter in the right-click menu. The limiter is OFF by default, but can be adjusted to the range 5V..50V. This can be helpful when the output gets hot due to high resonance settings.</li>
        <li>Added a channel count display that shows 1..16 in <a href="doc/Pop.md">Pop</a>, <a href="doc/Hiss.md">Hiss</a>, and <a href="doc/SplitAddMerge.md">SplitAddMerge</a>. This allows you to know the number of output channels just by looking at the panel.</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">11 Sep 2024</td>
<td align="center">2.5.1</td>
<td align="left">
    <ul>
        <li>Added new module <a href="doc/Gravy.md">Gravy</a>: a stereo filter with frequency and resonance controls.</li>
        <li>Galaxy and Gravy support polyphonic stereo input and output.</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">6 Sep 2024</td>
<td align="center">2.5.0</td>
<td align="left">
    <ul>
        <li><a href="doc/SplitAddMerge.md">Split/Add/Merge</a> now allows selecting any output channel count from 1..16. Before, the channel count was always 3. The default is still 3, for backward compatibility.</li>
        <li>Each Sapphire module now links to its individual documentation page instead of the Sapphire table of contents.</li>
        <li><a href="doc/Moots.md">Moots</a> now allows you to toggle the anti-ramping option on each of the 5 groups by right-clicking on the button for that group. You can still right-click the panel and toggle the option that way, although I believe most people will prefer the new method.</li>
        <li>Moots also draws a "ramp" symbol on top of a button when anti-click is enabled. This allows you to see at a glance how all the buttons are configured, without clicking on anything.</li>
        <li>Added a <a href="doc/VoltageFlipping.md">voltage-flip option</a> for X, Y, Z outputs on Frolic, Glee, Pivot, and Rotini.</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">23 Aug 2024</td>
<td align="center">2.4.9</td>
<td align="left">
    <ul>
        <li>New module: <a href="doc/Pop.md">Pop</a> that generates trigger pulses that match the statistical timing of radioactive decay.</li>
        <li>New module: <a href="doc/SplitAddMerge.md">Split/Add/Merge</a> to make working with 2- or 3-channel signals use less real estate in a patch.</li>
        <li>Panels with XYZP port groups now have a hexagon around the P port to make it easier to spot which one is 3D/polyphonic.</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">20 Jul 2024</td>
<td align="center">2.4.8</td>
<td align="left">
    <ul>
        <li>New module: <a href="doc/Pivot.md">Pivot</a> for re-orienting a 3D vector. This is another chaos toy.</li>
        <li><a href="doc/Rotini.md">Rotini</a>: added missing help text for ports.</li>
        <li><a href="doc/Glee.md">Glee</a> now has 4 different chaos modes that you can select from the context menu.</li>
        <li><a href="doc/Glee.md">Glee</a> and <a href="doc/Frolic.md">Frolic</a> now have a Turbo Mode that uses more CPU but allows the oscillators to go up to 32 times faster than before (+5 added to SPEED knob).</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">8 Jul 2024</td>
<td align="center">2.4.7</td>
<td align="left">
    <ul>
        <li>New module: <a href="doc/Rotini.md">Rotini</a>. It helps create more interesting chaotic 3D vector signals for fun CV.</li>
        <li>Galaxy: added option for polyphonic stereo input to a single input port (L or R).</li>
        <li>Galaxy: auto-reset if output becomes non-finite or goes outside 100&nbsp;V absolute value.</li>
        <li>Tube Unit: [Issue #56](https://github.com/cosinekitty/sapphire/issues/56) - added support for <a href="doc/LowSensitivityAttenuverterKnobs.md">low-sensitivity attenuverters</a>.
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">11 Jun 2024</td>
<td align="center">2.4.6</td>
<td align="left">
    <ul>
        <li>Added new module: Galaxy.</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">11 May 2024</td>
<td align="center">2.4.5</td>
<td align="left">
    <ul>
        <li>The following Sapphire modules already supported <a href="doc/LowSensitivityAttenuverterKnobs.md">low-sensitivity attenuverters</a>: Elastika, Nucleus, Polynucleus. Now, in addition to being able to right-click and toggle each attenuverter's sensitivity one at a time, you can right-click on any of the above modules to toggle all of its attenuverters.</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">22 Apr 2024</td>
<td align="center">2.4.4</td>
<td align="left">
    <ul>
        <li>Nucleus and Polynucleus: added a right-click slider for adjusting the DC reject filter's corner frequency. Before this change, both modules had a fixed corner frequency of 30&nbsp;Hz. This is still the default frequency, but now the right-click menu allows you to change the corner frequency to any value from 20&nbsp;Hz to 400&nbsp;Hz.</li>
        <li>Polynucleus: added a CLEAR button that resets the simulation, just like the right-click menu option does.</li>
        <li>Moots: Fixed cosmetic issue <a href="https://github.com/cosinekitty/sapphire/issues/45">#45</a>: when the user changes from gate mode to trigger mode, the text GATE on the panel now changes to TRIGGER.</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">18 Mar 2024</td>
<td align="center">2.4.3</td>
<td align="left">
    <ul>
        <li>Moots: added right-click menu option to toggle between the control port using a gate signal (default) or a trigger-toggle signal.</li>
    </ul>
</td>
</tr>


<tr valign="top">
<td align="center">3 Mar 2024</td>
<td align="center">2.4.2</td>
<td align="left">
    <ul>
        <li>Added a <a href="doc/LowSensitivityAttenuverterKnobs.md">low sensitivity</a> option to attenuverter knobs. Sometimes the adjustments were so tiny to get a desired affect that it was awkward to get it right. Low sensitivity mode allows you to more easily explore those delicate parameters!</li>
        <li>Tricorder's animation is much smoother now. I compensated for jitter caused by VCV Rack calling my animation update function at irregular time intervals.</li>
        <li>Tricorder now allows you to manually adjust its rotation speed using the right-click context menu. You can select any rotation speed from 0.01 RPM to 100 RPM.</li>
        <li>Tricorder: Fixed [Issue #40](https://github.com/cosinekitty/sapphire/issues/40): now you can right-click on the display area and the context menu will appear. Before, you had to click on the top of the panel to get the context menu to appear.</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">22 Feb 2024</td>
<td align="center">2.4.1</td>
<td align="left">
    <ul>
        <li>Added new module <a href="doc/Polynucleus.md">Polynucleus</a>. This module is the same as <a href="Nucleus.md">Nucleus</a>, only with 3-channel polyphonic ports instead of triplets of monophonic ports.</li>
        <li>Added new module <a href="doc/Tout.md">Tout</a>. This module can be placed to the right of a Tricoder to provide the vector that Tricorder is graphing as voltages on output ports. Tout is the inverse of <a href="Tin.md">Tin</a>.</li>
        <li>Added new module <a href="doc/Hiss.md">Hiss</a> for generating unbiased vectors in an N-dimensional space.</li>
        <li>Nucleus <a href="https://github.com/cosinekitty/sapphire/issues/30">issue #30</a>: watch out for infinite/NAN input. If it happens, reset the internal state and keep going.</li>
        <li>Added a "reset simulation" command in the right-click menu that allows you to manually bring the particles to a halt. This can be handy when the simulation gets out of control in a way that does not trigger the NAN auto-reset mentioned above.</li>
        <li>Elastika, Nucleus, and Tube Unit now auto-reset if infinite/NAN output is detected, and indicate the problem by turning the OUTPUT level knob bright pink for 1 second, every time it happens.</li>
        <li>Frolic and Glee now provide an additional polyphonic output with the entire vector represented as a 3-channel port.</li>
        <li>Tin now provides an additional polyphonic input to feed in a 3-channel (X, Y, Z) input vector.</li>
        <li>Tin and Tout now have a LEVEL knob, attenuverter, and CV input. This allows them to adjust the magnitude of vectors that flow through them.</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">7 Feb 2024</td>
<td align="center">2.4.0</td>
<td align="left">
    <ul>
        <li>Added a new module: <a href="doc/Nucleus.md">Nucleus</a>.</li>
        <li>Fixed issues where Elastika and Tube Unit did not persist AGC level or DC reject frequency. They were being reset to their default values every time a patch was loaded.</li>
        <li>Tricorder: the numeric display now includes a triangle that points up or down, depending on whether the value is increasing or decreasing.</li>
        <li>Tricorder: when first connected to a constant 0V input, immediately refresh the coordinate axes. Before this change, the display area stayed blank until the input voltage changed by more than 0.1V. This was confusing, because it looked like there is no input signal!</li>
        <li>Tricorder: Added a new command button `[]` to clear the path and start over with a fresh display.</li>
        <li>Tin: added a trigger input port that clears the Tricorder display.</li>
        <li>The modules that include an Automatic Gain Limiter (Elastika, Tube Unit, Nucleus) now allow the limiter to go as low as 1V, and the default has been changed from 8.5V to 4V. Backward compatibility is preserved: existing patches will still contain their original AGC settings, without any behavior changes.</li>
    </ul>
</td>
</tr>

<tr valign="top">
<td align="center">19 Nov 2023</td>
<td align="center">2.3.1</td>
<td align="left">
    Added bonus module Glee, a chaotic oscillator similar to Frolic but with a different 3D shape.
</td>
</tr>

<tr valign="top">
<td align="center">22 Oct 2023</td>
<td align="center">2.3.0</td>
<td align="left">
<ul>
    Added new modules:
    <ul>
        <li>Frolic: a low frequency chaotic oscillator with 3 outputs</li>
        <li>Tricorder: a 3D animation scope designed for Frolic and Tin</li>
        <li>Tin: sends any (x, y, z) values from input ports into Tricorder</li>
    </ul>
</ul>
</td>
</tr>

<tr valign="top">
<td align="center">12 Mar 2023</td>
<td align="center">2.2.2</td>
<td align="left">
<ul>
    Changes to Tube Unit:
    <li>Fixed display issues when there were multiple Tube Units in a patch, some with audio connected, others without. The yellow pathway around ROOT, DECAY, and ANGLE would sometimes be incorrectly shown/hidden.</li>
    <li>Added VENT/SEAL toggle menu option. Allows inverting the logic for the gate input.</li>
    <li>Added more descriptive hover-text for attenuverter knobs and CV input ports.</li>
</ul>
</td>
</tr>

<tr valign="top">
<td align="center">3 Mar 2023</td>
<td align="center">2.2.1</td>
<td align="left">
<ul>
    Initial release of Sapphire <a href="doc/TubeUnit.md">Tube Unit</a>.
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
Initial release of Sapphire <a href="doc/Elastika.md">Elastika</a>.
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
Initial release of Sapphire <a href="doc/Moots.md">Moots</a>.
</td>
</tr>

</table>
