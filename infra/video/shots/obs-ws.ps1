# Usage: obs-ws.ps1 <requestType> [<json requestData>] [<out file for imageData>]
param([string]$Type, [string]$Data = '{}', [string]$Out = '')
$ws = New-Object System.Net.WebSockets.ClientWebSocket
$ct = [Threading.CancellationToken]::None
$ws.ConnectAsync([Uri]'ws://127.0.0.1:4455', $ct).Wait()
function Recv { $ms = New-Object IO.MemoryStream; $buf = New-Object byte[] 1048576
  do { $r = $ws.ReceiveAsync((New-Object ArraySegment[byte] -ArgumentList (,$buf)), $ct).Result; $ms.Write($buf, 0, $r.Count) } while (-not $r.EndOfMessage)
  [Text.Encoding]::UTF8.GetString($ms.ToArray()) }
function Send($s) { $b = [Text.Encoding]::UTF8.GetBytes($s); $ws.SendAsync((New-Object ArraySegment[byte] -ArgumentList (,$b)), 'Text', $true, $ct).Wait() }
Recv | Out-Null
Send '{"op":1,"d":{"rpcVersion":1,"eventSubscriptions":0}}'; Recv | Out-Null
Send ('{"op":6,"d":{"requestType":"' + $Type + '","requestId":"r1","requestData":' + $Data + '}}')
$resp = Recv
$ws.CloseAsync('NormalClosure', '', $ct).Wait()
if ($Out) { $j = $resp | ConvertFrom-Json; $img = $j.d.responseData.imageData; [IO.File]::WriteAllBytes($Out, [Convert]::FromBase64String($img.Substring($img.IndexOf(',') + 1))); "image -> $Out" }
else { $resp }
