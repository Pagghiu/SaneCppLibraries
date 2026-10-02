// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include <arpa/inet.h>
#include <errno.h>
#include <iso646.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509_vfy.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <nghttp2/nghttp2.h>

#define RESPONSE_BODY      "filc-tls-http2"
#define RESPONSE_BODY_SIZE (sizeof(RESPONSE_BODY) - 1)
#define READ_BUFFER_SIZE   16384

typedef struct ResponseSource
{
    size_t offset;
} ResponseSource;

typedef struct ConnectionState
{
    char                  method[16];
    char                  path[512];
    int                   responseSubmitted;
    int*                  totalRequests;
    ResponseSource        responseSource;
    nghttp2_data_provider responseProvider;
} ConnectionState;

static int copyHeader(char* destination, size_t capacity, const uint8_t* value, size_t length)
{
    if (length >= capacity)
    {
        return -1;
    }

    memcpy(destination, value, length);
    destination[length] = '\0';
    return 0;
}

static ssize_t readResponseBody(nghttp2_session* session, int32_t streamId, uint8_t* output, size_t capacity,
                                uint32_t* flags, nghttp2_data_source* source, void* userData)
{
    (void)session;
    (void)streamId;
    (void)userData;
    ResponseSource* response  = (ResponseSource*)source->ptr;
    const size_t    remaining = RESPONSE_BODY_SIZE - response->offset;
    const size_t    count     = remaining < capacity ? remaining : capacity;

    if (count > 0)
    {
        memcpy(output, &RESPONSE_BODY[response->offset], count);
        response->offset += count;
    }

    if (response->offset == RESPONSE_BODY_SIZE)
    {
        *flags |= NGHTTP2_DATA_FLAG_EOF;
    }

    return (ssize_t)count;
}

static int onHeader(nghttp2_session* session, const nghttp2_frame* frame, const uint8_t* name, size_t nameLength,
                    const uint8_t* value, size_t valueLength, uint8_t flags, void* userData)
{
    (void)session;
    (void)flags;
    ConnectionState* state = (ConnectionState*)userData;

    if (frame->hd.type != NGHTTP2_HEADERS)
    {
        return 0;
    }

    if (nameLength == sizeof(":method") - 1 and memcmp(name, ":method", nameLength) == 0)
    {
        return copyHeader(state->method, sizeof(state->method), value, valueLength);
    }

    if (nameLength == sizeof(":path") - 1 and memcmp(name, ":path", nameLength) == 0)
    {
        return copyHeader(state->path, sizeof(state->path), value, valueLength);
    }

    return 0;
}

static int onFrameReceived(nghttp2_session* session, const nghttp2_frame* frame, void* userData)
{
    ConnectionState* state = (ConnectionState*)userData;
    char             contentLength[16];

    if (frame->hd.type != NGHTTP2_HEADERS or frame->headers.cat != NGHTTP2_HCAT_REQUEST or state->responseSubmitted)
    {
        return 0;
    }

    if (strcmp(state->method, "GET") != 0 or strcmp(state->path, "/") != 0)
    {
        fprintf(stderr, "unexpected HTTP/2 request method='%s' path='%s'; expected GET /\n", state->method,
                state->path);
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }

    snprintf(contentLength, sizeof(contentLength), "%zu", (size_t)RESPONSE_BODY_SIZE);
    nghttp2_nv responseHeaders[] = {
        {(uint8_t*)":status", (uint8_t*)"200", sizeof(":status") - 1, sizeof("200") - 1, NGHTTP2_NV_FLAG_NONE},
        {(uint8_t*)"content-type", (uint8_t*)"text/plain", sizeof("content-type") - 1, sizeof("text/plain") - 1,
         NGHTTP2_NV_FLAG_NONE},
        {(uint8_t*)"content-length", (uint8_t*)contentLength, sizeof("content-length") - 1, strlen(contentLength),
         NGHTTP2_NV_FLAG_NONE},
        {(uint8_t*)"x-fixture-alpn", (uint8_t*)"h2", sizeof("x-fixture-alpn") - 1, sizeof("h2") - 1,
         NGHTTP2_NV_FLAG_NONE},
    };

    state->responseSource.offset          = 0;
    state->responseProvider.source.ptr    = &state->responseSource;
    state->responseProvider.read_callback = readResponseBody;

    if (nghttp2_submit_response(session, frame->hd.stream_id, responseHeaders,
                                sizeof(responseHeaders) / sizeof(responseHeaders[0]), &state->responseProvider) != 0)
    {
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }

    state->responseSubmitted = 1;
    ++*state->totalRequests;
    printf("HTTP2_REQUEST method=%s path=%s status=200 body=%s\n", state->method, state->path, RESPONSE_BODY);
    return 0;
}

static int selectAlpn(SSL* ssl, const unsigned char** output, unsigned char* outputLength, const unsigned char* input,
                      unsigned int inputLength, void* argument)
{
    (void)ssl;
    (void)argument;
    unsigned int index = 0;

    while (index < inputLength)
    {
        const unsigned int protocolLength = input[index++];
        if (protocolLength > inputLength - index)
        {
            return SSL_TLSEXT_ERR_ALERT_FATAL;
        }

        if (protocolLength == 2 and input[index] == 'h' and input[index + 1] == '2')
        {
            *output       = input + index;
            *outputLength = 2;
            return SSL_TLSEXT_ERR_OK;
        }

        index += protocolLength;
    }

    return SSL_TLSEXT_ERR_ALERT_FATAL;
}

static int flushHttp2Output(SSL* ssl, nghttp2_session* session)
{
    const uint8_t* data = NULL;

    for (;;)
    {
        const ssize_t length = nghttp2_session_mem_send(session, &data);
        if (length < 0)
        {
            fprintf(stderr, "nghttp2_session_mem_send failed: %zd\n", length);
            return -1;
        }
        if (length == 0)
        {
            return 0;
        }
        ssize_t offset = 0;
        while (offset < length)
        {
            const int written = SSL_write(ssl, data + offset, (int)(length - offset));
            if (written <= 0)
            {
                fprintf(stderr, "SSL_write failed while sending HTTP/2 data\n");
                ERR_print_errors_fp(stderr);
                return -1;
            }
            offset += written;
        }
    }
}

static int serveConnection(int socket, SSL_CTX* context, int* totalRequests, unsigned long idleTimeout)
{
    int                        result    = 0;
    SSL*                       ssl       = SSL_new(context);
    nghttp2_session_callbacks* callbacks = NULL;
    nghttp2_session*           session   = NULL;
    ConnectionState            state     = {0};
    state.totalRequests                  = totalRequests;

    if (ssl == NULL or SSL_set_fd(ssl, socket) != 1)
    {
        fprintf(stderr, "TLS server session setup failed\n");
        result = -1;
        goto cleanup;
    }

    const int acceptResult = SSL_accept(ssl);
    if (acceptResult != 1)
    {
        const int sslError = SSL_get_error(ssl, acceptResult);
        printf("TLS_REJECT ssl_error=%d verify=%ld\n", sslError, SSL_get_verify_result(ssl));
        ERR_clear_error();
        goto cleanup;
    }

    const unsigned char* selectedAlpn       = NULL;
    unsigned int         selectedAlpnLength = 0;
    SSL_get0_alpn_selected(ssl, &selectedAlpn, &selectedAlpnLength);
    if (selectedAlpnLength != 2 or memcmp(selectedAlpn, "h2", 2) != 0)
    {
        printf("TLS_REJECT alpn=not-h2\n");
        goto cleanup;
    }
    printf("TLS_ACCEPT alpn=h2\n");

    if (nghttp2_session_callbacks_new(&callbacks) != 0)
    {
        fprintf(stderr, "nghttp2 callback allocation failed\n");
        result = -1;
        goto cleanup;
    }
    nghttp2_session_callbacks_set_on_header_callback(callbacks, onHeader);
    nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, onFrameReceived);

    if (nghttp2_session_server_new(&session, callbacks, &state) != 0)
    {
        fprintf(stderr, "nghttp2 server session creation failed\n");
        result = -1;
        goto cleanup;
    }

    nghttp2_settings_entry setting = {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 1};
    if (nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, &setting, 1) != 0)
    {
        result = -1;
        goto cleanup;
    }

    // Drain peer SETTINGS acknowledgements before TLS shutdown so closing does not reset unread response data.
    for (int readAttempt = 0; readAttempt < 20; ++readAttempt)
    {
        struct pollfd descriptor = {socket, POLLIN, 0};
        const int     ready      = SSL_pending(ssl) > 0 ? 1 : poll(&descriptor, 1, (int)idleTimeout);
        if (ready < 0 and errno == EINTR)
        {
            --readAttempt;
            continue;
        }
        if (ready <= 0)
        {
            printf("HTTP2_IDLE poll=%d response=%d\n", ready, state.responseSubmitted);
            break;
        }

        uint8_t   input[READ_BUFFER_SIZE];
        const int received = SSL_read(ssl, input, sizeof(input));
        if (received <= 0)
        {
            const int sslError = SSL_get_error(ssl, received);
            printf("TLS_PEER_CLOSED ssl_error=%d response=%d\n", sslError, state.responseSubmitted);
            ERR_print_errors_fp(stderr);
            ERR_clear_error();
            break;
        }

        const ssize_t consumed = nghttp2_session_mem_recv(session, input, (size_t)received);
        if (consumed != received)
        {
            fprintf(stderr, "nghttp2_session_mem_recv consumed %zd of %d bytes\n", consumed, received);
            result = -1;
            break;
        }

        if (flushHttp2Output(ssl, session) != 0)
        {
            result = -1;
            break;
        }
    }

    if (state.responseSubmitted)
    {
        if (SSL_shutdown(ssl) == 0)
        {
            (void)SSL_shutdown(ssl);
        }
        ERR_clear_error();
    }
    else
    {
        printf("CONNECTION_WITHOUT_REQUEST\n");
    }

cleanup:
    if (session != NULL)
    {
        nghttp2_session_del(session);
    }
    if (callbacks != NULL)
    {
        nghttp2_session_callbacks_del(callbacks);
    }
    if (ssl != NULL)
    {
        SSL_free(ssl);
    }
    close(socket);
    return result;
}

static int parseUnsigned(const char* text, unsigned long maximum, unsigned long* result)
{
    char* end                 = NULL;
    errno                     = 0;
    const unsigned long value = strtoul(text, &end, 10);
    if (errno != 0 or end == text or * end != '\0' or value > maximum)
    {
        return -1;
    }
    *result = value;
    return 0;
}

int main(int argc, char** argv)
{
    if (argc != 6)
    {
        fprintf(stderr,
                "usage: %s <port 0..65535> <max-connections> <idle-timeout-ms> <certificate.pem> "
                "<private-key.pem>\n",
                argv[0]);
        return 2;
    }

    unsigned long requestedPort  = 0;
    unsigned long maxConnections = 0;
    unsigned long idleTimeout    = 0;
    if (parseUnsigned(argv[1], 65535, &requestedPort) != 0 or parseUnsigned(argv[2], 100, &maxConnections) !=
        0 or maxConnections == 0 or parseUnsigned(argv[3], 300000, &idleTimeout) != 0 or idleTimeout == 0)
    {
        fputs("invalid server bounds\n", stderr);
        return 2;
    }

    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGPIPE, SIG_IGN);
    if (OPENSSL_init_ssl(0, NULL) != 1)
    {
        fputs("OpenSSL initialization failed\n", stderr);
        return 1;
    }

    const char*         opensslVersion = OpenSSL_version(OPENSSL_VERSION);
    const nghttp2_info* nghttp2Version = nghttp2_version(0);
    if (opensslVersion == NULL or strncmp(opensslVersion, "OpenSSL 3.6.5 ", 14) != 0 or nghttp2Version ==
        NULL or strcmp(nghttp2Version->version_str, "1.70.0") != 0)
    {
        fprintf(stderr, "unexpected runtime versions: %s / %s\n",
                opensslVersion == NULL ? "unknown OpenSSL" : opensslVersion,
                nghttp2Version == NULL ? "unknown nghttp2" : nghttp2Version->version_str);
        return 1;
    }
    printf("RUNTIME openssl=%s nghttp2=%s\n", opensslVersion, nghttp2Version->version_str);

    SSL_CTX* context = SSL_CTX_new(TLS_server_method());
    if (context == NULL)
    {
        fputs("TLS server context allocation failed\n", stderr);
        return 1;
    }
    if (SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION) !=
        1 or SSL_CTX_use_certificate_file(context, argv[4], SSL_FILETYPE_PEM) !=
        1 or SSL_CTX_use_PrivateKey_file(context, argv[5], SSL_FILETYPE_PEM) !=
        1 or SSL_CTX_check_private_key(context) != 1)
    {
        fputs("TLS server certificate setup failed\n", stderr);
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(context);
        return 1;
    }
    SSL_CTX_set_alpn_select_cb(context, selectAlpn, NULL);

    const int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0)
    {
        perror("socket");
        SSL_CTX_free(context);
        return 1;
    }

    int reuseAddress = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(reuseAddress));
    struct sockaddr_in address = {0};
    address.sin_family         = AF_INET;
    address.sin_addr.s_addr    = htonl(INADDR_LOOPBACK);
    address.sin_port           = htons((uint16_t)requestedPort);
    if (bind(listener, (struct sockaddr*)&address, sizeof(address)) != 0 or listen(listener, 8) != 0)
    {
        perror("bind/listen");
        close(listener);
        SSL_CTX_free(context);
        return 1;
    }

    socklen_t addressLength = sizeof(address);
    if (getsockname(listener, (struct sockaddr*)&address, &addressLength) != 0)
    {
        perror("getsockname");
        close(listener);
        SSL_CTX_free(context);
        return 1;
    }
    printf("READY port=%u max_connections=%lu idle_timeout_ms=%lu\n", (unsigned)ntohs(address.sin_port), maxConnections,
           idleTimeout);

    unsigned long accepted = 0;
    int           requests = 0;
    int           failures = 0;
    while (accepted < maxConnections)
    {
        struct pollfd descriptor = {listener, POLLIN, 0};
        const int     ready      = poll(&descriptor, 1, (int)idleTimeout);
        if (ready < 0 and errno == EINTR)
        {
            continue;
        }
        if (ready <= 0)
        {
            printf("IDLE_TIMEOUT accepted=%lu\n", accepted);
            failures = 1;
            break;
        }

        const int client = accept(listener, NULL, NULL);
        if (client < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("accept");
            failures = 1;
            break;
        }
        ++accepted;

        const unsigned long connectionTimeout = idleTimeout < 5000 ? idleTimeout : 5000;
        struct timeval      ioTimeout         = {(time_t)(connectionTimeout / 1000),
                                                 (suseconds_t)((connectionTimeout % 1000) * 1000)};
        if (setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &ioTimeout, sizeof(ioTimeout)) !=
            0 or setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &ioTimeout, sizeof(ioTimeout)) != 0)
        {
            perror("connection timeout");
            close(client);
            failures = 1;
            break;
        }
        if (serveConnection(client, context, &requests, connectionTimeout) != 0)
        {
            failures = 1;
        }
    }

    printf("SUMMARY accepted=%lu requests=%d failures=%d\n", accepted, requests, failures);
    close(listener);
    SSL_CTX_free(context);
    return failures == 0 and accepted == maxConnections and requests > 0 ? 0 : 1;
}
