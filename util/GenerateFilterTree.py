#!/usr/bin/env python3
import sys
from math import sqrt, pi
from typing import List

TEMPLATE = r'''#pragma once
namespace Sapphire
{
[[[PLAN_COMMENTS]]]

    template <typename value_t>
    class [[[CLASS_NAME]]]
    {
    public:
        static const int NBANDS = [[[NBANDS]]];

    private:
[[[VAR_DECL]]]
    public:
        void reset()
        {
            for (int i = 0; i < NFILTERS; ++i)
                xprev[i] = yprev[i] = 0;
        }

        void setSampleRate(value_t sampleRateHz)
        {
[[[SET_SAMPLE_RATE]]]
        }

        void update(value_t input, value_t output[NBANDS])
        {
[[[UPDATE]]]
        }
    };
}
'''


class FilterInfo:
    def __init__(self, corner:float, inputFilterIndex:int, inputFilterSelect:int) -> None:
        self.corner = corner
        self.inputFilterIndex = inputFilterIndex
        self.inputFilterSelect = inputFilterSelect


class BandInfo:
    def __init__(self, lo:float, hi:float, filterIndex:int, filterSelect:int) -> None:
        self.lo = lo
        self.hi = hi
        self.filterIndex = filterIndex
        self.filterSelect = filterSelect


class Plan:
    def __init__(self, height:int, loFreq:float, hiFreq:float) -> None:
        if height <= 0:
            raise Exception('Tree height must be a positive integer.')
        self.height = height
        self.filterList:List[FilterInfo] = []
        self.bandList:List[BandInfo] = []
        self._plan(0, loFreq, hiFreq, -1, -1)
        self.className = 'FilterTree_{}'.format(len(self.bandList))

    def _plan(self, depth:int, lofreq:float, hifreq:float, findex:int, fsel:int) -> None:
        if depth < self.height:
            midfreq = sqrt(lofreq * hifreq)
            nextFilterIndex = len(self.filterList)
            self.filterList.append(FilterInfo(midfreq, findex, fsel))
            self._plan(1+depth, lofreq, midfreq, nextFilterIndex, 0)
            self._plan(1+depth, midfreq, hifreq, nextFilterIndex, 1)
        else:
            self.bandList.append(BandInfo(lofreq, hifreq, findex, fsel))

    def comments(self) -> str:
        text = ''
        fn = len(self.filterList)
        fi = 0
        while fi < fn:
            f = self.filterList[fi]
            text += '    // filter[{}]: corner={}'.format(fi, f.corner)
            if f.inputFilterIndex >= 0:
                text += ', input={}, select={}'.format(f.inputFilterIndex, f.inputFilterSelect)
            fi += 1
            text += '\n'

        bn = len(self.bandList)
        bi = 0
        while bi < bn:
            b = self.bandList[bi]
            text += '    // band[{}]: findex={}, fsel={}, lo={}, hi={}'.format(bi, b.filterIndex, b.filterSelect, b.lo, b.hi)
            bi += 1
            if bi < bn:
                text += '\n'
        return text

    def setSampleRateCode(self) -> str:
        text = '            // Update all the filter coefficients.\n'
        text = '            value_t c;\n'
        fi = 0
        fn = len(self.filterList)
        while fi < fn:
            f = self.filterList[fi]
            text += '            c = sampleRateHz / {:0.16g};\n'.format(pi * f.corner)
            text += '            cnum[{}] = 1 - c;\n'.format(fi)
            text += '            cden[{}] = 1 + c;'.format(fi)
            fi += 1
            if fi < fn:
                text += '\n'
        return text

    def varDeclCode(self) -> str:
        text =  '        static const int NFILTERS = {};\n'.format(len(self.filterList))
        text += '        value_t cnum[NFILTERS] {};\n'
        text += '        value_t cden[NFILTERS] {};\n'
        text += '        value_t xprev[NFILTERS] {};\n'
        text += '        value_t yprev[NFILTERS] {};\n'
        return text

    def _inputText(self, findex:int, fsel:int) -> str:
        if fsel < 0:
            # This filter gets its input from the `input` parameter.
            return 'input'
        if fsel == 0:
            # LoPass case: just use yprev
            return 'yprev[{0}]'.format(findex)
        if fsel == 1:
            # HiPass case: subtract: xprev - yprev
            return 'xprev[{0}] - yprev[{0}]'.format(findex)
        raise Exception('Invalid filter select = {}'.format(fsel))

    def updateCode(self) -> str:
        # Generate code that updates each filter.
        text = ''
        fi = 0
        fn = len(self.filterList)
        text += '            // Update the filters.\n'
        text += '            value_t x;\n'
        while fi < fn:
            f = self.filterList[fi]
            inputText = self._inputText(f.inputFilterIndex, f.inputFilterSelect)
            text += '            x = {};\n'.format(inputText)
            text += '            yprev[{0}] = (x + xprev[{0}] - yprev[{0}]*cnum[{0}]) / cden[{0}];\n'.format(fi)
            text += '            xprev[{0}] = x;\n'.format(fi)
            fi += 1

        # Generate code that calculates the outputs from the filters
        text += '            // Calculate the output bands.\n'
        bi = 0
        bn = len(self.bandList)
        while bi < bn:
            b = self.bandList[bi]
            inputText = self._inputText(b.filterIndex, b.filterSelect)
            text += '            output[{}] = {};'.format(bi, inputText)
            bi += 1
            if bi < bn:
                text += '\n'

        return text


def GenerateFilterTree(outFileName:str, height:int, loFreq:float, hiFreq:float) -> int:
    with open(outFileName, 'wt') as outfile:
        plan = Plan(height, loFreq, hiFreq)
        text = TEMPLATE.replace('[[[PLAN_COMMENTS]]]', plan.comments())
        text = text.replace('[[[CLASS_NAME]]]', plan.className)
        text = text.replace('[[[NBANDS]]]', str(len(plan.bandList)))
        text = text.replace('[[[SET_SAMPLE_RATE]]]', plan.setSampleRateCode())
        text = text.replace('[[[VAR_DECL]]]', plan.varDeclCode())
        text = text.replace('[[[UPDATE]]]', plan.updateCode())
        outfile.write(text)
    print('GenerateFilterTree.py: wrote {}'.format(outFileName))
    return 0


if __name__ == '__main__':
    if len(sys.argv) != 5:
        print('USAGE: GenerateFilterTree.py outfile.hpp height loFreq hiFreq')
        sys.exit(1)
    outFileName = sys.argv[1]
    height = int(sys.argv[2])
    loFreq = float(sys.argv[3])
    hiFreq = float(sys.argv[4])
    sys.exit(GenerateFilterTree(outFileName, height, loFreq, hiFreq))
