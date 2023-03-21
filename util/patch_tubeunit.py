#!/usr/bin/env python3
#
#   patch_tubeunit.py   -   Don Cross <cosinekitty@gmail.com>
#
#   Utility for calculating SVG coordinates for Tube Unit's
#   decorative artwork.
#
import sys
from svgpanel import *

def PentagonOrigin(x, y):
    return (18.5 + x*24.0, 33.0 + y*21.0 - x*10.5)

def Move(x, y):
    return 'M {:0.2f},{:0.2f} '.format(x, y)

def Line(x, y):
    return 'L {:0.2f},{:0.2f} '.format(x, y)

def ArtworkText():
    PentDx1 = 14.0
    PentDx3 = 16.0
    PentDx2 =  8.0
    PentDy  = 10.5
    t = ''

    # Render the pentagons that surround all 8 control groups.
    for y in range(4):
        gradientId = 'gradient_{}'.format(y)
        t += '\n'
        t += '    <path style="fill:url(#' + gradientId + ');fill-opacity:1;stroke:#000000;stroke-width:0.1;stroke-linecap:round;stroke-linejoin:bevel;stroke-dasharray:none"\n'
        t += '        d="'
        for x in range(2):
            sx, sy = PentagonOrigin(x, y)
            xdir = 1.0 - 2.0*x
            t += Move(sx - xdir*PentDx1, sy - PentDy)
            t += Line(sx + xdir*PentDx2, sy - PentDy)
            t += Line(sx + xdir*PentDx3, sy)
            t += Line(sx + xdir*PentDx2, sy + PentDy)
            t += Line(sx - xdir*PentDx1, sy + PentDy )
            t += 'z '
        t += '"\n'
        t += '    />\n'

    # Padding before end-artwork xml comment...
    t += '    '
    return t


def AudioPathText():
    PentDx1 = 14.0
    PentDx3 = 16.0
    PentDx2 =  8.0
    PentDy  = 10.5
    t = '<?xml version="1.0" encoding="UTF-8"?>\n'
    t += '<svg xmlns="http://www.w3.org/2000/svg" width="60.96mm" height="128.5mm" viewBox="0 0 60.96 128.5">\n'

    # Render a serpentine default-hidden emphasis border around the control groups
    # that affect audio inputs. We will show these only when audio inputs are connected.

    t += '<path style="fill:#ffffff;fill-opacity:0.2;stroke:#e0e000;stroke-width:0.3;stroke-linecap:round;stroke-linejoin:bevel;stroke-dasharray:none" '
    t += 'd="'
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
    t += 'z" />\n'

    # Padding before end-artwork xml comment...
    t += '</svg>'
    return t


def PatchMainPanel() -> int:
    svgFileName = '../res/tubeunit.svg'
    print('patch_tubeunit.py: Inserting artwork into {}'.format(svgFileName))
    with open(svgFileName, 'rt') as infile:
        svgText = infile.read()
    frontText = '<!--begin artwork-->'
    backText = '<!--end artwork-->'
    frontIndex = svgText.index(frontText)
    backIndex = svgText.index(backText)
    if backIndex < frontIndex:
        raise Exception('Front and back markers are backwards!')
    frontIndex += len(frontText)
    outText = svgText[:frontIndex] + ArtworkText() + svgText[backIndex:]
    with open(svgFileName, 'wt') as outfile:
        outfile.write(outText)
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
    panel = Panel(12)

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


def Success() -> int:
    print('patch_tubeunit.py: SUCCESS')
    return 0


if __name__ == '__main__':
    sys.exit(
        PatchMainPanel() or
        GenerateAudioPathLayer() or
        GenerateLabelLayer() or
        Success()
    )
