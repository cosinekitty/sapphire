#!/usr/bin/env python3
#
#   patch_tubeunit.py   -   Don Cross <cosinekitty@gmail.com>
#
#   Utility for calculating SVG coordinates for Tube Unit's
#   decorative artwork.
#
import sys


def TubeUnitKnobPos(x, y):
    return (20.0 + x*27.0, 34.0 + y*20.0 - x*10.0)

def Move(x, y):
    return 'M {:0.2f},{:0.2f} '.format(x, y)

def Line(x, y):
    return 'L {:0.2f},{:0.2f} '.format(x, y)

def ArtworkText():
    HexHorRadius = 19.0
    HexHorStep = 8
    HexVerRadius = 10.0
    t = '\n'
    t += '    <path style="fill-opacity:0.0;fill:#000000;stroke:#000000;stroke-width:0.1;stroke-linecap:round;stroke-linejoin:bevel;stroke-dasharray:none"\n'
    t += '        d="'
    for y in range(4):
        for x in range(2):
            sx, sy = TubeUnitKnobPos(x, y)
            t += Move(sx - HexHorRadius, sy)
            t += Line(sx - HexHorStep, sy - HexVerRadius)
            t += Line(sx + HexHorStep, sy - HexVerRadius)
            t += Line(sx + HexHorRadius, sy)
            t += Line(sx + HexHorStep, sy + HexVerRadius)
            t += Line(sx - HexHorStep, sy + HexVerRadius)
            t += 'z '
    t += '"\n'
    t += '    />\n'
    t += '    '
    return t


def main():
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
        print('patch_tubeunit.py: SUCCESS')
        return 0

if __name__ == '__main__':
    sys.exit(main())
