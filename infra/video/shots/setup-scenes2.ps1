Obs 'CreateInput' '{"sceneName":"Ekran","inputName":"Cały ekran","inputKind":"monitor_capture","inputSettings":{"method":0,"capture_cursor":true}}' | Out-Null
Obs 'CreateInput' '{"sceneName":"Terminal","inputName":"Okno terminala","inputKind":"window_capture","inputSettings":{"window":"PS:CASCADIA_HOSTING_WINDOW_CLASS:WindowsTerminal.exe","priority":2,"method":0,"cursor":false}}' | Out-Null
$id = (Obs 'GetSceneItemId' '{"sceneName":"Terminal","sourceName":"Okno terminala"}' | ConvertFrom-Json).d.responseData.sceneItemId
Obs 'SetSceneItemTransform' ('{"sceneName":"Terminal","sceneItemId":' + $id + ',"sceneItemTransform":{"boundsType":"OBS_BOUNDS_SCALE_INNER","boundsWidth":1920,"boundsHeight":1080,"boundsAlignment":0,"positionX":0,"positionY":0,"alignment":5}}') | Out-Null
Scene 'Okno'
