#!/usr/bin/env python3

import json
import os
import subprocess
import sys
from pathlib import Path


PATH_PREFIX_FLAGS = ("/Fo", "/Fd", "/Fp", "/Fi", "/Fa", "/FR", "/I")
PATH_VALUE_FLAGS = ("/OUT:", "/LIBPATH:")
PATH_SUFFIXES = (".c", ".cc", ".cpp", ".cxx", ".m", ".mm", ".obj", ".lib", ".dll", ".exe", ".pch", ".pdb", ".res")


def looks_like_windows_path(value: str):
    return len(value) >= 3 and value[1] == ":" and value[2] in ("\\", "/")


def posix_to_windows(path_value: str):
    if not path_value:
        return path_value
    if looks_like_windows_path(path_value):
        return path_value.replace("/", "\\")
    path = Path(path_value)
    if not path.is_absolute():
        return path_value
    return "Z:" + str(path.resolve()).replace("/", "\\")


def strip_outer_quotes(value: str):
    if len(value) >= 2 and value[0] == value[-1] and value[0] in ("'", '"'):
        return value[1:-1]
    return value


def convert_response_file(response_path: Path):
    output_path = response_path.with_suffix(response_path.suffix + ".windows.rsp")
    lines = []
    for raw_line in response_path.read_text(encoding="utf-8").splitlines():
        value = strip_outer_quotes(raw_line.strip())
        if not value:
            continue
        lines.append(f'"{convert_argument(value)}"')
    output_path.write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")
    return output_path


def convert_argument(argument: str):
    for prefix in PATH_PREFIX_FLAGS:
        if argument[: len(prefix)].lower() == prefix.lower() and len(argument) > len(prefix):
            return argument[: len(prefix)] + posix_to_windows(argument[len(prefix) :])
    for prefix in PATH_VALUE_FLAGS:
        if argument[: len(prefix)].lower() == prefix.lower() and len(argument) > len(prefix):
            return argument[: len(prefix)] + posix_to_windows(argument[len(prefix) :])
    if argument.startswith("@") and len(argument) > 1:
        response_path = Path(argument[1:])
        if response_path.exists():
            return "@" + posix_to_windows(str(convert_response_file(response_path)))
    if argument.startswith("/") and not looks_like_windows_path(argument):
        if "/" not in argument[1:]:
            return argument
    if argument.startswith("/") and len(argument) > 2 and argument[1].isalpha() and argument[2] == ":":
        return argument
    if Path(argument).is_absolute():
        return posix_to_windows(argument)
    if "/" in argument and argument.endswith(PATH_SUFFIXES):
        return posix_to_windows(argument)
    return argument


def resolve_layout(root: Path):
    metadata = json.loads((root / "sc-msvc-package.json").read_text(encoding="utf-8"))
    msvc_version = metadata["msvcVersion"]
    sdk_version = metadata["sdkVersion"]
    wine = metadata["wine"]

    msvc_root = root / "VC" / "Tools" / "MSVC" / msvc_version
    sdk_root = root / "Windows Kits" / "10"
    return metadata, wine, msvc_root, sdk_root, sdk_version


def resolve_target_arch(script_path: Path):
    target_arch = script_path.parent.name.lower()
    if target_arch not in ("x64", "arm64"):
        raise RuntimeError(f"Unsupported MSVC wrapper target directory: {target_arch}")
    return target_arch


def resolve_sdk_tool_arch(target_arch: str):
    return "x64"


def prepare_prefix_headless(root: Path, wine: str, environment):
    stamp_path = root / ".wine-prefix" / ".sc-headless-ready"
    if stamp_path.exists():
        return

    commands = (
        (
            "reg",
            "add",
            r"HKEY_CURRENT_USER\Software\Wine\WineDbg",
            "/v",
            "ShowCrashDialog",
            "/t",
            "REG_DWORD",
            "/d",
            "0",
            "/f",
        ),
        (
            "reg",
            "add",
            r"HKEY_CURRENT_USER\Software\Wine\WineDbg",
            "/v",
            "BreakOnFirstChance",
            "/t",
            "REG_DWORD",
            "/d",
            "0",
            "/f",
        ),
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
        completed = subprocess.run([wine, *command], env=environment, check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if completed.returncode not in (0, 1):
            raise RuntimeError(f"Wine headless prefix preparation failed for: {' '.join(command)}")
    stamp_path.write_text("ready\n", encoding="utf-8")


def build_environment(root: Path, wine_prefix: Path, wine: str, msvc_root: Path, sdk_root: Path, sdk_version: str, target_arch: str):
    environment = os.environ.copy()
    environment["WINEPREFIX"] = str(wine_prefix)
    environment["WINEARCH"] = "win64"
    environment["WINEDLLOVERRIDES"] = "winemenubuilder.exe=d;winedbg.exe=d;vctip.exe=d"
    environment["WINEDEBUG"] = "-all"
    environment["MVK_CONFIG_LOG_LEVEL"] = "0"

    temp_directory = root / "tmp"
    temp_directory.mkdir(parents=True, exist_ok=True)
    temp_win = posix_to_windows(str(temp_directory))

    msvc_root_win = posix_to_windows(str(msvc_root))
    sdk_root_win = posix_to_windows(str(sdk_root))
    host_bin_win = f"{msvc_root_win}\\bin\\Hostx64\\{target_arch}"
    sdk_host_arch = resolve_sdk_tool_arch(target_arch)
    sdk_bin_root_win = f"{sdk_root_win}\\bin\\{sdk_version}"

    path_entries = [host_bin_win]
    sdk_tool_root = sdk_root / "bin" / sdk_version / sdk_host_arch
    if sdk_tool_root.is_dir():
        path_entries.append(f"{sdk_bin_root_win}\\{sdk_host_arch}")
    sdk_ucrt_root = sdk_tool_root / "ucrt"
    if sdk_ucrt_root.is_dir():
        path_entries.append(f"{sdk_bin_root_win}\\{sdk_host_arch}\\ucrt")
    path_entries.append("C:\\windows\\system32")

    environment["PATH"] = ";".join(path_entries)
    environment["INCLUDE"] = ";".join(
        (
            f"{msvc_root_win}\\include",
            f"{sdk_root_win}\\Include\\{sdk_version}\\ucrt",
            f"{sdk_root_win}\\Include\\{sdk_version}\\shared",
            f"{sdk_root_win}\\Include\\{sdk_version}\\um",
            f"{sdk_root_win}\\Include\\{sdk_version}\\winrt",
            f"{sdk_root_win}\\Include\\{sdk_version}\\cppwinrt",
        )
    )
    environment["LIB"] = ";".join(
        (
            f"{msvc_root_win}\\lib\\{target_arch}",
            f"{sdk_root_win}\\Lib\\{sdk_version}\\ucrt\\{target_arch}",
            f"{sdk_root_win}\\Lib\\{sdk_version}\\um\\{target_arch}",
        )
    )
    environment["TMP"] = temp_win
    environment["TEMP"] = temp_win
    return environment


def tool_executable(msvc_root: Path, target_arch: str, tool_name: str):
    return msvc_root / "bin" / "Hostx64" / target_arch / f"{tool_name}.exe"


def main():
    tool_name = Path(sys.argv[0]).name
    if tool_name.endswith(".py"):
        if len(sys.argv) < 2:
            print("Missing MSVC tool name", file=sys.stderr)
            return 1
        tool_name = sys.argv[1]
        arguments = sys.argv[2:]
    else:
        arguments = sys.argv[1:]

    script_path = Path(__file__).resolve()
    root = script_path.parents[2]
    wine_prefix = root / ".wine-prefix"
    wine_prefix.mkdir(parents=True, exist_ok=True)
    target_arch = resolve_target_arch(script_path)

    _, wine, msvc_root, sdk_root, sdk_version = resolve_layout(root)
    environment = build_environment(root, wine_prefix, wine, msvc_root, sdk_root, sdk_version, target_arch)
    prepare_prefix_headless(root, wine, environment)

    executable = posix_to_windows(str(tool_executable(msvc_root, target_arch, tool_name)))
    converted_arguments = [convert_argument(argument) for argument in arguments]
    return subprocess.call([wine, executable, *converted_arguments], env=environment)


if __name__ == "__main__":
    raise SystemExit(main())
