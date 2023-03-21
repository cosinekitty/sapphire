#!/usr/bin/env python3
"""Classes for generating VCV Rack panel designs.

For more information, see:
https://github.com/cosinekitty/svgpanel
"""
from typing import Any, List, Tuple, Optional, Union, Callable, Dict
from fontTools.ttLib import TTFont                          # type: ignore
from fontTools.pens.svgPathPen import SVGPathPen            # type: ignore
from fontTools.pens.transformPen import TransformPen        # type: ignore
from fontTools.misc.transform import DecomposedTransform    # type: ignore

class Error(Exception):
    """Indicates an error in an svgpanel function."""
    def __init__(self, message: str) -> None:
        Exception.__init__(self, message)


class Font:
    def __init__(self, filename: str) -> None:
        self.filename = filename
        self.ttfont = TTFont(filename)
        self.glyphs = self.ttfont.getGlyphSet()

    def render(self, text: str, xpos: float, ypos: float, points: float) -> str:
        # Calculate how many millimeters there are per font unit in this point size.
        mmPerEm = (25.4 / 72)*points
        mmPerUnit = mmPerEm / self.ttfont['head'].unitsPerEm
        x = xpos
        y = ypos
        spen = SVGPathPen(self.glyphs)
        for ch in text:
            if glyph := self.glyphs.get(ch):
                tran = DecomposedTransform(translateX = x, translateY = y, scaleX = mmPerUnit, scaleY = -mmPerUnit).toTransform()
                pen = TransformPen(spen, tran)
                glyph.draw(pen)
                x += mmPerUnit * glyph.width
            else:
                # Use a "3-em space", which confusingly is one-third of an em wide.
                x += mmPerEm / 3
        return str(spen.getCommands())


class Element:
    def __init__(self, tag: str) -> None:
        self.tag = tag
        self.attrib: Dict[str, str] = {}
        self.children: List[Element] = []

    def setAttrib(self, key: str, value: str) -> 'Element':
        self.attrib[key] = value
        return self

    def append(self, elem: 'Element') -> 'Element':
        self.children.append(elem)
        return self

    def svg(self) -> str:
        text = '<' + self.tag
        for (k, v) in self.attrib.items():
            text += ' {}="{}"'.format(k, v)
        if len(self.children) == 0:
            text += '/>\n'
        else:
            text += '>\n'
            for child in self.children:
                text += child.svg()
            text += '</{}>\n'.format(self.tag)
        return text


class TextPath(Element):
    def __init__(self, text:str, x:float, y:float, font:Font, points:float) -> None:
        super().__init__('path')
        self.setAttrib('d', font.render(text, x, y, points))


class Panel(Element):
    def __init__(self, hpWidth: int) -> None:
        super().__init__('svg')
        if hpWidth <= 0:
            raise Error('Invalid hpWidth={}'.format(hpWidth))
        self.mmWidth = 5.08 * hpWidth
        self.mmHeight = 128.5
        self.setAttrib('xmlns', 'http://www.w3.org/2000/svg')
        self.setAttrib('width', '{:0.2f}mm'.format(self.mmWidth))
        self.setAttrib('height', '{:0.2f}mm'.format(self.mmHeight))
        self.setAttrib('viewBox', '0 0 {:0.2f} {:0.2f}'.format(self.mmWidth, self.mmHeight))

    def svg(self) -> str:
        return '<?xml version="1.0" encoding="utf-8"?>\n' + super().svg()
