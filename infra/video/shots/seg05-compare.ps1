BrowserStart; Scene 'Okno'; MoveTo 960 600 100
RecStart; Start-Sleep 1
Go 'github.com/chromium/chromium/compare/156.0.8078.47...156.0.8078.73' 50
Start-Sleep 14; Keys '{PGDN}'; Start-Sleep 4
RecStop 'seg05-compare'
