import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch,MagicMock
spec=importlib.util.spec_from_file_location('youtube',Path(__file__).parents[1]/'helper/youtube.py')
youtube=importlib.util.module_from_spec(spec);spec.loader.exec_module(youtube)
class CatalogTest(unittest.TestCase):
 def test_album_metadata(self):
  r=youtube.normalize({'videoId':'abcdefghijk','title':'Track','duration_seconds':30},'song',{'type':'album','title':'Album','browseId':'MPRE123','artists':[{'name':'Artist','id':'UC123'}]})
  self.assertEqual(r['albumId'],'MPRE123');self.assertEqual(r['artist'],'Artist');self.assertEqual(r['seconds'],30)
 def test_lyrics(self):
  self.assertEqual(youtube.normalize_lyrics({'lyrics':'Plain lyrics'})['lyrics'],'Plain lyrics')
  self.assertEqual(youtube.normalize_lyrics({'lyrics':[{'text':'Timed','start_time':1200,'end_time':2300}]})['lines'][0]['start'],1200)
 def test_bad_video(self):
  with patch.dict('sys.modules',{'yt_dlp':MagicMock()}):
   with self.assertRaises(ValueError):youtube.run({'op':'buffer','id':'../../file','directory':'/tmp'})
 def test_foreign_link(self):
  with patch.dict('sys.modules',{'ytmusicapi':MagicMock()}):
   with self.assertRaises(ValueError):youtube.run({'op':'link','url':'https://example.com/watch?v=abcdefghijk'})
 def test_anonymous(self):
  api=MagicMock();api.search.return_value=[];factory=MagicMock(return_value=api)
  with patch.dict('sys.modules',{'ytmusicapi':MagicMock(YTMusic=factory)}):youtube.run({'op':'search','query':'music','filter':'songs','limit':50})
  factory.assert_called_once_with(requests_session=True)
if __name__=='__main__':unittest.main()
