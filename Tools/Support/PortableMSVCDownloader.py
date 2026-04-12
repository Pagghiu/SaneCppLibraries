#!/usr/bin/env python3

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path


MANIFEST_URL = "https://aka.ms/vs/17/release/channel"
WINDOWS_SDK_MSI_NAMES = (
    "Windows SDK for Windows Store Apps Tools-x86_en-us.msi",
    "Windows SDK for Windows Store Apps Headers-x86_en-us.msi",
    "Windows SDK Desktop Headers x86-x86_en-us.msi",
    "Windows SDK for Windows Store Apps Libs-x86_en-us.msi",
    "Windows SDK Desktop Libs x64-x86_en-us.msi",
    "Universal CRT Headers Libraries and Sources-x86_en-us.msi",
)


def download_json(url: str):
    with urllib.request.urlopen(url) as response:
        return json.loads(response.read().decode("utf-8"))


def first(items, predicate):
    for item in items:
        if predicate(item):
            return item
    raise KeyError("Requested item not found")


def cache_download(cache_dir: Path, url: str, sha256: str, label: str):
    cache_dir.mkdir(parents=True, exist_ok=True)
    base_name = Path(urllib.parse.urlparse(url).path).name or label.replace("/", "_")
    cache_path = cache_dir / f"{sha256[:16]}-{base_name}"
    if cache_path.is_file():
        return cache_path

    with urllib.request.urlopen(url) as response, open(cache_path, "wb") as output:
        digest = hashlib.sha256()
        while True:
            block = response.read(1 << 20)
            if not block:
                break
            output.write(block)
            digest.update(block)
    actual = digest.hexdigest().lower()
    if actual != sha256.lower():
        cache_path.unlink(missing_ok=True)
        raise RuntimeError(f"Hash mismatch for {label}")
    return cache_path


def get_msi_cabs(msi_path: Path):
    data = msi_path.read_bytes()
    index = 0
    result = []
    while True:
        index = data.find(b".cab", index + 4)
        if index < 0:
            return result
        start = max(0, index - 32)
        name = data[start : index + 4].decode("ascii", errors="ignore").strip("\0")
        if name:
            result.append(name)


def posix_to_windows(path: Path):
    resolved = str(path.resolve())
    return "Z:" + resolved.replace("/", "\\")


def build_payload_map(vs_manifest):
    packages = {}
    for package in vs_manifest["packages"]:
        packages.setdefault(package["id"].lower(), []).append(package)
    return packages


def resolve_versions(channel_manifest, packages, requested_msvc, requested_sdk):
    msvc_versions = {}
    sdk_versions = {}
    for package_id in packages:
        if package_id.startswith("microsoft.visualstudio.component.vc.") and package_id.endswith(".x86.x64"):
            parts = package_id.split(".")
            version_key = ".".join(parts[4:6])
            if version_key and version_key[0].isdigit():
                msvc_versions[version_key] = package_id
        if package_id.startswith("microsoft.visualstudio.component.windows10sdk.") or package_id.startswith(
            "microsoft.visualstudio.component.windows11sdk."
        ):
            version_key = package_id.split(".")[-1]
            if version_key.isdigit():
                sdk_versions[version_key] = package_id

    if requested_msvc:
        if requested_msvc not in msvc_versions:
            raise RuntimeError(f"Unknown MSVC version: {requested_msvc}")
        msvc_package_id = msvc_versions[requested_msvc]
    else:
        msvc_package_id = msvc_versions[max(msvc_versions)]

    if requested_sdk:
        if requested_sdk not in sdk_versions:
            raise RuntimeError(f"Unknown Windows SDK version: {requested_sdk}")
        sdk_package_id = sdk_versions[requested_sdk]
    else:
        sdk_package_id = sdk_versions[max(sdk_versions)]

    msvc_version = ".".join(msvc_package_id.split(".")[4:-2])
    sdk_version = sdk_package_id.split(".")[-1]
    build_tools = first(
        channel_manifest["channelItems"], lambda item: item["id"] == "Microsoft.VisualStudio.Product.BuildTools"
    )
    resource = first(build_tools["localizedResources"], lambda item: item["language"] == "en-us")
    return msvc_version, sdk_version, msvc_package_id, sdk_package_id, resource["license"]


def extract_msvc(packages, cache_dir: Path, destination: Path, msvc_version: str):
    package_names = (
        f"microsoft.vc.{msvc_version}.tools.hostx64.targetx64.base",
        f"microsoft.vc.{msvc_version}.tools.hostx64.targetx64.res.base",
        f"microsoft.vc.{msvc_version}.crt.headers.base",
        f"microsoft.vc.{msvc_version}.crt.x64.desktop.base",
        f"microsoft.vc.{msvc_version}.crt.x64.store.base",
        f"microsoft.vc.{msvc_version}.crt.source.base",
        f"microsoft.vc.{msvc_version}.asan.headers.base",
        f"microsoft.vc.{msvc_version}.asan.x64.base",
    )

    for package_name in package_names:
        package = first(packages[package_name], lambda item: item.get("language") in (None, "en-US"))
        for payload in package["payloads"]:
            archive_path = cache_download(cache_dir, payload["url"], payload["sha256"], package_name)
            with zipfile.ZipFile(archive_path) as archive:
                for member in archive.namelist():
                    if not member.startswith("Contents/"):
                        continue
                    output_path = destination / Path(member).relative_to("Contents")
                    output_path.parent.mkdir(parents=True, exist_ok=True)
                    output_path.write_bytes(archive.read(member))


def extract_sdk(packages, cache_dir: Path, destination: Path, sdk_package_id: str, wine: str, wine_prefix: Path):
    sdk_package = packages[sdk_package_id][0]
    sdk_package = packages[first(sdk_package["dependencies"], lambda _: True).lower()][0]

    with tempfile.TemporaryDirectory() as temporary_directory:
        temporary_path = Path(temporary_directory)
        msi_paths = []
        cab_names = []
        for msi_name in WINDOWS_SDK_MSI_NAMES:
            payload = first(sdk_package["payloads"], lambda item: item["fileName"] == f"Installers\\{msi_name}")
            msi_path = temporary_path / msi_name
            shutil.copyfile(cache_download(cache_dir, payload["url"], payload["sha256"], msi_name), msi_path)
            msi_paths.append(msi_path)
            cab_names.extend(get_msi_cabs(msi_path))

        unique_cabs = sorted(set(cab_names))
        for cab_name in unique_cabs:
            payload = first(sdk_package["payloads"], lambda item: item["fileName"] == f"Installers\\{cab_name}")
            shutil.copyfile(cache_download(cache_dir, payload["url"], payload["sha256"], cab_name), temporary_path / cab_name)

        environment = os.environ.copy()
        environment["WINEPREFIX"] = str(wine_prefix)
        environment["WINEARCH"] = "win64"
        environment["WINEDLLOVERRIDES"] = "winemenubuilder.exe=d;winedbg.exe=d;vctip.exe=d"
        environment["WINEDEBUG"] = "-all"
        environment["MVK_CONFIG_LOG_LEVEL"] = "0"
        prepare_prefix_headless(wine, wine_prefix, environment)
        destination.mkdir(parents=True, exist_ok=True)
        destination_win = posix_to_windows(destination)
        for msi_path in msi_paths:
            subprocess.check_call(
                [wine, "msiexec", "/a", posix_to_windows(msi_path), "/quiet", "/qn", f"TARGETDIR={destination_win}"],
                env=environment,
                cwd=temporary_directory,
            )


def cleanup_layout(destination: Path):
    for path in (
        destination / "Common7",
        destination / "VC" / "Auxiliary",
    ):
        shutil.rmtree(path, ignore_errors=True)

    msvc_roots = sorted((destination / "VC" / "Tools" / "MSVC").glob("*"))
    if msvc_roots:
        msvc_root = msvc_roots[0]
        for subdirectory in (
            msvc_root / "lib" / "x64" / "store",
            msvc_root / "lib" / "x64" / "uwp",
        ):
            shutil.rmtree(subdirectory, ignore_errors=True)

    sdk_bins = sorted((destination / "Windows Kits" / "10" / "bin").glob("*"))
    if sdk_bins:
        sdk_root = sdk_bins[0]
        for path in (
            destination / "Windows Kits" / "10" / "Catalogs",
            destination / "Windows Kits" / "10" / "DesignTime",
            destination / "Windows Kits" / "10" / "bin" / sdk_root.name / "chpe",
            destination / "Windows Kits" / "10" / "Lib" / sdk_root.name / "ucrt_enclave",
        ):
            shutil.rmtree(path, ignore_errors=True)


def resolve_layout_versions(destination: Path):
    msvc_roots = sorted((destination / "VC" / "Tools" / "MSVC").glob("*"))
    if not msvc_roots:
        raise RuntimeError("Extracted MSVC toolset directory is missing")

    sdk_roots = sorted((destination / "Windows Kits" / "10" / "bin").glob("*"))
    if not sdk_roots:
        raise RuntimeError("Extracted Windows SDK directory is missing")

    return msvc_roots[0].name, sdk_roots[0].name


def prepare_prefix_headless(wine: str, wine_prefix: Path, environment):
    stamp_path = wine_prefix / ".sc-headless-ready"
    if stamp_path.exists():
        return

    commands = (
        ("reg", "add", r"HKEY_CURRENT_USER\Software\Wine\WineDbg", "/v", "ShowCrashDialog", "/t", "REG_DWORD", "/d", "0", "/f"),
        ("reg", "add", r"HKEY_CURRENT_USER\Software\Wine\WineDbg", "/v", "BreakOnFirstChance", "/t", "REG_DWORD", "/d", "0", "/f"),
        (
            "reg",
            "delete",
            r"HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\RunServices",
            "/v",
            "winemenubuilder",
            "/f",
        ),
        (
            "reg",
            "delete",
            r"HKEY_LOCAL_MACHINE\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\RunServices",
            "/v",
            "winemenubuilder",
            "/f",
        ),
    )
    for command in commands:
        completed = subprocess.run(
            [wine, *command], env=environment, check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        )
        if completed.returncode not in (0, 1):
            raise RuntimeError(f"Wine headless prefix preparation failed for: {' '.join(command)}")
    stamp_path.write_text("ready\n", encoding="utf-8")


def write_metadata(destination: Path, msvc_version: str, sdk_version: str, license_url: str, wine: str):
    metadata = {
        "toolchain": "msvc",
        "host": "x64",
        "target": "x64",
        "msvcVersion": msvc_version,
        "sdkVersion": sdk_version,
        "license": license_url,
        "wine": wine,
    }
    (destination / "sc-msvc-package.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dest", required=True)
    parser.add_argument("--cache-dir", required=True)
    parser.add_argument("--wine", required=True)
    parser.add_argument("--wine-prefix", required=True)
    parser.add_argument("--accept-license", action="store_true")
    parser.add_argument("--msvc-version")
    parser.add_argument("--sdk-version")
    args = parser.parse_args()

    destination = Path(args.dest).resolve()
    cache_dir = Path(args.cache_dir).resolve()
    wine_prefix = Path(args.wine_prefix).resolve()

    channel_manifest = download_json(MANIFEST_URL)
    visual_studio_manifest = first(
        channel_manifest["channelItems"], lambda item: item["id"] == "Microsoft.VisualStudio.Manifests.VisualStudio"
    )
    vs_manifest = download_json(visual_studio_manifest["payloads"][0]["url"])
    packages = build_payload_map(vs_manifest)

    msvc_version, sdk_version, _, sdk_package_id, license_url = resolve_versions(
        channel_manifest, packages, args.msvc_version, args.sdk_version
    )

    if not args.accept_license:
        print(f"Visual Studio license must be accepted before download: {license_url}", file=sys.stderr)
        return 1

    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True, exist_ok=True)
    wine_prefix.mkdir(parents=True, exist_ok=True)

    extract_msvc(packages, cache_dir, destination, msvc_version)
    extract_sdk(packages, cache_dir, destination, sdk_package_id, args.wine, wine_prefix)
    cleanup_layout(destination)
    layout_msvc_version, layout_sdk_version = resolve_layout_versions(destination)
    write_metadata(destination, layout_msvc_version, layout_sdk_version, license_url, args.wine)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
