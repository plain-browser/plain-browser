BrowserStart; Scene 'Okno'; MoveTo 960 600 100
RecStart; Start-Sleep 1
Go 'kurier.test/news.html' 110
Start-Sleep 6
$s = FindLike 'Ochrona Plain Browser:*' 5; if ($s) { $r = $s.Current.BoundingRectangle; MoveTo ([int]($r.X + $r.Width/2)) ([int]($r.Y + $r.Height/2)) 1100 }
Start-Sleep 4; MoveTo 960 700 600
Go 'chrome://version' 80
Start-Sleep 6
RecStop 'seg01-hook'
