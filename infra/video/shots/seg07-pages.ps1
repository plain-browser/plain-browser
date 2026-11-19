BrowserStart -url 'http://kurier.test/news.html'; Scene 'Okno'; MoveTo 960 600 100; Start-Sleep 2
RecStart; Start-Sleep 1
Go 'chrome://adblock' 70; Start-Sleep 9
Go 'chrome://settings/privacy' 70; Start-Sleep 5
MoveTo 960 700 400; for ($i = 0; $i -lt 6; $i++) { [W]::mouse_event(0x800, 0, 0, -240, 0); Start-Sleep -Milliseconds 700 }
Start-Sleep 4
RecStop 'seg07-pages'
