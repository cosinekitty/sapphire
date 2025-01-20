#!/usr/bin/env python3
#
#   make_sapphire_svg.py
#   Don Cross <cosinekitty@gmail.com>
#   https://github.com/cosinekitty/sapphire
#
#   Generates panel artwork svg files for all Sapphire modules
#   except for Tube Unit, whose generator lives in tubeunit_svg.py.
#   Combining panel generation for multiple modules into one
#   script makes it easier to maintain a common style across modules.
#
import sys
import math
from typing import List, Tuple, Dict
from svgpanel import *
from sapphire import *


def Print(message:str) -> int:
    print('make_sapphire_svg.py:', message)
    return 0


def Save(panel:Panel, filename:str) -> int:
    panel.save(filename)
    return 0


def Gradient(y1: float, y2: float, color1: str, color2: str, id: str) -> Element:
    elem = Element('linearGradient', id)
    elem.setAttrib('x1', '0')
    elem.setAttribFloat('y1', y1)
    elem.setAttrib('x2', '0')
    elem.setAttribFloat('y2', y2)
    elem.setAttrib('gradientUnits', 'userSpaceOnUse')
    elem.append(Element('stop').setAttrib('style', 'stop-color:{};stop-opacity:1;'.format(color1)).setAttrib('offset', '0'))
    elem.append(Element('stop').setAttrib('style', 'stop-color:{};stop-opacity:1;'.format(color2)).setAttrib('offset', '1'))
    return elem


def ControlGroupArt(moduleName: str, id: str, panel: Panel, y1: float, y2: float, gradientId: str) -> Path:
    path = ''
    xMargin = 0.38
    arcRadius = 4.0
    x1 = xMargin
    x2 = panel.mmWidth - xMargin
    path += Move(x1, y2)
    path += Line(x1, y1 + arcRadius)
    # https://www.w3.org/TR/SVG2/paths.html#PathDataEllipticalArcCommands
    # arc for upper-left corner
    path += 'A {0:g} {0:g} 0 0 1 {1:g} {2:g} '.format(arcRadius, x1 + arcRadius, y1)
    path += Line(x2 - arcRadius, y1)
    # arc for upper-right corner
    if moduleName == 'glee':
        # right arc points upward, left arc points downward.
        path += 'A {0:g} {0:g} 0 0 0 {1:g} {2:g} '.format(arcRadius, x2, y1 - arcRadius)
    else:
        # both arcs point downward.
        path += 'A {0:g} {0:g} 0 0 1 {1:g} {2:g} '.format(arcRadius, x2, y1 + arcRadius)
    path += Line(x2, y1)
    path += Line(x2, y2)
    path += 'z'
    style = 'fill:url(#{});fill-opacity:1;stroke-width:0;'.format(gradientId)
    return Path(path, style, id)


def AddControlGroup(pl: Element, controls: ControlLayer, font: Font, symbol: str, label: str, x: float, y: float, dxText: float = -1.0) -> None:
    dxControlGroup = 5.0
    dyControlGroup = 11.0
    dyControlText = -11.6
    controls.append(Component(symbol + '_knob', x, y))
    controls.append(Component(symbol + '_atten', x - dxControlGroup, y + dyControlGroup))
    controls.append(Component(symbol + '_cv', x + dxControlGroup, y + dyControlGroup))
    if dxText < 0.0:
        pl.append(CenteredControlTextPath(font, label, x, y + dyControlText + 2.4))
    else:
        pl.append(ControlTextPath(font, label, x - dxText, y + dyControlText))
    # Draw a pair of connector lines:
    # (1) from knob to attenuverter
    # (2) from knob to CV input
    t = ''
    t += Move(x, y)
    t += Line(x - dxControlGroup, y + dyControlGroup)
    t += ClosePath()
    t += Line(x + dxControlGroup, y + dyControlGroup)
    t += ClosePath()
    pl.append(Path(t, CONNECTOR_LINE_STYLE))


def GeneralLine(x1:float, y1:float, x2:float, y2:float, id:str) -> Path:
    path = ''
    path += Move(x1, y1)
    path += Line(x2, y2)
    return Path(path, CONNECTOR_LINE_STYLE, id, 'none')


def HorizontalLine(x1:float, x2:float, y:float, id:str) -> Path:
    path = ''
    path += Move(x1, y)
    path += Line(x2, y)
    return Path(path, CONNECTOR_LINE_STYLE, id, 'none')


def VerticalLine(x:float, y1:float, y2:float, id:str) -> Path:
    path = ''
    path += Move(x, y1)
    path += Line(x, y2)
    return Path(path, CONNECTOR_LINE_STYLE, id, 'none')


def GenerateChaosOperatorsPanel(cdict:Dict[str,ControlLayer]) -> int:
    PANEL_WIDTH = 6
    name = 'chaops'
    svgFileName = '../res/{}.svg'.format(name)
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    defs = Element('defs')
    pl.append(defs)
    panel.append(pl)
    cdict[name] = controls = ControlLayer()

    xmid = panel.mmWidth / 2
    dxMemoryButton = 8.0
    xStore  = xmid - dxMemoryButton
    xRecall = xmid + dxMemoryButton
    xRightPanel = panel.mmWidth - 1.0
    dxDisplay = 4.75
    dxFreezePortButton = 7.0

    yMemorySelect  = 24.0
    yMemoryDisplay = 33.0
    yMemoryButton  = 42.0
    yMemoryTriggerPorts = 53.0
    yMorph = 95.0
    yFreezeButton = 115.0
    yRecallLine = 59.0
    yStoreLine  = 68.0
    dyButtonText = 7.0
    dyGradient = dyButtonText + 4.0
    y1MemoryGradient = yMemorySelect - dyGradient
    y2MemoryGradient = yMemoryButton + dyGradient
    y1FreezeGradient = yFreezeButton - dyGradient
    y2FreezeGradient = panel.mmHeight - 4.0
    y1MorphGradient  = yMorph - dyGradient
    y2MorphGradient  = y1FreezeGradient - 1.0

    smallArcRadius  = 1.5
    mediumArcRadius = 2.5
    bigArcRadius    = 7.0

    def LineArtPath(path:str, id:str) -> Path:
        return Path(path, SIGNAL_LINE_STYLE, id, 'none')

    def ArrowHead(xTip:float, yTip:float) -> str:
        dx = 1.7
        dy = 0.7
        path = ''
        path += Move(xTip, yTip)
        path += Line(xTip - dx, yTip - dy)
        path += Move(xTip, yTip)
        path += Line(xTip - dx, yTip + dy)
        return path

    def StoreLineArt() -> Path:
        path = ''
        path += Move(xRightPanel, yStoreLine)
        path += Line(xStore + bigArcRadius, yStoreLine)
        # https://www.w3.org/TR/SVG2/paths.html#PathDataEllipticalArcCommands
        path += 'A {0:g} {0:g} 0 0 1 {1:g} {2:g} '.format(bigArcRadius, xStore, yStoreLine - bigArcRadius)
        path += Line(xStore, yMemoryDisplay + smallArcRadius)
        path += 'A {0:g} {0:g} 0 0 1 {1:g} {2:g} '.format(smallArcRadius, xStore + smallArcRadius, yMemoryDisplay)
        path += Line(xmid-dxDisplay, yMemoryDisplay)
        path += ArrowHead(xmid-dxDisplay, yMemoryDisplay)
        return LineArtPath(path, 'store_line_art')

    def RecallLineArt() -> Path:
        path = ''
        path += Move(xRightPanel, yRecallLine)
        path += Line(xRecall + mediumArcRadius, yRecallLine)
        path += 'A {0:g} {0:g} 0 0 1 {1:g} {2:g} '.format(mediumArcRadius, xRecall, yRecallLine - mediumArcRadius)
        path += Line(xRecall, yMemoryDisplay + mediumArcRadius)
        path += 'A {0:g} {0:g} 0 0 0 {1:g} {2:g} '.format(mediumArcRadius, xRecall - mediumArcRadius, yMemoryDisplay)
        path += Line(xmid+dxDisplay, yMemoryDisplay)
        path += ArrowHead(xRightPanel, yRecallLine)
        return LineArtPath(path, 'recall_line_art')

    def AddGradient(y1:float, y2:float, color1:str, color2:str, id:str) -> None:
        gradientId = id + '_gradient'
        artworkId  = id + '_artwork'
        defs.append(Gradient(y1, y2, color1, color2, gradientId))
        pl.append(ControlGroupArt(name, artworkId, panel, y1, y2, gradientId))

    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
        AddGradient(y1MemoryGradient, y2MemoryGradient, SAPPHIRE_AZURE_COLOR,   SAPPHIRE_PANEL_COLOR, 'memory')
        AddGradient(y1FreezeGradient, y2FreezeGradient, SAPPHIRE_TEAL_COLOR,    SAPPHIRE_PANEL_COLOR, 'freeze')
        AddGradient(y1MorphGradient,  y2MorphGradient,  SAPPHIRE_MAGENTA_COLOR, SAPPHIRE_PANEL_COLOR, 'morph')
        pl.append(CenteredGemstone(panel))
        pl.append(ModelNamePath(panel, font, name))
        pl.append(StoreLineArt())
        pl.append(RecallLineArt())
        pl.append(CenteredControlTextPath(font, 'MEMORY', xmid, yMemorySelect - dyButtonText))
        pl.append(CenteredControlTextPath(font, 'FREEZE', xmid, yFreezeButton - dyButtonText))
        pl.append(CenteredControlTextPath(font, 'MORPH',  xmid, yMorph - dyButtonText))
        pl.append(HorizontalLine(xmid - dxFreezePortButton, xmid + dxFreezePortButton, yFreezeButton, 'freeze_line_art'))
        pl.append(VerticalLine(xmid, yMemorySelect, yMemoryDisplay, 'memory_vline'))
        AddFlatControlGroup(pl, controls, xmid, yMemorySelect, 'memsel')
        controls.append(Component('store_button',   xStore,  yMemoryButton))
        controls.append(Component('recall_button',  xRecall, yMemoryButton))
        controls.append(Component('store_trigger',  xStore,  yMemoryTriggerPorts))
        controls.append(Component('recall_trigger', xRecall, yMemoryTriggerPorts))
        controls.append(Component('memory_address_display', xmid, yMemoryDisplay))
        controls.append(Component('freeze_button', xmid + dxFreezePortButton, yFreezeButton))
        controls.append(Component('freeze_input',  xmid - dxFreezePortButton, yFreezeButton))
        AddFlatControlGroup(pl, controls, xmid, yMorph, 'morph')
    return Save(panel, svgFileName)


def GenerateChaosPanel(cdict:Dict[str,ControlLayer], name: str) -> int:
    PANEL_WIDTH = 4
    svgFileName = '../res/{}.svg'.format(name)
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    defs = Element('defs')
    pl.append(defs)
    panel.append(pl)
    cdict[name] = controls = ControlLayer()
    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
        pl.append(CenteredGemstone(panel))
        pl.append(ModelNamePath(panel, font, name))
        outputPortY1 = 88.0
        outputPortDY =  9.0
        xmid = panel.mmWidth / 2
        controls.append(Component('x_output', xmid, outputPortY1 + 0*outputPortDY))
        controls.append(Component('y_output', xmid, outputPortY1 + 1*outputPortDY))
        controls.append(Component('z_output', xmid, outputPortY1 + 2*outputPortDY))
        controls.append(Component('p_output', xmid, outputPortY1 + 3*outputPortDY))
        ySpeedKnob = 26.0
        yChaosKnob = 57.0
        xOutLabel = xmid - 3.9
        yOutLabel = outputPortY1 - 10.5
        artSpaceAboveKnob = 13.0
        artSpaceBelowKnob = 25.0
        xPortLabel = xmid - 7.5
        yPortLabel = outputPortY1 - 2.4
        outGradY1 = yOutLabel - 1.5
        outGradY2 = yPortLabel + 2*outputPortDY + 10.0
        defs.append(Gradient(ySpeedKnob-artSpaceAboveKnob, ySpeedKnob+artSpaceBelowKnob, SAPPHIRE_AZURE_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_blue'))
        defs.append(Gradient(yChaosKnob-artSpaceAboveKnob, yChaosKnob+artSpaceBelowKnob, SAPPHIRE_MAGENTA_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_purple'))
        defs.append(Gradient(outGradY1, outGradY2, SAPPHIRE_TEAL_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_out'))
        pl.append(ControlGroupArt(name, 'speed_art', panel, ySpeedKnob-artSpaceAboveKnob, ySpeedKnob+artSpaceBelowKnob, 'gradient_blue'))
        pl.append(ControlGroupArt(name, 'chaos_art', panel, yChaosKnob-artSpaceAboveKnob, yChaosKnob+artSpaceBelowKnob, 'gradient_purple'))
        pl.append(ControlGroupArt(name, 'out_art', panel, outGradY1, outGradY2, 'gradient_out'))
        AddControlGroup(pl, controls, font, 'speed', 'SPEED', xmid, ySpeedKnob)
        AddControlGroup(pl, controls, font, 'chaos', 'CHAOS', xmid, yChaosKnob)
        pl.append(ControlTextPath(font, 'OUT', xOutLabel, yOutLabel, 'out_label'))
        pl.append(ControlTextPath(font, 'X',  xPortLabel, yPortLabel + 0*outputPortDY, 'port_label_x'))
        pl.append(ControlTextPath(font, 'Y',  xPortLabel, yPortLabel + 1*outputPortDY, 'port_label_y'))
        pl.append(ControlTextPath(font, 'Z',  xPortLabel, yPortLabel + 2*outputPortDY, 'port_label_z'))
        pl.append(ControlTextPath(font, 'P',  xPortLabel, yPortLabel + 3*outputPortDY, 'port_label_p'))
        pl.append(PolyPortHexagon(xmid, outputPortY1 + 3*outputPortDY))
    return Save(panel, svgFileName)


def GenerateTricorderPanel() -> int:
    PANEL_WIDTH = 25
    svgFileName = '../res/tricorder.svg'
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
        pl.append(SapphireModelInsignia(panel, font, 'tricorder'))
    panel.append(pl)
    return Save(panel, svgFileName)


def GenerateTinToutPanel(cdict:Dict[str,ControlLayer], name:str, dir:str, ioLabel:str, dxCoordLabel:float) -> int:
    PANEL_WIDTH = 4
    svgFileName = '../res/{}.svg'.format(name)
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    panel.append(pl)
    defs = Element('defs')
    pl.append(defs)
    cdict[name] = controls = ControlLayer()
    xmid = panel.mmWidth/2
    inputPortY1 = 25.0
    inputPortDY = 10.0
    inputPortY2 = 110.0
    xPortLabel = xmid + dxCoordLabel
    yInPortLabel = inputPortY1 - 2.4
    yInLabel = inputPortY1 - 10.5
    ioPortsGradY1 = yInLabel - 1.5
    ioPortsGradY2 = yInPortLabel + 2*inputPortDY + 10.0
    xTriggerPortLabel = xmid - 5.3
    yTriggerPortLabel = inputPortY2 - 10.0
    clearGradY1 = yTriggerPortLabel - 1.5
    clearGradY2 = clearGradY1 + 10.0
    levelGradY1 = 62.0
    levelGradY2 = 90.0
    defs.append(Gradient(ioPortsGradY1, ioPortsGradY2, SAPPHIRE_AZURE_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_io'))
    defs.append(Gradient(levelGradY1, levelGradY2, SAPPHIRE_MAGENTA_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_level'))
    defs.append(Gradient(clearGradY1, clearGradY2, SAPPHIRE_TEAL_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_clear'))
    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
        pl.append(ModelNamePath(panel, font, name))
        pl.append(CenteredGemstone(panel))
        pl.append(ControlGroupArt(name, 'io_art', panel, ioPortsGradY1, ioPortsGradY2, 'gradient_io'))
        pl.append(ControlGroupArt(name, 'level_art', panel, levelGradY1, levelGradY2, 'gradient_level'))
        pl.append(ControlGroupArt(name, 'clear_art', panel, clearGradY1, clearGradY2, 'gradient_clear'))
        pl.append(ControlTextPath(font, 'X',  xPortLabel, yInPortLabel + 0*inputPortDY, 'port_label_x'))
        pl.append(ControlTextPath(font, 'Y',  xPortLabel, yInPortLabel + 1*inputPortDY, 'port_label_y'))
        pl.append(ControlTextPath(font, 'Z',  xPortLabel, yInPortLabel + 2*inputPortDY, 'port_label_z'))
        pl.append(ControlTextPath(font, 'P',  xPortLabel, yInPortLabel + 3*inputPortDY, 'port_label_p'))
        controls.append(Component('x_' + dir, xmid, inputPortY1 + 0*inputPortDY))
        controls.append(Component('y_' + dir, xmid, inputPortY1 + 1*inputPortDY))
        controls.append(Component('z_' + dir, xmid, inputPortY1 + 2*inputPortDY))
        controls.append(Component('p_' + dir, xmid, inputPortY1 + 3*inputPortDY))
        controls.append(Component('clear_trigger_' + dir, xmid, inputPortY2))
        AddControlGroup(pl, controls, font, 'level', 'LEVEL', xmid, 75.0, 5.5)
        pl.append(CenteredControlTextPath(font, ioLabel, xmid, yInLabel + 2.5, 'io_label'))
        pl.append(ControlTextPath(font, 'CLEAR', xTriggerPortLabel, yTriggerPortLabel, 'clear_label'))
        pl.append(PolyPortHexagon(xmid, inputPortY1 + 3*inputPortDY))
    return Save(panel, svgFileName)


def SaveLabelLayer(hpWidth:int, text:str, outSvgFileName:str, xCenter:float, yCenter:float) -> int:
    with Font(SAPPHIRE_FONT_FILENAME) as font:
        panel = Panel(hpWidth)
        group = Element('g', 'labels')
        group.setAttrib('style', CONTROL_LABEL_STYLE)
        panel.append(group)
        ti = TextItem(text, font, CONTROL_LABEL_POINTS)
        (w, h) = ti.measure()
        group.append(TextPath(ti, xCenter - w/2, yCenter - h/2))
        return Save(panel, outSvgFileName)


def RectangularBubble(x1:float, y1:float, dx:float, dy:float, radius:float, style:str, id:str = '') -> Path:
    x2 = x1 + dx
    y2 = y1 + dy
    x1r = x1 + radius
    x2r = x2 - radius
    y1r = y1 + radius
    y2r = y2 - radius
    p = ''
    p += Move(x1, y1r)
    p += Arc(radius, x1r, y1, 1)
    p += Line(x2r, y1)
    p += Arc(radius, x2, y1r, 1)
    p += Line(x2, y2r)
    p += Arc(radius, x2r, y2, 1)
    p += Line(x1r, y2)
    p += Arc(radius, x1, y2r, 1)
    p += ClosePath()
    return Path(p, style, id)


def FillStyle(color:str, opacity:float = 1.0) -> str:
    return 'fill:{:s};fill-opacity:{:g};stroke:none;'.format(color, opacity)


def GradientStyle(gradientId:str, opacity:float) -> str:
    return 'fill:url(#{:s});opacity:{:g};stroke:none'.format(gradientId, opacity)


def GenerateNucleusPanel(cdict:Dict[str,ControlLayer]) -> int:
    name = 'nucleus'
    svgFileName = '../res/{}.svg'.format(name)
    PANEL_WIDTH = 16
    panel = Panel(PANEL_WIDTH)
    defs = Element('defs')
    xBubbleLeft = 4.0
    xBubbleRight = 73.2
    bubbleRadius = 6.0
    xSerpentLeft = xBubbleLeft + bubbleRadius
    xSerpentRight = xBubbleRight - bubbleRadius
    defs.append(LinearGradient('gradient_controls', xSerpentLeft,  0.0, xSerpentRight, 0.0, SAPPHIRE_PANEL_COLOR, SAPPHIRE_PURPLE_COLOR))
    defs.append(LinearGradient('gradient_input',    xSerpentLeft,  0.0, xSerpentRight, 0.0, SAPPHIRE_AZURE_COLOR, SAPPHIRE_PANEL_COLOR))
    defs.append(LinearGradient('gradient_output',   xSerpentLeft,  0.0, xSerpentRight, 0.0, SAPPHIRE_PANEL_COLOR, SAPPHIRE_SALMON_COLOR))
    panel.append(defs)
    pl = Element('g', 'PanelLayer')
    panel.append(pl)
    cdict[name] = controls = ControlLayer()
    xmid = panel.mmWidth / 2
    dxPort = 12.5               # horizontal distance between X, Y, Z columns.
    yIn = 58.0                  # vertical position of center of input X, Y, Z ports.
    yCushion = 7.5              # vertical space above input ports to put labels ("X", "Y", "Z").
    yOutTop = 86.0              # vertical position of the top row of output ports.
    yOutBottom = 112.0          # vertical position of the bottom row of output ports.
    nOutputParticles = 4        # how many particles are used for output.
    dyOut = (yOutBottom - yOutTop) / (nOutputParticles - 1)     # vertical space between output rows.
    yInVarNames = yIn - yCushion  # vertical positions of the labels "X", "Y", "Z".
    yOutVarNames = yOutTop - yCushion
    xOutLeft = xmid
    dxRightMargin = 4.0
    dxLeft = 5.0
    dxTotal = panel.mmWidth - xOutLeft + dxPort/2 - dxRightMargin + dxLeft
    yKnobRow1 = 25.0
    yOutLevel = yOutTop + (yOutBottom-yOutTop)/2 + 4.0

    # Write a C++ header file that contains bounding rectangles for the 4 output rows.
    # This script remains the Single Source Of Truth for how the panel design is laid out.
    headerText = ''
    headerText += '// {}_panel.hpp - AUTO-GENERATED; DO NOT EDIT.\n'.format(name)
    headerText += '#pragma once\n'
    headerText += 'namespace Sapphire\n'
    headerText += '{\n'
    headerText += '    namespace Nucleus\n'
    headerText += '    {\n'
    headerText += '        namespace Panel\n'
    headerText += '        {\n'
    headerText += '            const float DxOut   = {:9.3f}f;    // horizontal distance between output port columns.\n'.format(dxPort)
    headerText += '            const float DyOut   = {:9.3f}f;    // vertical distance between output port rows.\n'.format(dyOut)
    headerText += '            const float X1Out   = {:9.3f}f;    // x-coord of upper left output port\'s center.\n'.format(xOutLeft)
    headerText += '            const float Y1Out   = {:9.3f}f;    // y-coord of upper left output port\'s center.\n'.format(yOutTop)
    headerText += '            const float DxTotal = {:9.3f}f;    // total horizontal space to allocate to each bounding box.\n'.format(dxTotal)
    headerText += '            const float DxLeft  = {:9.3f}f;    // extra space reserved on the left for BCDE.\n'.format(dxLeft)
    headerText += '        }\n'
    headerText += '    }\n'
    headerText += '}\n'
    headerFileName = '../src/{}_panel.hpp'.format(name)
    UpdateFileIfChanged(headerFileName, headerText)

    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
        pl.append(ModelNamePath(panel, font, name))
        pl.append(SapphireInsignia(panel, font))
        # Rectangular bubbles are background patterns that visually group related controls/ports.
        pl.append(RectangularBubble(xBubbleLeft, 12.0, xBubbleRight, 31.0, bubbleRadius, GradientStyle('gradient_controls', 0.7), 'controls_bubble'))
        pl.append(RectangularBubble(xBubbleLeft, 43.0, xBubbleRight, 30.0, bubbleRadius, GradientStyle('gradient_input',    1.0), 'input_bubble'))
        pl.append(RectangularBubble(xBubbleLeft, 73.0, xBubbleRight, 45.0, bubbleRadius, GradientStyle('gradient_output',   0.8), 'output_bubble'))
        xInputCenter = xmid - 12.0
        xInPos = xInputCenter - dxPort
        xOutPos = xmid
        for varname in 'xyz':
            pl.append(CenteredControlTextPath(font, varname.upper(), xInPos , yInVarNames))
            pl.append(CenteredControlTextPath(font, varname.upper(), xOutPos, yOutVarNames))
            # Input row A
            varlabel = Component(varname + '_input', xInPos, yIn)
            controls.append(varlabel)
            ypos = yOutTop
            for i in range(1, 1+nOutputParticles):
                # Output rows BCDE
                controls.append(Component(varname + str(i) + '_output', xOutPos, ypos))
                ypos += dyOut
            xInPos += dxPort
            xOutPos += dxPort

        pl.append(ControlTextPath(font, 'A', xInputCenter - dxPort - 8.5, yIn - 2.5))
        xpos = xmid - 9.0
        ypos = yOutTop - 2.5
        for label in 'BCDE':
            pl.append(ControlTextPath(font, label, xpos, ypos))
            ypos += dyOut

        dxKnob = 25.0
        xKnobLeft  = xmid - dxKnob
        xKnobRight = xmid + dxKnob
        AddControlGroup(pl, controls, font, 'speed',    'SPEED',  xKnobLeft,    yKnobRow1, 5.5)
        AddControlGroup(pl, controls, font, 'decay',    'DECAY',  xmid,         yKnobRow1, 5.5)
        AddControlGroup(pl, controls, font, 'magnet',   'MAGNET', xKnobRight,   yKnobRow1, 7.0)
        AddControlGroup(pl, controls, font, 'in_drive', 'IN',     xKnobRight,   yIn - 2.5, 1.5)
        AddControlGroup(pl, controls, font, 'out_level','OUT',    xKnobLeft,    yOutLevel, 3.5)

        # Add toggle button with alternating text labels AUDIO and CONTROL.
        # We do this by creating two extra SVG files that contain one word each.
        # Then we hide/show the layers as needed to show only AUDIO or CONTROL (not both).
        xButton = xKnobLeft
        yButton = yOutLevel - 19.5
        controls.append(Component('audio_mode_button', xButton, yButton))
        xText = xButton
        yText = yButton - 6.5
        SaveLabelLayer(PANEL_WIDTH, 'AUDIO',   '../res/{}_label_audio.svg'.format(name),   xText, yText)
        SaveLabelLayer(PANEL_WIDTH, 'CONTROL', '../res/{}_label_control.svg'.format(name), xText, yText)
    return Save(panel, svgFileName)


def GeneratePolynucleusPanel(cdict:Dict[str,ControlLayer]) -> int:
    name = 'polynucleus'
    svgFileName = '../res/{}.svg'.format(name)
    PANEL_WIDTH = 16
    panel = Panel(PANEL_WIDTH)
    defs = Element('defs')
    xBubbleLeft = 4.0
    xBubbleRight = 73.2
    bubbleRadius = 6.0
    xSerpentLeft = xBubbleLeft + bubbleRadius
    xSerpentRight = xBubbleRight - bubbleRadius
    defs.append(LinearGradient('gradient_controls', xSerpentLeft,  0.0, xSerpentRight, 0.0, SAPPHIRE_PANEL_COLOR, SAPPHIRE_PURPLE_COLOR))
    defs.append(LinearGradient('gradient_input',    xSerpentLeft,  0.0, xSerpentRight, 0.0, SAPPHIRE_AZURE_COLOR, SAPPHIRE_PANEL_COLOR))
    defs.append(LinearGradient('gradient_output',   xSerpentLeft,  0.0, xSerpentRight, 0.0, SAPPHIRE_PANEL_COLOR, SAPPHIRE_SALMON_COLOR))
    panel.append(defs)
    pl = Element('g', 'PanelLayer')
    panel.append(pl)
    cdict[name] = controls = ControlLayer()
    xmid = panel.mmWidth / 2
    dxKnob = 25.0
    xKnobLeft  = xmid - dxKnob
    xKnobRight = xmid + dxKnob
    yIn = 58.0
    yInPort = yIn - 8.0         # vertical position of center of Particle A input port
    yOutTop = 81.0              # vertical position of the top row of output ports.
    yOutBottom = 110.0          # vertical position of the bottom row of output ports.
    nOutputParticles = 4        # how many particles are used for output.
    dyOut = (yOutBottom - yOutTop) / (nOutputParticles - 1)     # vertical space between output rows.
    xOutLeft = xKnobRight - 5.0
    dxLeft = 5.0
    dxTotal = 21.5
    dxLabel = 7.0
    yKnobRow1 = 25.0
    yKnobRow2 = yIn - 2.5
    yKnobRow3 = yOutTop + (yOutBottom-yOutTop)/2 + 6.0

    ENABLE_EXTRA_CONTROLS = False

    # Write a C++ header file that contains bounding rectangles for the 4 output rows.
    # This script remains the Single Source Of Truth for how the panel design is laid out.

    headerText = ''
    headerText += '// {}_panel.hpp - AUTO-GENERATED; DO NOT EDIT.\n'.format(name)
    headerText += '#pragma once\n'
    headerText += '#define POLYNUCLEUS_ENABLE_EXTRA_CONTROLS {:d}\n\n'.format(int(ENABLE_EXTRA_CONTROLS))
    headerText += 'namespace Sapphire\n'
    headerText += '{\n'
    headerText += '    namespace Polynucleus\n'
    headerText += '    {\n'
    headerText += '        namespace Panel\n'
    headerText += '        {\n'
    headerText += '            const float DyOut   = {:9.3f}f;    // vertical distance between output ports.\n'.format(dyOut)
    headerText += '            const float X1Out   = {:9.3f}f;    // x-coord of output port\'s center.\n'.format(xOutLeft)
    headerText += '            const float Y1Out   = {:9.3f}f;    // y-coord of output port\'s center.\n'.format(yOutTop)
    headerText += '            const float DxTotal = {:9.3f}f;    // total horizontal space to allocate to each bounding box.\n'.format(dxTotal)
    headerText += '            const float DxLeft  = {:9.3f}f;    // extra space reserved on the left for BCDE.\n'.format(dxLeft)
    headerText += '        }\n'
    headerText += '    }\n'
    headerText += '}\n'
    headerFileName = '../src/{}_panel.hpp'.format(name)
    UpdateFileIfChanged(headerFileName, headerText)

    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
        pl.append(ModelNamePath(panel, font, name))
        pl.append(SapphireInsignia(panel, font))
        # Rectangular bubbles are background patterns that visually group related controls/ports.
        pl.append(RectangularBubble(xBubbleLeft, 12.0, xBubbleRight, 31.0, bubbleRadius, GradientStyle('gradient_controls', 0.7), 'controls_bubble'))
        pl.append(RectangularBubble(xBubbleLeft, 43.0, xBubbleRight, 30.0, bubbleRadius, GradientStyle('gradient_input',    1.0), 'input_bubble'))
        pl.append(RectangularBubble(xBubbleLeft, 73.0, xBubbleRight, 45.0, bubbleRadius, GradientStyle('gradient_output',   0.8), 'output_bubble'))

        # Create label + input port for particle A.
        controls.append(Component('a_input', xKnobLeft, yInPort))
        pl.append(CenteredControlTextPath(font, 'A', xKnobLeft + dxLabel, yInPort))

        # Create labels + output ports for particles BCDE.
        ypos = yOutTop
        for label in 'BCDE':
            controls.append(Component(label.lower() + '_output', xKnobRight, ypos))
            pl.append(CenteredControlTextPath(font, label, xKnobRight - dxLabel, ypos))
            ypos += dyOut

        AddControlGroup(pl, controls, font, 'speed',    'SPEED',  xKnobLeft,  yKnobRow1, 5.5)
        AddControlGroup(pl, controls, font, 'decay',    'DECAY',  xmid,       yKnobRow1, 5.5)
        AddControlGroup(pl, controls, font, 'magnet',   'MAGNET', xKnobRight, yKnobRow1, 7.0)
        AddControlGroup(pl, controls, font, 'in_drive', 'IN',     xmid,       yKnobRow2, 1.5)
        AddControlGroup(pl, controls, font, 'out_level','OUT',    xKnobRight, yKnobRow2, 3.5)
        if ENABLE_EXTRA_CONTROLS:
            AddControlGroup(pl, controls, font, 'spin',     'SPIN',   xKnobLeft,  yKnobRow3, 3.7)
            AddControlGroup(pl, controls, font, 'visc',     'VISC',   xmid,       yKnobRow3, 3.7)

        xButton = xKnobLeft
        yButton = yOutTop + 3*dyOut
        controls.append(Component('clear_button', xButton, yButton))
        pl.append(CenteredControlTextPath(font, 'CLEAR', xButton, yButton - 6.5))

        # Add toggle button with alternating text labels AUDIO and CONTROL.
        # We do this by creating two extra SVG files that contain one word each.
        # Then we hide/show the layers as needed to show only AUDIO or CONTROL (not both).
        xButton = xKnobLeft
        yButton = yIn + 8.0
        controls.append(Component('audio_mode_button', xButton, yButton))
        xText = xButton
        yText = yButton - 6.5
        SaveLabelLayer(PANEL_WIDTH, 'AUDIO',   '../res/{}_label_audio.svg'.format(name),   xText, yText)
        SaveLabelLayer(PANEL_WIDTH, 'CONTROL', '../res/{}_label_control.svg'.format(name), xText, yText)
    return Save(panel, svgFileName)



def GenerateHissPanel(cdict:Dict[str,ControlLayer]) -> int:
    numOutputs = 10      # Keep in sync with src/hiss.cpp ! Sapphire::Hiss::NumOutputs
    svgFileName = '../res/hiss.svg'
    PANEL_WIDTH = 3
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    panel.append(pl)
    defs = Element('defs')
    pl.append(defs)
    cdict['hiss'] = controls = ControlLayer()
    xmid = panel.mmWidth / 2
    yChannelDisplay = 14.75
    ytop = 27.0
    ybottom = 114.0
    dyGradientSpacing = 6.0
    dyOut = (ybottom - ytop) / (numOutputs - 1)
    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
        defs.append(Gradient(ytop - dyGradientSpacing, panel.mmHeight - 15.0, '#1058ef', SAPPHIRE_PANEL_COLOR, 'gradient_hiss'))
        pl.append(ControlGroupArt('hiss', 'hiss_art', panel, ytop - dyGradientSpacing, panel.mmHeight - 15.0, 'gradient_hiss'))
        pl.append(CenteredGemstone(panel))
        pl.append(ModelNamePath(panel, font, 'hiss'))
        yout = ytop
        for i in range(numOutputs):
            name = 'random_output_{:d}'.format(i + 1)
            controls.append(Component(name, xmid, yout))
            yout += dyOut
        controls.append(Component('channel_display', xmid, yChannelDisplay))
    return Save(panel, svgFileName)


MootsPanelLayerXml = r'''
<g id="sapphire_moots_legacy_panel">
    <rect
       style="display:inline;fill:#4f8df2;fill-opacity:1;fill-rule:nonzero;stroke:#5021d4;stroke-width:0.401661;stroke-linecap:round;stroke-linejoin:round;stroke-dasharray:none;stroke-opacity:1;image-rendering:auto"
       width="50.384171"
       height="128.0802"
       x="0.20660542"
       y="0.2061476"/>
    <path style="fill:#29aab4;fill-opacity:1;stroke:#000000;stroke-width:0.290222;stroke-linecap:square"
       d="M 2.9716623,17.689808 9.6879004,11.709386 H 40.513186 l 6.732173,5.520255 -17.048685,12.535574 -9.948611,5.39e-4 z"
    />
    <path style="fill:#0081d7;fill-opacity:1;stroke:#000000;stroke-width:0.292593;stroke-linecap:square"
       d="M 2.9606053,39.55876 9.6763825,33.479847 H 40.499555 l 6.731712,5.611167 -17.051887,12.336407 -10.209716,-0.0042 z"
    />
    <path style="fill:#976de4;fill-opacity:1;stroke:#000000;stroke-width:0.289434;stroke-linecap:square"
       d="M 2.8997186,60.998855 9.6161045,55.051004 H 40.442086 l 6.732325,5.490191 -17.065335,12.228021 -9.926461,-0.01446 z"
    />
    <path style="fill:#0060f9;fill-opacity:1;stroke:#000000;stroke-width:0.291063;stroke-linecap:square"
       d="M 2.9154563,82.47698 9.6315298,76.461708 H 40.456067 l 6.73201,5.552424 -17.236589,12.516471 -9.740299,-0.02176 z"
    />
    <path style="fill:#637cdb;fill-opacity:1;stroke:#000000;stroke-width:0.288099;stroke-linecap:square"
       d="M 2.9726673,103.97346 9.6893159,98.080573 H 40.51649 l 6.732585,5.439457 -17.437172,12.66642 -9.441437,-0.0102 z"
    />
    <g id="brand_text" style="fill:#0b0607;stroke:#000000;stroke-width:0.265;stroke-linecap:square" transform="translate(-0.4457988,-0.44557112)">
      <path d="m 10.804944,125.41509 q -0.05482,-0.0862 -0.05482,-0.1488 0,-0.0548 0.06265,-0.10181 0.03916,-0.0392 0.09398,-0.0392 0.07831,0 0.125304,0.0627 0.454228,0.65001 1.268705,0.65001 0.41507,0 0.72833,-0.20362 0.313261,-0.21145 0.313261,-0.56387 0,-0.36025 -0.274103,-0.54037 -0.274103,-0.18013 -0.751825,-0.2976 -0.634353,-0.15663 -1.002433,-0.41507 -0.36025,-0.26627 -0.36025,-0.74399 0,-0.46206 0.368081,-0.75966 0.368081,-0.30543 0.947613,-0.30543 0.321092,0 0.634352,0.12531 0.321092,0.1253 0.579532,0.3994 0.05482,0.047 0.05482,0.10965 0,0.0705 -0.05482,0.1253 -0.06265,0.0313 -0.10181,0.0313 -0.05482,0 -0.09398,-0.047 -0.407238,-0.4464 -1.049422,-0.4464 -0.41507,0 -0.704836,0.20362 -0.281935,0.20362 -0.281935,0.56387 0.02349,0.33675 0.305429,0.52471 0.289766,0.18012 0.845803,0.32109 0.399407,0.10181 0.657847,0.22712 0.266272,0.11747 0.422902,0.33675 0.15663,0.21928 0.15663,0.5717 0,0.49339 -0.383744,0.78315 -0.375912,0.28194 -0.98677,0.28194 -0.892792,0 -1.464493,-0.70484 z"/>
      <path d="m 17.9316,122.13369 q 0.07048,0 0.109641,0.047 0.04699,0.047 0.04699,0.10964 v 3.59466 q 0,0.0627 -0.04699,0.10964 -0.04699,0.047 -0.109641,0.047 -0.07048,0 -0.117473,-0.047 -0.03916,-0.047 -0.03916,-0.10964 v -0.70484 q -0.195788,0.39941 -0.603026,0.67351 -0.407239,0.26628 -0.900624,0.26628 -0.51688,0 -0.93195,-0.26628 -0.41507,-0.2741 -0.650015,-0.74399 -0.234945,-0.47772 -0.234945,-1.06508 0,-0.57954 0.234945,-1.0416 0.242777,-0.46989 0.657847,-0.72833 0.41507,-0.26627 0.924118,-0.26627 0.509048,0 0.916287,0.26627 0.407238,0.26628 0.587363,0.71267 v -0.697 q 0,-0.0626 0.03916,-0.10964 0.04699,-0.047 0.117473,-0.047 z m -1.644617,3.68864 q 0.430733,0 0.77532,-0.22712 0.352417,-0.23494 0.548205,-0.64218 0.195788,-0.40724 0.195788,-0.90845 0,-0.47773 -0.195788,-0.87713 -0.195788,-0.39941 -0.548205,-0.62652 -0.344587,-0.23495 -0.77532,-0.23495 -0.430733,0 -0.783151,0.22711 -0.344586,0.21929 -0.540374,0.61869 -0.195788,0.39158 -0.195788,0.8928 0,0.50121 0.195788,0.90845 0.195788,0.40724 0.540374,0.64218 0.344586,0.22712 0.783151,0.22712 z"/>
      <path d="m 21.205166,122.0867 q 0.51688,0 0.93195,0.25844 0.41507,0.25844 0.650015,0.71267 0.234945,0.45422 0.234945,1.02592 0,0.56387 -0.242777,1.02593 -0.234945,0.45423 -0.650015,0.7205 -0.41507,0.25844 -0.924118,0.25844 -0.469891,0 -0.861466,-0.23495 -0.391576,-0.23494 -0.642184,-0.66567 v 2.2633 q 0,0.0627 -0.04699,0.10964 -0.03916,0.047 -0.109641,0.047 -0.06265,0 -0.109641,-0.047 -0.04699,-0.047 -0.04699,-0.10964 v -5.20795 q 0,-0.0627 0.03916,-0.10964 0.04699,-0.047 0.117473,-0.047 0.07048,0 0.109641,0.047 0.04699,0.047 0.04699,0.10964 v 0.7205 q 0.234945,-0.4464 0.626521,-0.65785 0.391575,-0.21928 0.877129,-0.21928 z m -0.01566,3.7043 q 0.430733,0 0.775319,-0.21928 0.344587,-0.21928 0.540375,-0.61086 0.203619,-0.39157 0.203619,-0.87713 0,-0.48555 -0.203619,-0.86929 -0.195788,-0.39158 -0.540375,-0.61086 -0.344586,-0.21928 -0.775319,-0.21928 -0.438565,0 -0.790983,0.21928 -0.344586,0.21145 -0.540374,0.60302 -0.187956,0.38375 -0.187956,0.87713 0,0.49339 0.187956,0.88496 0.195788,0.38375 0.540374,0.60303 0.352418,0.21928 0.790983,0.21928 z"/>
      <path d="m 25.864909,122.0867 q 0.51688,0 0.93195,0.25844 0.41507,0.25844 0.650015,0.71267 0.234946,0.45422 0.234946,1.02592 0,0.56387 -0.242777,1.02593 -0.234946,0.45423 -0.650016,0.7205 -0.41507,0.25844 -0.924118,0.25844 -0.46989,0 -0.861466,-0.23495 -0.391575,-0.23494 -0.642184,-0.66567 v 2.2633 q 0,0.0627 -0.04699,0.10964 -0.03916,0.047 -0.109641,0.047 -0.06265,0 -0.109641,-0.047 -0.04699,-0.047 -0.04699,-0.10964 v -5.20795 q 0,-0.0627 0.03916,-0.10964 0.04699,-0.047 0.117473,-0.047 0.07048,0 0.109641,0.047 0.04699,0.047 0.04699,0.10964 v 0.7205 q 0.234946,-0.4464 0.626521,-0.65785 0.391576,-0.21928 0.877129,-0.21928 z m -0.01566,3.7043 q 0.430733,0 0.77532,-0.21928 0.344586,-0.21928 0.540374,-0.61086 0.203619,-0.39157 0.203619,-0.87713 0,-0.48555 -0.203619,-0.86929 -0.195788,-0.39158 -0.540374,-0.61086 -0.344587,-0.21928 -0.77532,-0.21928 -0.438564,0 -0.790982,0.21928 -0.344587,0.21145 -0.540375,0.60302 -0.187956,0.38375 -0.187956,0.87713 0,0.49339 0.187956,0.88496 0.195788,0.38375 0.540375,0.60303 0.352418,0.21928 0.790982,0.21928 z"/>
      <path d="m 30.415011,122.0867 q 0.67351,0 1.03376,0.43073 0.360249,0.4229 0.360249,1.08075 v 2.2868 q 0,0.0627 -0.04699,0.10964 -0.04699,0.047 -0.109641,0.047 -0.07048,0 -0.117473,-0.047 -0.03916,-0.047 -0.03916,-0.10964 v -2.2868 q 0,-0.54037 -0.266272,-0.87713 -0.266271,-0.33675 -0.814477,-0.33675 -0.336755,0 -0.665678,0.16446 -0.321092,0.16446 -0.524711,0.44639 -0.203619,0.28194 -0.203619,0.60303 v 2.2868 q 0,0.0627 -0.04699,0.10964 -0.04699,0.047 -0.109642,0.047 -0.07048,0 -0.117472,-0.047 -0.03916,-0.047 -0.03916,-0.10964 v -5.48206 q 0,-0.0626 0.04699,-0.10964 0.04699,-0.047 0.109641,-0.047 0.07048,0 0.109642,0.047 0.04699,0.047 0.04699,0.10964 v 2.47476 q 0.219282,-0.34459 0.603026,-0.56387 0.391575,-0.22711 0.790982,-0.22711 z"/>
      <path d="m 33.422299,125.88498 q 0,0.0627 -0.04699,0.10964 -0.04699,0.047 -0.109641,0.047 -0.07048,0 -0.117473,-0.047 -0.03916,-0.047 -0.03916,-0.10964 v -3.71997 q 0,-0.0626 0.04699,-0.10964 0.04699,-0.047 0.109641,-0.047 0.07048,0 0.109641,0.047 0.04699,0.047 0.04699,0.10964 z m -0.15663,-4.40914 q -0.125304,0 -0.203619,-0.0705 -0.07048,-0.0705 -0.07048,-0.18013 v -0.0626 q 0,-0.10964 0.07832,-0.18012 0.07831,-0.0705 0.20362,-0.0705 0.109641,0 0.180124,0.0705 0.07831,0.0705 0.07831,0.18012 v 0.0626 q 0,0.10964 -0.07831,0.18013 -0.07048,0.0705 -0.187956,0.0705 z"/>
      <path d="m 36.492245,122.00838 q 0.36025,0 0.36025,0.18013 0,0.0705 -0.03916,0.11747 -0.03916,0.047 -0.10181,0.047 -0.02349,0 -0.117472,-0.0313 -0.08615,-0.0392 -0.187956,-0.0392 -0.321092,0 -0.642184,0.25844 -0.321092,0.2506 -0.524711,0.64218 -0.20362,0.38374 -0.20362,0.74399 v 1.95788 q 0,0.0627 -0.04699,0.10964 -0.04699,0.047 -0.109641,0.047 -0.07048,0 -0.117473,-0.047 -0.03916,-0.047 -0.03916,-0.10964 v -3.59466 q 0,-0.0626 0.04699,-0.10964 0.04699,-0.047 0.109641,-0.047 0.07048,0 0.109641,0.047 0.04699,0.047 0.04699,0.10964 v 0.83014 q 0.180125,-0.47772 0.556037,-0.79099 0.383744,-0.31326 0.900624,-0.32109 z"/>
      <path d="m 40.525449,125.10183 q 0.05482,0 0.09398,0.047 0.03916,0.0392 0.03916,0.094 0,0.0548 -0.03133,0.094 -0.25844,0.34459 -0.642184,0.56387 -0.383744,0.21929 -0.822309,0.21929 -0.587363,0 -1.041591,-0.25061 -0.454227,-0.25061 -0.712667,-0.71267 -0.250608,-0.46206 -0.250608,-1.07292 0,-0.62652 0.242776,-1.09641 0.250609,-0.47772 0.657847,-0.72833 0.407239,-0.25061 0.869298,-0.25061 0.462059,0 0.853635,0.20362 0.399407,0.19579 0.650015,0.5952 0.250608,0.3994 0.266271,0.98677 0,0.0627 -0.04699,0.11747 -0.04699,0.047 -0.109641,0.047 h -3.077783 v 0.10181 q 0,0.48555 0.195787,0.89279 0.195788,0.39941 0.579532,0.63435 0.383744,0.23495 0.924118,0.23495 0.375913,0 0.704836,-0.18013 0.336755,-0.18795 0.524711,-0.46989 0.05482,-0.0705 0.133136,-0.0705 z m -1.597628,-2.78019 q -0.289766,0 -0.595195,0.15663 -0.297597,0.1488 -0.532543,0.45423 -0.227113,0.2976 -0.305428,0.72833 h 2.881995 v -0.0705 q -0.03916,-0.39941 -0.25844,-0.68134 -0.21145,-0.28977 -0.532542,-0.43857 -0.313261,-0.1488 -0.657847,-0.1488 z"/>
    </g>
    <g id="title_text" style="fill:#000000;fill-opacity:1;stroke:#000000;stroke-width:0.264999;stroke-linecap:square">
      <path d="m 18.535592,3.1676164 q 0.704836,0 1.033759,0.4385645 0.336755,0.430733 0.336755,1.1434004 v 2.2163172 q 0,0.1018096 -0.07048,0.1722932 -0.06265,0.062652 -0.164462,0.062652 -0.10181,0 -0.172293,-0.062652 -0.06265,-0.070484 -0.06265,-0.1722932 V 4.7730758 q 0,-0.5168796 -0.250609,-0.8379715 -0.242777,-0.3289234 -0.759656,-0.3289234 -0.328924,0 -0.626521,0.1566302 -0.289766,0.1566302 -0.469891,0.430733 -0.180124,0.2662714 -0.180124,0.5795317 v 2.1928227 q 0,0.1018096 -0.07048,0.1722932 -0.06265,0.062652 -0.164462,0.062652 -0.101809,0 -0.172293,-0.062652 -0.06265,-0.070484 -0.06265,-0.1722932 V 4.7495813 q 0,-0.5090481 -0.234945,-0.8223085 -0.234945,-0.3210919 -0.736162,-0.3210919 -0.321092,0 -0.610858,0.1566302 -0.281934,0.1566302 -0.462059,0.4229015 -0.172293,0.2584398 -0.172293,0.5638687 v 2.2163172 q 0,0.1018096 -0.07048,0.1722932 -0.06265,0.062652 -0.164461,0.062652 -0.10181,0 -0.172294,-0.062652 -0.06265,-0.070484 -0.06265,-0.1722932 V 3.4573822 q 0,-0.1018096 0.06265,-0.1644617 0.07048,-0.070484 0.172294,-0.070484 0.101809,0 0.164461,0.070484 0.07048,0.062652 0.07048,0.1644617 v 0.4542276 q 0.211451,-0.3210919 0.5717,-0.5325427 0.36025,-0.2114507 0.759657,-0.2114507 0.454227,0 0.783151,0.2192822 0.328923,0.2192823 0.46989,0.6421838 0.164462,-0.3445864 0.603026,-0.6030262 0.438565,-0.2584398 0.884961,-0.2584398 z"/>
      <path d="m 24.832116,5.1959774 q 0,0.5873632 -0.25844,1.0650853 -0.25844,0.477722 -0.712667,0.7518249 -0.454228,0.2662713 -1.010265,0.2662713 -0.556037,0 -1.010265,-0.2662713 -0.454227,-0.2741029 -0.712667,-0.7518249 -0.25844,-0.4777221 -0.25844,-1.0650853 0,-0.5873633 0.25844,-1.0650853 0.25844,-0.4777221 0.712667,-0.751825 0.454228,-0.2741028 1.010265,-0.2741028 0.556037,0 1.010265,0.2741028 0.454227,0.2741029 0.712667,0.751825 0.25844,0.477722 0.25844,1.0650853 z m -0.469891,0 q 0,-0.4698906 -0.195788,-0.8458031 Q 23.97065,3.9742619 23.618232,3.7628111 23.273645,3.5435288 22.850744,3.5435288 q -0.422902,0 -0.767488,0.2192823 -0.344587,0.2114508 -0.548206,0.5873632 -0.195788,0.3759125 -0.195788,0.8458031 0,0.462059 0.195788,0.8379715 0.203619,0.3759124 0.548206,0.5951947 0.344586,0.2114508 0.767488,0.2114508 0.422901,0 0.767488,-0.2114508 0.352418,-0.2114507 0.548205,-0.5873632 0.195788,-0.3759125 0.195788,-0.845803 z"/>
      <path d="m 29.491859,5.1959774 q 0,0.5873632 -0.25844,1.0650853 -0.25844,0.477722 -0.712668,0.7518249 -0.454227,0.2662713 -1.010264,0.2662713 -0.556038,0 -1.010265,-0.2662713 -0.454228,-0.2741029 -0.712667,-0.7518249 -0.25844,-0.4777221 -0.25844,-1.0650853 0,-0.5873633 0.25844,-1.0650853 0.258439,-0.4777221 0.712667,-0.751825 0.454227,-0.2741028 1.010265,-0.2741028 0.556037,0 1.010264,0.2741028 0.454228,0.2741029 0.712668,0.751825 0.25844,0.477722 0.25844,1.0650853 z m -0.469891,0 q 0,-0.4698906 -0.195788,-0.8458031 -0.195787,-0.3759124 -0.548205,-0.5873632 -0.344587,-0.2192823 -0.767488,-0.2192823 -0.422902,0 -0.767488,0.2192823 -0.344587,0.2114508 -0.548206,0.5873632 -0.195788,0.3759125 -0.195788,0.8458031 0,0.462059 0.195788,0.8379715 0.203619,0.3759124 0.548206,0.5951947 0.344586,0.2114508 0.767488,0.2114508 0.422901,0 0.767488,-0.2114508 0.352418,-0.2114507 0.548205,-0.5873632 0.195788,-0.3759125 0.195788,-0.845803 z"/>
      <path d="m 31.23828,3.65317 v 2.6235557 q 0,0.2975973 0.109641,0.4072385 0.109642,0.1018096 0.289766,0.1018096 0.04699,0 0.117473,-0.015663 0.07048,-0.023494 0.109641,-0.023494 0.07048,0 0.117473,0.062652 0.05482,0.054821 0.05482,0.1331357 0,0.1096411 -0.125304,0.1879562 -0.125304,0.070484 -0.297597,0.070484 -0.211451,0 -0.375913,-0.039157 Q 31.073819,7.1225287 30.917188,6.926741 30.76839,6.7309532 30.76839,6.3002202 V 3.65317 h -0.5717 q -0.09398,0 -0.164462,-0.062652 -0.06265,-0.062652 -0.06265,-0.1566302 0,-0.093978 0.06265,-0.1566302 0.07048,-0.062652 0.164462,-0.062652 h 0.5717 V 2.3453079 q 0,-0.1018097 0.06265,-0.1644617 0.07048,-0.070484 0.172293,-0.070484 0.10181,0 0.164462,0.070484 0.07048,0.062652 0.07048,0.1644617 v 0.8692975 h 0.783151 q 0.08615,0 0.148799,0.070484 0.07048,0.070484 0.07048,0.1566302 0,0.093978 -0.06265,0.1566302 -0.06265,0.054821 -0.15663,0.054821 z"/>
      <path d="m 33.039507,6.5664915 q -0.06265,-0.1018096 -0.06265,-0.1722932 0,-0.1018096 0.101809,-0.1566302 0.04699,-0.046989 0.125304,-0.046989 0.09398,0 0.172293,0.078315 0.46206,0.6108577 1.229547,0.6108577 0.375913,0 0.650016,-0.1722932 0.281934,-0.1801247 0.281934,-0.5168796 0,-0.3289234 -0.25844,-0.4933851 Q 35.020879,5.5327323 34.558819,5.4152596 33.908804,5.2507979 33.540723,4.9923581 33.172642,4.7260868 33.172642,4.2170387 q 0,-0.3210919 0.180125,-0.5717002 0.180125,-0.2584399 0.485554,-0.399407 0.31326,-0.1409672 0.704835,-0.1409672 0.336755,0 0.67351,0.1331357 0.344587,0.1253041 0.587363,0.4072385 0.07048,0.054821 0.07048,0.1566301 0,0.093978 -0.07832,0.1644618 -0.05482,0.046989 -0.140967,0.046989 -0.07832,0 -0.140967,-0.062652 -0.187957,-0.2114507 -0.446396,-0.3210919 -0.25844,-0.1096411 -0.556038,-0.1096411 -0.368081,0 -0.634352,0.1722932 -0.25844,0.1722932 -0.25844,0.5168796 0.01566,0.3132604 0.281935,0.4777221 0.274102,0.1644617 0.798814,0.2975974 0.407238,0.1018096 0.673509,0.2271138 0.266272,0.1253041 0.430733,0.3524179 0.172294,0.2271138 0.172294,0.6030262 0,0.5090482 -0.407239,0.814477 -0.407238,0.2975974 -1.010265,0.2975974 -0.955444,0 -1.519312,-0.7126674 z"/>
    </g>
    <g id="gem1" style="fill:#0000ff;stroke:#2e2114;stroke-width:0;stroke-linecap:square" transform="translate(0,-0.52916667)">
      <path d="m 7.7984528,123.2582 q 0,0.34175 -0.2425346,0.63114 -0.044097,0.0524 -0.2039495,0.21497 -0.187413,0.19017 -0.7854811,0.81304 l -1.444183,1.51309 q -0.019292,0.003 -0.044097,0.003 -0.027561,0 -0.041341,-0.003 -0.082682,-0.0827 -0.6476774,-0.67524 -0.4685327,-0.47405 -0.9343092,-0.94809 -0.556727,-0.56775 -0.8984802,-0.98116 -0.1322916,-0.15985 -0.1681206,-0.29766 -0.027561,-0.10197 -0.027561,-0.34175 0,-0.11024 0.071658,-0.28939 0.066146,-0.16536 0.1405598,-0.26458 0.1185112,-0.1571 0.4216794,-0.42444 0.2838756,-0.2508 0.4740448,-0.37482 0.2728513,-0.17915 0.818554,-0.36656 0.096463,-0.0331 0.110243,-0.0331 h 1.3615007 q 0.011024,0 0.1047308,0.0331 0.5677513,0.20119 0.8240662,0.36656 0.1653645,0.10473 0.4740448,0.37758 0.3224607,0.28387 0.4216794,0.42168 0.2149738,0.2949 0.2149737,0.62563 z m -0.1681206,-0.27286 q -0.033073,-0.10748 -0.1350476,-0.30868 -0.071658,-0.0937 -0.2094616,-0.19843 -0.1185112,-0.0854 -0.2342664,-0.17088 0.2452907,0.3638 0.5787756,0.67799 z m -0.5732634,-0.52365 -0.3582897,-0.44648 v 0.41065 z m 0.6366532,0.75792 q 0,-0.0524 -0.3417533,-0.39412 l 0.2287542,0.7028 h 0.049609 q 0.06339,-0.21773 0.06339,-0.30868 z m -0.2315103,0.27285 -0.3169485,-0.90399 -0.4106551,-0.0634 -0.1598523,0.6146 z m 0.00276,0.15434 -0.7717008,-0.3197 h -0.030317 l 0.3445093,0.60633 q 0.079926,-0.003 0.2342663,-0.0469 0.024805,-0.0138 0.223242,-0.23978 z m -0.8571391,-1.05006 -0.7193354,0.28387 0.6035803,0.226 z m 0,-0.16261 v -0.51539 l -0.9177728,-0.38033 h -1.223697 l -0.9177727,0.38033 v 0.51539 l 0.8984802,0.4079 h 1.2540138 z m 0.5732634,1.53789 q -0.068902,0.011 -0.1984373,0.0524 -0.085438,0.0551 -0.259071,0.16261 0.019292,0 0.06339,0.003 0.041341,0 0.06339,0 0.1185112,0 0.1901691,-0.0689 0.071658,-0.0717 0.1405598,-0.14883 z m -0.2893878,0.003 -0.3472653,-0.63115 -0.3638018,0.87919 q 0.024805,-0.0165 0.066146,-0.0165 0.030317,0 0.085438,0.011 0.057878,0.008 0.088194,0.008 0.049609,0 0.2287542,-0.10749 0.2287542,-0.1378 0.2425345,-0.14331 z m -0.4409719,-0.74139 -0.6835064,-0.27561 -0.592556,0.54846 0.9039924,0.65595 z m -0.854383,-0.28387 H 4.5600655 l 0.5236541,0.47404 z m 0.7496522,1.36701 q -0.2287541,-0.0303 -0.2287541,-0.0303 -0.035829,0 -0.2728514,0.0937 l 0.1901691,0.0193 q 0.011024,0 0.3114364,-0.0827 z m -0.3886065,-0.0882 -0.8433587,-0.61184 v 0.67523 l 0.4712887,0.0827 z m 0.8543831,0.0496 q -0.099219,0.006 -0.2949,0.0386 -0.011024,0.003 -0.3775821,0.17639 -0.1295355,0.34726 -0.3803383,1.04179 z m -2.5438566,-1.39733 -0.7193354,-0.28387 0.1240233,0.51814 z M 3.465904,122.42586 v -0.41065 l -0.3665579,0.44648 z m 1.5158408,1.07211 -0.5925559,-0.54019 -0.6835065,0.27561 0.3803383,0.9288 z m 0.369314,0.94258 q -0.033073,-0.003 -0.1350476,-0.0248 -0.085438,-0.0165 -0.1378038,-0.0165 -0.052365,0 -0.1378037,0.0165 -0.1019747,0.022 -0.1350476,0.0248 0.2645831,0.0469 0.2728513,0.0469 0.00827,0 0.2728514,-0.0469 z m -0.3086803,-0.1378 v -0.68626 l -0.8433588,0.61184 0.3720701,0.14608 z m 0.9508456,0.2067 -0.3968747,-0.0551 -0.4795569,0.14056 v 1.27606 z m -2.8883659,-2.2021 q -0.1185112,0.0854 -0.2342663,0.17363 -0.1460719,0.10749 -0.2094616,0.19568 -0.044097,0.10474 -0.1350477,0.3142 0.369314,-0.3638 0.5787756,-0.68351 z m 0.4768009,0.83233 -0.1598523,-0.6146 -0.4106551,0.0634 -0.3169485,0.90399 z m 0.3941186,1.08314 -0.3638018,-0.87919 -0.3555336,0.63115 q 0.4354598,0.2508 0.4519962,0.2508 0.09095,0 0.2673392,-0.003 z m 1.8217651,0.82958 q -0.1322915,0.21222 -0.4051429,0.6339 -0.07717,0.10748 -0.2204859,0.32797 -0.057878,0.10473 -0.055122,0.226 0.1598523,-0.15159 0.4327036,-0.49609 0.07717,-0.12954 0.1405598,-0.339 0.055122,-0.17639 0.1074874,-0.35278 z m -1.4937922,-0.66146 -0.2700953,-0.0882 -0.00827,-0.011 q -0.024805,0 -0.1074869,0.011 -0.066146,0.006 -0.1074869,0.006 0.2728513,0.0909 0.3114364,0.0909 0.024805,0 0.1819009,-0.008 z M 3.4934647,123.3271 h -0.030317 l -0.7717008,0.3197 q 0.066146,0.0965 0.223242,0.23978 0.07717,0.0138 0.2342663,0.0469 z m -0.6890186,-0.50161 q -0.3417532,0.34727 -0.3417532,0.39412 0,0.0799 0.06339,0.30868 h 0.049609 z m 2.2351763,3.04546 v -1.27606 l -0.4795569,-0.14056 -0.3968747,0.0551 z m -1.6067913,-1.67569 q -0.2563149,-0.1819 -0.4575084,-0.22324 0.066146,0.0772 0.2039495,0.22324 z m 1.6095474,2.04501 v -0.13781 L 4.3588721,125.0524 q 0.052365,0.17639 0.1074868,0.35278 0.06339,0.20946 0.1405598,0.339 0.068902,0.11851 0.2039495,0.25907 0.1157552,0.11851 0.2315103,0.23702 z m -0.6449214,-0.70556 q -0.1157551,-0.32797 -0.3803383,-1.04179 -0.1350476,-0.0661 -0.2728513,-0.12954 -0.1598523,-0.0717 -0.2893878,-0.0717 -0.052365,0 -0.110243,-0.0138 z"/>
    </g>
    <g id="gem2" style="fill:#0000ff;stroke:#2e2114;stroke-width:0;stroke-linecap:square" transform="translate(0.52916667,-0.52916667)">
      <path d="m 47.771429,123.55357 q 0,0.34175 -0.242534,0.63114 -0.0441,0.0524 -0.20395,0.21497 -0.187413,0.19017 -0.785481,0.81304 l -1.444183,1.51309 q -0.01929,0.003 -0.0441,0.003 -0.02756,0 -0.04134,-0.003 -0.08268,-0.0827 -0.647678,-0.67524 -0.468532,-0.47404 -0.934309,-0.94809 -0.556727,-0.56775 -0.89848,-0.98116 -0.132292,-0.15985 -0.168121,-0.29766 -0.02756,-0.10197 -0.02756,-0.34175 0,-0.11024 0.07166,-0.28939 0.06614,-0.16536 0.140559,-0.26458 0.118512,-0.1571 0.42168,-0.42444 0.283875,-0.2508 0.474044,-0.37482 0.272852,-0.17915 0.818555,-0.36656 0.09646,-0.0331 0.110243,-0.0331 h 1.3615 q 0.01102,0 0.104731,0.0331 0.567751,0.20119 0.824066,0.36656 0.165365,0.10473 0.474045,0.37758 0.322461,0.28388 0.421679,0.42168 0.214974,0.2949 0.214974,0.62563 z m -0.16812,-0.27285 q -0.03307,-0.10749 -0.135048,-0.30868 -0.07166,-0.0937 -0.209462,-0.19844 -0.118511,-0.0854 -0.234266,-0.17088 0.245291,0.3638 0.578776,0.678 z m -0.573264,-0.52366 -0.358289,-0.44648 v 0.41065 z m 0.636653,0.75792 q 0,-0.0524 -0.341753,-0.39412 l 0.228754,0.7028 h 0.04961 q 0.06339,-0.21773 0.06339,-0.30868 z m -0.23151,0.27285 -0.316948,-0.90399 -0.410655,-0.0634 -0.159853,0.61461 z m 0.0028,0.15434 -0.771701,-0.3197 h -0.03032 l 0.344509,0.60634 q 0.07993,-0.003 0.234266,-0.0469 0.02481,-0.0138 0.223242,-0.23978 z m -0.857139,-1.05006 -0.719335,0.28388 0.60358,0.22599 z m 0,-0.16261 v -0.51538 l -0.917773,-0.38034 h -1.223697 l -0.917772,0.38034 v 0.51538 l 0.89848,0.4079 h 1.254014 z m 0.573264,1.53789 q -0.0689,0.011 -0.198438,0.0524 -0.08544,0.0551 -0.259071,0.16261 0.01929,0 0.06339,0.003 0.04134,0 0.06339,0 0.118511,0 0.190169,-0.0689 0.07166,-0.0717 0.14056,-0.14883 z m -0.289388,0.003 -0.347266,-0.63114 -0.363801,0.87918 q 0.0248,-0.0165 0.06615,-0.0165 0.03032,0 0.08544,0.011 0.05788,0.008 0.08819,0.008 0.04961,0 0.228754,-0.10749 0.228754,-0.1378 0.242535,-0.14331 z m -0.440972,-0.74139 -0.683507,-0.2756 -0.592556,0.54845 0.903993,0.65595 z m -0.854383,-0.28387 h -1.036284 l 0.523654,0.47404 z m 0.749652,1.36701 q -0.228754,-0.0303 -0.228754,-0.0303 -0.03583,0 -0.272851,0.0937 l 0.190169,0.0193 q 0.01102,0 0.311436,-0.0827 z m -0.388606,-0.0882 -0.843359,-0.61185 v 0.67524 l 0.471289,0.0827 z m 0.854383,0.0496 q -0.09922,0.006 -0.2949,0.0386 -0.01102,0.003 -0.377583,0.17639 -0.129535,0.34726 -0.380338,1.0418 z m -2.543857,-1.39733 -0.719335,-0.28388 0.124023,0.51814 z m -0.802018,-0.45476 v -0.41065 l -0.366557,0.44648 z m 1.515841,1.07212 -0.592556,-0.54019 -0.683506,0.2756 0.380338,0.9288 z m 0.369314,0.94257 q -0.03307,-0.003 -0.135047,-0.0248 -0.08544,-0.0165 -0.137804,-0.0165 -0.05237,0 -0.137804,0.0165 -0.101975,0.0221 -0.135047,0.0248 0.264583,0.0469 0.272851,0.0469 0.0083,0 0.272851,-0.0469 z m -0.30868,-0.1378 v -0.68626 l -0.843359,0.61185 0.37207,0.14607 z m 0.950846,0.20671 -0.396875,-0.0551 -0.479557,0.14056 v 1.27607 z m -2.888366,-2.20211 q -0.118512,0.0854 -0.234267,0.17364 -0.146072,0.10748 -0.209461,0.19568 -0.0441,0.10473 -0.135048,0.31419 0.369314,-0.3638 0.578776,-0.68351 z m 0.476801,0.83234 -0.159853,-0.61461 -0.410655,0.0634 -0.316948,0.90399 z m 0.394118,1.08313 -0.363802,-0.87918 -0.355533,0.63114 q 0.43546,0.2508 0.451996,0.2508 0.09095,0 0.267339,-0.003 z m 1.821765,0.82958 q -0.132291,0.21222 -0.405143,0.6339 -0.07717,0.10749 -0.220486,0.32797 -0.05788,0.10473 -0.05512,0.226 0.159852,-0.15158 0.432704,-0.49609 0.07717,-0.12954 0.140559,-0.339 0.05512,-0.17639 0.107486,-0.35278 z m -1.493792,-0.66146 -0.270095,-0.0882 -0.0083,-0.011 q -0.02481,0 -0.107487,0.011 -0.06615,0.006 -0.107487,0.006 0.272851,0.091 0.311436,0.091 0.02481,0 0.181901,-0.008 z m -0.810286,-1.06384 h -0.03032 l -0.7717,0.3197 q 0.06615,0.0965 0.223242,0.23978 0.07717,0.0138 0.234266,0.0469 z m -0.689018,-0.50161 q -0.341754,0.34727 -0.341754,0.39412 0,0.0799 0.06339,0.30868 h 0.04961 z m 2.235176,3.04547 v -1.27607 l -0.479557,-0.14056 -0.396875,0.0551 z m -1.606791,-1.6757 q -0.256315,-0.1819 -0.457509,-0.22324 0.06615,0.0772 0.20395,0.22324 z m 1.609547,2.04501 v -0.1378 l -0.683507,-1.05007 q 0.05237,0.17639 0.107487,0.35278 0.06339,0.20946 0.14056,0.339 0.0689,0.11851 0.20395,0.25907 0.115755,0.11851 0.23151,0.23702 z m -0.644921,-0.70555 q -0.115756,-0.32798 -0.380339,-1.0418 -0.135047,-0.0662 -0.272851,-0.12954 -0.159852,-0.0716 -0.289388,-0.0716 -0.05237,0 -0.110243,-0.0138 z"/>
    </g>
    <g id="horizontal_lines" style="fill:#000000;stroke:#000000;stroke-width:0.2;stroke-linecap:round;stroke-linejoin:round;stroke-dasharray:none;stroke-opacity:1">
      <path d="M 10.422823,17.292023 H 39.755249"/>
      <path d="M 10.295112,38.714519 H 39.627538"/>
      <path d="M 10.646172,60.19444 H 39.978598"/>
      <path d="M 10.486349,81.727338 H 39.818775"/>
      <path d="M 10.431169,103.1942 H 39.763595"/>
    </g>
    <g id="in_label" style="stroke:#000000;stroke-width:0.2;stroke-linecap:round;stroke-linejoin:round;stroke-dasharray:none" transform="rotate(-38.639651,67.552846,33.182459)">
      <path d="m 3.4471021,13.631746 q -0.00282,0.03669 -0.028222,0.06209 -0.0254,0.0254 -0.062089,0.0254 -0.039511,0 -0.064911,-0.02258 -0.022578,-0.0254 -0.022578,-0.06491 v -1.800577 q 0,-0.03951 0.0254,-0.06209 0.0254,-0.0254 0.064911,-0.0254 0.036689,0 0.062089,0.0254 0.0254,0.0254 0.0254,0.06209 z"/>
      <path d="m 5.4198286,11.74368 q 0.036689,0 0.056444,0.02258 0.022578,0.02258 0.022578,0.05645 v 1.80622 q 0,0.04233 -0.0254,0.06773 -0.0254,0.02258 -0.062089,0.02258 -0.019756,0 -0.039511,-0.0085 -0.016933,-0.0085 -0.028222,-0.01976 L 4.1328962,12.059769 v 1.583266 q 0,0.03104 -0.0254,0.05362 -0.022578,0.02258 -0.053622,0.02258 -0.033867,0 -0.056444,-0.02258 -0.022578,-0.02258 -0.022578,-0.05362 v -1.814688 q 0,-0.03951 0.022578,-0.06209 0.0254,-0.02258 0.059267,-0.02258 0.045156,0 0.067733,0.03105 l 1.2163769,1.63971 v -1.591732 q 0,-0.03387 0.022578,-0.05645 0.022578,-0.02258 0.056444,-0.02258 z"/>
    </g>
    <g id="out_label" transform="rotate(39.175362,-61.243351,21.475507)" style="stroke:#000000;stroke-width:0.2;stroke-linecap:round;stroke-linejoin:round;stroke-dasharray:none">
      <path d="m 42.372381,-18.213284 q 0,0.290689 -0.121356,0.522112 -0.121356,0.231423 -0.335845,0.364068 -0.21449,0.129822 -0.488246,0.129822 -0.273756,0 -0.488246,-0.129822 -0.214489,-0.132645 -0.335845,-0.364068 -0.121356,-0.231423 -0.121356,-0.522112 0,-0.293512 0.121356,-0.524935 0.121356,-0.231423 0.335845,-0.361246 0.21449,-0.129822 0.488246,-0.129822 0.273756,0 0.488246,0.129822 0.214489,0.129823 0.335845,0.361246 0.121356,0.231423 0.121356,0.524935 z m -0.180623,0 q 0,-0.245534 -0.09878,-0.437446 -0.09596,-0.194734 -0.270934,-0.301979 -0.172156,-0.107244 -0.395112,-0.107244 -0.222956,0 -0.397935,0.107244 -0.172156,0.107245 -0.270934,0.301979 -0.09596,0.191912 -0.09596,0.437446 0,0.245534 0.09596,0.440267 0.09878,0.191912 0.270934,0.299157 0.174979,0.107245 0.397935,0.107245 0.222956,0 0.395112,-0.107245 0.174978,-0.107245 0.270934,-0.299157 0.09878,-0.194733 0.09878,-0.440267 z"/>
      <path d="m 44.170138,-19.201065 q 0.03669,0 0.05927,0.0254 0.0254,0.02258 0.0254,0.05927 v 1.168403 q 0,0.206023 -0.09878,0.378179 -0.09878,0.172156 -0.268112,0.270934 -0.169334,0.09878 -0.372534,0.09878 -0.206023,0 -0.375357,-0.09878 -0.169334,-0.09878 -0.268112,-0.270934 -0.09878,-0.172156 -0.09878,-0.378179 v -1.168403 q 0,-0.03669 0.02258,-0.05927 0.0254,-0.0254 0.06773,-0.0254 0.03387,0 0.05927,0.0254 0.0254,0.02258 0.0254,0.05927 v 1.168403 q 0,0.158045 0.0762,0.29069 0.0762,0.132645 0.206023,0.211667 0.132644,0.0762 0.285045,0.0762 0.155222,0 0.285045,-0.0762 0.132645,-0.07902 0.208845,-0.211667 0.07902,-0.132645 0.07902,-0.29069 v -1.168403 q 0,-0.03669 0.02258,-0.05927 0.02258,-0.0254 0.05927,-0.0254 z"/>
      <path d="m 46.044097,-19.201065 q 0.03669,0 0.05927,0.02258 0.0254,0.02258 0.0254,0.05927 0,0.03669 -0.0254,0.05927 -0.02258,0.01975 -0.05927,0.01975 h -0.584201 v 1.730027 q 0,0.03669 -0.0254,0.06209 -0.0254,0.02258 -0.06209,0.02258 -0.03951,0 -0.06491,-0.02258 -0.02258,-0.0254 -0.02258,-0.06209 v -1.730027 h -0.584201 q -0.03669,0 -0.06209,-0.02258 -0.02258,-0.02258 -0.02258,-0.05927 0,-0.03387 0.02258,-0.05644 0.0254,-0.02258 0.06209,-0.02258 z"/>
    </g>
</g>
'''

MOOTS_PANEL_WIDTH = 10


def GenerateMootsPanel() -> int:
    svgFileName = '../res/moots.svg'
    panel = Panel(MOOTS_PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    panel.append(pl)
    pl.append(LiteralXml(MootsPanelLayerXml))
    return Save(panel, svgFileName)


def GenerateMootsLabel(svgFileName:str, text:str) -> int:
    panel = Panel(MOOTS_PANEL_WIDTH)
    x = panel.mmWidth / 2
    y = 74.5
    with Font(SAPPHIRE_FONT_FILENAME) as font:
        panel.append(CenteredControlTextPath(font, text, x, y, pointSize = 8.0))
    return Save(panel, svgFileName)


def GenerateStereoOutputLabels(svgFileName:str, leftPortLabel:str, rightPortLabel:str) -> int:
    # Assumes the same panel layout and output port locations as Galaxy and Gravy.
    PANEL_WIDTH = 6
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    panel.append(pl)
    xmid = panel.mmWidth / 2
    dxPortFromCenter = 6.0
    dxLeftRight = dxPortFromCenter + 6.3
    yRow = FencePost(22.0, 114.0, 7)
    yOutPort = yRow.value(6)
    with Font(SAPPHIRE_FONT_FILENAME) as font:
        if leftPortLabel:
            pl.append(CenteredControlTextPath(font, leftPortLabel, xmid - dxLeftRight, yOutPort))
        if rightPortLabel:
            pl.append(CenteredControlTextPath(font, rightPortLabel, xmid + dxLeftRight, yOutPort))
    return Save(panel, svgFileName)


def GenerateStereoInputLabels(svgFileName:str, leftPortLabel:str, rightPortLabel:str) -> int:
    # Assumes the same panel layout and output port locations as Galaxy and Gravy.
    PANEL_WIDTH = 6
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    panel.append(pl)
    xmid = panel.mmWidth / 2
    dxPortFromCenter = 6.0
    dxLeftRight = dxPortFromCenter + 6.3
    yRow = FencePost(22.0, 114.0, 7)
    yInPort  = yRow.value(0)
    with Font(SAPPHIRE_FONT_FILENAME) as font:
        if leftPortLabel:
            pl.append(CenteredControlTextPath(font, leftPortLabel, xmid - dxLeftRight, yInPort))
        if rightPortLabel:
            pl.append(CenteredControlTextPath(font, rightPortLabel, xmid + dxLeftRight, yInPort))
    return Save(panel, svgFileName)


def GenerateGalaxyPanel(cdict:Dict[str,ControlLayer], name:str) -> int:
    table:List[Tuple[str, str]] = [
        ('replace',     'REPLACE'),
        ('brightness',  'BRIGHT'),
        ('detune',      'DETUNE'),
        ('bigness',     'SIZE'),
        ('mix',         'MIX')
    ]

    svgFileName = '../res/{}.svg'.format(name)
    PANEL_WIDTH = 6
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    panel.append(pl)
    defs = Element('defs')
    pl.append(defs)
    cdict[name] = controls = ControlLayer()
    xmid = panel.mmWidth / 2
    dxPortFromCenter = 6.0

    yRow = FencePost(22.0, 114.0, 7)
    yInPort  = yRow.value(0)
    yOutPort = yRow.value(6)
    dyGrad = 6.0
    dyText = 6.5

    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
        pl.append(ModelNamePath(panel, font, name))
        pl.append(CenteredGemstone(panel))

        y1 = yInPort - 9.5
        y2 = yInPort + dyGrad
        defs.append(Gradient(y1, y2, SAPPHIRE_MAGENTA_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_in'))
        pl.append(ControlGroupArt(name, 'in_art', panel, y1, y2, 'gradient_in'))

        y1 = yRow.value(1) - 9.5
        y2 = yRow.value(5) + dyGrad
        defs.append(Gradient(y1, y2, SAPPHIRE_AZURE_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_controls'))
        pl.append(ControlGroupArt(name, 'controls_art', panel, y1, y2, 'gradient_controls'))

        y1 = yOutPort - 9.5
        y2 = yOutPort + dyGrad
        defs.append(Gradient(y1, y2, SAPPHIRE_EGGPLANT_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_out'))
        pl.append(ControlGroupArt(name, 'out_art', panel, y1, y2, 'gradient_out'))

        pl.append(CenteredControlTextPath(font, 'IN',  xmid, yInPort - dyText))
        pl.append(CenteredControlTextPath(font, 'OUT', xmid, yOutPort - dyText))

        controls.append(Component('audio_left_input',   xmid - dxPortFromCenter, yInPort ))
        controls.append(Component('audio_right_input',  xmid + dxPortFromCenter, yInPort ))
        controls.append(Component('audio_left_output',  xmid - dxPortFromCenter, yOutPort))
        controls.append(Component('audio_right_output', xmid + dxPortFromCenter, yOutPort))

        pl.append(HorizontalLinePath(xmid - dxPortFromCenter, xmid + dxPortFromCenter, yInPort))
        pl.append(HorizontalLinePath(xmid - dxPortFromCenter, xmid + dxPortFromCenter, yOutPort))

        row = 1
        for (symbol, label) in table:
            y = yRow.value(row)
            pl.append(CenteredControlTextPath(font, label, xmid, y-dyText))
            AddFlatControlGroup(pl, controls, xmid, y, symbol)
            row += 1
    return Save(panel, svgFileName)


def GenerateGravyPanel(cdict:Dict[str,ControlLayer], name:str) -> int:
    table:List[Tuple[str, str]] = [
        ('frequency',   'FREQ'),
        ('resonance',   'RES'),
        ('mix',         'MIX'),
        ('gain',        'GAIN')
    ]
    svgFileName = '../res/{}.svg'.format(name)
    PANEL_WIDTH = 6
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    panel.append(pl)
    defs = Element('defs')
    pl.append(defs)
    cdict[name] = controls = ControlLayer()
    xmid = panel.mmWidth / 2
    dxPortFromCenter = 6.0

    yRow = FencePost(22.0, 114.0, 7)
    yInPort  = yRow.value(0)
    ySwitch  = yRow.value(5)
    yOutPort = yRow.value(6)
    dyGrad = 6.0
    dyText = 6.5

    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
        pl.append(ModelNamePath(panel, font, name))
        pl.append(CenteredGemstone(panel))

        y1 = yInPort - 9.5
        y2 = yInPort + dyGrad
        defs.append(Gradient(y1, y2, SAPPHIRE_MAGENTA_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_in'))
        pl.append(ControlGroupArt(name, 'in_art', panel, y1, y2, 'gradient_in'))

        y1 = yRow.value(1) - 9.5
        y2 = yRow.value(5) + dyGrad
        defs.append(Gradient(y1, y2, SAPPHIRE_AZURE_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_controls'))
        pl.append(ControlGroupArt(name, 'controls_art', panel, y1, y2, 'gradient_controls'))

        y1 = yOutPort - 9.5
        y2 = yOutPort + dyGrad
        defs.append(Gradient(y1, y2, SAPPHIRE_EGGPLANT_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_out'))
        pl.append(ControlGroupArt(name, 'out_art', panel, y1, y2, 'gradient_out'))

        pl.append(CenteredControlTextPath(font, 'IN',  xmid, yInPort  - dyText))
        pl.append(CenteredControlTextPath(font, 'OUT', xmid, yOutPort - dyText))

        controls.append(Component('audio_left_input',   xmid - dxPortFromCenter, yInPort ))
        controls.append(Component('audio_right_input',  xmid + dxPortFromCenter, yInPort ))
        controls.append(Component('audio_left_output',  xmid - dxPortFromCenter, yOutPort))
        controls.append(Component('audio_right_output', xmid + dxPortFromCenter, yOutPort))

        pl.append(CenteredControlTextPath(font, 'MODE',  xmid, ySwitch - dyText))
        controls.append(Component('mode_switch', xmid, ySwitch))

        pl.append(HorizontalLinePath(xmid - dxPortFromCenter, xmid + dxPortFromCenter, yInPort))
        pl.append(HorizontalLinePath(xmid - dxPortFromCenter, xmid + dxPortFromCenter, yOutPort))

        row = 1
        for (symbol, label) in table:
            y = yRow.value(row)
            pl.append(CenteredControlTextPath(font, label, xmid, y-dyText))
            AddFlatControlGroup(pl, controls, xmid, y, symbol)
            row += 1
    return Save(panel, svgFileName)


def GenerateSaucePanel(cdict:Dict[str,ControlLayer], name:str) -> int:
    table:List[Tuple[str, str]] = [
        ('frequency',   'FREQ'),
        ('resonance',   'RES'),
        ('mix',         'MIX'),
        ('gain',        'GAIN')
    ]
    svgFileName = '../res/{}.svg'.format(name)
    PANEL_WIDTH = 6
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    panel.append(pl)
    defs = Element('defs')
    pl.append(defs)
    cdict[name] = controls = ControlLayer()
    xmid = panel.mmWidth / 2

    yRow = FencePost(22.0, 114.0, 7)
    yInPort  = yRow.value(0)
    dyOutPort = 10.0
    yOutLowPort  = yRow.value(5) - 3.0
    yOutBandPort = yOutLowPort + 1*dyOutPort
    yOutHighPort = yOutLowPort + 2*dyOutPort
    dyGrad = 6.0
    dyText = 6.5
    dxOutPortText = 7.5

    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
        pl.append(ModelNamePath(panel, font, name))
        pl.append(CenteredGemstone(panel))

        y1 = yInPort - 9.5
        y2 = yInPort + dyGrad
        defs.append(Gradient(y1, y2, SAPPHIRE_MAGENTA_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_in'))
        pl.append(ControlGroupArt(name, 'in_art', panel, y1, y2, 'gradient_in'))

        y1 = yRow.value(1) - 9.5
        y2 = yRow.value(5) + dyGrad
        defs.append(Gradient(y1, y2, SAPPHIRE_AZURE_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_controls'))
        pl.append(ControlGroupArt(name, 'controls_art', panel, y1, y2, 'gradient_controls'))

        y1 = yOutLowPort - 6.0
        y2 = yOutLowPort + dyGrad
        defs.append(Gradient(y1, y2, SAPPHIRE_EGGPLANT_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_out'))
        pl.append(ControlGroupArt(name, 'out_art', panel, y1, y2, 'gradient_out'))

        pl.append(CenteredControlTextPath(font, 'IN',  xmid, yInPort  - dyText))

        controls.append(Component('audio_input',  xmid, yInPort ))
        controls.append(Component('audio_lp_output', xmid, yOutLowPort))
        controls.append(Component('audio_bp_output', xmid, yOutBandPort))
        controls.append(Component('audio_hp_output', xmid, yOutHighPort))

        pl.append(CenteredControlTextPath(font, 'LP', xmid - dxOutPortText, yOutLowPort))
        pl.append(CenteredControlTextPath(font, 'BP', xmid - dxOutPortText, yOutBandPort))
        pl.append(CenteredControlTextPath(font, 'HP', xmid - dxOutPortText, yOutHighPort))

        row = 1
        for (symbol, label) in table:
            y = yRow.value(row)
            pl.append(CenteredControlTextPath(font, label, xmid, y-dyText))
            AddFlatControlGroup(pl, controls, xmid, y, symbol)
            row += 1
    return Save(panel, svgFileName)



def GenerateRotiniPanel(cdict:Dict[str,ControlLayer]) -> int:
    name = 'rotini'
    svgFileName = '../res/{}.svg'.format(name)
    PANEL_WIDTH = 4
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    panel.append(pl)
    defs = Element('defs')
    pl.append(defs)
    cdict[name] = controls = ControlLayer()
    xmid = panel.mmWidth / 2
    NROWS = 7
    yRow = FencePost(22.0, 110.0, NROWS)
    dyGrad = 6.0
    dyText = 6.0
    dxText = 6.25
    outputPortDY =  9.0
    outPortY = 88.0
    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
        pl.append(ModelNamePath(panel, font, name))
        pl.append(CenteredGemstone(panel))
        controls.append(Component('a_input',  xmid, yRow.value(0)))
        controls.append(Component('b_input',  xmid, yRow.value(1)))
        controls.append(Component('x_output', xmid, outPortY + 0*outputPortDY))
        controls.append(Component('y_output', xmid, outPortY + 1*outputPortDY))
        controls.append(Component('z_output', xmid, outPortY + 2*outputPortDY))
        controls.append(Component('c_output', xmid, outPortY + 3*outputPortDY))

        y1 = yRow.value(0) - 9.5
        y2 = yRow.value(0) + dyGrad
        defs.append(Gradient(y1, y2, SAPPHIRE_MAGENTA_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_a'))
        pl.append(ControlGroupArt(name, 'a_art', panel, y1, y2, 'gradient_a'))

        y1 = yRow.value(1) - 9.5
        y2 = yRow.value(1) + dyGrad
        defs.append(Gradient(y1, y2, SAPPHIRE_AZURE_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_b'))
        pl.append(ControlGroupArt(name, 'b_art', panel, y1, y2, 'gradient_b'))

        outy1 = 76.0
        outy2 = 113.6
        defs.append(Gradient(outy1, outy2, SAPPHIRE_TEAL_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_c'))
        pl.append(ControlGroupArt(name, 'c_art', panel, outy1, outy2, 'gradient_c'))

        pl.append(CenteredControlTextPath(font, 'A', xmid, yRow.value(0) - dyText))
        pl.append(CenteredControlTextPath(font, 'B', xmid, yRow.value(1) - dyText))
        pl.append(CenteredControlTextPath(font, 'A x B', xmid, outy1 + 3.5))

        pl.append(CenteredControlTextPath(font, 'X', xmid - dxText, outPortY + 0*outputPortDY))
        pl.append(CenteredControlTextPath(font, 'Y', xmid - dxText, outPortY + 1*outputPortDY))
        pl.append(CenteredControlTextPath(font, 'Z', xmid - dxText, outPortY + 2*outputPortDY))
        pl.append(CenteredControlTextPath(font, 'P', xmid - dxText, outPortY + 3*outputPortDY))
        pl.append(PolyPortHexagon(xmid, outPortY + 3*outputPortDY))
    return Save(panel, svgFileName)


def GeneratePivotPanel(cdict:Dict[str,ControlLayer]) -> int:
    name = 'pivot'
    svgFileName = '../res/{}.svg'.format(name)
    PANEL_WIDTH = 4
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    panel.append(pl)
    defs = Element('defs')
    pl.append(defs)
    cdict[name] = controls = ControlLayer()
    xmid = panel.mmWidth / 2
    yRow = 22.0
    dyGrad = 6.0
    dyText = 6.0
    dxText = 6.25
    outputPortDY =  9.0
    outPortY = 88.0
    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
        pl.append(ModelNamePath(panel, font, name))
        pl.append(CenteredGemstone(panel))
        controls.append(Component('a_input',  xmid, yRow))
        controls.append(Component('x_output', xmid, outPortY + 0*outputPortDY))
        controls.append(Component('y_output', xmid, outPortY + 1*outputPortDY))
        controls.append(Component('z_output', xmid, outPortY + 2*outputPortDY))
        controls.append(Component('c_output', xmid, outPortY + 3*outputPortDY))

        y1 = yRow - 9.5
        y2 = yRow + dyGrad
        defs.append(Gradient(y1, y2, SAPPHIRE_MAGENTA_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_a'))
        pl.append(ControlGroupArt(name, 'a_art', panel, y1, y2, 'gradient_a'))

        yTwist = 57.0
        y1 = yTwist - 13.0
        y2 = yTwist + dyGrad
        defs.append(Gradient(y1, y2, SAPPHIRE_AZURE_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_b'))
        pl.append(ControlGroupArt(name, 'b_art', panel, y1, y2, 'gradient_b'))

        outy1 = 76.0
        outy2 = 113.6
        defs.append(Gradient(outy1, outy2, SAPPHIRE_TEAL_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_c'))
        pl.append(ControlGroupArt(name, 'c_art', panel, outy1, outy2, 'gradient_c'))

        pl.append(CenteredControlTextPath(font, 'IN',  xmid, yRow - dyText))
        pl.append(CenteredControlTextPath(font, 'OUT', xmid, outy1 + 3.5))

        AddControlGroup(pl, controls, font, 'twist', 'TWIST', xmid, yTwist, 5.5)

        pl.append(CenteredControlTextPath(font, 'X', xmid - dxText, outPortY + 0*outputPortDY))
        pl.append(CenteredControlTextPath(font, 'Y', xmid - dxText, outPortY + 1*outputPortDY))
        pl.append(CenteredControlTextPath(font, 'Z', xmid - dxText, outPortY + 2*outputPortDY))
        pl.append(CenteredControlTextPath(font, 'P', xmid - dxText, outPortY + 3*outputPortDY))

        pl.append(PolyPortHexagon(xmid, outPortY + 3*outputPortDY))
    return Save(panel, svgFileName)


def VerticalArrow(x:float, y1:float, y2:float, dx:float, dy:float) -> Path:
    t = ''
    t += Move(x, y1)
    t += Line(x, y2)
    t += ClosePath()
    t += Move(x, y2)
    t += Line(x + dx, y2 - dy)
    t += ClosePath()
    t += Move(x, y2)
    t += Line(x - dx, y2 - dy)
    t += ClosePath()
    return Path(t, ARROW_LINE_STYLE)


def PolygonVertices(numSides:int, radius:float, xCenter:float, yCenter:float) -> List[Tuple[float, float]]:
    vertices:List[Tuple[float, float]] = []
    for k in range(numSides):
        angle = (2 * math.pi * (k + 0.5)) / numSides
        x = xCenter + radius*math.cos(angle)
        y = yCenter + radius*math.sin(angle)
        vertices.append((x, y))
    return vertices


def PolyPortHexagon(xCenter:float, yCenter:float, radius:float = 5.25) -> Path:
    style = f'stroke:#000000;fill:#2455a6;stroke-width:0.15;stroke-linecap:round;stroke-linejoin:round;stroke-dasharray:none'
    t = ''
    vertices = PolygonVertices(6, radius, xCenter, yCenter)
    first = True
    for (x, y) in vertices:
        if first:
            first = False
            t += Move(x, y)
        else:
            t += Line(x, y)
    t += ClosePath()
    return Path(t, style)


def GenerateSamPanel(cdict:Dict[str,ControlLayer]) -> int:
    svgFileName = '../res/sam.svg'
    PANEL_WIDTH = 2
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    panel.append(pl)
    defs = Element('defs')
    pl.append(defs)
    cdict['sam'] = controls = ControlLayer()
    yInput  = FencePost(25.0,  52.0, 4)
    yOutput = FencePost(88.0, 115.0, 4)     # cannot change - visually match Frolic/Glee
    dyArrowMargin = 10.0
    dxArrow = 2.75
    dyArrow = 5.0
    xmid = panel.mmWidth / 2.0
    yChannelDisplay = 14.75
    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
        pl.append(ModelNamePath(panel, font, 's'))
        pl.append(CenteredGemstone(panel))

        controls.append(Component('x_input' , xmid, yInput.value(0)))
        controls.append(Component('y_input' , xmid, yInput.value(1)))
        controls.append(Component('z_input' , xmid, yInput.value(2)))
        controls.append(Component('p_input' , xmid, yInput.value(3)))

        pl.append(PolyPortHexagon(xmid, yInput.value(3)))

        y1Arrow = yInput.value(3) + dyArrowMargin
        y2Arrow = yOutput.value(0) - dyArrowMargin
        pl.append(VerticalArrow(xmid, y1Arrow, y2Arrow, dxArrow, dyArrow))

        controls.append(Component('x_output', xmid, yOutput.value(0)))
        controls.append(Component('y_output', xmid, yOutput.value(1)))
        controls.append(Component('z_output', xmid, yOutput.value(2)))
        controls.append(Component('p_output', xmid, yOutput.value(3)))

        controls.append(Component('channel_display', xmid, yChannelDisplay))

        pl.append(PolyPortHexagon(xmid, yOutput.value(3)))
    return Save(panel, svgFileName)


def GeneratePopPanel(cdict:Dict[str,ControlLayer]) -> int:
    name = 'pop'
    svgFileName = '../res/{}.svg'.format(name)
    PANEL_WIDTH = 4
    panel = Panel(PANEL_WIDTH)
    pl = Element('g', 'PanelLayer')
    defs = Element('defs')
    pl.append(defs)
    panel.append(pl)
    cdict[name] = controls = ControlLayer()
    xmid = panel.mmWidth / 2
    syncDy = 16.0       # vertical distance between SYNC input port and TRIGGER output port
    ySpeedKnob = 26.0
    yChaosKnob = 57.0
    dxControlGroup = 5.0
    dyControlGroup = 11.0
    dyControlText = -11.6
    yOutLabel = 108.0
    ySyncLabel = yOutLabel - syncDy
    artSpaceAboveKnob = 13.0
    artSpaceBelowKnob = 25.0
    outputPortY1 = 88.0
    outputPortDY =  9.0
    yPortLabel = outputPortY1 - 2.4
    outGradY1 = yOutLabel - 3.0
    outGradY2 = yPortLabel + 2*outputPortDY + 10.0
    syncGradY1 = outGradY1 - syncDy
    syncGradY2 = syncGradY1 + (outGradY2 - outGradY1)
    yTriggerPort = outputPortY1 + 3*outputPortDY
    ySyncPort = yTriggerPort - syncDy
    yChannelDisplay = 80.0
    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
        pl.append(CenteredGemstone(panel))
        pl.append(ModelNamePath(panel, font, name))
        defs.append(Gradient(ySpeedKnob-artSpaceAboveKnob, ySpeedKnob+artSpaceBelowKnob, SAPPHIRE_AZURE_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_blue'))
        defs.append(Gradient(yChaosKnob-artSpaceAboveKnob, yChaosKnob+artSpaceBelowKnob, SAPPHIRE_MAGENTA_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_purple'))
        defs.append(Gradient(syncGradY1, syncGradY2, SAPPHIRE_TEAL_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_sync'))
        defs.append(Gradient(outGradY1, outGradY2, SAPPHIRE_SALMON_COLOR, SAPPHIRE_PANEL_COLOR, 'gradient_out'))
        pl.append(ControlGroupArt(name, 'speed_art', panel, ySpeedKnob-artSpaceAboveKnob, ySpeedKnob+artSpaceBelowKnob, 'gradient_blue'))
        pl.append(ControlGroupArt(name, 'chaos_art', panel, yChaosKnob-artSpaceAboveKnob, yChaosKnob+artSpaceBelowKnob, 'gradient_purple'))
        pl.append(ControlGroupArt(name, 'sync_art', panel, syncGradY1, syncGradY2, 'gradient_sync'))
        pl.append(ControlGroupArt(name, 'out_art', panel, outGradY1, outGradY2, 'gradient_out'))
        lineGroup = Element('g', 'connector_lines').setAttrib('style', 'fill:none;stroke:#000000;stroke-width:0.25;stroke-linecap:round;stroke-linejoin:bevel;stroke-dasharray:none')
        pl.append(lineGroup)
        lineGroup.append(LineElement(xmid, ySpeedKnob, xmid - dxControlGroup, ySpeedKnob + dyControlGroup))
        lineGroup.append(LineElement(xmid, ySpeedKnob, xmid + dxControlGroup, ySpeedKnob + dyControlGroup))
        lineGroup.append(LineElement(xmid, yChaosKnob, xmid - dxControlGroup, yChaosKnob + dyControlGroup))
        lineGroup.append(LineElement(xmid, yChaosKnob, xmid + dxControlGroup, yChaosKnob + dyControlGroup))
        controls.append(Component('speed_knob', xmid, ySpeedKnob))
        controls.append(Component('speed_atten', xmid - dxControlGroup, ySpeedKnob + dyControlGroup))
        controls.append(Component('speed_cv', xmid + dxControlGroup, ySpeedKnob + dyControlGroup))
        controls.append(Component('chaos_knob', xmid, yChaosKnob))
        controls.append(Component('chaos_atten', xmid - dxControlGroup, yChaosKnob + dyControlGroup))
        controls.append(Component('chaos_cv', xmid + dxControlGroup, yChaosKnob + dyControlGroup))
        controls.append(Component('sync_input', xmid, ySyncPort))
        controls.append(Component('pulse_output', xmid, yTriggerPort))
        pl.append(ControlTextPath(font, 'SPEED', xmid - 5.5, ySpeedKnob + dyControlText))
        pl.append(ControlTextPath(font, 'CHAOS', xmid - 6.0, yChaosKnob + dyControlText))
        pl.append(CenteredControlTextPath(font, 'PULSE', xmid, yOutLabel, 'pulse_label'))
        pl.append(CenteredControlTextPath(font, 'SYNC', xmid, ySyncLabel, 'sync_label'))
        controls.append(Component('channel_display', xmid, yChannelDisplay))
    return Save(panel, svgFileName)


def PadRight(text:str, length:int) -> str:
    while len(text) < length:
        text += ' '
    return text


def SaveControls(cdict:Dict[str, ControlLayer]) -> int:
    text = '// *** GENERATED CODE *** DO NOT EDIT ***\n'
    text += '#include "sapphire_panel.hpp"\n'
    text += '\n'
    text += 'namespace Sapphire\n'
    text += '{\n'
    text += '    using ComponentMap = std::map<std::string, ComponentLocation>;\n'
    text += '    using ModuleMap = std::map<std::string, ComponentMap>;\n'
    text += '\n'
    text += '    static const ModuleMap TheModuleMap\n'
    text += '    {\n'

    for (modname, controls) in sorted(cdict.items()):
        text += '        { "' + modname + '", {\n'
        for comp in sorted(controls.componentList):
            text += '            {'
            text += PadRight('"' + comp.id + '",', 25) + ' {'
            text += '{:8.3f}'.format(comp.cx)
            text += ', '
            text += '{:8.3f}'.format(comp.cy)
            text += '}},\n'
        text += '            }},\n'
    text += '    };\n'

    text += r'''
    ComponentLocation FindComponent(
        const std::string& modCode,
        const std::string& label)
    {
        auto m = TheModuleMap.find(modCode);
        if (m == TheModuleMap.end()) return ComponentLocation{0, 0};
        auto c = m->second.find(label);
        if (c == m->second.end()) return ComponentLocation{0, 0};
        return c->second;
    }
'''

    text += '}\n'
    UpdateFileIfChanged('../src/sapphire_panel.cpp', text)
    return 0


def ElastikaCoord(x:float, y:float) -> str:
    return ' {:0.2f},{:0.2f}'.format(x, y)


ELASTIKA_SLIDER_DX = 11.22
ELASTIKA_SLIDER_YMID = 64.0


def ElastikaPathForShape(n:int, isVcvRack:bool) -> str:
    # Make a string that looks like:
    # "M 2.7,32.0 8.15,28.5 13.6,32.0 13.6,86.0 8.2,89.0 2.7,86.0 z"
    # Start with "M" for absolute path.
    if n == -1:
        # The POWER hexagon is special: much smaller than the others.
        x0 = 2*ELASTIKA_SLIDER_DX + 2.4
        y0 = 90.0
        h = 19.0
    else:
        # Follow with 6 coordinate pairs "x,y".
        x0 = n*ELASTIKA_SLIDER_DX + 2.4
        y0 = 32.0
        if isVcvRack:
            h = 54.0
        else:
            h = 38.0

    p = 'M'
    p += ElastikaCoord(x0, y0)
    (dx, dy) = (ELASTIKA_SLIDER_DX/2.0, -3.5)
    (x1, y1) = (x0 + dx, y0 + dy)
    p += ElastikaCoord(x1, y1)
    (x2, y2) = (x1 + dx, y0)
    p += ElastikaCoord(x2, y2)
    p += ElastikaCoord(x2, y2 + h)
    p += ElastikaCoord(x1, y2 + h - dy)
    p += ElastikaCoord(x0, y2 + h)
    p += ' z'
    return p


def ElastikaSliderLabel(font:Font, n:int, label:str) -> TextPath:
    if n == -1:
        xc = ELASTIKA_SLIDER_DX*(2.5) + 2.4
        y = 90.0 + 19.0/2
    else:
        xc = ELASTIKA_SLIDER_DX*(n + 0.5) + 2.4
        y = ELASTIKA_SLIDER_YMID
    return CenteredControlTextPath(font, label, xc, y)


def ElastikaShape(font:Font, n:int, prefix:str, isVcvRack:bool) -> Element:
    group = Element('g', 'artwork_' + prefix)
    text = ElastikaPathForShape(n, isVcvRack)
    style = 'fill:url(#gradient_{});fill-opacity:1;stroke:#000000;stroke-width:0.0;stroke-linecap:square'.format(prefix)
    path = Path(text, style, 'boundary_' + prefix)
    group.append(path)
    if n != -1:     # exclude label for power control
        group.append(ElastikaSliderLabel(font, n, prefix.upper()))
    return group


def PlaceElastikaControls(controls: ControlLayer, shrink:float) -> None:
    controls.append(Component("fric_slider",         8.00,  46.00))
    controls.append(Component("stif_slider",        19.24,  46.00))
    controls.append(Component("span_slider",        30.48,  46.00))
    controls.append(Component("curl_slider",        41.72,  46.00))
    controls.append(Component("mass_slider",        52.96,  46.00))
    controls.append(Component("drive_knob",         14.00, 102.00 - shrink))
    controls.append(Component("level_knob",         46.96, 102.00 - shrink))
    controls.append(Component("input_tilt_knob",    19.24,  17.50))
    controls.append(Component("output_tilt_knob",   41.72,  17.50))


def PlaceElastikaRackControls(controls: ControlLayer) -> None:
    controls.append(Component("fric_atten",          8.00,  72.00))
    controls.append(Component("stif_atten",         19.24,  72.00))
    controls.append(Component("span_atten",         30.48,  72.00))
    controls.append(Component("curl_atten",         41.72,  72.00))
    controls.append(Component("mass_atten",         52.96,  72.00))
    controls.append(Component("input_tilt_atten",    8.00,  12.50))
    controls.append(Component("output_tilt_atten",  53.00,  12.50))
    controls.append(Component("fric_cv",             8.00,  81.74))
    controls.append(Component("stif_cv",            19.24,  81.74))
    controls.append(Component("span_cv",            30.48,  81.74))
    controls.append(Component("curl_cv",            41.72,  81.74))
    controls.append(Component("mass_cv",            52.96,  81.74))
    controls.append(Component("input_tilt_cv",       8.00,  22.50))
    controls.append(Component("output_tilt_cv",     53.00,  22.50))
    controls.append(Component("audio_left_input",    7.50, 115.00))
    controls.append(Component("audio_right_input",  20.50, 115.00))
    controls.append(Component("power_gate_input",   30.48, 104.00))
    controls.append(Component("audio_left_output",  40.46, 115.00))
    controls.append(Component("audio_right_output", 53.46, 115.00))
    controls.append(Component("power_toggle",       30.48,  95.00))


def ElastikaConnectorArt(pl:Element, font:Font, tx1:float, tx2:float, ty:float) -> None:
    # We need connector lines from the knob to CV input and attenuverter knob.
    tdx = 11.24
    tdy = 5.0
    pl.append(GeneralLine(tx1, ty, tx1-tdx, ty-tdy, 'tilt_input_atten_line'))
    pl.append(GeneralLine(tx1, ty, tx1-tdx, ty+tdy, 'tilt_input_cv_line'))
    pl.append(GeneralLine(tx2, ty, tx2+tdx, ty-tdy, 'tilt_output_atten_line'))
    pl.append(GeneralLine(tx2, ty, tx2+tdx, ty+tdy, 'tilt_output_cv_line'))

    # Connector lines from drive knob to CV/atten.
    kx1 = 14.0      # horizontal position of IN/drive knob
    kx2 = 46.96     # horizontal position of OUT/level knob
    ky = 102.0      # vertical position of both knobs
    kdx = 6.5
    kdy = 13.0
    qx1 = kx1 - 0.1
    qx2 = kx2 - 0.1
    qdx = 6.6       # horizontal offset of text labels "L", "R"
    qy = 109.3
    pl.append(GeneralLine(kx1, ky, kx1-kdx, ky+kdy, 'left_input_line'))
    pl.append(GeneralLine(kx1, ky, kx1+kdx, ky+kdy, 'right_input_line'))
    pl.append(GeneralLine(kx2, ky, kx2-kdx, ky+kdy, 'left_output_line'))
    pl.append(GeneralLine(kx2, ky, kx2+kdx, ky+kdy, 'right_output_line'))
    pl.append(CenteredControlTextPath(font, 'L', qx1-qdx, qy))
    pl.append(CenteredControlTextPath(font, 'R', qx1+qdx, qy))
    pl.append(CenteredControlTextPath(font, 'L', qx2-qdx, qy))
    pl.append(CenteredControlTextPath(font, 'R', qx2+qdx, qy))


def GenerateElastikaPanel(cdict:Dict[str, ControlLayer], svgFileName:str, isVcvRack:bool) -> int:
    controls = ControlLayer()
    if isVcvRack:
        cdict['elastika'] = controls
    else:
        cdict['elastika_export'] = controls
    PANEL_WIDTH = 12
    height = PANEL_HEIGHT_MM if isVcvRack else 100.0
    shrink = PANEL_HEIGHT_MM - height
    panel = Panel(PANEL_WIDTH, height)
    pl = Element('g', 'PanelLayer')
    defs = Element('defs')
    pl.append(defs)
    panel.append(pl)
    xmid = panel.mmWidth / 2.0
    PlaceElastikaControls(controls, shrink)
    if isVcvRack:
        PlaceElastikaRackControls(controls)
    (gy1, gy2) = (32.0, 89.5)
    defs.append(Gradient(gy1, gy2, '#5754c4', SAPPHIRE_PANEL_COLOR, 'gradient_fric'))
    defs.append(Gradient(gy2, gy1, '#0060f9', SAPPHIRE_PANEL_COLOR, 'gradient_stif'))
    defs.append(Gradient(gy1, gy2, '#976de4', SAPPHIRE_PANEL_COLOR, 'gradient_span'))
    defs.append(Gradient(gy2, gy1, '#0081d7', SAPPHIRE_PANEL_COLOR, 'gradient_curl'))
    defs.append(Gradient(gy1, gy2, '#29aab4', SAPPHIRE_PANEL_COLOR, 'gradient_mass'))
    defs.append(Gradient(112.5, 90.0, '#b9818b', SAPPHIRE_PANEL_COLOR, 'gradient_power'))
    with Font(SAPPHIRE_FONT_FILENAME) as font:
        pl.append(BorderRect(PANEL_WIDTH, SAPPHIRE_PANEL_COLOR, SAPPHIRE_BORDER_COLOR))
        pl.append(ModelNamePath(panel, font, 'elastika'))
        pl.append(SapphireInsignia(panel, font))
        pl.append(ElastikaShape(font,  0, 'fric', isVcvRack))
        pl.append(ElastikaShape(font,  1, 'stif', isVcvRack))
        pl.append(ElastikaShape(font,  2, 'span', isVcvRack))
        pl.append(ElastikaShape(font,  3, 'curl', isVcvRack))
        pl.append(ElastikaShape(font,  4, 'mass', isVcvRack))
        if isVcvRack:
            pl.append(ElastikaShape(font, -1, 'power', isVcvRack))
        pl.append(CenteredControlTextPath(font, 'TILT', xmid, 20.0))

        tx1 = ELASTIKA_SLIDER_DX*(1.5) + 2.4
        tx2 = ELASTIKA_SLIDER_DX*(3.5) + 2.4
        ty = 17.5
        pl.append(HorizontalLine(tx1, tx2, ty, 'tilt_hor_line'))
        if isVcvRack:
            ElastikaConnectorArt(pl, font, tx1, tx2, ty)

        # IN/OUT labels for TILT knobs...
        pl.append(CenteredControlTextPath(font, 'IN',   tx1, 26.0))
        pl.append(CenteredControlTextPath(font, 'OUT',  tx2, 26.0))

        # IN/OUT labels for drive/level knobs...
        if isVcvRack:
            ty = 93.5
        else:
            ty = 76.0
        pl.append(CenteredControlTextPath(font, 'IN',   ELASTIKA_SLIDER_DX*(1.0) + 2.6, ty))
        pl.append(CenteredControlTextPath(font, 'OUT',  ELASTIKA_SLIDER_DX*(4.0) + 2.4, ty))
    return Save(panel, svgFileName)


def TubeUnitPos(xGrid:int, yGrid:int) -> Tuple[float, float]:
    x = 20.5 + xGrid*20.0
    y = 34.0 + yGrid*21.0 - xGrid*10.5
    return (x, y)

def AddTubeUnitControl(controls:ControlLayer, name:str, column:int, row:int, xofs:float = 0.0, yofs:float = 0.0) -> None:
    (xCenter, yCenter) = TubeUnitPos(column, row)
    controls.append(Component(name, xCenter + xofs, yCenter + yofs))

def AddTubeUnitGroup(controls:ControlLayer, prefix:str, column:int, row:int) -> None:
    xdir = 1 - 2*column     # map column=[0,1] to direction [+1, -1]
    AddTubeUnitControl(controls, prefix + '_knob',  column, row)
    AddTubeUnitControl(controls, prefix + '_atten', column, row, -10.0*xdir, -4.0)
    AddTubeUnitControl(controls, prefix + '_cv',    column, row, -10.0*xdir, +4.0)

def PlaceTubeUnitControls(cdict:Dict[str, ControlLayer]) -> int:
    controls = cdict['tubeunit'] = ControlLayer()
    outJackDx = 12.0
    outJackDy = 5.0
    AddTubeUnitControl(controls, 'level_knob', 1, 4)
    AddTubeUnitControl(controls, 'audio_output_left',  1, 4, +outJackDx, -outJackDy)
    AddTubeUnitControl(controls, 'audio_output_right', 1, 4, +outJackDx, +outJackDy)
    controls.append(Component('audio_input_left',   9.0, 114.5))
    controls.append(Component('audio_input_right', 23.0, 114.5))
    AddTubeUnitGroup(controls, 'airflow', 0, 0)
    AddTubeUnitGroup(controls, 'vortex',  1, 0)
    AddTubeUnitGroup(controls, 'width',   0, 1)
    AddTubeUnitGroup(controls, 'center',  1, 1)
    AddTubeUnitGroup(controls, 'decay',   0, 2)
    AddTubeUnitGroup(controls, 'angle',   1, 2)
    AddTubeUnitGroup(controls, 'root',    0, 3)
    AddTubeUnitGroup(controls, 'spring',  1, 3)
    return 0

if __name__ == '__main__':
    cdict:Dict[str, ControlLayer] = {}
    sys.exit(
        GenerateChaosPanel(cdict, 'frolic') or
        GenerateChaosPanel(cdict, 'glee') or
        GenerateChaosPanel(cdict, 'lark') or
        GenerateChaosOperatorsPanel(cdict) or
        GenerateTricorderPanel() or
        GenerateTinToutPanel(cdict, 'tin',  'input',  'IN',  +5.2) or
        GenerateTinToutPanel(cdict, 'tout', 'output', 'OUT', -7.1) or
        GenerateNucleusPanel(cdict) or
        GeneratePolynucleusPanel(cdict) or
        GenerateHissPanel(cdict) or
        GenerateMootsPanel() or
        GenerateMootsLabel('../res/moots_label_gate.svg', 'GATE') or
        GenerateMootsLabel('../res/moots_label_trigger.svg', 'TRIGGER') or
        GenerateStereoOutputLabels('../res/stereo_out_lr.svg', 'L', 'R') or
        GenerateStereoOutputLabels('../res/stereo_out_2.svg', '2', '') or
        GenerateStereoInputLabels('../res/stereo_in_lr.svg', 'L', 'R') or
        GenerateStereoInputLabels('../res/stereo_in_l2.svg', '2', '') or
        GenerateStereoInputLabels('../res/stereo_in_r2.svg', '', '2') or
        GenerateGalaxyPanel(cdict, 'galaxy') or
        GenerateGravyPanel(cdict, 'gravy') or
        GenerateSaucePanel(cdict, 'sauce') or
        GenerateRotiniPanel(cdict) or
        GeneratePivotPanel(cdict) or
        GenerateSamPanel(cdict) or
        GeneratePopPanel(cdict) or
        GenerateElastikaPanel(cdict, '../res/elastika.svg', True) or
        GenerateElastikaPanel(cdict, '../export/elastika.svg', False) or
        PlaceTubeUnitControls(cdict) or
        SaveControls(cdict) or
        Print('SUCCESS')
    )
