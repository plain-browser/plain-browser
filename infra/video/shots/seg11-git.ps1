Term; Scene 'Terminal'; RecStart; Start-Sleep 1
Cmd 'git log --oneline -14' 6
Cmd 'git show d9c9cda --stat --format="%h %s%n%n%b" | Select-Object -First 16' 8
RecStop 'seg11-git'
