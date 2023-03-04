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


def Coord(x:float, y:float) -> str:
    return ' {:0.2f},{:0.2f}'.format(x, y)


def PathForShape(n:int) -> str:
    # Make a string that looks like:
    # "M 2.7,32.0 8.15,28.5 13.6,32.0 13.6,86.0 8.2,89.0 2.7,86.0 z"
    # Start with "M" for absolute path.
    p = 'M'
    w = 11.22
    if n == -1:
        # The POWER hexagon is special: much smaller than the others.
        x0 = 2*w + 2.4
        y0 = 90.0
        h = 19.0
    else:
        # Follow with 6 coordinate pairs "x,y".
        x0 = n*w + 2.4
        y0 = 32.0
        h = 54.0
    p += Coord(x0, y0)
    (dx, dy) = (w/2.0, 28.5-32.0)
    (x1, y1) = (x0 + dx, y0 + dy)
    p += Coord(x1, y1)
    (x2, y2) = (x1 + dx, y0)
    p += Coord(x2, y2)
    p += Coord(x2, y2 + h)
    p += Coord(x1, y2 + h - dy)
    p += Coord(x0, y2 + h)
    # Terminate with "z" to close the path.
    p += ' z'
    return p


if __name__ == '__main__':
    # Read the entire text of the SVG file.
    svgFileName = '../res/elastika.svg'
    print('patch_elastika.py: Inserting artwork into {}'.format(svgFileName))
    with open(svgFileName, 'rt') as infile:
        svg = infile.read()
    shapeList = GetShapeList(svg)
    controlNumber = {
        'fric': 0,
        'stif': 1,
        'span': 2,
        'curl': 3,
        'tilt': 4,
        'power': -1,
    }

    text = ''
    offset = 0
    for shape in shapeList:
        text += svg[offset:shape.frontIndex]
        n = controlNumber.get(shape.name)
        if n is not None:
            p = PathForShape(n)
            #print(shape, n, p)
            text += shape.ReplacePath(p)
        else:
            text += shape.text
        offset = shape.backIndex
    text += svg[offset:]
    with open(svgFileName, 'wt') as outfile:
        outfile.write(text)
    print('patch_elastika.py: SUCCESS')
    sys.exit(0)
