Term; Scene 'Terminal'; RecStart; Start-Sleep 1
Cmd 'Get-ChildItem scripts\test-*.ps1 | Format-Wide Name -Column 2' 5
Cmd 'cls; Get-ChildItem .github\workflows | Format-Wide Name -Column 2' 4
Cmd 'Get-Content .github\workflows\upstream-watch.yml -TotalCount 30' 8
RecStop 'seg09-tests'
