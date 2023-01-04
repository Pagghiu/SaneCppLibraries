#!/usr/bin/env bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
ROOT_DIR="${SCRIPT_DIR}/.."

pushd ${ROOT_DIR}
./Dependencies/imgui/DownloadImgui.sh
./Dependencies/sokol/DownloadSokol.sh
popd