#!/usr/bin/env python3

from svgpanel import *
from typing import List

SAPPHIRE_FONT_FILENAME = 'Quicksand-Light.ttf'

SAPPHIRE_PANEL_COLOR  = '#4f8df2'
SAPPHIRE_BORDER_COLOR = '#5021d4'
SAPPHIRE_AZURE_COLOR = '#3068ff'
SAPPHIRE_MAGENTA_COLOR = '#a06de4'
SAPPHIRE_PURPLE_COLOR = '#b242bd'
SAPPHIRE_EGGPLANT_COLOR = '#ba60d1'
SAPPHIRE_SALMON_COLOR = '#b9818b'
SAPPHIRE_TEAL_COLOR = '#29aab4'

MODEL_NAME_POINTS   = 22.0
MODEL_NAME_STYLE    = 'display:inline;stroke:#000000;stroke-width:0.35;stroke-linecap:round;stroke-linejoin:bevel'

BRAND_NAME_POINTS   = 22.0
BRAND_NAME_STYLE    = 'fill:#000000;stroke:#000000;stroke-width:0.265;stroke-linecap:square'

CONTROL_LABEL_POINTS = 10.0
CONTROL_LABEL_STYLE  = 'stroke:#000000;stroke-width:0.25;stroke-linecap:round;stroke-linejoin:bevel'

GEMSTONE_STYLE = 'stroke-width:0;fill:#0000ff;stroke:#2e2114;stroke-linecap:square;stroke-opacity:1'
CONNECTOR_LINE_STYLE = 'stroke:#000000;stroke-width:0.1;stroke-linecap:round;stroke-linejoin:bevel;stroke-dasharray:none'


class SapphireGemstone(Element):
    mmWidth = 5.43
    mmHeight = 5.03
    def __init__(self, x:float, y:float) -> None:
        super().__init__('path')
        self.setAttrib('d', 'm {:0.3f},{:0.3f} q 0,0.34175 -0.242534,0.63114 -0.0441,0.0524 -0.20395,0.21497 -0.187413,0.19017 -0.785481,0.81304 l -1.444183,1.51309 q -0.01929,0.003 -0.0441,0.003 -0.02756,0 -0.04134,-0.003 -0.08268,-0.0827 -0.647678,-0.67524 -0.468532,-0.47404 -0.934309,-0.94809 -0.556727,-0.56775 -0.89848,-0.98116 -0.132292,-0.15985 -0.168121,-0.29766 -0.02756,-0.10197 -0.02756,-0.34175 0,-0.11024 0.07166,-0.28939 0.06614,-0.16536 0.140559,-0.26458 0.118512,-0.1571 0.42168,-0.42444 0.283875,-0.2508 0.474044,-0.37482 0.272852,-0.17915 0.818555,-0.36656 0.09646,-0.0331 0.110243,-0.0331 h 1.3615 q 0.01102,0 0.104731,0.0331 0.567751,0.20119 0.824066,0.36656 0.165365,0.10473 0.474045,0.37758 0.322461,0.28388 0.421679,0.42168 0.214974,0.2949 0.214974,0.62563 z m -0.16812,-0.27285 q -0.03307,-0.10749 -0.135048,-0.30868 -0.07166,-0.0937 -0.209462,-0.19844 -0.118511,-0.0854 -0.234266,-0.17088 0.245291,0.3638 0.578776,0.678 z m -0.573264,-0.52366 -0.358289,-0.44648 v 0.41065 z m 0.636653,0.75792 q 0,-0.0524 -0.341753,-0.39412 l 0.228754,0.7028 h 0.04961 q 0.06339,-0.21773 0.06339,-0.30868 z m -0.23151,0.27285 -0.316948,-0.90399 -0.410655,-0.0634 -0.159853,0.61461 z m 0.0028,0.15434 -0.771701,-0.3197 h -0.03032 l 0.344509,0.60634 q 0.07993,-0.003 0.234266,-0.0469 0.02481,-0.0138 0.223242,-0.23978 z m -0.857139,-1.05006 -0.719335,0.28388 0.60358,0.22599 z m 0,-0.16261 v -0.51538 l -0.917773,-0.38034 h -1.223697 l -0.917772,0.38034 v 0.51538 l 0.89848,0.4079 h 1.254014 z m 0.573264,1.53789 q -0.0689,0.011 -0.198438,0.0524 -0.08544,0.0551 -0.259071,0.16261 0.01929,0 0.06339,0.003 0.04134,0 0.06339,0 0.118511,0 0.190169,-0.0689 0.07166,-0.0717 0.14056,-0.14883 z m -0.289388,0.003 -0.347266,-0.63114 -0.363801,0.87918 q 0.0248,-0.0165 0.06615,-0.0165 0.03032,0 0.08544,0.011 0.05788,0.008 0.08819,0.008 0.04961,0 0.228754,-0.10749 0.228754,-0.1378 0.242535,-0.14331 z m -0.440972,-0.74139 -0.683507,-0.2756 -0.592556,0.54845 0.903993,0.65595 z m -0.854383,-0.28387 h -1.036284 l 0.523654,0.47404 z m 0.749652,1.36701 q -0.228754,-0.0303 -0.228754,-0.0303 -0.03583,0 -0.272851,0.0937 l 0.190169,0.0193 q 0.01102,0 0.311436,-0.0827 z m -0.388606,-0.0882 -0.843359,-0.61185 v 0.67524 l 0.471289,0.0827 z m 0.854383,0.0496 q -0.09922,0.006 -0.2949,0.0386 -0.01102,0.003 -0.377583,0.17639 -0.129535,0.34726 -0.380338,1.0418 z m -2.543857,-1.39733 -0.719335,-0.28388 0.124023,0.51814 z m -0.802018,-0.45476 v -0.41065 l -0.366557,0.44648 z m 1.515841,1.07212 -0.592556,-0.54019 -0.683506,0.2756 0.380338,0.9288 z m 0.369314,0.94257 q -0.03307,-0.003 -0.135047,-0.0248 -0.08544,-0.0165 -0.137804,-0.0165 -0.05237,0 -0.137804,0.0165 -0.101975,0.0221 -0.135047,0.0248 0.264583,0.0469 0.272851,0.0469 0.0083,0 0.272851,-0.0469 z m -0.30868,-0.1378 v -0.68626 l -0.843359,0.61185 0.37207,0.14607 z m 0.950846,0.20671 -0.396875,-0.0551 -0.479557,0.14056 v 1.27607 z m -2.888366,-2.20211 q -0.118512,0.0854 -0.234267,0.17364 -0.146072,0.10748 -0.209461,0.19568 -0.0441,0.10473 -0.135048,0.31419 0.369314,-0.3638 0.578776,-0.68351 z m 0.476801,0.83234 -0.159853,-0.61461 -0.410655,0.0634 -0.316948,0.90399 z m 0.394118,1.08313 -0.363802,-0.87918 -0.355533,0.63114 q 0.43546,0.2508 0.451996,0.2508 0.09095,0 0.267339,-0.003 z m 1.821765,0.82958 q -0.132291,0.21222 -0.405143,0.6339 -0.07717,0.10749 -0.220486,0.32797 -0.05788,0.10473 -0.05512,0.226 0.159852,-0.15158 0.432704,-0.49609 0.07717,-0.12954 0.140559,-0.339 0.05512,-0.17639 0.107486,-0.35278 z m -1.493792,-0.66146 -0.270095,-0.0882 -0.0083,-0.011 q -0.02481,0 -0.107487,0.011 -0.06615,0.006 -0.107487,0.006 0.272851,0.091 0.311436,0.091 0.02481,0 0.181901,-0.008 z m -0.810286,-1.06384 h -0.03032 l -0.7717,0.3197 q 0.06615,0.0965 0.223242,0.23978 0.07717,0.0138 0.234266,0.0469 z m -0.689018,-0.50161 q -0.341754,0.34727 -0.341754,0.39412 0,0.0799 0.06339,0.30868 h 0.04961 z m 2.235176,3.04547 v -1.27607 l -0.479557,-0.14056 -0.396875,0.0551 z m -1.606791,-1.6757 q -0.256315,-0.1819 -0.457509,-0.22324 0.06615,0.0772 0.20395,0.22324 z m 1.609547,2.04501 v -0.1378 l -0.683507,-1.05007 q 0.05237,0.17639 0.107487,0.35278 0.06339,0.20946 0.14056,0.339 0.0689,0.11851 0.20395,0.25907 0.115755,0.11851 0.23151,0.23702 z m -0.644921,-0.70555 q -0.115756,-0.32798 -0.380339,-1.0418 -0.135047,-0.0662 -0.272851,-0.12954 -0.159852,-0.0716 -0.289388,-0.0716 -0.05237,0 -0.110243,-0.0138 z'.format(x + 5.43, y + 1.9))


def ControlTextPath(font:Font, text:str, xpos:float, ypos:float, id:str = '') -> TextPath:
    ti = TextItem(text, font, CONTROL_LABEL_POINTS)
    tp = TextPath(ti, xpos, ypos, id)
    tp.setAttrib('style', CONTROL_LABEL_STYLE)
    return tp

def CenteredControlTextPath(font:Font, text:str, xcenter:float, ycenter:float, id:str = '', pointSize:float = CONTROL_LABEL_POINTS) -> TextPath:
    ti = TextItem(text, font, pointSize)
    tp = ti.toPath(xcenter, ycenter, HorizontalAlignment.Center, VerticalAlignment.Middle, CONTROL_LABEL_STYLE, id)
    return tp

def ModelNamePath(panel:Panel, font:Font, name:str) -> TextPath:
    ti = TextItem(name, font, MODEL_NAME_POINTS)
    tp = ti.toPath(panel.mmWidth/2, 0.2, HorizontalAlignment.Center, VerticalAlignment.Top, MODEL_NAME_STYLE, 'model_name')
    return tp


def SapphireInsignia(panel:Panel, font:Font) -> Element:
    '''Creates a bottom-centered Sapphire insignia with gemstones on either side.'''
    insignia = Element('g', 'sapphire_insignia')
    gemSpacing = 2.284
    gemY = 121.0
    ti = TextItem('sapphire', font, BRAND_NAME_POINTS)
    (dx, dy) = ti.measure()
    x1 = (panel.mmWidth - dx)/2
    y1 = panel.mmHeight - (dy + 0.5)
    brandTextPath = TextPath(ti, x1, y1, 'brand_name')
    brandTextPath.setAttrib('style', BRAND_NAME_STYLE)
    gemGroup = Element('g', 'gemstones')
    gemGroup.setAttrib('style', GEMSTONE_STYLE)
    gdx = gemSpacing + SapphireGemstone.mmWidth
    gemGroup.append(SapphireGemstone(x1 - gdx, gemY))
    gemGroup.append(SapphireGemstone(x1 + (dx + gemSpacing), gemY))
    insignia.append(brandTextPath)
    insignia.append(gemGroup)
    return insignia


def SapphireModelInsignia(panel:Panel, font:Font, modelName:str) -> Element:
    '''Creates a compound sapphire/model label at the top of the panel.'''
    insignia = Element('g', 'sapphire_insignia')
    gemSpacing = 3.0
    sapphireTextItem = TextItem('sapphire', font, BRAND_NAME_POINTS)
    (sdx, sdy) = sapphireTextItem.measure()
    modelTextItem = TextItem(modelName, font, MODEL_NAME_POINTS)
    (mdx, mdy) = modelTextItem.measure()
    # Render "* sapphire * model *", where * = gemstone.
    # We center this at the top of the panel.
    gdx = SapphireGemstone.mmWidth + gemSpacing
    gmdx = gdx + gemSpacing
    dx = gdx + sdx + gmdx + mdx + gdx
    x1 = (panel.mmWidth - dx)/2
    gy1 = 3.5
    ty1 = 0.2
    insignia.append(SapphireGemstone(x1, gy1).setAttrib('style', GEMSTONE_STYLE))
    x1 += gdx
    brandTextPath = TextPath(sapphireTextItem, x1, ty1)
    brandTextPath.setAttrib('style', BRAND_NAME_STYLE)
    insignia.append(brandTextPath)
    x1 += sdx + gemSpacing
    insignia.append(SapphireGemstone(x1, gy1).setAttrib('style', GEMSTONE_STYLE))
    x1 += gdx
    modelTextPath = TextPath(modelTextItem, x1, ty1)
    modelTextPath.setAttrib('style', MODEL_NAME_STYLE)
    insignia.append(modelTextPath)
    x1 += mdx + gemSpacing
    insignia.append(SapphireGemstone(x1, gy1).setAttrib('style', GEMSTONE_STYLE))
    return insignia


def CenteredGemstone(panel:Panel) -> SapphireGemstone:
    '''Use for Sapphire modules that are thin and have room only for a gemstone at the bottom.'''
    gem = SapphireGemstone((panel.mmWidth - SapphireGemstone.mmWidth)/2, 121.0)
    gem.setAttrib('id', 'sapphire_gemstone')
    gem.setAttrib('style', GEMSTONE_STYLE)
    return gem


def HorizontalLinePath(x1:float, x2:float, y:float) -> Path:
    return Path(Move(x1,y) + Line(x2,y) + ClosePath(), CONNECTOR_LINE_STYLE)


def AddFlatControlGroup(pl: Element, controls: ControlLayer, x: float, y: float, symbol: str) -> None:
    dx = 9.0
    controls.append(Component(symbol + '_cv', x - dx, y))
    controls.append(Component(symbol + '_atten', x, y))
    controls.append(Component(symbol + '_knob', x + dx, y))
    pl.append(HorizontalLinePath(x - dx, x + dx, y))


class FencePost:
    '''Calculates the position of evenly aligned items on along a closed range of values.'''
    def __init__(self, lowValue: float, highValue: float, nItems: int) -> None:
        self.lowValue = lowValue
        self.highValue = highValue
        self.nItems = nItems
        self.delta = (highValue - lowValue) / (nItems - 1)

    def value(self, itemIndex: int) -> float:
        return self.lowValue + (itemIndex * self.delta)


def AddFlatControlGrid(
        pl: Element,
        controls: ControlLayer,
        xpos: FencePost,
        ypos: FencePost,
        rowSymbols: List[str],
        columnSymbols: List[str],
        id: str = 'control_grid') -> None:
    grid = Element('g', id)
    pl.append(grid)
    xIndex = 0
    for colSymbol in columnSymbols:
        x = xpos.value(xIndex)
        yIndex = 0
        for rowSymbol in rowSymbols:
            y = ypos.value(yIndex)
            AddFlatControlGroup(pl, controls, x, y, colSymbol + '_' + rowSymbol)
            yIndex += 1
        xIndex += 1


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
        print('Kept:', filename)
        return False
    with open(filename, 'wt') as outfile:
        outfile.write(newText)
    print('Wrote:', filename)
    return True
