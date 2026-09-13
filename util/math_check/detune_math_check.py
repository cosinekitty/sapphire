#!/usr/bin/env python3
import sys


def Original(x:float) -> float:
    return 2.0 ** (x / 100.0)


def Cheap(x:float) -> float:
    # 2**0.01 - 1
    return 1.0 + (0.006955550056718884 * x)


def CheckMath() -> int:
    x = 0.0
    while x <= 1.01:
        y = Original(x)
        z = Cheap(x)
        print('x={:10.6f}, y={:10.6f}, z={:10.6f}, diff={:g}'.format(x, y, z, z-y))
        x += 0.01
    return 0

if __name__ == '__main__':
    sys.exit(CheckMath())
