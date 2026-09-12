#!/usr/bin/env python3
"""One request per process. No server, browser, telemetry, or idle worker."""
import json
import re
import sys
from urllib.parse import urlparse, parse_qs


def artwork(item):
    thumbs = item.get('thumbnails') or []
    if not thumbs:
        return ''
    src = thumbs[-1].get('url', '')
    # Google returns tiny search thumbnails; ask the same image service for 544px.
    if 'googleusercontent.com/' in src or 'ggpht.com/' in src:
        src = re.sub(r'=w\d+-h\d+[^?]*$', '=w544-h544-l90-rj', src)
    return src


def normalize(item, kind='', parent=None):
    parent = parent or {}
    video = item.get('videoId') or ''
    browse = item.get('browseId') or item.get('playlistId') or ''
    kind = item.get('resultType') or kind or ('song' if video else 'playlist' if item.get('playlistId') else 'album')
    artists = item.get('artists') or parent.get('artists') or []
    if isinstance(artists, str):
        artists = [{'name': artists}]
    album = item.get('album') or {}
    if isinstance(album, str):
        album = {'name': album}
    if parent.get('type') == 'album':
        album = {'name': parent.get('title', ''), 'id': parent.get('browseId', '')}
    author = item.get('author') or {}
    artist_name = ', '.join(x.get('name', '') for x in artists)
    if not artist_name and isinstance(author, dict):
        artist_name = author.get('name', '')
    return {'id': video or browse, 'videoId': video, 'browseId': browse,
            'kind': kind, 'title': item.get('title') or item.get('name') or (item.get('artist') if isinstance(item.get('artist'), str) else '') or 'Untitled',
            'artist': artist_name,
            'artistId': next((a.get('id') for a in artists if a.get('id')), browse if kind == 'artist' else ''),
            'album': album.get('name', ''), 'albumId': album.get('id', ''),
            'art': artwork(item) or artwork(parent), 'duration': item.get('duration') or item.get('length') or '',
            'seconds': item.get('duration_seconds') or 0,
            'discNumber': item.get('discNumber') or item.get('disc_number') or 1,
            'explicit': bool(item.get('isExplicit')), 'available': item.get('isAvailable', True)}


def clean(items, kind='', parent=None):
    return [t for i in items if isinstance(i, dict) and (t := normalize(i, kind, parent))['id']]


def normalize_lyrics(data):
    from dataclasses import asdict, is_dataclass
    if is_dataclass(data): data=asdict(data)
    data=data or {}
    raw=data.get('lyrics') or ''
    if isinstance(raw,str):return {'lyrics':raw,'lines':[]}
    lines=[]
    for line in raw:
        if is_dataclass(line):line=asdict(line)
        if not isinstance(line,dict):continue
        start=line.get('start_time');end=line.get('end_time');text=line.get('text','')
        if not isinstance(start,(int,float)) or start<0 or not isinstance(text,str):continue
        lines.append({'start':int(start),'end':int(end) if isinstance(end,(int,float)) and end>=start else 0,'text':text})
    lines.sort(key=lambda line:line['start'])
    return {'lyrics':'\n'.join(line['text'] for line in lines),'lines':lines}



def run(req):
    op = req.get('op', '')
    if op == 'check':
        import ytmusicapi, yt_dlp, shutil
        if not shutil.which('node'):
            raise RuntimeError('Node.js is required for YouTube playback.')
        return {'ready': True}
    if op == 'buffer':
        import yt_dlp
        from pathlib import Path
        vid = req.get('id', '')
        if not re.fullmatch(r'[A-Za-z0-9_-]{11}', vid):
            raise ValueError('Invalid YouTube song')
        directory = Path(req['directory']).resolve(strict=True)
        def bound_size(progress):
            if progress.get('downloaded_bytes', 0) > 64*1024*1024:
                raise RuntimeError('This song exceeds the 64 MiB playback buffer.')
        opts = dict(quiet=True, noprogress=True, no_warnings=True, noplaylist=True,
                    format='bestaudio[ext=m4a]/bestaudio', socket_timeout=15,
                    retries=1, extractor_retries=1, cachedir=False,
                    js_runtimes={'node': {}}, max_filesize=64*1024*1024,
                    outtmpl=str(directory / (vid + '.%(ext)s')),
                    progress_hooks=[bound_size])
        with yt_dlp.YoutubeDL(opts) as dl:
            info = dl.extract_info('https://music.youtube.com/watch?v=' + vid, download=True)
            path = Path(dl.prepare_filename(info))
        if not path.is_file() or path.stat().st_size > 64*1024*1024:
            raise RuntimeError('Could not buffer this song within the playback limit.')
        return {'file': str(path), 'seconds': info.get('duration', 0)}
    from ytmusicapi import YTMusic
    api = YTMusic(requests_session=True)
    original = api._session.request
    def bounded(*args, **kwargs):
        kwargs.setdefault('timeout', 15)
        return original(*args, **kwargs)
    api._session.request = bounded
    if op == 'home':
        return {'sections': [{'title': s.get('title', ''), 'items': clean(s.get('contents', []))}
                             for s in api.get_home(limit=5) if s.get('contents')]}
    if op == 'search':
        limit = min(max(int(req.get('limit', 30)), 1), 200)
        return {'items': clean(api.search(req['query'], filter=req.get('filter') or None, limit=limit))}
    if op == 'album':
        data = api.get_album(req['id'])
        data.update(type='album', browseId=req['id'])
        return {'title': data.get('title', ''), 'artist': ', '.join(a.get('name','') for a in data.get('artists',[]) if isinstance(a,dict)), 'year': data.get('year',''), 'art': artwork(data), 'items': clean(data.get('tracks', []), 'song', data)}
    if op == 'playlist':
        data = api.get_playlist(req['id'], limit=min(int(req.get('limit', 100)), 5000))
        return {'title': data.get('title', ''), 'art': artwork(data), 'items': clean(data.get('tracks', []), 'song', data), 'total': data.get('trackCount', 0)}
    if op == 'artist':
        data = api.get_artist(req['id'])
        sections = []
        for key, title, kind in [('songs','Songs','song'),('albums','Albums','album'),('singles','Singles','album'),('videos','Videos','video'),('related','Related artists','artist')]:
            section = data.get(key) or {}
            items = section.get('results', []) if isinstance(section, dict) else section
            if items:
                sections.append({'title':title,'items':clean(items,kind)})
        return {'title':data.get('name',''), 'art':artwork(data), 'sections':sections}
    if op == 'radio':
        data = api.get_watch_playlist(videoId=req['id'], radio=True, limit=30)
        return {'items': clean(data.get('tracks', []), 'song')}
    if op == 'lyrics':
        data = api.get_watch_playlist(videoId=req['id'], limit=1)
        if not data.get('lyrics'): return {'lyrics': '', 'lines': []}
        try: result = api.get_lyrics(data['lyrics'], timestamps=True)
        except Exception: result = api.get_lyrics(data['lyrics'])
        return normalize_lyrics(result)
    if op == 'link':
        parsed = urlparse(req['url'])
        if parsed.hostname not in ('youtube.com','www.youtube.com','music.youtube.com','m.youtube.com','youtu.be'):
            raise ValueError('Paste a YouTube or YouTube Music link')
        query = parse_qs(parsed.query)
        video = (query.get('v') or [''])[0]
        if parsed.hostname == 'youtu.be':
            video = parsed.path.strip('/')
        if video:
            data = api.get_song(video).get('videoDetails', {})
            if not data:
                raise ValueError('This song is unavailable')
            t = normalize({'videoId':video,'title':data.get('title'), 'artists':[{'name':data.get('author',''),'id':data.get('channelId','')}], 'thumbnails':data.get('thumbnail',{}).get('thumbnails',[]), 'duration_seconds':int(data.get('lengthSeconds') or 0)}, 'song')
            return {'title':t['title'],'items':[t]}
        playlist = (query.get('list') or [''])[0]
        if playlist:
            return run({'op':'playlist','id':playlist,'limit':100})
        raise ValueError('This link has no song or playlist')
    raise ValueError('Unknown request')


if __name__ == '__main__':
    try:
        payload = sys.stdin.read(65537)
        if len(payload) > 65536: raise ValueError('Request is too large')
        print(json.dumps({'ok': True, **run(json.loads(payload))}, ensure_ascii=False))
    except Exception as exc:
        # Keep provider diagnostics on stderr; the application displays a short,
        # actionable message without stream URLs or server response bodies.
        print(str(exc)[-1800:], file=sys.stderr)
        print(json.dumps({'ok': False, 'error': 'YouTube could not complete this request.'}))
        sys.exit(1)
