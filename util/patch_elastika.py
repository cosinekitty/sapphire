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
        self.pathLoc = Locate(text, ' d="', '"')

    def __repr__(self):
        return 'Shape({}, {}, "{}")'.format(self.frontIndex, self.backIndex, self.name)

    def IsValid(self):
        return (self.name is not None) and (self.pathLoc is not None)

    def ReplacePath(self, path):
        frontIndex, backIndex = self.pathLoc
        return self.text[:frontIndex] + path + self.text[backIndex:]


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
        shape = Shape(frontIndex, searchIndex, svg[frontIndex:searchIndex])
        if shape.IsValid():
            shapeList.append(shape)
    return shapeList


def PathForShape(n:int) -> str:
    return ''


if __name__ == '__main__':
    # Read the entire text of the SVG file.
    svgFileName = '../res/elastika.svg'
    with open(svgFileName, 'rt') as infile:
        svg = infile.read()
    shapeList = GetShapeList(svg)
    controlNumber = {
        'fric': 0,
        'stif': 1,
        'span': 2,
        'curl': 3,
        'tilt': 4
    }

    text = ''
    offset = 0
    for shape in shapeList:
        text += svg[offset:shape.frontIndex]
        n = controlNumber.get(shape.name)
        if n is not None:
            print(shape, n)
            p = PathForShape(n)
            text += shape.ReplacePath(p)
        else:
            text += shape.text
        offset = shape.backIndex
    text += svg[offset:]
    with open('t.svg', 'wt') as outfile:
        outfile.write(text)
    sys.exit(0)
