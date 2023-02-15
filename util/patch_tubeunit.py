#!/usr/bin/env python3
#
#   patch_tubeunit.py   -   Don Cross <cosinekitty@gmail.com>
#
#   Utility for calculating SVG coordinates for Tube Unit's
#   decorative artwork.
#
import sys

def ArtworkText():
    return '\n    '

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
