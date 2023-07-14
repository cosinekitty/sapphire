#!/usr/bin/env python3
#
#   waterpool_svg.py   -   Don Cross <cosinekitty@gmail.com>
#
#   Generates WaterPool's SVG panel.
#
import sys
from typing import Tuple
from svgpanel import *
from sapphire import *

PANEL_WIDTH = 12

def Print(message:str) -> int:
    print('waterpool_svg.py:', message)
    return 0


def Save(panel:Panel, filename:str) -> int:
    panel.save(filename)
    return Print('Wrote: ' + filename)


def KnobPos(x:int, y:int) -> Tuple[float,float]:
    return (20.5 + x*20.0, 34.0 + y*21.0 - x*10.5)


def GenerateMainPanel() -> int:
    svgFileName = '../res/waterpool.svg'
    panel = Panel(PANEL_WIDTH)

    pl = Element('g', 'PanelLayer')
    pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))

    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(SapphireInsignia(panel, font))
        pl.append(ModelNamePath(panel, font, 'water pool'))
        pl.append(ControlTextPath(font, 'IN',  14.3, 109.4))
        pl.append(ControlTextPath(font, 'L',    8.1, 106.5))
        pl.append(ControlTextPath(font, 'R',   21.8, 106.5))
        pl.append(ControlTextPath(font, 'OUT', 36.7,  96.2))
        pl.append(ControlTextPath(font, 'L',   57.0, 100.0))
        pl.append(ControlTextPath(font, 'R',   57.0, 110.0))

    inputConnectorPath = Element('path', 'input_connector_path')
    inputConnectorPath.setAttrib('style', CONNECTOR_LINE_STYLE)
    inputConnectorPath.setAttrib('d', 'M 9,114.5 L 23,114.5 z')
    pl.append(inputConnectorPath)

    driveConnectorPath = Element('path', 'drive_connector_path')
    driveConnectorPath.setAttrib('style', CONNECTOR_LINE_STYLE)
    driveConnectorPath.setAttrib('d', 'M 40.5,107.5 L 52.5,102.5 z L 52.5,112.5 z')
    pl.append(driveConnectorPath)

    pl.append(Component('audio_left_input',   9.0, 114.5))
    pl.append(Component('audio_right_input', 23.0, 114.5))

    outJackDx = 12.0
    outJackDy =  5.0
    (levelKnobX, levelKnobY) = KnobPos(1, 4)
    (ljx, ljy) = (levelKnobX + outJackDx, levelKnobY - outJackDy)
    (rjx, rjy) = (levelKnobX + outJackDx, levelKnobY + outJackDy)

    pl.append(Component('audio_left_output',  ljx, ljy))
    pl.append(Component('audio_right_output', rjx, rjy))

    panel.append(pl)
    return Save(panel, svgFileName)


if __name__ == '__main__':
    sys.exit(
        GenerateMainPanel() or
        Print('SUCCESS')
    )
