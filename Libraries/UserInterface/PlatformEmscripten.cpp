#include "PlatformApplication.h"
#include "PlatformResource.h"

#include "../../Dependencies/emscripten-browser-file/_emscripten-browser-file/emscripten_browser_file.h"
#include <string.h>

const char* SC::PlatformResourceLoader::lookupPathNative(char* buffer, int bufferLength, const char* directory,
                                                         const char* file)
{
    char executablePath[2048] = {0};
    if (executablePath[0] == 0)
    {
        snprintf(buffer, bufferLength, "%s/%s", directory, file);
    }
    snprintf(buffer, bufferLength, "%s/%s", directory, file);
    return buffer;
}

void SC::PlatformApplication::initNative() {}

void SC::PlatformApplication::openFiles()
{
    emscripten_browser_file::download("Salve.txt", "application/text/plain", "Hello world!\n",
                                      strlen("Hello world!\n"));
}

void handle_upload_file(char const* filename, char const* mime_type, char* buffer, size_t buffer_size, void*) {}

void SC::PlatformApplication::saveFiles()
{
    emscripten_browser_file::upload(".png,.jpg,.jpeg,.txt", handle_upload_file);
}
