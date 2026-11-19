BrowserStart -extra @('--load-extension=C:\PlainBrowser\ubo\uBlock0.chromium'); Scene 'Okno'; MoveTo 960 600 100; Start-Sleep 4
RecStart; Start-Sleep 1
Go 'chrome://extensions' 70; Start-Sleep 8
MoveTo 700 400 900; Start-Sleep 3
RecStop 'seg08-mv2'
