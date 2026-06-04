import os, pathlib, urllib.request, json
base = pathlib.Path('bot.py').absolute().parent
print('base', base)
env = {}
for path in ['.env', 'render-env.txt']:
    p = base / path
    print('path', p, 'exists', p.exists())
    if p.exists():
        for line in p.read_text().splitlines():
            if '=' in line:
                k, v = line.split('=', 1)
                env.setdefault(k, v)
os.environ.update(env)
print('GIST_ID', os.getenv('GIST_ID'))
print('GITHUB_TOKEN', 'set' if os.getenv('GITHUB_TOKEN') else 'unset')
gid = os.getenv('GIST_ID')
tok = os.getenv('GITHUB_TOKEN')
if not gid or not tok:
    raise SystemExit('missing creds')
req = urllib.request.Request(f'https://api.github.com/gists/{gid}', headers={'Authorization': f'token {tok}', 'Accept': 'application/vnd.github+json', 'User-Agent': 'ImperiumBot/1.0'})
with urllib.request.urlopen(req, timeout=10) as resp:
    gist = json.loads(resp.read().decode('utf-8'))
print('files', list(gist.get('files', {}).keys()))
for name, info in gist.get('files', {}).items():
    print('---', name)
    print(info.get('content'))
