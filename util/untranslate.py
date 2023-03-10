#!/usr/bin/env python3
#
#   untranslate.py  -  Don Cross  -  2023-03-09
#
#   Remove "translate(x,y)" from an SVG file, updating
#   the coordinates in the paths it affects.
#
import sys
import re
import xml.etree.ElementTree as et

# Prevent seeing lots of "ns0:" everywhere when we re-serialized the XML.
et.register_namespace('', 'http://www.w3.org/2000/svg')

def FixPath(path, dx, dy):
    # m 3.9801697,10.193323 q -0.261055,0 -0.4550823,-0.08819
    # Q 3.33106,10.013406 3.1405604,9.8229067 3.1193938,9.80174 3.1193938,9.7699901 q 0,-0.028222 0.024694,-0.052916 0.024694,-0.028222 0.052916,-0.028222 0.03175,0 0.059972,0.03175 0.1375831,0.1622775 0.3245549,0.2469439 0.1904996,0.084667 0.398638,0.084667 0.2716383,0 0.4480268,-0.1305278 Q 4.604585,9.7911567 4.604585,9.5759628 4.6010572,9.4066298 4.512863,9.2972689 4.4246687,9.1879081 4.2941412,9.1244082 4.1671415,9.0573806 3.944892,8.9762419 3.7014759,8.8845198 3.5638928,8.8139644 3.4298376,8.743409 3.33106,8.6128815 3.2358102,8.482354 3.2358102,8.2706878 q 0,-0.1728607 0.09525,-0.3139715 0.09525,-0.1411108 0.2716383,-0.2222495 0.1763885,-0.081139 0.4092213,-0.081139 0.2081385,0 0.3951103,0.077611 0.1869718,0.074083 0.2963327,0.2116662 0.035278,0.049389 0.035278,0.074083 0,0.028222 -0.028222,0.052917 -0.024694,0.021167 -0.056444,0.021167 -0.028222,0 -0.045861,-0.021167 Q 4.5093352,7.9426052 4.3541133,7.8685221 4.1988914,7.7944389 4.0119196,7.7944389 q -0.2681105,0 -0.4480268,0.1269997 -0.1763885,0.1269997 -0.1763885,0.3457215 0,0.1516941 0.084666,0.2575272 0.088194,0.1058331 0.2187217,0.1728607 0.1305275,0.0635 0.3245549,0.1340553 0.2469439,0.088194 0.3915825,0.1622774 0.1446385,0.074083 0.2469439,0.215194 0.1023053,0.1411108 0.1023053,0.3739436 0,0.1622774 -0.09525,0.3033882 -0.09525,0.1411105 -0.2751661,0.2257775 -0.1763885,0.08114 -0.4056935,0.08114 z
    # The first point after 'm', and all points after 'M', 'Q', need to be adjusted.
    fix = []
    shiftNext = False
    shiftRest = False
    command = None
    startPath = True
    for t in path.split():
        u = t
        if t == 'm':
            shiftNext = startPath
            shiftRest = False
            command = t
        elif t in ['M', 'Q', 'H', 'V', 'L']:
            shiftNext = True
            shiftRest = True
            command = t
        elif t in ['q', 'h', 'v', 'l']:
            shiftNext = False
            shiftRest = False
            command = t
        elif (comma := t.find(',')) > 0:
            x = float(t[:comma])
            y = float(t[1+comma:])
            if shiftNext or shiftRest:
                shiftNext = False
                u = '{:0.6f},{:0.6f}'.format(x+dx, y+dy)
        elif re.match(r'^[\-\+0-9]', t):
            z = float(t)
            if shiftNext or shiftRest:
                shiftNext = False
                if command in ['H', 'h']:
                    u = '{:0.6f}'.format(z+dx)
                elif command in ['V', 'v']:
                    u = '{:0.6f}'.format(z+dy)
                else:
                    raise Exception('Unknown command [{}] before scalar [{}]'.format(command, t))
        elif t in ['z', 'Z']:
            shiftNext = shiftRest = False
        else:
            raise Exception('Unknown path token [{}]'.format(t))
        fix.append(u)
        startPath = False
    return ' '.join(fix)

def RemoveTranslates(elem, xOffset, yOffset):
    '''Remove all transform="translate(x,y)" from the SVG document.'''
    # Look for attribute transform="translate(x,y)"
    xAdjust = xOffset
    yAdjust = yOffset
    if transform := elem.attrib.get('transform'):
        # translate(-2.2,-6.95)
        if m := re.match(r'^\s*translate\s*\(\s*([\+\-\.0-9]+)\s*,\s*([\+\-\.0-9]+)\s*\)\s*$', transform):
            xAdjust += float(m.group(1))
            yAdjust += float(m.group(2))
            del elem.attrib['transform']
        else:
            print('Ignoring unknown transform: {}'.format(transform))

    # Update all path coordinates for this node.
    if elem.tag == '{http://www.w3.org/2000/svg}path':
        if data := elem.attrib.get('d'):
            fix = FixPath(data, xAdjust, yAdjust)
            elem.attrib['d'] = fix
    # Recurse through all children.
    # When found, vector-add <xOffset+x, yOffset+y> for children's offset.
    for child in elem:
        RemoveTranslates(child, xAdjust, yAdjust)

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('USAGE: untranslate.py input.svg output.svg')
        sys.exit(1)
    inFileName = sys.argv[1]
    outFileName = sys.argv[2]
    svg = et.parse(inFileName)
    RemoveTranslates(svg.getroot(), 0.0, 0.0)
    svg.write(outFileName, encoding='utf-8', xml_declaration=True)
    sys.exit(0)
