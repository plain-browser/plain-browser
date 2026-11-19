Term; Scene 'Terminal'; RecStart; Start-Sleep 1
Cmd 'python scripts\check-patches.py' 2
$i = 0; while ($i -lt 240 -and (Get-CimInstance Win32_Process -Filter "Name = 'python.exe'" | Where-Object { $_.CommandLine -like '*check-patches*' })) { Start-Sleep 2; $i += 2 }
L "check-patches ran ~$i s"; Start-Sleep 5
RecStop 'seg04-check'
