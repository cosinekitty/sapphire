#!/usr/bin/env python3
#
#   voltjerky_svg.py  -  Don Cross <cosinekitty@gmail.com>
#
#   Generates the SVG panel design for Sapphire Volt Jerky.
#
import sys
from svgpanel import *
from sapphire import *

PANEL_WIDTH = 12


def Print(message:str) -> int:
    print('voltjerky_svg.py:', message)
    return 0


def Save(panel:Panel, filename:str) -> int:
    panel.save(filename)
    return Print('Wrote: ' + filename)


def GenerateMainPanel() -> int:
    svgFileName = '../res/voltjerky.svg'
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
        pl.append(SapphireInsignia(panel, font))
        pl.append(ModelNamePath(panel, font, 'volt jerky'))
        outputPortY1 = 90.0
        outputPortDY = 10.0
        pl.append(Component('w_output', 40.0, outputPortY1 + 0*outputPortDY))
        pl.append(Component('x_output', 40.0, outputPortY1 + 1*outputPortDY))
        pl.append(Component('y_output', 40.0, outputPortY1 + 2*outputPortDY))
    panel.append(pl)
    return Save(panel, svgFileName)

if __name__ == '__main__':
    sys.exit(
        GenerateMainPanel() or
        Print('SUCCESS')
    )
