#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PATH=$PATH:/opt/homebrew/bin/
ROOT_DIR="${SCRIPT_DIR}/../../../.."
cd "${ROOT_DIR}"

source _Build/HostTools/emsdk/emsdk_env.sh

POSITIONAL_ARGS=()

EMSC_DEBUG_FLAG=-g
EMSC_CONFIGURATION=Debug
EMSC_DIRECTORY=emsc-generic

while [[ $# -gt 0 ]]; do
  case $1 in
    -r|--release)
      EMSC_DEBUG_FLAG="-O2"
      EMSC_CONFIGURATION="Release"
      shift # past argument
      ;;
    -d|--debug)
      EMSC_DEBUG_FLAG="-g"
      EMSC_CONFIGURATION="Debug"
      shift # past argument
      ;;
    -*|--*)
      echo "Unknown option $1"
      exit 1
      ;;
    *)
      POSITIONAL_ARGS+=("$1") # save positional arg
      EMSC_DIRECTORY="$1"
      shift # past argument
      ;;
  esac
done

set -- "${POSITIONAL_ARGS[@]}" # restore positional parameters

PROJECT_NAME="SCExample"

INCLUDE_DIRS="\
-I${SCRIPT_DIR}/../../ \
-I${SCRIPT_DIR}/../../../../Dependencies/freetype/_freetype/include \
-I${SCRIPT_DIR}/../../../../Dependencies/imgui/_imgui"
EMSC_LINKER_OPTIONS="--memory-init-file 0 -s STACK_SIZE=512KB -s ERROR_ON_UNDEFINED_SYMBOLS=1 -s NO_EXIT_RUNTIME=1 -s ALLOW_MEMORY_GROWTH=1 -s USE_WEBGL2=1 -s ""MALLOC='emmalloc'"" -s NO_FILESYSTEM=1 -s WASM=1 -s USE_GLFW=3"
EMSC_COMPILE_OPTIONS="-s DISABLE_EXCEPTION_CATCHING=1 ${EMSC_DEBUG_FLAG}"
EMSC_CPP_OPTIONS="-std=c++20 -nostdinc++ -nostdlib++ -fno-rtti -fno-exceptions"
EMSC_C_OPTIONS=""
INTERMEDIATE_PATH="${ROOT_DIR}/_Build/Intermediate/${PROJECT_NAME}/${EMSC_DIRECTORY}-${EMSC_CONFIGURATION}"
OUTPUT_PATH="${ROOT_DIR}/_Build/Output/${EMSC_DIRECTORY}-${EMSC_CONFIGURATION}/${PROJECT_NAME}"
OUTPUT_FILE="${OUTPUT_PATH}/${PROJECT_NAME}.html"

# Create Directories
rm -rf "${OUTPUT_PATH}"
rm -rf "${INTERMEDIATE_PATH}"
mkdir -p "${OUTPUT_PATH}"
mkdir -p "${INTERMEDIATE_PATH}"

# Compile files
emcc -c ${EMSC_C_OPTIONS} ${INCLUDE_DIRS} ${EMSC_COMPILE_OPTIONS} "${ROOT_DIR}/Dependencies/freetype/DependencyFreetype.c" -o "${INTERMEDIATE_PATH}/DependencyFreetype.o" &
emcc -c ${EMSC_C_OPTIONS} ${INCLUDE_DIRS} ${EMSC_COMPILE_OPTIONS} "${ROOT_DIR}/Dependencies/nanosvg/DependencyNanosvg.c" -o "${INTERMEDIATE_PATH}/DependencyNanosvg.o" &
emcc -c ${EMSC_C_OPTIONS} ${INCLUDE_DIRS} ${EMSC_COMPILE_OPTIONS} "${ROOT_DIR}/Dependencies/fcft/DependencyFcft.c" -o "${INTERMEDIATE_PATH}/DependencyFcft.o" &
em++ -c ${EMSC_CPP_OPTIONS} ${INCLUDE_DIRS} ${EMSC_COMPILE_OPTIONS} "${ROOT_DIR}/Dependencies/imgui/DependencyImgui.cpp" -o "${INTERMEDIATE_PATH}/DependencyImgui.o" &
em++ -c ${EMSC_CPP_OPTIONS} ${INCLUDE_DIRS} ${EMSC_COMPILE_OPTIONS} "${ROOT_DIR}/Libraries/UserInterface/Platform.cpp" -o "${INTERMEDIATE_PATH}/Platform.o" &
em++ -c ${EMSC_CPP_OPTIONS} ${INCLUDE_DIRS} ${EMSC_COMPILE_OPTIONS} "${SCRIPT_DIR}/../../${PROJECT_NAME}.cpp" -o "${INTERMEDIATE_PATH}/${PROJECT_NAME}.o" &
wait

# Link into final product
em++ ${EMSC_COMPILE_OPTIONS} ${EMSC_LINKER_OPTIONS} -o "${OUTPUT_FILE}" \
--shell-file "${ROOT_DIR}/Libraries/UserInterface/Platform.html" \
"${INTERMEDIATE_PATH}/DependencyImgui.o" \
"${INTERMEDIATE_PATH}/Platform.o" \
"${INTERMEDIATE_PATH}/DependencyFreetype.o" \
"${INTERMEDIATE_PATH}/DependencyNanosvg.o" \
"${INTERMEDIATE_PATH}/DependencyFcft.o" \
"${INTERMEDIATE_PATH}/${PROJECT_NAME}.o"

mkdir -p "${OUTPUT_PATH}/Fonts"
cp "${ROOT_DIR}/Dependencies/imgui/_imgui/misc/fonts/DroidSans.ttf" "${OUTPUT_PATH}/Fonts"
cp "${ROOT_DIR}/Dependencies/notoemoji/_notocoloremojiregular/notocoloremojiregular_497a6598/NotoColorEmoji-Regular.ttf" "${OUTPUT_PATH}/Fonts"