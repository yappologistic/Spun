#!/usr/bin/env python3
"""Test Spun against a disposable Navidrome server and generated audio."""
import argparse
import concurrent.futures
import json
import hashlib
import os
from pathlib import Path
import secrets
import socket
import subprocess
import time
import urllib.error
import urllib.parse
import urllib.request


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--server-binary',type=Path,required=True)
    p.add_argument('--test-binary',type=Path)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--renderer',choices=['software','3d','native'],default='software',help='Use native only in a separate test desktop session')
    p.add_argument('--prepare-only',action='store_true',help='Leave the fixture server running for interactive development')
    args=p.parse_args()
    if not args.prepare_only and not args.test_binary:
        p.error("--test-binary is required unless --prepare-only is used")
    prepared=False
    root=args.output.resolve();root.mkdir(parents=True,exist_ok=False);root.chmod(0o700)
    for folder in ['music','data','config','cache','client-config','client-data','client-cache']:(root/folder).mkdir()
    def encode(i):
        album=root/'music'/f'Album {i//35:02}';album.mkdir(exist_ok=True)
        ext=['flac','mp3','opus'][i%3];song=album/f'{i:03}.{ext}'
        subprocess.run(['ffmpeg','-nostdin','-v','error','-f','lavfi','-i','sine=frequency=220:sample_rate=48000','-t','12','-af','volume=0.001','-metadata',f'title=Fixture {i:03}','-metadata','artist=Spun Test Artist','-metadata',f'album=Fixture Album {i//35:02}','-metadata','album_artist=Spun Test Artist','-metadata','genre=Test','-metadata',f'track={i%35+1}','-metadata','disc=1',str(song)],check=True)
        if i!=104:song.with_suffix('.lrc').write_text('[00:00.00]First test line\n[00:04.00]Second test line\n[00:08.00]Last test line\n')
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(encode,range(105)))
    for album in (root/'music').iterdir():
        subprocess.run(['ffmpeg','-nostdin','-v','error','-f','lavfi','-i','color=c=0x88ccee:s=96x96','-frames:v','1',str(album/'cover.jpg')],check=True)
    with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
    base=f'http://127.0.0.1:{port}/music'
    config=root/'navidrome.toml'
    config.write_text(f'Address = "127.0.0.1"\nPort = {port}\nBaseUrl = "/music"\nMusicFolder = {json.dumps(str(root/"music"))}\nDataFolder = {json.dumps(str(root/"data"))}\nCacheFolder = {json.dumps(str(root/"cache"))}\nScanSchedule = "0"\nEnableInsightsCollector = false\nLogLevel = "error"\n')
    username='spun-fixture';password=secrets.token_hex(24);token=''
    def request(path,body=None,method=None):
        headers={'Content-Type':'application/json'}
        if token: headers['X-ND-Authorization']='Bearer '+token
        req=urllib.request.Request(base+path,data=None if body is None else json.dumps(body).encode(),headers=headers,method=method)
        with urllib.request.urlopen(req,timeout=15) as res:
            data=res.read();return json.loads(data) if data else {}
    def api(method,**params):
        salt=secrets.token_hex(8)
        params.update(u=username,s=salt,t=hashlib.md5((password+salt).encode()).hexdigest(),v='1.16.1',c='SpunTest',f='json')
        result=request('/rest/'+method+'.view?'+urllib.parse.urlencode(params))['subsonic-response']
        if result['status']!='ok':raise RuntimeError('Fixture API failed: '+method)
        return result
    with (root/'server.log').open('w') as log:
        server=subprocess.Popen([str(args.server_binary.resolve()),'--configfile',str(config)],stdout=log,stderr=log)
    try:
        for _ in range(180):
            if server.poll() is not None:raise RuntimeError('Navidrome exited; see server.log')
            try:
                auth=request('/auth/createAdmin',{'username':username,'password':password});token=auth['token'];break
            except (OSError,ValueError):time.sleep(.25)
        else:raise RuntimeError('Navidrome did not start')
        request('/api/user',{'userName':'spun-reader','name':'Fixture reader','password':password,'isAdmin':False})
        api('startScan')
        for _ in range(240):
            result=api('search3',query='Fixture',songCount=200,albumCount=0,artistCount=0)
            if len(result.get('searchResult3',{}).get('song',[]))==105:break
            time.sleep(.25)
        else:raise RuntimeError('Navidrome music scan did not finish')
        fixture_env={'SPUN_TEST_SERVER':base,'SPUN_TEST_USER':username,'SPUN_TEST_PASSWORD':password,'SPUN_TEST_READER':'spun-reader','XDG_CONFIG_HOME':str(root/'client-config'),'XDG_DATA_HOME':str(root/'client-data'),'XDG_CACHE_HOME':str(root/'client-cache'),'QT_QPA_PLATFORM':'offscreen','QT_QUICK_BACKEND':'software','QT_QPA_PLATFORMTHEME':'','QT_FFMPEG_DECODING_HW_DEVICE_TYPES':',','QT_FFMPEG_ENCODING_HW_DEVICE_TYPES':','}
        env=os.environ|fixture_env
        if args.renderer=='3d':
            env.update(QT_QUICK_BACKEND='rhi',QSG_RHI_BACKEND='opengl',QSG_RENDER_LOOP='basic',LIBGL_ALWAYS_SOFTWARE='1',__GLX_VENDOR_LIBRARY_NAME='mesa',LP_NUM_THREADS='2')
        elif args.renderer=='native':
            env.update(QT_QPA_PLATFORM='wayland',QT_QUICK_BACKEND='rhi')
        # Fixture credentials remain only in this private test output directory.
        (root/'environment.json').write_text(json.dumps(fixture_env,indent=2));(root/'environment.json').chmod(0o600)
        (root/'server.pid').write_text(str(server.pid))
        print('Navidrome '+api('ping').get('serverVersion','unknown'),flush=True)
        if args.prepare_only:
            prepared=True
            print('Fixture server prepared at '+str(root),flush=True)
            return
        with (root/'test.log').open('w') as log:
            result=subprocess.run([str(args.test_binary.resolve()),'--test-subsonic','--capture-dir',str(root/'captures')],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=360)
        print((root/'test.log').read_text())
        if result.returncode:raise SystemExit(result.returncode)
    finally:
        if not prepared:
            server.terminate()
            try: server.wait(timeout=15)
            except subprocess.TimeoutExpired: server.kill(); server.wait()

if __name__ == '__main__': main()
