// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include <nghttp2/nghttp2.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static nghttp2_ssize sendCallback(nghttp2_session* session, const uint8_t* data, size_t length, int flags,
                                  void* userData)
{
    (void)session;
    (void)data;
    (void)flags;
    (void)userData;
    return (nghttp2_ssize)length;
}

int main(void)
{
    const nghttp2_info* version = nghttp2_version(0);
    if (!version || !version->version_str || strcmp(version->version_str, "1.70.0") != 0)
        return 2;

    nghttp2_session_callbacks* callbacks = NULL;
    if (nghttp2_session_callbacks_new(&callbacks) != 0 || !callbacks)
        return 3;
    nghttp2_session_callbacks_set_send_callback2(callbacks, sendCallback);
    nghttp2_session* session       = NULL;
    int              sessionResult = nghttp2_session_client_new(&session, callbacks, NULL);
    nghttp2_session_callbacks_del(callbacks);
    if (sessionResult != 0 || !session)
        return 4;
    if (nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, NULL, 0) != 0)
    {
        nghttp2_session_del(session);
        return 5;
    }

    static const uint8_t clientMagic[]      = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    int                  settingsSerialized = 0;
    for (int attempt = 0; attempt < 4 && !settingsSerialized; ++attempt)
    {
        const uint8_t* frame  = NULL;
        nghttp2_ssize  length = nghttp2_session_mem_send2(session, &frame);
        if (length < 0 || (!frame && length != 0))
        {
            nghttp2_session_del(session);
            return 6;
        }
        if (length == 0)
            break;

        size_t offset = 0;
        if ((size_t)length >= sizeof(clientMagic) - 1 && memcmp(frame, clientMagic, sizeof(clientMagic) - 1) == 0)
        {
            offset = sizeof(clientMagic) - 1;
        }
        if ((size_t)length - offset >= 9 && frame[offset + 3] == NGHTTP2_SETTINGS)
            settingsSerialized = 1;
    }

    nghttp2_session_del(session);
    return settingsSerialized ? 0 : 7;
}
