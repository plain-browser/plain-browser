"""Builds an MSIX package of Plain Browser from the portable zip (run on the Windows build VM, from anywhere).

  python scripts\\package-msix.py --zip <PlainBrowser_x.y.z_x64.zip> --out <dir> \\
      [--identity-name <Partner Center Package/Identity/Name>] [--publisher "CN=<Partner Center Package/Identity/Publisher>"] \\
      [--publisher-display "<Publisher display name>"] [--version 0.1.0.0] [--sign <pfx> --sign-password <pw>]

Without --identity-name/--publisher it builds a dev package (identity PlainBrowser.Dev, publisher CN=PlainBrowser Dev)
that only installs on a machine you control. For the Store, pass the three values Partner Center shows under
Product identity; the Store signs the package itself, so upload it unsigned. Needs MakeAppx.exe from the Windows SDK.
The last version part must be 0 for the Store."""
import argparse, glob, os, shutil, subprocess, sys, zipfile
from xml.sax.saxutils import escape

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(REPO, "branding", "icons", "msix")
HTML_TYPES = [".htm", ".html", ".xht", ".xhtml", ".shtml", ".mht", ".mhtml"]

MANIFEST = """<?xml version="1.0" encoding="utf-8"?>
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
         xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
         xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities"
         IgnorableNamespaces="uap rescap">
  <Identity Name="{name}" Publisher="{publisher}" Version="{version}" ProcessorArchitecture="x64"/>
  <Properties>
    <DisplayName>Plain Browser</DisplayName>
    <PublisherDisplayName>{publisher_display}</PublisherDisplayName>
    <Logo>Assets\\StoreLogo.png</Logo>
  </Properties>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.19041.0" MaxVersionTested="10.0.26100.0"/>
  </Dependencies>
  <Resources>
    <Resource Language="en-us"/>
    <Resource Language="pl-pl"/>
  </Resources>
  <Applications>
    <Application Id="PlainBrowser" Executable="chrome.exe" EntryPoint="Windows.FullTrustApplication">
      <uap:VisualElements DisplayName="Plain Browser"
          Description="Chromium-based browser with no Google services and built-in ad blocking"
          BackgroundColor="transparent" Square150x150Logo="Assets\\Square150x150Logo.png"
          Square44x44Logo="Assets\\Square44x44Logo.png">
        <uap:DefaultTile Wide310x150Logo="Assets\\Wide310x150Logo.png" Square71x71Logo="Assets\\Square71x71Logo.png"
            Square310x310Logo="Assets\\Square310x310Logo.png"/>
        <uap:SplashScreen Image="Assets\\SplashScreen.png"/>
      </uap:VisualElements>
      <Extensions>
        <uap:Extension Category="windows.protocol"><uap:Protocol Name="http"><uap:DisplayName>Plain Browser</uap:DisplayName></uap:Protocol></uap:Extension>
        <uap:Extension Category="windows.protocol"><uap:Protocol Name="https"><uap:DisplayName>Plain Browser</uap:DisplayName></uap:Protocol></uap:Extension>
        <uap:Extension Category="windows.fileTypeAssociation">
          <uap:FileTypeAssociation Name="plainbrowserhtml">
            <uap:DisplayName>Plain Browser HTML Document</uap:DisplayName>
            <uap:SupportedFileTypes>{html_types}</uap:SupportedFileTypes>
          </uap:FileTypeAssociation>
        </uap:Extension>
      </Extensions>
    </Application>
  </Applications>
  <Capabilities>
    <rescap:Capability Name="runFullTrust"/>
  </Capabilities>
</Package>
"""


def find_tool(name):
    hits = sorted(glob.glob(r"C:\Program Files (x86)\Windows Kits\10\bin\10.*\x64\%s" % name))
    if not hits:
        raise SystemExit("%s not found (install the Windows 10/11 SDK)" % name)
    return hits[-1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--zip", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--identity-name", default="PlainBrowser.Dev")
    ap.add_argument("--publisher", default="CN=PlainBrowser Dev")
    ap.add_argument("--publisher-display", default="Plain Browser contributors")
    ap.add_argument("--version", default="0.1.0.0")
    ap.add_argument("--sign")
    ap.add_argument("--sign-password", default="")
    a = ap.parse_args()
    if len(a.version.split(".")) != 4:
        raise SystemExit("--version must have four parts, e.g. 0.1.0.0")

    stage = os.path.join(a.out, "msix-stage")
    shutil.rmtree(stage, ignore_errors=True)
    os.makedirs(stage)
    with zipfile.ZipFile(a.zip) as z:
        z.extractall(stage)
    # The zip has a single top-level folder with chrome.exe in it; the package root must hold chrome.exe itself.
    if not os.path.exists(os.path.join(stage, "chrome.exe")):
        tops = [d for d in os.listdir(stage) if os.path.isfile(os.path.join(stage, d, "chrome.exe"))]
        if len(tops) != 1:
            raise SystemExit("chrome.exe not found in the zip")
        inner = os.path.join(stage, tops[0])
        for name in os.listdir(inner):
            shutil.move(os.path.join(inner, name), os.path.join(stage, name))
        os.rmdir(inner)
    if not os.path.isdir(ASSETS):
        raise SystemExit("logo assets missing: %s (run branding/make-icons.py)" % ASSETS)
    shutil.copytree(ASSETS, os.path.join(stage, "Assets"))
    with open(os.path.join(stage, "AppxManifest.xml"), "w", encoding="utf-8") as f:
        f.write(MANIFEST.format(name=escape(a.identity_name), publisher=escape(a.publisher, {'"': "&quot;"}),
                                version=a.version, publisher_display=escape(a.publisher_display),
                                html_types="".join("<uap:FileType>%s</uap:FileType>" % t for t in HTML_TYPES)))

    out = os.path.join(a.out, "PlainBrowser_%s_x64.msix" % a.version)
    if os.path.exists(out):
        os.remove(out)
    subprocess.check_call([find_tool("makeappx.exe"), "pack", "/d", stage, "/p", out, "/o"])
    if a.sign:
        subprocess.check_call([find_tool("signtool.exe"), "sign", "/fd", "SHA256", "/f", a.sign, "/p", a.sign_password, out])
    print("wrote", out, "(%d MB)" % (os.path.getsize(out) // 2**20), "signed" if a.sign else "unsigned")


if __name__ == "__main__":
    main()
