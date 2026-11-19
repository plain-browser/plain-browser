BrowserStart; Scene 'Ekran'; MoveTo 960 600 100
RecStart; Start-Sleep 1
Go 'chrome://version' 70; Start-Sleep 6
$p = Start-Process explorer.exe -ArgumentList "$env:LOCALAPPDATA\PlainBrowser" -PassThru; Start-Sleep 4
$e = Get-Process explorer | Where-Object { $_.MainWindowTitle -like '*PlainBrowser*' } | Select-Object -First 1; if ($e) { [W]::ShowWindow($e.MainWindowHandle, 3) | Out-Null; Front $e.MainWindowHandle }
Start-Sleep 5; MoveTo 1204 1056 900; Start-Sleep 4
RecStop 'seg06-branding'
if ($e) { $e.CloseMainWindow() | Out-Null }
