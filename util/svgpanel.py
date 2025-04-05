#!/usr/bin/env python3
"""Classes for generating VCV Rack panel designs.

For more information, see:
https://github.com/cosinekitty/svgpanel
"""
from typing import Any, List, Tuple, Dict, Optional
from enum import Enum, unique
import xml.etree.ElementTree as et
from fontTools.ttLib import TTFont                          # type: ignore
from fontTools.pens.svgPathPen import SVGPathPen            # type: ignore
from fontTools.pens.transformPen import TransformPen        # type: ignore
from fontTools.misc.transform import DecomposedTransform    # type: ignore

# Prevent seeing lots of "ns0:" everywhere when we re-serialized the XML.
et.register_namespace('', 'http://www.w3.org/2000/svg')

HP_WIDTH_MM = 5.08
PANEL_HEIGHT_MM = 128.5

class Error(Exception):
    """Indicates an error in an svgpanel function."""
    def __init__(self, message:str) -> None:
        Exception.__init__(self, message)


@unique
class HorizontalAlignment(Enum):
    """Specifies how a text block is to be aligned horizontally."""
    Left = 0
    Center = 1
    Right = 2


@unique
class VerticalAlignment(Enum):
    """Specifies how a text block is to be aligned vertically."""
    Top = 0
    Middle = 1
    Bottom = 2


def _HorAdjust(hor:HorizontalAlignment) -> float:
    if hor == HorizontalAlignment.Left:
        return 0.0
    if hor == HorizontalAlignment.Center:
        return -0.5
    if hor == HorizontalAlignment.Right:
        return -1.0
    raise Error('Invalid horizontal alignment: {}'.format(hor))


def _VerAdjust(ver:VerticalAlignment) -> float:
    if ver == VerticalAlignment.Top:
        return 0.0
    if ver == VerticalAlignment.Middle:
        return -0.5
    if ver == VerticalAlignment.Bottom:
        return -1.0
    raise Error('Invalid vertical alignment: {}'.format(ver))


def Move(x:float, y:float) -> str:
    """Move the pen in an SVG path."""
    return 'M {:0.2f},{:0.2f} '.format(x, y)


def Line(x:float, y:float) -> str:
    """Draw a line segment with the pen in an SVG path."""
    return 'L {:0.2f},{:0.2f} '.format(x, y)


def Arc(r:float, x:float, y:float, sweep:int) -> str:
    """Draw a circular arc from the current point to (x, y) in an SVG path."""
    # https://developer.mozilla.org/en-US/docs/Web/SVG/Tutorial/Paths#arcs
    # A rx ry x-axis-rotation large-arc-flag sweep-flag x y
    return 'A {0:g} {0:g} 0 0 {3:d} {1:g} {2:g} '.format(r, x, y, sweep)

def ClosePath() -> str:
    return 'z '


def _FormatMillimeters(x: float) -> str:
    return '{:0.6g}'.format(x)


class Font:
    """A TrueType/OpenType font used for generating SVG paths."""
    def __init__(self, filename:str) -> None:
        self.filename = filename
        self.ttfont = TTFont(filename)
        self.glyphs = self.ttfont.getGlyphSet()

        # The following were obtained by dumping ttfont to an xml file:
        # font.ttfont.saveXML('t.xml')
        # Then I looked at t.xml and discovered that many symbols have to be
        # converted into a "glyph name" before finding the glyph!
        self.glyphNameTable = {
            '0': 'zero',
            '1': 'one',
            '2': 'two',
            '3': 'three',
            '4': 'four',
            '5': 'five',
            '6': 'six',
            '7': 'seven',
            '8': 'eight',
            '9': 'nine',
            ' ': 'space',
            '!': 'exclam',
            '"': 'quotedbl',
            '#': 'numbersign',
            '$': 'dollar',
            '%': 'percent',
            '&': 'ampersand',
            "'": 'quotesingle',
            '(': 'parenleft',
            ')': 'parenright',
            '*': 'asterisk',
            '+': 'plus',
            ',': 'comma',
            '-': 'hyphen',
            '.': 'period',
            '/': 'slash',
            ':': 'colon',
            ';': 'semicolon',
            '<': 'less',
            '=': 'equal',
            '>': 'greater',
            '?': 'question',
            '@': 'at',
            '[': 'bracketleft',
            ']': 'bracketright',
            '\\': 'backslash',
            '^': 'asciicircum',
            '_': 'underscore',
            '`': 'grave',
            '{': 'braceleft',
            '}': 'braceright',
            '|': 'bar',
            '~': 'asciitilde',
        }

    def __enter__(self) -> 'Font':
        self.ttfont.__enter__()
        return self

    def __exit__(self, exc_type:Any, exc_val:Any, exc_tb:Any) -> Any:
        return self.ttfont.__exit__(exc_type, exc_val, exc_tb)

    def render(self, text:str, xpos:float, ypos:float, points:float) -> str:
        """Generate the SVG path that draws this text block at a given location."""
        # Calculate how many millimeters there are per font unit in this point size.
        mmPerEm = (25.4 / 72)*points
        mmPerUnit = mmPerEm / self.ttfont['head'].unitsPerEm
        x = xpos
        y = ypos + mmPerUnit * (self.ttfont['head'].yMax + self.ttfont['head'].yMin/2)
        spen = SVGPathPen(self.glyphs, _FormatMillimeters)
        for ch in text:
            gn = self.glyphNameTable.get(ch, ch)
            if glyph := self.glyphs.get(gn):
                tran = DecomposedTransform(translateX = x, translateY = y, scaleX = mmPerUnit, scaleY = -mmPerUnit).toTransform()
                pen = TransformPen(spen, tran)
                glyph.draw(pen)
                x += mmPerUnit * glyph.width
            else:
                raise Error('Unknown character [' + ch + '], glyph name: [' + gn + ']')
        return str(spen.getCommands())

    def measure(self, text:str, points:float) -> Tuple[float,float]:
        """Returns a (width, height) tuple of a rectangle that fits around this text block."""
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
    """A line of text to be rendered using a given font and size."""
    def __init__(self, text:str, font:Font, points:float):
        self.text = text
        self.font = font
        self.points = points

    def render(self, x:float, y:float) -> str:
        """Generate the SVG path that draws this text block at a given location."""
        return self.font.render(self.text, x, y, self.points)

    def measure(self, xscale:float = 1.0, yscale:float = 1.0) -> Tuple[float,float]:
        """Returns a (width, height) tuple of a rectangle that fits around this text block."""
        (width, height) = self.font.measure(self.text, self.points)
        return (xscale*width, yscale*height)

    def toPath(
            self,
            xpos: float,
            ypos: float,
            horizontal: HorizontalAlignment,
            vertical: VerticalAlignment,
            style: str = '',
            id: str = '',
            xscale: float = 1.0,
            yscale: float = 1.0) -> 'TextPath':
        """Converts this text block to a path with a given vertical and horizontal alignment."""
        (dx, dy) = self.measure(xscale, yscale)
        x = xpos + dx*_HorAdjust(horizontal)
        y = ypos + dy*_VerAdjust(vertical)
        tp = TextPath(self, x, y, id)
        if (xscale != 1.0) or (yscale != 1.0):
            tp.setAttrib('transform', 'scale({:0.6g},{:0.6g})'.format(xscale, yscale))
        tp.setAttrib('style', style)
        return tp


class Element:
    """An XML element inside an SVG file."""
    def __init__(self, tag:str, id:str = '', children: Optional[List['Element']] = None) -> None:
        self.literal: Optional[et.Element] = None
        self.tag = tag
        self.attrib: Dict[str, str] = {}
        self.children: List[Element] = children or []
        self.setAttrib('id', id)

    def setAttrib(self, key:str, value:str) -> 'Element':
        """Define or replace an attribute in this XML element."""
        if value:
            self.attrib[key] = value
        return self

    def setAttribFloat(self, key:str, value:float) -> 'Element':
        """Set a floating point attribute in this XML element."""
        return self.setAttrib(key, '{:0.6g}'.format(value))

    def append(self, elem:'Element') -> 'Element':
        """Add a child element to this element's list of children."""
        self.children.append(elem)
        return self

    def xml(self) -> et.Element:
        """Convert this element to XML."""
        if 'id' in self.attrib:
            # When the element has an id, put it first. Take advantage of Python's
            # ability to preserve insertion order in a dictionary.
            attrib = {'id': self.attrib['id']}
            for (key, value) in self.attrib.items():
                if key != 'id':
                    attrib[key] = value
        else:
            attrib = self.attrib
        elem = et.Element(self.tag, attrib)
        for child in self.children:
            elem.append(child.xml())
        return elem


class LiteralXml(Element):
    """For supporting legacy SVG that was generated in InkScape."""
    def __init__(self, panelXmlText:str, id:str = ''):
        super().__init__('g', id)
        self.literal = et.fromstring(panelXmlText)

    def xml(self) -> et.Element:
        if self.literal is None:
            raise Error('Internal error: missing ElementTree instance.')
        return self.literal


class Path(Element):
    """An SVG path with pre-rendered coordinates."""
    def __init__(self, dpath:str, style:str = '', id:str = '', fillStyle:str = '') -> None:
        super().__init__('path', id)
        self.setAttrib('d', dpath)
        self.setAttrib('style', style)
        self.setAttrib('fill', fillStyle)


class LineElement(Element):
    """SVG Line"""
    def __init__(self, x1:float, y1:float, x2:float, y2:float, style:str = '', id:str = '') -> None:
        super().__init__('line', id)
        self.setAttrib('x1', '{:0.2f}'.format(x1))
        self.setAttrib('y1', '{:0.2f}'.format(y1))
        self.setAttrib('x2', '{:0.2f}'.format(x2))
        self.setAttrib('y2', '{:0.2f}'.format(y2))
        self.setAttrib('style', style)


class Polyline(Element):
    """SVG Polyline"""
    def __init__(self, points:str, style:str = '', id:str = '') -> None:
        super().__init__('polyline', id)
        self.setAttrib('points', points)
        self.setAttrib('style', style)


class Polygon(Element):
    """SVG Polygon"""
    def __init__(self, points:str, style:str = '', id:str = '') -> None:
        super().__init__('polygon', id)
        self.setAttrib('points', points)
        self.setAttrib('style', style)


class Circle(Element):
    """SVG Circle"""
    def __init__(self, cx:float, cy:float, radius:float, stroke:str, strokeWidth:float, fill:str, id:str = '') -> None:
        super().__init__('circle', id)
        self.setAttribFloat('cx', cx)
        self.setAttribFloat('cy', cy)
        self.setAttribFloat('r', radius)
        self.setAttrib('stroke', stroke)
        self.setAttribFloat('stroke-width', strokeWidth)
        self.setAttrib('fill', fill)


class TextPath(Element):
    """An SVG path that is rendered from text expressed in a given font and size."""
    def __init__(self, textItem:TextItem, x:float, y:float, id:str = '') -> None:
        super().__init__('path', id)
        self.setAttrib('d', textItem.render(x, y))


class Rectangle(Element):
    """SVG Rectangle"""
    def __init__(self, cx:float, cy:float, width:float, height:float, stroke:str = 'black', strokeWidth:float = 0.1, fill:str = 'none', id:str = '') -> None:
        super().__init__('rect', id)
        self.setAttribFloat('x', cx - width/2)
        self.setAttribFloat('y', cy - height/2)
        self.setAttribFloat('width', width)
        self.setAttribFloat('height', height)
        self.setAttrib('stroke', stroke)
        self.setAttribFloat('stroke-width', strokeWidth)
        self.setAttrib('fill', fill)


class BorderRect(Element):
    """A filled rectangle for the base layer of your panel design."""
    def __init__(self, hpWidth:int, fillColor:str, borderColor:str, mmHeight:float = PANEL_HEIGHT_MM) -> None:
        super().__init__('rect', 'border_rect')
        if hpWidth <= 0:
            raise Error('Invalid hpWidth={}'.format(hpWidth))
        self.setAttribFloat('width', HP_WIDTH_MM * hpWidth)
        self.setAttribFloat('height', mmHeight)
        self.setAttrib('x', '0')
        self.setAttrib('y', '0')
        if borderColor:
            self.setAttrib('style', 'display:inline;fill:{};fill-opacity:1;fill-rule:nonzero;stroke:{};stroke-width:0.7;stroke-linecap:round;stroke-linejoin:round;stroke-dasharray:none;stroke-opacity:1;image-rendering:auto'.format(fillColor, borderColor))
        else:
            self.setAttrib('style', 'display:inline;fill:{};fill-opacity:1;stroke:none;'.format(fillColor))


class LinearGradient(Element):
    """Defines a linear gradient SVG element to be used for filled patterns later in your panel."""
    def __init__(self, id:str, x1:float, y1:float, x2:float, y2:float, color1:str, color2:str) -> None:
        super().__init__('linearGradient', id)
        self.setAttribFloat('x1', x1)
        self.setAttribFloat('y1', y1)
        self.setAttribFloat('x2', x2)
        self.setAttribFloat('y2', y2)
        self.setAttrib('gradientUnits', 'userSpaceOnUse')
        self.append(Element('stop').setAttrib('offset', '0').setAttrib('style', 'stop-color:{};stop-opacity:1;'.format(color1)))
        self.append(Element('stop').setAttrib('offset', '1').setAttrib('style', 'stop-color:{};stop-opacity:1;'.format(color2)))


class Component:
    def __init__(self, id:str, xCenter:float, yCenter:float) -> None:
        self.id = id
        self.cx = xCenter
        self.cy = yCenter

    def __lt__(self, other:'Component') -> bool:
        return self.id < other.id


class ControlLayer:
    def __init__(self, panel: 'Panel') -> None:
        self.componentList = [Component('_panel', panel.mmWidth, panel.mmHeight)]

    def append(self, comp:Component) -> None:
        self.componentList.append(comp)


def UpdateFileIfChanged(filename:str, newText:str) -> bool:
    # Do not write to the file unless the newText is different from
    # what already exists in the file, or we can't even read text from the file.
    # This allows us to change the modification date on a file only
    # when something has really changed. This way, we don't trick
    # `make` into doing unnecessary work
    # (like compiling C++ code that hasn't changed).
    try:
        with open(filename, 'rt') as infile:
            oldText = infile.read()
    except:
        oldText = ''
    if newText == oldText:
        print('Kept: ', filename)
        return False
    with open(filename, 'wt') as outfile:
        outfile.write(newText)
    print('Wrote:', filename)
    return True


class BasePanel(Element):
    def __init__(self, mmWidth:float, mmHeight:float) -> None:
        super().__init__('svg')
        self.mmWidth = mmWidth
        self.mmHeight = mmHeight
        self.setAttrib('xmlns', 'http://www.w3.org/2000/svg')
        self.setAttrib('width', '{:0.2f}mm'.format(self.mmWidth))
        self.setAttrib('height', '{:0.2f}mm'.format(self.mmHeight))
        self.setAttrib('viewBox', '0 0 {:0.2f} {:0.2f}'.format(self.mmWidth, self.mmHeight))

    def svg(self, indent:str = '    ') -> str:
        """Convert this panel to a complete SVG document."""
        root = self.xml()
        et.indent(root, indent)
        rootBytes = et.tostring(root, encoding='utf-8')
        # Just being picky, but I prefer generating my own <?xml ... ?> declaration,
        # just so I can use double-quotes for consistency.
        # See: https://bugs.python.org/issue36233
        return '<?xml version="1.0" encoding="utf-8"?>\n' + rootBytes.decode('utf8') + '\n'    # type: ignore

    def save(self, outFileName:str, indent:str = '    ') -> bool:
        """Write this panel to an SVG file."""
        return UpdateFileIfChanged(outFileName, self.svg(indent))


class Panel(BasePanel):
    """A rectangular region that can be either your panel's base layer or a transparent layer on top."""
    def __init__(self, hpWidth:int, mmHeight:float = PANEL_HEIGHT_MM) -> None:
        if hpWidth <= 0:
            raise Error('Invalid hpWidth={}'.format(hpWidth))
        super().__init__(HP_WIDTH_MM * hpWidth, mmHeight)

