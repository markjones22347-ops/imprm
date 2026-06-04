import os, sys, pathlib
from dotenv import load_dotenv
base = pathlib.Path().absolute()
load_dotenv(base / '.env')
load_dotenv(base / 'render-env.txt', override=False)
print('DISCORD_TOKEN', bool(os.getenv('DISCORD_TOKEN')))
print('GIST_ID', os.getenv('GIST_ID'))
print('GITHUB_TOKEN', 'set' if os.getenv('GITHUB_TOKEN') else 'unset')
sys.path.insert(0, str(base))
from cogs.database import create_key, get_all_keys
key = 'IMPERIUM-TEST-0001-0001-0001'
print('create_key start')
if key in get_all_keys():
    print('already exists')
else:
    create_key(key, 'lifetime', 0)
    print('created')
print('keys now', list(get_all_keys().keys())[:20])
