"""Deterministic provider process for cancellation, transport and UI regression tests."""
import json
import os
from pathlib import Path
import shutil
import sys
import time

req=json.load(sys.stdin)
op=req['op']
def song(i=1):
    return dict(id=f'fixture00{i:02}',kind='song',title=f'First Light {i}',artist='Spun Sound Lab',album='The first mixtape',albumId='MPREfixture',seconds=32)
result={'ok':True}
if op=='check':result['ready']=True
elif op=='buffer':
    if req['id']=='fixture0003':time.sleep(2)
    if req['id']=='fixture0004':result={'ok':False,'error':'Unavailable'}
    else:
        path=Path(req['directory'])/'song.flac';shutil.copyfile(os.environ['SPUN_YOUTUBE_FIXTURE_AUDIO'],path);result['file']=str(path)
elif op=='lyrics':result['lines']=[{'start':0,'end':5000,'text':'First light through the window'},{'start':5000,'end':30000,'text':'A quiet room, a new day'}]
elif op=='search':
    if req['query']=='slow':time.sleep(2)
    if req['query']=='error':result={'ok':False,'error':'Test network failure'}
    elif req['query']=='empty':result['items']=[]
    elif req.get('filter')=='albums':result['items']=[dict(id='MPREfixture',kind='album',title='The first mixtape',artist='Spun Sound Lab')]
    elif req.get('filter')=='artists':result['items']=[dict(id='UCfixture',kind='artist',title='Spun Sound Lab')]
    elif req.get('filter')=='playlists':result['items']=[dict(id='PLfixture',kind='playlist',title='Fixture playlist')]
    else:result['items']=[song(1),song(2)]
elif op in ('album','playlist','radio','link'):result.update(items=[song(1),song(2)],title='The first mixtape')
elif op in ('artist','home'):result['sections']=[dict(title='Songs',items=[song(1),song(2)])]
print(json.dumps(result))
