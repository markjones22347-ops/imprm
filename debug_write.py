import os, sys, pathlib
base = pathlib.Path().absolute()
sys.path.insert(0, str(base))
from cogs.database import create_key, get_all_keys
key = 'IMPERIUM-TEST-0000-0000'
print('creating', key)
if key in get_all_keys():
    print('already exists');
else:
    create_key(key, 'lifetime', 0)
print('keys now', list(get_all_keys().keys())[:10])
