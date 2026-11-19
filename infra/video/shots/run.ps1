$name = (Get-Content C:\PlainBrowser\shots\current.txt -Raw).Trim()
. C:\PlainBrowser\shots\shotlib.ps1
L "=== $name"
try { . "C:\PlainBrowser\shots\$name.ps1" } catch { L "ERROR $_" }
L "=== end $name"
