Term; Scene 'Terminal'; RecStart; Start-Sleep 1
Cmd 'Get-ChildItem | Format-Wide Name -Column 4' 3
Cmd '"{0:N1} MB w repozytorium" -f ((Get-ChildItem -Recurse -File -Exclude *.git* | Where-Object FullName -notmatch "\\.git\\" | Measure-Object Length -Sum).Sum / 1MB)' 5
Start-Sleep 3; RecStop 'seg03-repo'
