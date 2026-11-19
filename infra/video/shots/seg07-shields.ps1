BrowserStart -url 'http://kurier.test/news.html'; Scene 'Ekran'; MoveTo 960 600 100; Start-Sleep 3
RecStart; Start-Sleep 3
ClickEl (FindLike 'Ochrona Plain Browser:*' 5); Start-Sleep 3
Click 1808 171; Start-Sleep 7
MoveTo 960 650 600; Start-Sleep 3
ClickEl (FindLike 'Ochrona Plain Browser:*' 5); Start-Sleep 3
Click 1808 171; Start-Sleep 7
MoveTo 960 650 600; Start-Sleep 2
RecStop 'seg07-shields'
