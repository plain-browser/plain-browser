Term; Scene 'Terminal'; RecStart; Start-Sleep 1
Cmd 'Get-Content versions.toml' 5
Cmd 'cls; Get-Content patches\series' 7
Cmd 'cls; Get-Content (Get-ChildItem patches\00-build\*.patch | Select-Object -First 1) -TotalCount 40' 8
Cmd 'cls; Get-Content patches\upstream-revert.list -TotalCount 30' 7
RecStop 'seg04-patches'
