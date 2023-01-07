#!/usr/bin/env bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
ROOT_DIR="${SCRIPT_DIR}/.."

pushd ${ROOT_DIR}
./Dependencies/imgui/DownloadImgui.sh
./Dependencies/sokol/DownloadSokol.sh
./Dependencies/freetype/DownloadFreetype.sh
./Dependencies/nanosvg/DownloadNanosvg.sh
./Dependencies/notoemoji/DownloadNotoColorEmojiRegular.sh.sh
./Dependencies/fcft/DownloadFcft.sh
popd