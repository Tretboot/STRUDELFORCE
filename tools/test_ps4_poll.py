import ctypes, time
from ctypes import wintypes
winmm = ctypes.windll.winmm

class JOYINFOEX(ctypes.Structure):
    _fields_ = [
        ('dwSize', wintypes.DWORD),
        ('dwFlags', wintypes.DWORD),
        ('dwXpos', wintypes.DWORD),
        ('dwYpos', wintypes.DWORD),
        ('dwZpos', wintypes.DWORD),
        ('dwRpos', wintypes.DWORD),
        ('dwUpos', wintypes.DWORD),
        ('dwVpos', wintypes.DWORD),
        ('dwButtons', wintypes.DWORD),
        ('dwButtonNumber', wintypes.DWORD),
        ('dwPOV', wintypes.DWORD),
        ('dwReserved1', wintypes.DWORD),
        ('dwReserved2', wintypes.DWORD),
    ]

# Poll Device 1 (Sony DS4) for 2 seconds
jex = JOYINFOEX()
jex.dwSize = ctypes.sizeof(JOYINFOEX)
jex.dwFlags = 0xFF

t0 = time.time()
print("Polling Device 1 (PS4) for 2 seconds...")
last_btn = None
while time.time() - t0 < 2.0:
    if winmm.joyGetPosEx(1, ctypes.byref(jex)) == 0:
        if jex.dwButtons != last_btn:
            print(f"Buttons changed: {jex.dwButtons:#06x} | X={jex.dwXpos} Y={jex.dwYpos} Z={jex.dwZpos} R={jex.dwRpos} POV={jex.dwPOV}")
            last_btn = jex.dwButtons
    time.sleep(0.02)
print("Done.")

