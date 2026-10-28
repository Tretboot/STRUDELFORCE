import os, zlib

games = {}
for root, dirs, files in os.walk('TFMX Professional'):
    gname = os.path.basename(root) if root != 'TFMX Professional' else '_SingleTracks'
    for f in files:
        if f.endswith(('.tfx', '.sam')):
            fp = os.path.join(root, f)
            sz = os.path.getsize(fp)
            if gname not in games:
                games[gname] = {'tfx': 0, 'sam': 0, 'raw': 0, 'files': []}
            if f.endswith('.tfx'): games[gname]['tfx'] += 1
            if f.endswith('.sam'): games[gname]['sam'] += 1
            games[gname]['raw'] += sz
            games[gname]['files'].append(fp)

sorted_games = sorted(games.items(), key=lambda x: -x[1]['tfx'])
print(f"Total Folders/Games: {len(sorted_games)}")
print("-" * 55)
total_tfx = 0
total_sam = 0
total_raw = 0

for g, info in sorted_games:
    total_tfx += info['tfx']
    total_sam += info['sam']
    total_raw += info['raw']
    print(f"{g:28s}: {info['tfx']:2d} tfx, {info['sam']:2d} sam | {info['raw']/1024:6.1f} KB")

print("-" * 55)
print(f"TOTAL: {total_tfx} .tfx, {total_sam} .sam | {total_raw/1024/1024:.2f} MB raw")
