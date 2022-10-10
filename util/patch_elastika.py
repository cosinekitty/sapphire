#!/usr/bin/env python3
#
#   patch_elastika.py   -   Don Cross <cosinekitty@gmail.com>
#
#   Utility for calculating SVG coordinates for Elastika's
#   decorative artwork.
#
import sys


class Shape:
    def __init__(self, frontIndex: int, backIndex: int, text: str):
        self.frontIndex = frontIndex
        self.backIndex = backIndex
        self.text = text

    def __repr__(self):
        return 'Shape({}, {})'.format(self.frontIndex, self.backIndex)


def GetShapeList(svg):
    # Find all <path ... /> elements.
    # We don't need a general XML parser, just hack to work with this particular SVG file.
    frontText = '<path'
    backText = '/>'
    searchIndex = 0
    shapeList = []
    backIndex = 0
    while (frontIndex := svg.find(frontText, searchIndex)) >= 0:
        if (backIndex := svg.find(backText, searchIndex+len(frontText))) < 0:
            searchIndex = frontIndex + len(frontText)
        else:
            backIndex += len(backText)
            elem = svg[frontIndex:backIndex]
            if 'id="boundary_' in elem:
                shapeList.append(Shape(frontIndex, backIndex, elem))
            searchIndex = backIndex + len(backText)
    return shapeList


if __name__ == '__main__':
    # Read the entire text of the SVG file.
    svgFileName = '../res/elastika.svg'
    with open(svgFileName, 'rt') as infile:
        svg = infile.read()
    shapeList = GetShapeList(svg)
    print(shapeList)
    sys.exit(0)
