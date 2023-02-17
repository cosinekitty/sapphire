#!/usr/bin/env python3
#
#   patch_tubeunit.py   -   Don Cross <cosinekitty@gmail.com>
#
#   Utility for calculating SVG coordinates for Tube Unit's
#   decorative artwork.
#
import sys


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
