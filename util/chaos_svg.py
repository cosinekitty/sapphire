#!/usr/bin/env python3
#
#   chaos_svg.py  -  Don Cross <cosinekitty@gmail.com>
#
#   Generates the SVG panel design for Sapphire Frolic.
#
import sys
from svgpanel import *
from sapphire import *


def Print(message:str) -> int:
    print('chaos_svg.py:', message)
    return 0


def Save(panel:Panel, filename:str) -> int:
    panel.save(filename)
    return Print('Wrote: ' + filename)


def GenerateChaosPanel(name: str) -> int:
    PANEL_WIDTH = 4
    svgFileName = '../res/{}.svg'.format(name)
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
        pl.append(CenteredGemstone(panel))
        pl.append(ModelNamePath(panel, font, name))
        outputPortY1 = 90.0
        outputPortDY = 10.0
        xmid = panel.mmWidth / 2
        controls = ControlLayer()
        pl.append(controls)
        controls.append(Component('x_output', xmid, outputPortY1 + 0*outputPortDY))
        controls.append(Component('y_output', xmid, outputPortY1 + 1*outputPortDY))
        controls.append(Component('z_output', xmid, outputPortY1 + 2*outputPortDY))
        ySpeedKnob = 27.0
        yChaosKnob = 59.0
        dxControlGroup = 5.0
        dyControlGroup = 11.0
        dyControlText = -11.6
        controls.append(Component('speed_knob', xmid, ySpeedKnob))
        controls.append(Component('speed_atten', xmid - dxControlGroup, ySpeedKnob + dyControlGroup))
        controls.append(Component('speed_cv', xmid + dxControlGroup, ySpeedKnob + dyControlGroup))
        controls.append(Component('chaos_knob', xmid, yChaosKnob))
        controls.append(Component('chaos_atten', xmid - dxControlGroup, yChaosKnob + dyControlGroup))
        controls.append(Component('chaos_cv', xmid + dxControlGroup, yChaosKnob + dyControlGroup))
        pl.append(ControlTextPath(font, 'SPEED', xmid - 5.5, ySpeedKnob + dyControlText))
        pl.append(ControlTextPath(font, 'CHAOS', xmid - 6.0, yChaosKnob + dyControlText))
        lineGroup = Element('g', 'connector_lines').setAttrib('style', 'fill:none;stroke:#000000;stroke-width:0.25;stroke-linecap:round;stroke-linejoin:bevel;stroke-dasharray:none')
        pl.append(lineGroup)
        lineGroup.append(LineElement(xmid, ySpeedKnob, xmid - dxControlGroup, ySpeedKnob + dyControlGroup))
        lineGroup.append(LineElement(xmid, ySpeedKnob, xmid + dxControlGroup, ySpeedKnob + dyControlGroup))
        lineGroup.append(LineElement(xmid, yChaosKnob, xmid - dxControlGroup, yChaosKnob + dyControlGroup))
        lineGroup.append(LineElement(xmid, yChaosKnob, xmid + dxControlGroup, yChaosKnob + dyControlGroup))
        xOutLabel = xmid - 3.9
        yOutLabel = outputPortY1 - 10.0
        pl.append(ControlTextPath(font, 'OUT', xOutLabel, yOutLabel, 'out_label'))
        xPortLabel = xmid - 7.5
        yPortLabel = outputPortY1 - 2.4
        pl.append(ControlTextPath(font, 'X',  xPortLabel, yPortLabel + 0*outputPortDY, 'port_label_x'))
        pl.append(ControlTextPath(font, 'Y',  xPortLabel, yPortLabel + 1*outputPortDY, 'port_label_y'))
        pl.append(ControlTextPath(font, 'Z',  xPortLabel, yPortLabel + 2*outputPortDY, 'port_label_z'))
    panel.append(pl)
    return Save(panel, svgFileName)


def GenerateTricorderPanel() -> int:
    PANEL_WIDTH = 25
    svgFileName = '../res/tricorder.svg'
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
        pl.append(SapphireModelInsignia(panel, font, 'tricorder'))
    panel.append(pl)
    return Save(panel, svgFileName)


if __name__ == '__main__':
    sys.exit(
        GenerateChaosPanel('frolic') or
        GenerateTricorderPanel() or
        Print('SUCCESS')
    )
