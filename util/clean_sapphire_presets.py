#!/usr/bin/env python3
#
#   clean_sapphire_presets.py
#   Don Cross <cosinekitty@gmail.com>
#   https://github.com/cosinekitty/sapphire
#
#   Validates and cleans factory presets for Sapphire Zoo.
#
import sys
import os
import json
from typing import List, Any

def Print(message:str) -> int:
    print('clean_sapphire_presets.py:', message)
    return 0


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
        Print('Kept:  {}'.format(filename))
        return False
    with open(filename, 'wt') as outfile:
        outfile.write(newText)
    Print('Wrote: {}'.format(filename))
    return True


def FactoryPresets(moduleSlug:str) -> List[str]:
    path = os.path.join('../presets', moduleSlug)
    fnlist:List[str] = []
    for fn in os.listdir(path):
        if fn.endswith('.vcvm'):
            fullname = os.path.join(path, fn)
            fnlist.append(fullname)
    return fnlist


def CleanZooPreset(fn:str) -> int:
    with open(fn, 'rt') as infile:
        oldText = infile.read()
    preset = json.loads(oldText)
    if 'params' in preset:
        del preset['params']
    if 'data' not in preset:
        Print('ERROR: Missing "data" property in JSON: ' + fn)
        return 1
    data = preset['data']
    if len(data['memory']) > 1:
        data['memory'] = data['memory'][0:1]
    for key in ['lowSensitivityAttenuverters', 'voltageFlippedOutputPorts', 'neonMode', 'turboMode', 'flashPanelOnOverflow']:
        if key in data:
            del data[key]
    newText = json.dumps(preset, indent=2)
    if newText == oldText:
        Print('Kept:  {}'.format(fn))
    else:
        with open(fn, 'wt') as outfile:
            outfile.write(newText)
        Print('Wrote: {}'.format(fn))
    return 0


def CleanZooPresets() -> int:
    # Iterate through all Zoo vcvm files.
    for fn in FactoryPresets('Zoo'):
        if CleanZooPreset(fn): return 1
    return Print('SUCCESS')

if __name__ == '__main__':
    sys.exit(CleanZooPresets())
