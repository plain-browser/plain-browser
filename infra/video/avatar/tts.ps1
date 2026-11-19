# Placeholder narration through the OneCore voices (Windows.Media.SpeechSynthesis), one WAV per line of parts.txt
# ("name|text"). Prefers the Polish male voice Adam; the real video should use the author's own recording.
param([string]$Dir = 'C:\PlainBrowser\avatar', [string]$VoiceLike = '*Adam*')
Add-Type -AssemblyName System.Runtime.WindowsRuntime
$null = [Windows.Media.SpeechSynthesis.SpeechSynthesizer, Windows.Media.SpeechSynthesis, ContentType = WindowsRuntime]
$null = [Windows.Storage.Streams.DataReader, Windows.Storage.Streams, ContentType = WindowsRuntime]
$asTask = [System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' } | Select-Object -First 1
function Await($op, [Type]$t) { $task = $asTask.MakeGenericMethod($t).Invoke($null, @($op)); $task.Wait() | Out-Null; $task.Result }
$s = New-Object Windows.Media.SpeechSynthesis.SpeechSynthesizer
$v = [Windows.Media.SpeechSynthesis.SpeechSynthesizer]::AllVoices | Where-Object { $_.Language -eq 'pl-PL' -and $_.DisplayName -like $VoiceLike } | Select-Object -First 1
if ($v) { $s.Voice = $v }; "voice: " + $s.Voice.DisplayName
$s.Options.SpeakingRate = 0.95
foreach ($line in Get-Content (Join-Path $Dir 'parts.txt') -Encoding UTF8) {
  $n, $t = $line.Split('|', 2); if (-not $t) { continue }
  $stream = Await ($s.SynthesizeTextToStreamAsync($t)) ([Windows.Media.SpeechSynthesis.SpeechSynthesisStream])
  $reader = New-Object Windows.Storage.Streams.DataReader($stream.GetInputStreamAt(0))
  $size = [uint32]$stream.Size; Await ($reader.LoadAsync($size)) ([uint32]) | Out-Null
  $bytes = New-Object byte[] $size; $reader.ReadBytes($bytes)
  [IO.File]::WriteAllBytes((Join-Path $Dir "$n-raw.wav"), $bytes); "wrote $n-raw.wav ($size bytes)" }
