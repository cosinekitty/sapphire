#!/usr/bin/env python3
#
#   spectrum_svg.py   -   Don Cross <cosinekitty@gmail.com>
#
#   Generates Sapphire Spectrum's SVG panel.
#
import sys
from typing import Tuple
from svgpanel import *
from sapphire import *

PANEL_WIDTH = 8

def Print(message:str) -> int:
    print('spectrum_svg.py:', message)
    return 0

def Save(panel:Panel, filename:str) -> int:
    panel.save(filename)
    return Print('Wrote: ' + filename)

def GenerateMainPanel() -> int:
    svgFileName = '../res/spectrum.svg'
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(SapphireInsignia(panel, font))
        pl.append(ModelNamePath(panel, font, 'spectrum'))
        pl.append(ControlTextPath(font, 'IN',  10.0, 25.0))
        pl.append(ControlTextPath(font, 'OUT', 10.0, 55.0))
    pl.append(Component('audio_input', 10.0, 20.0))
    pl.append(Component('audio_bands_output', 10.0, 50.0))
    panel.append(pl)
    return Save(panel, svgFileName)

if __name__ == '__main__':
    sys.exit(
        GenerateMainPanel() or
        Print('SUCCESS')
    )
