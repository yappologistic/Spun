#!/usr/bin/env python3
"""Test Spun against a disposable Jellyfin server and generated audio."""
import argparse
import concurrent.futures
import json
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
    for folder in ['music','other-music','data','config','cache','logs','client-config','client-data','client-cache']:(root/folder).mkdir()
    def encode(i):
        album=root/'music'/f'Album {i//35:02}';album.mkdir(exist_ok=True)
        ext=['flac','mp3','opus'][i%3];song=album/f'{i:03}.{ext}'
        subprocess.run(['ffmpeg','-nostdin','-v','error','-f','lavfi','-i','sine=frequency=220:sample_rate=48000','-t','12','-af','volume=0.001','-metadata',f'title=Fixture {i:03}','-metadata','artist=Spun Test Artist','-metadata',f'album=Fixture Album {i//35:02}','-metadata','album_artist=Spun Test Artist','-metadata','genre=Test','-metadata',f'track={i%35+1}','-metadata','disc=1',str(song)],check=True)
        if i!=104:song.with_suffix('.lrc').write_text('[00:00.00]First test line\n[00:04.00]Second test line\n[00:08.00]Last test line\n')
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(encode,range(105)))
    for album in (root/'music').iterdir():
        subprocess.run(['ffmpeg','-nostdin','-v','error','-f','lavfi','-i','color=c=0x88ccee:s=96x96','-frames:v','1',str(album/'cover.jpg')],check=True)
    (root/'other-music/Other Album').mkdir()
    subprocess.run(['ffmpeg','-nostdin','-v','error','-f','lavfi','-i','anullsrc','-t','2','-metadata','title=Other library song','-metadata','album=Other album',str(root/'other-music/Other Album/other.flac')],check=True)
    with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
    base=f'http://127.0.0.1:{port}/jellyfin'
    (root/'config/network.xml').write_text(f'''<?xml version="1.0"?><NetworkConfiguration><BaseUrl>/jellyfin</BaseUrl><InternalHttpPort>{port}</InternalHttpPort><EnableIPv4>true</EnableIPv4><EnableIPv6>false</EnableIPv6><EnableHttps>false</EnableHttps><RequireHttps>false</RequireHttps><EnableRemoteAccess>false</EnableRemoteAccess><EnableUPnP>false</EnableUPnP><EnablePublishedServerUriByRequest>false</EnablePublishedServerUriByRequest><LocalNetworkAddresses><string>127.0.0.1</string></LocalNetworkAddresses></NetworkConfiguration>''')
    username='spun-fixture';password=secrets.token_hex(24);token=''
    def api(path,body=None,method=None):
        headers={'Content-Type':'application/json','Authorization':'MediaBrowser Client="SpunTest", Device="Fixture", DeviceId="spun-fixture", Version="1"'+(f', Token="{token}"' if token else '')}
        req=urllib.request.Request(base+path,data=None if body is None else json.dumps(body).encode(),headers=headers,method=method)
        with urllib.request.urlopen(req,timeout=15) as res:
            data=res.read();return json.loads(data) if data else {}
    with (root/'server.log').open('w') as log:
        server=subprocess.Popen([str(args.server_binary.resolve()),'--nowebclient','--nonetchange','--datadir',str(root/'data'),'--configdir',str(root/'config'),'--cachedir',str(root/'cache'),'--logdir',str(root/'logs'),'--ffmpeg','/usr/bin/ffmpeg'],stdout=log,stderr=log)
    try:
        for _ in range(180):
            if server.poll() is not None:raise RuntimeError('Jellyfin exited; see server.log')
            try:api('/Startup/Configuration');break
            except (OSError,ValueError):time.sleep(.5)
        else:raise RuntimeError('Jellyfin did not start')
        api('/Startup/Configuration',{'UICulture':'en-US','MetadataCountryCode':'US','PreferredMetadataLanguage':'en'})
        api('/Startup/User')
        api('/Startup/User',{'Name':username,'Password':password})
        api('/Startup/RemoteAccess',{'EnableRemoteAccess':False,'EnableAutomaticPortMapping':False})
        api('/Startup/Complete',{})
        auth=api('/Users/AuthenticateByName',{'Username':username,'Pw':password});token=auth['AccessToken'];uid=auth['User']['Id']
        for name,folder in [('Fixture Music','music'),('Other Music','other-music')]:
            api('/Library/VirtualFolders?'+urllib.parse.urlencode({'name':name,'collectionType':'music','refreshLibrary':'false'}),{'LibraryOptions':{'PathInfos':[{'Path':str(root/folder)}],'EnableRealtimeMonitor':False,'EnableInternetProviders':False,'EnableAutomaticSeriesGrouping':False,'MetadataSavers':[],'TypeOptions':[{'Type':kind,'MetadataFetchers':[],'MetadataFetcherOrder':[],'ImageFetchers':[],'ImageFetcherOrder':[]} for kind in ['MusicAlbum','MusicArtist','Audio']]}})
        api('/Library/Refresh',{})
        for _ in range(240):
            result=api('/Items?'+urllib.parse.urlencode({'UserId':uid,'Recursive':'true','IncludeItemTypes':'Audio','SearchTerm':'Fixture','Limit':200}))
            if len(result.get('Items',[]))==105:break
            time.sleep(.5)
        else:raise RuntimeError('Jellyfin music scan did not finish')
        guest=api('/Users/New',{'Name':'spun-reader','Password':password})
        policy=guest['Policy'];policy.update(IsAdministrator=False,EnableAllFolders=True,EnableMediaPlayback=True,EnableAudioPlaybackTranscoding=True,EnableContentDeletion=False,EnableSharedDeviceControl=False)
        api('/Users/'+guest['Id']+'/Policy',policy)
        fixture_env={'SPUN_TEST_SERVER':base,'SPUN_TEST_USER':username,'SPUN_TEST_PASSWORD':password,'SPUN_TEST_READER':'spun-reader','SPUN_TEST_READER_ID':guest['Id'],'SPUN_TEST_ADMIN_ID':uid,'SPUN_TEST_ADMIN_TOKEN':token,'XDG_CONFIG_HOME':str(root/'client-config'),'XDG_DATA_HOME':str(root/'client-data'),'XDG_CACHE_HOME':str(root/'client-cache'),'QT_QPA_PLATFORM':'offscreen','QT_QUICK_BACKEND':'software','QT_QPA_PLATFORMTHEME':'','QT_FFMPEG_DECODING_HW_DEVICE_TYPES':',','QT_FFMPEG_ENCODING_HW_DEVICE_TYPES':','}
        env=os.environ|fixture_env
        if args.renderer=='3d':
            env.update(QT_QUICK_BACKEND='rhi',QSG_RHI_BACKEND='opengl',QSG_RENDER_LOOP='basic',LIBGL_ALWAYS_SOFTWARE='1',__GLX_VENDOR_LIBRARY_NAME='mesa',LP_NUM_THREADS='2')
        elif args.renderer=='native':
            env.update(QT_QPA_PLATFORM='wayland',QT_QUICK_BACKEND='rhi')
        # Fixture credentials remain only in this private test output directory.
        (root/'environment.json').write_text(json.dumps(fixture_env,indent=2));(root/'environment.json').chmod(0o600)
        (root/'server.pid').write_text(str(server.pid))
        print('Jellyfin '+api('/System/Info/Public').get('Version','unknown'),flush=True)
        if args.prepare_only:
            prepared=True
            print('Fixture server prepared at '+str(root),flush=True)
            return
        with (root/'test.log').open('w') as log:
            result=subprocess.run([str(args.test_binary.resolve()),'--test-jellyfin','--capture-dir',str(root/'captures')],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=360)
        print((root/'test.log').read_text())
        if result.returncode:raise SystemExit(result.returncode)
    finally:
        if not prepared:
            server.terminate()
            try: server.wait(timeout=15)
            except subprocess.TimeoutExpired: server.kill()

if __name__ == '__main__': main()
