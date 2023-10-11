#!/usr/bin/env python3
#
#   voltjerky_svg.py  -  Don Cross <cosinekitty@gmail.com>
#
#   Generates the SVG panel design for Sapphire Volt Jerky.
#
import sys
from svgpanel import *
from sapphire import *

PANEL_WIDTH = 4


def Print(message:str) -> int:
    print('chaos_svg.py:', message)
    return 0


def Save(panel:Panel, filename:str) -> int:
    panel.save(filename)
    return Print('Wrote: ' + filename)


def GenerateMainPanel(name: str) -> int:
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
        pl.append(Component('x_output', xmid, outputPortY1 + 0*outputPortDY))
        pl.append(Component('y_output', xmid, outputPortY1 + 1*outputPortDY))
        pl.append(Component('z_output', xmid, outputPortY1 + 2*outputPortDY))
        pl.append(Component('speed_knob', xmid, 30.0))
        pl.append(Component('chaos_knob', xmid, 60.0))
    panel.append(pl)
    return Save(panel, svgFileName)


if __name__ == '__main__':
    sys.exit(
        GenerateMainPanel('frolic') or
        Print('SUCCESS')
    )
