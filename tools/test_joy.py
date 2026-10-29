import ctypes
from ctypes import wintypes
winmm = ctypes.windll.winmm

class JOYCAPSW(ctypes.Structure):
    _fields_ = [
        ('wMid', wintypes.WORD),
        ('wPid', wintypes.WORD),
        ('szPname', ctypes.c_wchar * 32),
        ('wXmin', wintypes.UINT),
        ('wXmax', wintypes.UINT),
        ('wYmin', wintypes.UINT),
        ('wYmax', wintypes.UINT),
        ('wZmin', wintypes.UINT),
        ('wZmax', wintypes.UINT),
        ('wNumButtons', wintypes.UINT),
        ('wPeriodMin', wintypes.UINT),
        ('wPeriodMax', wintypes.UINT),
        ('wRmin', wintypes.UINT),
        ('wRmax', wintypes.UINT),
        ('wUmin', wintypes.UINT),
        ('wUmax', wintypes.UINT),
        ('wVmin', wintypes.UINT),
        ('wVmax', wintypes.UINT),
        ('wCaps', wintypes.UINT),
        ('wMaxAxes', wintypes.UINT),
        ('wNumAxes', wintypes.UINT),
        ('wMaxButtons', wintypes.UINT),
        ('szRegKey', ctypes.c_wchar * 32),
        ('szOEMVxD', ctypes.c_wchar * 260),
    ]

for i in (0, 1):
    caps = JOYCAPSW()
    if winmm.joyGetDevCapsW(i, ctypes.byref(caps), ctypes.sizeof(caps)) == 0:
        print(f'Device {i}: name="{caps.szPname}" mid={caps.wMid:#x} pid={caps.wPid:#x} buttons={caps.wNumButtons} axes={caps.wNumAxes}')

