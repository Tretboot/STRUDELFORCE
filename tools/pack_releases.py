# pack_releases.py - Erstellt die Pre-Release ZIP-Archive fuer GitHub
import os
import zipfile

os.makedirs('releases', exist_ok=True)

# 1. strudelforce_v0.5.0_win64.zip
win64_zip = 'releases/strudelforce_v0.5.0_win64.zip'
print(f'Erstelle {win64_zip}...')

with zipfile.ZipFile(win64_zip, 'w', zipfile.ZIP_DEFLATED) as zf:
    # Binaries
    if os.path.exists('strudelforce_gui.exe'):
        zf.write('strudelforce_gui.exe', 'strudelforce_gui.exe')
    if os.path.exists('strudelforce_game.exe'):
        zf.write('strudelforce_game.exe', 'strudelforce_game.exe')
    
    # Dokumentation & Scripts
    for fn in ['README.md', 'AGENTS.md', 'Makefile', 'build_msvc.bat']:
        if os.path.exists(fn):
            zf.write(fn, fn)
            
    # Tools
    for fn in os.listdir('tools'):
        p = os.path.join('tools', fn)
        if os.path.isfile(p):
            zf.write(p, p)
            
    # user_music
    if os.path.exists('user_music'):
        for fn in os.listdir('user_music'):
            p = os.path.join('user_music', fn)
            if os.path.isfile(p):
                zf.write(p, p)

    # Ausgewaehlte Top-TFMX-Module (Apidya, Battle Isle, Turrican, etc.)
    top_games = ['Apidya', 'Battle Isle', 'Apprentice', 'BC Kid', 'Hard\'n\'Heavy', 'Gem X']
    for tg in top_games:
        tg_dir = os.path.join('TFMX Professional', tg)
        if os.path.exists(tg_dir):
            for fn in os.listdir(tg_dir):
                p = os.path.join(tg_dir, fn)
                if os.path.isfile(p):
                    zf.write(p, p)

print(f'  {win64_zip} erstellt: {os.path.getsize(win64_zip)} Bytes')

# 2. strudelforce_v0.5.0_source.zip
src_zip = 'releases/strudelforce_v0.5.0_source.zip'
print(f'Erstelle {src_zip}...')

with zipfile.ZipFile(src_zip, 'w', zipfile.ZIP_DEFLATED) as zf:
    # Root files
    for fn in ['README.md', 'AGENTS.md', 'Makefile', 'build_msvc.bat', 'kiwi_toolkit.py', 'kiwi_toolkit_multifolder.py']:
        if os.path.exists(fn):
            zf.write(fn, fn)
            
    # src
    for root, _, files in os.walk('src'):
        for f in files:
            if not f.endswith(('.obj', '.exe')):
                p = os.path.join(root, f)
                zf.write(p, p)
                
    # tools
    for root, _, files in os.walk('tools'):
        for f in files:
            p = os.path.join(root, f)
            zf.write(p, p)
            
    # STRUDELFORCE & STRUDELFORCE_Smith_Core
    for branch in ['STRUDELFORCE', 'STRUDELFORCE_Smith_Core']:
        for root, _, files in os.walk(branch):
            for f in files:
                if not f.endswith(('.obj', '.exe', '.wav')):
                    p = os.path.join(root, f)
                    zf.write(p, p)

print(f'  {src_zip} erstellt: {os.path.getsize(src_zip)} Bytes')
print('Pre-Release ZIPs erfolgreich fertiggestellt!')

