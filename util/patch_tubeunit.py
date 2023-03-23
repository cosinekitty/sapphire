#!/usr/bin/env python3
#
#   patch_tubeunit.py   -   Don Cross <cosinekitty@gmail.com>
#
#   Generates Tube Unit's SVG panel and transparency layers.
#
import sys
from svgpanel import *
from sapphire import *

PANEL_WIDTH = 12


def PentagonOrigin(x, y):
    return (18.5 + x*24.0, 33.0 + y*21.0 - x*10.5)


def AudioPathText():
    PentDx1 = 14.0
    PentDx3 = 16.0
    PentDx2 =  8.0
    PentDy  = 10.5

    # Render a serpentine default-hidden emphasis border around the control groups
    # that affect audio inputs. We will show these only when audio inputs are connected.

    t = ''
    sx, sy = PentagonOrigin(0, 3)
    t += Move(sx - PentDx1, sy + PentDy)
    sx, sy = PentagonOrigin(0, 2)
    t += Line(sx - PentDx1, sy - PentDy)
    t += Line(sx + PentDx2, sy - PentDy)
    sx, sy = PentagonOrigin(1, 2)
    t += Line(sx - PentDx2, sy - PentDy)
    t += Line(sx + PentDx1, sy - PentDy)
    sx, sy = PentagonOrigin(1, 3)
    t += Line(sx + PentDx1, sy - PentDy)
    t += Line(sx - PentDx2, sy - PentDy)
    sx, sy = PentagonOrigin(0, 3)
    t += Line(sx + PentDx2, sy - PentDy)
    t += Line(sx + PentDx3, sy)
    t += Line(sx + PentDx2, sy + PentDy)
    t += 'z'

    path = Element('path')
    path.setAttrib('style', 'fill:#ffffff;fill-opacity:0.2;stroke:#e0e000;stroke-width:0.3;stroke-linecap:round;stroke-linejoin:bevel;stroke-dasharray:none')
    path.setAttrib('d', t)
    panel = Panel(PANEL_WIDTH)
    panel.append(path)
    return panel.svg()


def GenerateMainPanel() -> str:
    svgFileName = '../res/tubeunit.svg'
    panel = Panel(PANEL_WIDTH)

    defs = Element('defs')
    defs.append(LinearGradient('gradient_0', 50.0,  0.0,  0.0, 0.0, '#906be8', '#b242bd'))
    defs.append(LinearGradient('gradient_1', 60.0,  0.0,  0.0, 0.0, '#6d96d6', '#3372d4'))
    defs.append(LinearGradient('gradient_2',  0.0,  0.0, 60.0, 0.0, '#986de4', '#4373e6'))
    defs.append(LinearGradient('gradient_3', 60.0,  0.0,  0.0, 0.0, '#3d81a0', '#26abbf'))
    panel.append(defs)

    pl = Element('g', 'PanelLayer')
    pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))

    # Pentagons that surround all 8 control groups.
    PentDx1 = 14.0
    PentDx3 = 16.0
    PentDx2 =  8.0
    PentDy  = 10.5
    for y in range(4):
        style = 'fill:url(#gradient_{});fill-opacity:1;stroke:#000000;stroke-width:0.1;stroke-linecap:round;stroke-linejoin:bevel;stroke-dasharray:none'.format(y)
        t = ''
        for x in range(2):
            sx, sy = PentagonOrigin(x, y)
            xdir = 1.0 - 2.0*x
            t += Move(sx - xdir*PentDx1, sy - PentDy)
            t += Line(sx + xdir*PentDx2, sy - PentDy)
            t += Line(sx + xdir*PentDx3, sy)
            t += Line(sx + xdir*PentDx2, sy + PentDy)
            t += Line(sx - xdir*PentDx1, sy + PentDy )
            t += 'z '
        pl.append(Element('path').setAttrib('style', style).setAttrib('d', t))

    with Font('Quicksand-Light.ttf') as font:
        # Model name 'tube unit' at top of panel.
        ti = TextItem('tube unit', font, 22.0)
        (dx, _) = ti.measure()
        tp = TextPath(ti, (panel.mmWidth - dx)/2, 0.2)
        tp.setAttrib('style', 'display:inline;stroke:#000000;stroke-width:0.35;stroke-linecap:round;stroke-linejoin:bevel')
        pl.append(tp)
        # Brand name 'sapphire' at bottom of panel.
        ti = TextItem('sapphire', font, 22.0)
        (dx, dy) = ti.measure()
        tp = TextPath(ti, (panel.mmWidth - dx)/2, panel.mmHeight - (dy + 0.5))
        tp.setAttrib('style', 'fill:#000000;stroke:#000000;stroke-width:0.265;stroke-linecap:square')
        pl.append(tp)
        # left input label
        ti = TextItem('L', font, 10.0)
        tp = TextPath(ti, 8.1, 106.5)
        tp.setAttrib('style', 'stroke:#000000;stroke-width:0.25;stroke-linecap:round;stroke-linejoin:bevel')
        pl.append(tp)
        # right input label
        ti = TextItem('R', font, 10.0)
        tp = TextPath(ti, 21.8, 106.5)
        tp.setAttrib('style', 'stroke:#000000;stroke-width:0.25;stroke-linecap:round;stroke-linejoin:bevel')
        pl.append(tp)
        # 'IN' label
        ti = TextItem('IN', font, 10.0)
        tp = TextPath(ti, 14.3, 109.4)
        tp.setAttrib('style', 'stroke:#000000;stroke-width:0.25;stroke-linecap:round;stroke-linejoin:bevel')
        pl.append(tp)
        # 'OUT' label
        ti = TextItem('OUT', font, 10.0)
        tp = TextPath(ti, 36.7, 96.2)
        tp.setAttrib('style', 'stroke:#000000;stroke-width:0.25;stroke-linecap:round;stroke-linejoin:bevel')
        pl.append(tp)
        # left output label
        ti = TextItem('L', font, 10.0)
        tp = TextPath(ti, 57.0, 100.0)
        tp.setAttrib('style', 'stroke:#000000;stroke-width:0.25;stroke-linecap:round;stroke-linejoin:bevel')
        pl.append(tp)
        # right output label
        ti = TextItem('R', font, 10.0)
        tp = TextPath(ti, 57.0, 110.0)
        tp.setAttrib('style', 'stroke:#000000;stroke-width:0.25;stroke-linecap:round;stroke-linejoin:bevel')
        pl.append(tp)

    # Gemstones
    gg = Element('g', 'gemstones')
    gg.setAttrib('style', 'stroke-width:0;fill:#0000ff;stroke:#2e2114;stroke-linecap:square;stroke-opacity:1')
    gemMarginX = 7.5
    gemY = 121.0
    gg.append(SapphireGemstone(gemMarginX, gemY))
    gg.append(SapphireGemstone(panel.mmWidth - (gemMarginX + SapphireGemstone.mmWidth), gemY))
    pl.append(gg)

    inputConnectorPath = Element('path', 'input_connector_path')
    inputConnectorPath.setAttrib('style', 'fill:#141789;stroke:#000000;stroke-width:0.1;stroke-linecap:round;stroke-linejoin:bevel;stroke-dasharray:none')
    inputConnectorPath.setAttrib('d', 'M 9,114.5 L 23,114.5 z')
    pl.append(inputConnectorPath)

    driveConnectorPath = Element('path', 'drive_connector_path')
    driveConnectorPath.setAttrib('style', 'fill:#141789;stroke:#000000;stroke-width:0.1;stroke-linecap:round;stroke-linejoin:bevel;stroke-dasharray:none')
    driveConnectorPath.setAttrib('d', 'M 40.5,107.5 L 52.5,102.5 z L 52.5,112.5 z')
    pl.append(driveConnectorPath)

    panel.append(pl)
    with open(svgFileName, 'wt') as outfile:
        outfile.write(panel.svg())
    print('patch_tubeunit.py: Wrote {}'.format(svgFileName))
    return 0


def GenerateAudioPathLayer() -> int:
    audioPathFileName = '../res/tubeunit_audio_path.svg'
    print('patch_tubeunit.py: Creating {}'.format(audioPathFileName))
    with open(audioPathFileName, 'wt') as outfile:
        outfile.write(AudioPathText())
    return 0


def LabelRJ(text:str, font:Font, i:int, j:int) -> TextPath:
    """Create a right-justified text label."""
    ti = TextItem(text, font, 10.0)
    (w, h) = ti.measure()
    (x, y) = PentagonOrigin(i, j)
    (dx, dy) = (7.0, -12.8)
    return TextPath(ti, x-w+dx, y+(h/2)+dy, text.lower() + '_label')


def LabelLJ(text:str, font:Font, i:int, j:int) -> TextPath:
    """Create a left-justified text label."""
    ti = TextItem(text, font, 10.0)
    (_, h) = ti.measure()
    (x, y) = PentagonOrigin(i, j)
    (dx, dy) = (-7.0, -12.8)
    return TextPath(ti, x+dx, y+(h/2)+dy, text.lower() + '_label')


def GenerateLabelLayer() -> int:
    svgFileName = '../res/tubeunit_labels.svg'
    panel = Panel(PANEL_WIDTH)

    group = Element('g', 'control_labels')
    group.setAttrib('style', 'stroke:#000000;stroke-width:0.25;stroke-linecap:round;stroke-linejoin:bevel')

    with Font('Quicksand-Light.ttf') as font:
        group.append(LabelRJ('AIRFLOW', font, 0, 0))
        group.append(LabelRJ('WIDTH',   font, 0, 1))
        group.append(LabelRJ('DECAY',   font, 0, 2))
        group.append(LabelRJ('ROOT',    font, 0, 3))
        group.append(LabelLJ('VORTEX',  font, 1, 0))
        group.append(LabelLJ('CENTER',  font, 1, 1))
        group.append(LabelLJ('ANGLE',   font, 1, 2))
        group.append(LabelLJ('SPRING',  font, 1, 3))

    panel.append(group)

    with open(svgFileName, 'wt') as outfile:
        outfile.write(panel.svg())

    print('patch_tubeunit.py: Wrote {}'.format(svgFileName))
    return 0


def GenerateVentLayer(name:str) -> int:
    filename = '../res/tubeunit_{}.svg'.format(name.lower())
    (x, y) = (20.1, 16.0)
    with Font('Quicksand-Light.ttf') as font:
        ti = TextItem(name, font, 10.0)
        (w, h) = ti.measure()
        tp = TextPath(ti, x - w/2, y - h/2)
        tp.setAttrib('style', 'stroke:#000000;stroke-width:0.25;stroke-linecap:round;stroke-linejoin:bevel')
    panel = Panel(PANEL_WIDTH)
    panel.append(tp)
    with open(filename, 'wt') as outfile:
        outfile.write(panel.svg())


def Success() -> int:
    print('patch_tubeunit.py: SUCCESS')
    return 0


if __name__ == '__main__':
    sys.exit(
        GenerateMainPanel() or
        GenerateAudioPathLayer() or
        GenerateLabelLayer() or
        GenerateVentLayer('VENT') or
        GenerateVentLayer('SEAL') or
        Success()
    )
