#!/usr/bin/env bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
pushd "${SCRIPT_DIR}"
COLOR_RED="\033[31m"
COLOR_GREEN="\033[32m"
COLOR_NONE="\033[00m"

SC_PACKAGE_NAME="nanosvg"
SC_PACKAGE_VERSION="9da543e"
SC_PACKAGE_FILE_NAME="${SC_PACKAGE_NAME}_${SC_PACKAGE_VERSION}"
SC_PACKAGE_URL="https://github.com/memononen/nanosvg"
SC_PACKAGE_LOCAL_TXT=${SC_PACKAGE_NAME}.txt
SC_PACKAGE_LOCAL_DIR="${SCRIPT_DIR}/_${SC_PACKAGE_NAME}"

if [ -f ${SC_PACKAGE_LOCAL_TXT} ]; then
    SC_PACKAGE_INSTALLED=true
else
    SC_PACKAGE_INSTALLED=false
fi

if [ ${SC_PACKAGE_INSTALLED} = true ]; then
    echo -e "Package ${COLOR_GREEN}'${SC_PACKAGE_NAME}'${COLOR_NONE} is installed in version '${SC_PACKAGE_VERSION}'"
else
    echo -e "Package  ${COLOR_GREEN}'${SC_PACKAGE_NAME}'${COLOR_NONE} is missing, installing it in version '${SC_PACKAGE_VERSION}'"
    rm -rf "${SC_PACKAGE_LOCAL_DIR}" || { echo -e "${COLOR_RED}FATAL ERROR: Cannot remove '${SC_PACKAGE_LOCAL_DIR}'" ; exit 1; }
    mkdir -p "${SC_PACKAGE_LOCAL_DIR}" || { echo -e "${COLOR_RED}FATAL ERROR: Cannot create '${SC_PACKAGE_LOCAL_DIR}'" ; exit 1; }
    git clone "${SC_PACKAGE_URL}" "${SC_PACKAGE_LOCAL_DIR}" || { echo -e "${COLOR_RED}FATAL ERROR: Cannot clone package ${SC_PACKAGE_URL} to ${SC_PACKAGE_LOCAL_DIR} at ${SC_PACKAGE_VERSION}" ; exit 1; }
    pushd "${SC_PACKAGE_LOCAL_DIR}"
    git checkout -b sc ${SC_PACKAGE_VERSION} || { echo -e "${COLOR_RED}FATAL ERROR: Cannot switch git repo ${SC_PACKAGE_URL} at ${SC_PACKAGE_LOCAL_DIR} to ${SC_PACKAGE_VERSION}" ; exit 1; }
    popd

    printf "SC_PACKAGE_NAME=${SC_PACKAGE_NAME}\nSC_PACKAGE_VERSION=${SC_PACKAGE_VERSION}\nSC_PACKAGE_URL=${SC_PACKAGE_URL}" > "${SC_PACKAGE_LOCAL_TXT}"
fi

popd