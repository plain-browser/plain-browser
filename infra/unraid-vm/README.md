# VMs on the Unraid host

The host's address is not in the repository: the scripts that need it read `PB_UNRAID=user@host` (`infra/video/host/`), `PB_VIDEO_VM` and
`PB_BUILD_VM` (`scripts/vm-sync.sh`).

Two Windows 11 VMs, both installed unattended from the same multi-edition ISO
(`/mnt/ssd/isos/pl-pl_windows_11_25h2_26200.8117_multi_editions_*.iso`, virtio drivers from `virtio-win-0.1.285-1.iso`):

| libvirt name | Hostname | Edition | vCPU / RAM | Disk | Purpose |
|---|---|---|---|---|---|
| `draftway-build` | DW-BUILD | Pro | 12 / 9 GB | `/mnt/nvme/domains/draftway-build/vdisk1.img` (300 GB) | Chromium build, headless tests (`scripts/build.ps1`, `scripts/test-*.ps1`) |
| `plainbrowser-video` | DW-VIDEO | Enterprise | 4 / 6 GB | `/mnt/nvme/domains/plainbrowser-video/vdisk1.img` (120 GB, sparse) | Screen recordings for the YouTube video (`docs/VIDEO_PLAN.md`), created 2026-10-28 |

Both use the same user (`builder`), the same SSH key (`authorized_keys`) and the same first-boot script.

## How a VM is created

1. `autounattend.xml` with `@@PASSWORD@@` substituted. For DW-VIDEO three things differ from the committed file:
   `ComputerName` DW-VIDEO, the generic Enterprise installation key `NPPR9-FWDCX-D2C8J-H872K-2YT43` in place of
   the Pro one, and an `InstallFrom` block selecting the image by name:
   `<InstallFrom><MetaData wcm:action="add"><Key>/IMAGE/NAME</Key><Value>Windows 11 Enterprise</Value></MetaData></InstallFrom>`
   (the ISO's `install.wim` index 6; list them with `dism /Get-WimInfo /WimFile:D:\sources\install.wim`).
2. On the host, a 64 MB FAT image carries the three files and is attached as a removable USB disk:
   ```
   dd if=/dev/zero of=$D/unattend.img bs=1M count=64 && mkfs.vfat -n UNATTEND $D/unattend.img
   mcopy -i $D/unattend.img autounattend.xml firstboot.ps1 authorized_keys ::
   qemu-img create -f raw $D/vdisk1.img 120G
   cp /usr/share/qemu/ovmf-x64/OVMF_VARS-pure-efi-tpm.fd /etc/libvirt/qemu/nvram/<uuid>_VARS-pure-efi-tpm.fd
   virsh define <domain>.xml && virsh start <name>
   ```
   The domain XML is the build VM's (`virsh dumpxml draftway-build`) with a new name, uuid, MAC, disk paths,
   vCPU/memory and `autoport` VNC.
3. The ISO prints "Press any key to boot from CD or DVD" and OVMF gives up if nothing is pressed. With no one at
   the console: `virsh reset <name>` and then `virsh send-key <name> KEY_ENTER` once a second for about 45 s.
4. Setup runs unattended (about 20 min), `firstboot.ps1` installs guest tools and sshd. The VM's IP:
   `ip neigh | grep <mac>` on the host. Screenshots of the console: `virsh screenshot <name> /tmp/x.png`.
5. Activation is manual (`cscript //nologo C:\Windows\System32\slmgr.vbs /ipk <key>` then `/ato` over SSH; the
   default shell is PowerShell 5, so separate commands with `;`, not `&&`). The generic keys above select the
   edition only and leave Windows unactivated.
