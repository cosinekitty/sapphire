#!/usr/bin/env python3
#
#   patch_elastika.py   -   Don Cross <cosinekitty@gmail.com>
#
#   Utility for calculating SVG coordinates for Elastika's
#   decorative artwork.
#
import sys

if __name__ == '__main__':
    # Read the entire text of the SVG file.
    svgFileName = '../res/elastika.svg'
    with open(svgFileName, 'rt') as infile:
        svg = infile.read()

    # Find all <path ... /> elements.
    # We don't need a general XML parser, just hack to work with this particular SVG file.
    frontText = '<path'
    backText = '/>'
    searchIndex = 0
    while (frontIndex := svg.find(frontText, searchIndex)) >= searchIndex:
        if (backIndex := svg.find(backText, searchIndex+len(frontText))) < 0:
            searchIndex = frontIndex + len(frontText)
        else:
            elem = svg[frontIndex:backIndex+len(backText)]
            if 'id="boundary_' in elem:
                print(elem)
            searchIndex = backIndex + len(backText)

    sys.exit(0)
