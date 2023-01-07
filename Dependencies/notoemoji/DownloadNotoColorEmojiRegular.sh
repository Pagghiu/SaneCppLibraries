#!/usr/bin/env bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
pushd "${SCRIPT_DIR}"
COLOR_RED="\033[31m"
COLOR_GREEN="\033[32m"
COLOR_NONE="\033[00m"
# https://fonts.google.com/download?family=Noto%20Color%20Emoji
SC_PACKAGE_NAME="notocoloremojiregular"
SC_PACKAGE_VERSION="${SC_PACKAGE_NAME}_497a6598"
SC_PACKAGE_MD5="497a6598c3aec3438f77538134418d4c"
SC_PACKAGE_URL="https://fonts.google.com/download?family=Noto%20Color%20Emoji"
SC_PACKAGE_LOCAL_FILE="${SCRIPT_DIR}/_${SC_PACKAGE_NAME}/${SC_PACKAGE_VERSION}.zip"
SC_PACKAGE_LOCAL_TXT="${SCRIPT_DIR}/_${SC_PACKAGE_NAME}/${SC_PACKAGE_VERSION}.txt"
SC_PACKAGE_LOCAL_DIR="${SCRIPT_DIR}/_${SC_PACKAGE_NAME}/${SC_PACKAGE_VERSION}"
if [ -f ${SC_PACKAGE_LOCAL_TXT} ]; then
    SC_PACKAGE_INSTALLED=true
else
    SC_PACKAGE_INSTALLED=false
fi

if [ "$SC_PACKAGE_INSTALLED" = true ]; then
    echo -e "Package ${COLOR_GREEN}'${SC_PACKAGE_NAME}'${COLOR_NONE} is installed in version '${SC_PACKAGE_VERSION}'"
else
    echo -e "Package  ${COLOR_GREEN}'${SC_PACKAGE_NAME}'${COLOR_NONE} is missing, installing it in version '${SC_PACKAGE_VERSION}'"
    mkdir -p "${SC_PACKAGE_LOCAL_DIR}" || { echo -e "${COLOR_RED}FATAL ERROR: Cannot create directory ${SC_PACKAGE_LOCAL_DIR} ${COLOR_NONE}" ; exit 1; }
    SC_PACKAGE_DOWNLOADED_MD5=""
    if [ -e "$SC_PACKAGE_LOCAL_FILE" ]; then
        # md5="$(md5sum ${SC_PACKAGE_LOCAL_FILE} | cut -f1 -d' ')"
        SC_PACKAGE_DOWNLOADED_MD5="$(md5 ${SC_PACKAGE_LOCAL_FILE} | rev | cut -f1 -d' ' | rev)"
        echo "${SC_PACKAGE_NAME} SC_PACKAGE_DOWNLOADED_MD5='$SC_PACKAGE_DOWNLOADED_MD5'"
    fi
    if [ "$SC_PACKAGE_DOWNLOADED_MD5" != ${SC_PACKAGE_MD5} ] ; then
        rm -f ${SC_PACKAGE_LOCAL_FILE}
        curl -L -o "${SC_PACKAGE_LOCAL_FILE}" "${SC_PACKAGE_URL}" || { echo -e "${COLOR_RED}FATAL ERROR: Cannot download ${SC_PACKAGE_URL} to ${SC_PACKAGE_LOCAL_FILE} ${COLOR_NONE}" ; exit 1; }
        # md5="$(md5sum ${SC_PACKAGE_LOCAL_FILE} | cut -f1 -d' ')"
        SC_PACKAGE_DOWNLOADED_MD5="$(md5 ${SC_PACKAGE_LOCAL_FILE} | rev | cut -f1 -d' ' | rev)"
        
        if [ "${SC_PACKAGE_DOWNLOADED_MD5}" != ${SC_PACKAGE_MD5} ] ; then
            echo -e "${COLOR_RED}FATAL ERROR: Uncorract md5 for ${SC_PACKAGE_URL} to ${SC_PACKAGE_LOCAL_FILE} (${SC_PACKAGE_DOWNLOADED_MD5}) ${COLOR_NONE}"
            exit 1
        fi
    else
        echo "Skipped downloading ${SC_PACKAGE_NAME}"
    fi
    rm -rf "${SC_PACKAGE_LOCAL_DIR}"
    # tar -xvf "${SC_PACKAGE_LOCAL_FILE}" -C "${SC_PACKAGE_LOCAL_DIR}" --strip-components=1 || { echo -e "${COLOR_RED}FATAL ERROR: Cannot untar ${SC_PACKAGE_LOCAL_FILE} ${COLOR_NONE}" ; exit 1; }
    unzip "${SC_PACKAGE_LOCAL_FILE}" -d  "${SC_PACKAGE_LOCAL_DIR}"  || { echo -e "${COLOR_RED}FATAL ERROR: Cannot unzip ${SC_PACKAGE_LOCAL_FILE} ${COLOR_NONE}" ; exit 1; }
    if [ "$SC_CLEANUP_SOURCE_PACKAGE" = true ]; then
        rm -f ${SC_PACKAGE_LOCAL_FILE}
    fi
    xattr -r -d com.apple.quarantine "${SC_PACKAGE_LOCAL_DIR}"/* || { echo -e "${COLOR_RED}FATAL ERROR: Cannot remove quarantine attr on ${SC_PACKAGE_LOCAL_DIR} ${COLOR_NONE}" ; exit 1; }
    printf "SC_PACKAGE_URL=%s\nSC_PACKAGE_MD5=%s\n" "${SC_PACKAGE_URL}" "${SC_PACKAGE_MD5}" > "${SC_PACKAGE_LOCAL_TXT}"
fi
popd