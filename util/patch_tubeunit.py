#!/usr/bin/env python3
#
#   patch_tubeunit.py   -   Don Cross <cosinekitty@gmail.com>
#
#   Utility for calculating SVG coordinates for Tube Unit's
#   decorative artwork.
#
import sys
from svgpanel import *


PANEL_WIDTH = 12
SAPPHIRE_PANEL_COLOR  = '#4f8df2'
SAPPHIRE_BORDER_COLOR = '#5021d4'

def PentagonOrigin(x, y):
    return (18.5 + x*24.0, 33.0 + y*21.0 - x*10.5)


class LinearGradient(Element):
    def __init__(self, id:str, x1:float, y1:float, x2:float, y2:float, color1:str, color2:str) -> None:
        super().__init__('linearGradient', id)
        self.setAttribFloat('x1', x1)
        self.setAttribFloat('y1', y1)
        self.setAttribFloat('x2', x2)
        self.setAttribFloat('y2', y2)
        self.setAttrib('gradientUnits', 'userSpaceOnUse')
        self.append(Element('stop').setAttrib('offset', '0').setAttrib('style', 'stop-color:{};stop-opacity:1;'.format(color1)))
        self.append(Element('stop').setAttrib('offset', '1').setAttrib('style', 'stop-color:{};stop-opacity:1;'.format(color2)))


class SapphireGemstone(Element):
    mmWidth = 5.43
    mmHeight = 5.03
    def __init__(self, x:float, y:float) -> None:
        super().__init__('path')
        self.setAttrib('d', 'm {:0.3f},{:0.3f} q 0,0.34175 -0.242534,0.63114 -0.0441,0.0524 -0.20395,0.21497 -0.187413,0.19017 -0.785481,0.81304 l -1.444183,1.51309 q -0.01929,0.003 -0.0441,0.003 -0.02756,0 -0.04134,-0.003 -0.08268,-0.0827 -0.647678,-0.67524 -0.468532,-0.47404 -0.934309,-0.94809 -0.556727,-0.56775 -0.89848,-0.98116 -0.132292,-0.15985 -0.168121,-0.29766 -0.02756,-0.10197 -0.02756,-0.34175 0,-0.11024 0.07166,-0.28939 0.06614,-0.16536 0.140559,-0.26458 0.118512,-0.1571 0.42168,-0.42444 0.283875,-0.2508 0.474044,-0.37482 0.272852,-0.17915 0.818555,-0.36656 0.09646,-0.0331 0.110243,-0.0331 h 1.3615 q 0.01102,0 0.104731,0.0331 0.567751,0.20119 0.824066,0.36656 0.165365,0.10473 0.474045,0.37758 0.322461,0.28388 0.421679,0.42168 0.214974,0.2949 0.214974,0.62563 z m -0.16812,-0.27285 q -0.03307,-0.10749 -0.135048,-0.30868 -0.07166,-0.0937 -0.209462,-0.19844 -0.118511,-0.0854 -0.234266,-0.17088 0.245291,0.3638 0.578776,0.678 z m -0.573264,-0.52366 -0.358289,-0.44648 v 0.41065 z m 0.636653,0.75792 q 0,-0.0524 -0.341753,-0.39412 l 0.228754,0.7028 h 0.04961 q 0.06339,-0.21773 0.06339,-0.30868 z m -0.23151,0.27285 -0.316948,-0.90399 -0.410655,-0.0634 -0.159853,0.61461 z m 0.0028,0.15434 -0.771701,-0.3197 h -0.03032 l 0.344509,0.60634 q 0.07993,-0.003 0.234266,-0.0469 0.02481,-0.0138 0.223242,-0.23978 z m -0.857139,-1.05006 -0.719335,0.28388 0.60358,0.22599 z m 0,-0.16261 v -0.51538 l -0.917773,-0.38034 h -1.223697 l -0.917772,0.38034 v 0.51538 l 0.89848,0.4079 h 1.254014 z m 0.573264,1.53789 q -0.0689,0.011 -0.198438,0.0524 -0.08544,0.0551 -0.259071,0.16261 0.01929,0 0.06339,0.003 0.04134,0 0.06339,0 0.118511,0 0.190169,-0.0689 0.07166,-0.0717 0.14056,-0.14883 z m -0.289388,0.003 -0.347266,-0.63114 -0.363801,0.87918 q 0.0248,-0.0165 0.06615,-0.0165 0.03032,0 0.08544,0.011 0.05788,0.008 0.08819,0.008 0.04961,0 0.228754,-0.10749 0.228754,-0.1378 0.242535,-0.14331 z m -0.440972,-0.74139 -0.683507,-0.2756 -0.592556,0.54845 0.903993,0.65595 z m -0.854383,-0.28387 h -1.036284 l 0.523654,0.47404 z m 0.749652,1.36701 q -0.228754,-0.0303 -0.228754,-0.0303 -0.03583,0 -0.272851,0.0937 l 0.190169,0.0193 q 0.01102,0 0.311436,-0.0827 z m -0.388606,-0.0882 -0.843359,-0.61185 v 0.67524 l 0.471289,0.0827 z m 0.854383,0.0496 q -0.09922,0.006 -0.2949,0.0386 -0.01102,0.003 -0.377583,0.17639 -0.129535,0.34726 -0.380338,1.0418 z m -2.543857,-1.39733 -0.719335,-0.28388 0.124023,0.51814 z m -0.802018,-0.45476 v -0.41065 l -0.366557,0.44648 z m 1.515841,1.07212 -0.592556,-0.54019 -0.683506,0.2756 0.380338,0.9288 z m 0.369314,0.94257 q -0.03307,-0.003 -0.135047,-0.0248 -0.08544,-0.0165 -0.137804,-0.0165 -0.05237,0 -0.137804,0.0165 -0.101975,0.0221 -0.135047,0.0248 0.264583,0.0469 0.272851,0.0469 0.0083,0 0.272851,-0.0469 z m -0.30868,-0.1378 v -0.68626 l -0.843359,0.61185 0.37207,0.14607 z m 0.950846,0.20671 -0.396875,-0.0551 -0.479557,0.14056 v 1.27607 z m -2.888366,-2.20211 q -0.118512,0.0854 -0.234267,0.17364 -0.146072,0.10748 -0.209461,0.19568 -0.0441,0.10473 -0.135048,0.31419 0.369314,-0.3638 0.578776,-0.68351 z m 0.476801,0.83234 -0.159853,-0.61461 -0.410655,0.0634 -0.316948,0.90399 z m 0.394118,1.08313 -0.363802,-0.87918 -0.355533,0.63114 q 0.43546,0.2508 0.451996,0.2508 0.09095,0 0.267339,-0.003 z m 1.821765,0.82958 q -0.132291,0.21222 -0.405143,0.6339 -0.07717,0.10749 -0.220486,0.32797 -0.05788,0.10473 -0.05512,0.226 0.159852,-0.15158 0.432704,-0.49609 0.07717,-0.12954 0.140559,-0.339 0.05512,-0.17639 0.107486,-0.35278 z m -1.493792,-0.66146 -0.270095,-0.0882 -0.0083,-0.011 q -0.02481,0 -0.107487,0.011 -0.06615,0.006 -0.107487,0.006 0.272851,0.091 0.311436,0.091 0.02481,0 0.181901,-0.008 z m -0.810286,-1.06384 h -0.03032 l -0.7717,0.3197 q 0.06615,0.0965 0.223242,0.23978 0.07717,0.0138 0.234266,0.0469 z m -0.689018,-0.50161 q -0.341754,0.34727 -0.341754,0.39412 0,0.0799 0.06339,0.30868 h 0.04961 z m 2.235176,3.04547 v -1.27607 l -0.479557,-0.14056 -0.396875,0.0551 z m -1.606791,-1.6757 q -0.256315,-0.1819 -0.457509,-0.22324 0.06615,0.0772 0.20395,0.22324 z m 1.609547,2.04501 v -0.1378 l -0.683507,-1.05007 q 0.05237,0.17639 0.107487,0.35278 0.06339,0.20946 0.14056,0.339 0.0689,0.11851 0.20395,0.25907 0.115755,0.11851 0.23151,0.23702 z m -0.644921,-0.70555 q -0.115756,-0.32798 -0.380339,-1.0418 -0.135047,-0.0662 -0.272851,-0.12954 -0.159852,-0.0716 -0.289388,-0.0716 -0.05237,0 -0.110243,-0.0138 z'.format(x + 5.43, y + 1.9))


def ArtworkText():
    PentDx1 = 14.0
    PentDx3 = 16.0
    PentDx2 =  8.0
    PentDy  = 10.5
    t = ''

    # Render the pentagons that surround all 8 control groups.
    for y in range(4):
        gradientId = 'gradient_{}'.format(y)
        t += '\n'
        t += '    <path style="fill:url(#' + gradientId + ');fill-opacity:1;stroke:#000000;stroke-width:0.1;stroke-linecap:round;stroke-linejoin:bevel;stroke-dasharray:none"\n'
        t += '        d="'
        for x in range(2):
            sx, sy = PentagonOrigin(x, y)
            xdir = 1.0 - 2.0*x
            t += Move(sx - xdir*PentDx1, sy - PentDy)
            t += Line(sx + xdir*PentDx2, sy - PentDy)
            t += Line(sx + xdir*PentDx3, sy)
            t += Line(sx + xdir*PentDx2, sy + PentDy)
            t += Line(sx - xdir*PentDx1, sy + PentDy )
            t += 'z '
        t += '"\n'
        t += '    />\n'

    # Padding before end-artwork xml comment...
    t += '    '
    return t


def AudioPathText():
    PentDx1 = 14.0
    PentDx3 = 16.0
    PentDx2 =  8.0
    PentDy  = 10.5

    # Render a serpentine default-hidden emphasis border around the control groups
    # that affect audio inputs. We will show these only when audio inputs are connected.

    t = ''
    sx, sy = PentagonOrigin(0, 3)
    t += Move(sx - PentDx1, sy + PentDy)
    sx, sy = PentagonOrigin(0, 2)
    t += Line(sx - PentDx1, sy - PentDy)
    t += Line(sx + PentDx2, sy - PentDy)
    sx, sy = PentagonOrigin(1, 2)
    t += Line(sx - PentDx2, sy - PentDy)
    t += Line(sx + PentDx1, sy - PentDy)
    sx, sy = PentagonOrigin(1, 3)
    t += Line(sx + PentDx1, sy - PentDy)
    t += Line(sx - PentDx2, sy - PentDy)
    sx, sy = PentagonOrigin(0, 3)
    t += Line(sx + PentDx2, sy - PentDy)
    t += Line(sx + PentDx3, sy)
    t += Line(sx + PentDx2, sy + PentDy)
    t += 'z'

    path = Element('path')
    path.setAttrib('style', 'fill:#ffffff;fill-opacity:0.2;stroke:#e0e000;stroke-width:0.3;stroke-linecap:round;stroke-linejoin:bevel;stroke-dasharray:none')
    path.setAttrib('d', t)
    panel = Panel(PANEL_WIDTH)
    panel.append(path)
    return panel.svg()


def PatchMainPanel() -> int:
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
    return 0


def GenerateMainPanel() -> str:
    svgFileName = '../res/tubeunit2.svg'
    panel = Panel(PANEL_WIDTH)

    defs = Element('defs')
    defs.append(LinearGradient('gradient_0', 50.0,  0.0,  0.0, 0.0, '#906be8', '#b242bd'))
    defs.append(LinearGradient('gradient_1', 60.0,  0.0,  0.0, 0.0, '#6d96d6', '#3372d4'))
    defs.append(LinearGradient('gradient_2',  0.0,  0.0, 60.0, 0.0, '#986de4', '#4373e6'))
    defs.append(LinearGradient('gradient_3', 50.0,  0.0,  0.0, 0.0, '#3d81a0', '#26abbf'))
    panel.append(defs)

    pl = Element('g', 'PanelLayer')
    pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))

    # Pentagons that surround all 8 control groups.
    PentDx1 = 14.0
    PentDx3 = 16.0
    PentDx2 =  8.0
    PentDy  = 10.5
    for y in range(4):
        style = 'fill:url(#gradient_{});fill-opacity:1;stroke:#000000;stroke-width:0.1;stroke-linecap:round;stroke-linejoin:bevel;stroke-dasharray:none'.format(y)
        t = ''
        for x in range(2):
            sx, sy = PentagonOrigin(x, y)
            xdir = 1.0 - 2.0*x
            t += Move(sx - xdir*PentDx1, sy - PentDy)
            t += Line(sx + xdir*PentDx2, sy - PentDy)
            t += Line(sx + xdir*PentDx3, sy)
            t += Line(sx + xdir*PentDx2, sy + PentDy)
            t += Line(sx - xdir*PentDx1, sy + PentDy )
            t += 'z '
        pl.append(Element('path').setAttrib('style', style).setAttrib('d', t))

    with Font('Quicksand-Light.ttf') as font:
        # Model name 'tube unit' at top of panel.
        ti = TextItem('tube unit', font, 22.0)
        (dx, _) = ti.measure()
        tp = TextPath(ti, (panel.mmWidth - dx)/2, 0.3)
        tp.setAttrib('style', 'display:inline;stroke:#000000;stroke-width:0.35;stroke-linecap:round;stroke-linejoin:bevel')
        pl.append(tp)
        # Brand name 'sapphire' at bottom of panel.
        ti = TextItem('sapphire', font, 22.0)
        (dx, dy) = ti.measure()
        tp = TextPath(ti, (panel.mmWidth - dx)/2, panel.mmHeight - (dy + 0.5))
        tp.setAttrib('style', 'fill:#000000;stroke:#000000;stroke-width:0.265;stroke-linecap:square')
        pl.append(tp)

    # Gemstones
    gg = Element('g', 'gemstones')
    gg.setAttrib('style', 'stroke-width:0;fill:#0000ff;stroke:#2e2114;stroke-linecap:square;stroke-opacity:1')
    gemMarginX = 7.5
    gemY = 121.0
    gg.append(SapphireGemstone(gemMarginX, gemY))
    gg.append(SapphireGemstone(panel.mmWidth - (gemMarginX + SapphireGemstone.mmWidth), gemY))
    pl.append(gg)

    panel.append(pl)
    with open(svgFileName, 'wt') as outfile:
        outfile.write(panel.svg())
    print('patch_tubeunit.py: Wrote {}'.format(svgFileName))
    return 0


def GenerateAudioPathLayer() -> int:
    audioPathFileName = '../res/tubeunit_audio_path.svg'
    print('patch_tubeunit.py: Creating {}'.format(audioPathFileName))
    with open(audioPathFileName, 'wt') as outfile:
        outfile.write(AudioPathText())
    return 0


def LabelRJ(text:str, font:Font, i:int, j:int) -> TextPath:
    """Create a right-justified text label."""
    ti = TextItem(text, font, 10.0)
    (w, h) = ti.measure()
    (x, y) = PentagonOrigin(i, j)
    (dx, dy) = (7.0, -12.8)
    return TextPath(ti, x-w+dx, y+(h/2)+dy, text.lower() + '_label')


def LabelLJ(text:str, font:Font, i:int, j:int) -> TextPath:
    """Create a left-justified text label."""
    ti = TextItem(text, font, 10.0)
    (_, h) = ti.measure()
    (x, y) = PentagonOrigin(i, j)
    (dx, dy) = (-7.0, -12.8)
    return TextPath(ti, x+dx, y+(h/2)+dy, text.lower() + '_label')


def GenerateLabelLayer() -> int:
    svgFileName = '../res/tubeunit_labels.svg'
    panel = Panel(PANEL_WIDTH)

    group = Element('g', 'control_labels')
    group.setAttrib('style', 'stroke:#000000;stroke-width:0.25;stroke-linecap:round;stroke-linejoin:bevel')

    with Font('Quicksand-Light.ttf') as font:
        group.append(LabelRJ('AIRFLOW', font, 0, 0))
        group.append(LabelRJ('WIDTH',   font, 0, 1))
        group.append(LabelRJ('DECAY',   font, 0, 2))
        group.append(LabelRJ('ROOT',    font, 0, 3))
        group.append(LabelLJ('VORTEX',  font, 1, 0))
        group.append(LabelLJ('CENTER',  font, 1, 1))
        group.append(LabelLJ('ANGLE',   font, 1, 2))
        group.append(LabelLJ('SPRING',  font, 1, 3))

    panel.append(group)

    with open(svgFileName, 'wt') as outfile:
        outfile.write(panel.svg())

    print('patch_tubeunit.py: Wrote {}'.format(svgFileName))
    return 0


def Success() -> int:
    print('patch_tubeunit.py: SUCCESS')
    return 0


if __name__ == '__main__':
    sys.exit(
        PatchMainPanel() or
        GenerateMainPanel() or
        GenerateAudioPathLayer() or
        GenerateLabelLayer() or
        Success()
    )
