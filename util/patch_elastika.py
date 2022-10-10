#!/usr/bin/env python3
#
#   patch_elastika.py   -   Don Cross <cosinekitty@gmail.com>
#
#   Utility for calculating SVG coordinates for Elastika's
#   decorative artwork.
#
import sys

def Locate(text:str, frontText:str, backText:str, searchIndex:int = 0) -> str:
    frontIndex = text.find(frontText, searchIndex)
    if frontIndex < 0:
        return None
    frontIndex += len(frontText)
    backIndex = text.find(backText, frontIndex)
    if backIndex < 0:
        return None
    return (frontIndex, backIndex)


def Extract(text:str, frontText:str, backText:str) -> str:
    loc = Locate(text, frontText, backText)
    if loc is None:
        return None
    frontIndex, backIndex = loc
    return text[frontIndex:backIndex]

class Shape:
    def __init__(self, frontIndex: int, backIndex: int, text: str):
        self.frontIndex = frontIndex
        self.backIndex = backIndex
        self.text = text
        self.name = Extract(text, 'id="boundary_', '"')

    def __repr__(self):
        return 'Shape({}, {}, "{}")'.format(self.frontIndex, self.backIndex, self.name)


def GetShapeList(svg):
    # Find all <path ... /> elements.
    # We don't need a general XML parser, just hack to work with this particular SVG file.
    frontText = '<path'
    backText = '/>'
    searchIndex = 0
    shapeList = []
    while (loc := Locate(svg, frontText, backText, searchIndex)) is not None:
        frontIndex, backIndex = loc
        searchIndex = backIndex + len(backText)
        shape = Shape(frontIndex, backIndex, svg[frontIndex:searchIndex])
        if shape.name:
            shapeList.append(shape)
    return shapeList


if __name__ == '__main__':
    # Read the entire text of the SVG file.
    svgFileName = '../res/elastika.svg'
    with open(svgFileName, 'rt') as infile:
        svg = infile.read()
    shapeList = GetShapeList(svg)
    print(shapeList)
    sys.exit(0)
