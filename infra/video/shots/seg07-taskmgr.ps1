BrowserStart -url 'http://kurier.test/news.html'; Scene 'Ekran'; MoveTo 960 600 100; Start-Sleep 2
RecStart; Start-Sleep 2
Keys '+{ESC}'; Start-Sleep 9
RecStop 'seg07-taskmgr'
Keys '%{F4}'
