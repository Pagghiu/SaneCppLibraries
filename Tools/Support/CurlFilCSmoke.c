// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include <curl/curl.h>
#include <dlfcn.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static size_t collect(char* data, size_t size, size_t count, void* context)
{
    size_t* bytes = (size_t*)context;
    *bytes += size * count;
    return size * count;
}

static int progress(void* context, curl_off_t a, curl_off_t b, curl_off_t c, curl_off_t d)
{
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    ++*(int*)context;
    return 0;
}

int main(void)
{
    void* library = dlopen("libcurl.so.4", RTLD_NOW);
    if (!library)
        return 2;
    CURL* (*init)(void)    = (CURL * (*)(void)) dlsym(library, "curl_easy_init");
    void (*cleanup)(CURL*) = (void (*)(CURL*))dlsym(library, "curl_easy_cleanup");
    CURLcode (*setopt)(CURL*, CURLoption, ...) =
        (CURLcode (*)(CURL*, CURLoption, ...))dlsym(library, "curl_easy_setopt");
    CURLcode (*getinfo)(CURL*, CURLINFO, ...) = (CURLcode (*)(CURL*, CURLINFO, ...))dlsym(library, "curl_easy_getinfo");
    CURLcode (*perform)(CURL*)                = (CURLcode (*)(CURL*))dlsym(library, "curl_easy_perform");
    curl_version_info_data* (*version)(CURLversion) =
        (curl_version_info_data * (*)(CURLversion)) dlsym(library, "curl_version_info");
    if (!init || !cleanup || !setopt || !getinfo || !perform || !version)
        return 3;
    curl_version_info_data* features         = version(CURLVERSION_FIRST);
    const int               requiredFeatures = CURL_VERSION_ASYNCHDNS | CURL_VERSION_SSL | CURL_VERSION_HTTP2;
    if (!features || strcmp(features->version, "8.22.0") || (features->features & requiredFeatures) != requiredFeatures)
        return 4;

    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0)
        return 5;
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(server, (struct sockaddr*)&address, sizeof(address)) || listen(server, 1))
        return 6;
    socklen_t length = sizeof(address);
    if (getsockname(server, (struct sockaddr*)&address, &length))
        return 7;
    pid_t child = fork();
    if (child < 0)
        return 8;
    if (child == 0)
    {
        alarm(10);
        int  client = accept(server, NULL, NULL);
        char request[4096];
        if (client < 0 || read(client, request, sizeof(request)) <= 0)
            _exit(9);
        const char reply[] = "HTTP/1.1 200 OK\r\nContent-Length: 15\r\nConnection: close\r\n\r\nfilc-curl-smoke";
        size_t     sent    = 0;
        while (sent < sizeof(reply) - 1)
        {
            ssize_t written = write(client, reply + sent, sizeof(reply) - 1 - sent);
            if (written <= 0)
                _exit(10);
            sent += (size_t)written;
        }
        close(client);
        close(server);
        _exit(0);
    }
    close(server);
    CURL* handle = init();
    char  url[128];
    snprintf(url, sizeof(url), "http://127.0.0.1:%u/", (unsigned)ntohs(address.sin_port));
    size_t body = 0, headers = 0;
    int    callbacks = 0;
    int    result    = 0;
    if (!handle || setopt(handle, CURLOPT_NOSIGNAL, 1L) || setopt(handle, CURLOPT_TIMEOUT_MS, 5000L) ||
        setopt(handle, CURLOPT_PROXY, "") || setopt(handle, CURLOPT_URL, url) ||
        setopt(handle, CURLOPT_WRITEFUNCTION, collect) || setopt(handle, CURLOPT_WRITEDATA, &body) ||
        setopt(handle, CURLOPT_HEADERFUNCTION, collect) || setopt(handle, CURLOPT_HEADERDATA, &headers) ||
        setopt(handle, CURLOPT_NOPROGRESS, 0L) || setopt(handle, CURLOPT_XFERINFOFUNCTION, progress) ||
        setopt(handle, CURLOPT_XFERINFODATA, &callbacks) || perform(handle))
        result = 11;
    long  status    = 0;
    char* effective = NULL;
    if (!result &&
        (getinfo(handle, CURLINFO_RESPONSE_CODE, &status) || getinfo(handle, CURLINFO_EFFECTIVE_URL, &effective) ||
         status != 200 || body != 15 || !headers || !callbacks || !effective || strcmp(effective, url)))
        result = 12;
    if (handle)
        cleanup(handle);
    int childStatus = 0;
    if (waitpid(child, &childStatus, 0) != child || !WIFEXITED(childStatus) || WEXITSTATUS(childStatus))
        result = 13;
    return result;
}
