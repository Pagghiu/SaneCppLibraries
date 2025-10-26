#!/bin/bash
LIBRARY_DIR="$1"        # Directory where "Libraries" exists
TOOL_SOURCE_DIR="$2"    # Directory where SC-${TOOL}.cpp file exists
BUILD_DIR="$3"          # Directory where output subdirectories must be placed
TOOL_NAME="${4:-build}" # Tool to execute (build by default)

TOOL_OUTPUT_DIR="${BUILD_DIR}/_Tools"   # Directory where the ${TOOL} executable will be generated

TOOL=SC-${TOOL_NAME}
if [ ! -e "${TOOL_SOURCE_DIR}/${TOOL}.cpp" ]; then # Try looking in the TOOL_SOURCE_DIR
    if [ ! -e "${TOOL_NAME}" ]; then
        { echo "Error: Tool \"${TOOL_NAME}\" doesn't exist" ; exit 1; }
    fi
    TOOL_NAME=$(readlink -f "${TOOL_NAME}")
    filename=$(basename -- "$TOOL_NAME")
    TOOL="${filename%.*}" # just name without extension
    TOOL_SOURCE_DIR=$(dirname -- "$TOOL_NAME")
    extension="${filename##*.}"
    if [ "$extension" != "cpp" ]; then
        { echo "Error: ${extension} Tool \"${TOOL_NAME}\" doesn't end with .cpp" ; exit 1; }
    fi
fi

call_make () {
    make $1 -s -j --no-print-directory -C "${LIBRARY_DIR}/Tools/Build/Posix" CONFIG=Debug "TOOL=${TOOL}" "TOOL_SOURCE_DIR=${TOOL_SOURCE_DIR}" "TOOL_OUTPUT_DIR=${TOOL_OUTPUT_DIR}"
}

execute_tool () {
    echo "Starting ${TOOL}"
    # Construct the command with properly quoted arguments
    cmd="\"$TOOL_OUTPUT_DIR/${OS}/${TOOL}\""
    for arg in "$@"; do
        if [[ "$arg" =~ \  ]]; then
            cmd+=" \"$arg\""
        else
            cmd+=" $arg"
        fi
    done
    # Execute the constructed command
    eval "$cmd"
}

OS=$(uname -s)

# Cross-platform time measurement function
get_time_ms() {
    if [[ "$OS" == "Darwin" ]]; then
        # macOS doesn't support %N, use Perl (pre-installed) for millisecond precision
        perl -MTime::HiRes=time -e 'printf "%.0f\n", time * 1000'
    else
        # Linux supports nanoseconds
        echo $(($(date +%s%N) / 1000000))
    fi
}

# Time the initial build
start_time=$(get_time_ms)
call_make build
initial_exit_code=$?
end_time=$(get_time_ms)
build_time=$(( (end_time - start_time) / 1000 ))
build_time_frac=$(( (end_time - start_time) % 1000 ))
printf "Time to compile \"${TOOL_NAME}\" tool: %d.%03d seconds\n" $build_time $build_time_frac

if [ $initial_exit_code -eq 0 ]; then
    execute_tool "$@"
else
    # It could have failed because of moved files, let's re-try after cleaning
    call_make clean
    # Time the clean and rebuild
    start_time=$(get_time_ms)
    call_make build
    rebuild_exit_code=$?
    end_time=$(get_time_ms)
    rebuild_time=$(( (end_time - start_time) / 1000 ))
    rebuild_time_frac=$(( (end_time - start_time) % 1000 ))
    printf "Time to re-compile \"${TOOL_NAME}\" tool after clean: %d.%03d seconds\n" $rebuild_time $rebuild_time_frac
    if [ $rebuild_exit_code -eq 0 ]; then
        execute_tool "$@"
    else
        exit $rebuild_exit_code
    fi
fi
