#!/usr/bin/env python3
#
#   cantilever_svg.py   -   Don Cross <cosinekitty@gmail.com>
#
#   Generates SVG panel and widget positions for Cantilever module.
#
import sys
from typing import Tuple
from svgpanel import *
from sapphire import *

PANEL_WIDTH = 12

def Print(message:str) -> int:
    print('tubeunit_svg.py:', message)
    return 0


def Save(panel:Panel, filename:str) -> int:
    panel.save(filename)
    return Print('Wrote: ' + filename)


def GenerateMainPanel() -> int:
    svgFileName = '../res/cantilever.svg'
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(SapphireInsignia(panel, font))
        pl.append(ModelNamePath(panel, font, 'cantilever'))
    panel.append(pl)
    cl = ControlLayer()
    cl.append(Component('audio_left_output',  45.0,  90.0))
    cl.append(Component('audio_right_output', 45.0, 100.0))
    cl.append(Component('stretch_knob', 30.0, 30.0))
    cl.append(Component('bend_knob', 30.0, 45.0))
    panel.append(cl)
    return Save(panel, svgFileName)


if __name__ == '__main__':
    sys.exit(
        GenerateMainPanel() or
        Print('SUCCESS')
    )
