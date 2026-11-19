BrowserStart; Scene 'Okno'; MoveTo 960 600 100
RecStart; Start-Sleep 1
Go 'kurier.test/artykul.html?utm_source=newsletter&utm_campaign=jesien&fbclid=IwAR0xDemo' 60
Start-Sleep 7
Go 'l.facebook.com/l.php?u=http%3A%2F%2Fkurier.test%2Fartykul.html' 60
Start-Sleep 7
RecStop 'seg08-utm'
