# First logon inside the build VM: guest tools, power, OpenSSH with the operator's key.
$log = 'C:\firstboot.log'
function L($m){ "$(Get-Date -Format s) $m" | Out-File $log -Append -Encoding utf8 }
L 'start'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
# virtio guest tools (QEMU guest agent + remaining drivers) from the virtio CD
$gt = Get-PSDrive -PSProvider FileSystem | ForEach-Object { Join-Path $_.Root 'virtio-win-guest-tools.exe' } | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($gt) { Start-Process $gt -ArgumentList '/S','/norestart' -Wait; L "guest tools: $gt" } else { L 'guest tools not found' }
powercfg /change standby-timeout-ac 0; powercfg /change monitor-timeout-ac 0; powercfg /hibernate off
# OpenSSH server
Add-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0 | Out-Null
Set-Service sshd -StartupType Automatic; Start-Service sshd
if (-not (Get-NetFirewallRule -Name 'OpenSSH-Server-In-TCP' -ErrorAction SilentlyContinue)) {
  New-NetFirewallRule -Name 'OpenSSH-Server-In-TCP' -DisplayName 'OpenSSH Server (sshd)' -Enabled True -Direction Inbound -Protocol TCP -Action Allow -LocalPort 22 | Out-Null }
# The OpenSSH capability's rule covers the Private profile only; the home LAN is trusted.
Get-NetConnectionProfile | Set-NetConnectionProfile -NetworkCategory Private
New-ItemProperty -Path 'HKLM:\SOFTWARE\OpenSSH' -Name DefaultShell -Value 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' -PropertyType String -Force | Out-Null
$ak = 'C:\ProgramData\ssh\administrators_authorized_keys'
Copy-Item (Join-Path $here 'authorized_keys') $ak -Force
icacls $ak /inheritance:r /grant 'Administrators:F' /grant 'SYSTEM:F' | Out-Null
L "sshd: $((Get-Service sshd).Status)"
L "SAC state: $((Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy').VerifiedAndReputablePolicyState)"
L 'DONE'
