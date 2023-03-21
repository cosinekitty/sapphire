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
    def __init__(self, message:str) -> None:
        Exception.__init__(self, message)


class Font:
    def __init__(self, filename:str) -> None:
        self.filename = filename
        self.ttfont = TTFont(filename)
        self.glyphs = self.ttfont.getGlyphSet()

    def __enter__(self) -> 'Font':
        self.ttfont.__enter__()
        return self

    def __exit__(self, exc_type:Any, exc_val:Any, exc_tb:Any) -> Any:
        return self.ttfont.__exit__(exc_type, exc_val, exc_tb)

    def render(self, text:str, xpos:float, ypos:float, points:float) -> str:
        # Calculate how many millimeters there are per font unit in this point size.
        mmPerEm = (25.4 / 72)*points
        mmPerUnit = mmPerEm / self.ttfont['head'].unitsPerEm
        x = xpos
        y = ypos + mmPerUnit * (self.ttfont['head'].yMax + self.ttfont['head'].yMin/2)
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

    def measure(self, text:str, points:float) -> Tuple[float,float]:
        mmPerEm = (25.4 / 72)*points
        mmPerUnit = mmPerEm / self.ttfont['head'].unitsPerEm
        x = 0.0
        y = mmPerUnit * (self.ttfont['head'].yMax - self.ttfont['head'].yMin)
        for ch in text:
            if glyph := self.glyphs.get(ch):
                x += mmPerUnit * glyph.width
            else:
                # Use a "3-em space", which confusingly is one-third of an em wide.
                x += mmPerEm / 3
        return (x, y)


class TextItem:
    def __init__(self, text:str, font:Font, points:float):
        self.text = text
        self.font = font
        self.points = points

    def render(self, x:float, y:float) -> str:
        return self.font.render(self.text, x, y, self.points)

    def measure(self) -> Tuple[float,float]:
        return self.font.measure(self.text, self.points)


class Element:
    def __init__(self, tag:str, id:str = '') -> None:
        self.tag = tag
        self.attrib: Dict[str, str] = {}
        self.children: List[Element] = []
        self.setAttrib('id', id)

    def setAttrib(self, key:str, value:str) -> 'Element':
        if value:
            self.attrib[key] = value
        return self

    def setAttribFloat(self, key:str, value:float) -> 'Element':
        return self.setAttrib(key, '{:0.3g}'.format(value))

    def append(self, elem:'Element') -> 'Element':
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
    def __init__(self, textItem:TextItem, x:float, y:float, id:str = '') -> None:
        super().__init__('path', id)
        self.setAttrib('d', textItem.render(x, y))


class Panel(Element):
    def __init__(self, hpWidth:int) -> None:
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
