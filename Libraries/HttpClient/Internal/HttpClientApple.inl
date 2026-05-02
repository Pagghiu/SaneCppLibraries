// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
// Apple backend for HttpClient using NSURLSession via objc_msgSend.

#include <objc/message.h>
#include <objc/runtime.h>

template <typename ReturnType, typename... Args>
inline ReturnType sc_objc_msgSend(id obj, SEL sel, Args... args)
{
    auto func = reinterpret_cast<ReturnType (*)(id, SEL, Args...)>(objc_msgSend);
    return func(obj, sel, args...);
}

namespace
{
static constexpr unsigned long NSUTF8StringEncoding = 4;

static constexpr long NSStreamStatusNotOpen = 0;
static constexpr long NSStreamStatusOpen    = 2;
static constexpr long NSStreamStatusAtEnd   = 5;
static constexpr long NSStreamStatusClosed  = 6;
static constexpr long NSStreamStatusError   = 7;

static bool httpClientAppleAsciiEqualsIgnoreCase(SC::StringSpan a, SC::StringSpan b)
{
    if (a.sizeInBytes() != b.sizeInBytes())
    {
        return false;
    }
    for (size_t idx = 0; idx < a.sizeInBytes(); ++idx)
    {
        char ac = a.toCharSpan()[idx];
        char bc = b.toCharSpan()[idx];
        if (ac >= 'A' and ac <= 'Z')
        {
            ac = static_cast<char>(ac - 'A' + 'a');
        }
        if (bc >= 'A' and bc <= 'Z')
        {
            bc = static_cast<char>(bc - 'A' + 'a');
        }
        if (ac != bc)
        {
            return false;
        }
    }
    return true;
}

static bool httpClientAppleHasHeader(const SC::HttpClientRequest& request, SC::StringSpan name)
{
    for (size_t idx = 0; idx < request.headers.sizeInElements(); ++idx)
    {
        if (httpClientAppleAsciiEqualsIgnoreCase(request.headers[idx].name, name))
        {
            return true;
        }
    }
    return false;
}

static size_t httpClientAppleWriteUnsigned(uint64_t value, SC::Span<char> dest)
{
    char   reversed[32];
    size_t count = 0;
    do
    {
        reversed[count++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value != 0 and count < sizeof(reversed));

    if (count > dest.sizeInBytes())
    {
        return 0;
    }
    for (size_t idx = 0; idx < count; ++idx)
    {
        dest[idx] = reversed[count - 1 - idx];
    }
    return count;
}
} // namespace

struct SC::HttpClient::Internal
{
};

struct SC::HttpClientOperation::Internal
{
    id session  = nullptr;
    id delegate = nullptr;
    id task     = nullptr;

    id       bodyStream      = nullptr;
    bool     cancelRequested = false;
    uint32_t redirectCount   = 0;
};

struct SC::HttpClientAppleCallbacks
{
    static Class getDelegateClass()
    {
        static Class delegateClass = nullptr;
        if (delegateClass == nullptr)
        {
            Class parentClass = objc_getClass("NSObject");
            delegateClass     = objc_allocateClassPair(parentClass, "SCHttpClientOperationDelegate_Dynamic", 0);
            class_addIvar(delegateClass, "sc_operation", sizeof(void*), alignof(void*), "^v");

            class_addMethod(delegateClass, sel_getUid("URLSession:dataTask:didReceiveResponse:completionHandler:"),
                            reinterpret_cast<IMP>(&didReceiveResponse), "v@:@@@@");
            class_addMethod(delegateClass, sel_getUid("URLSession:dataTask:didReceiveData:"),
                            reinterpret_cast<IMP>(&didReceiveData), "v@:@@@");
            class_addMethod(delegateClass, sel_getUid("URLSession:task:didCompleteWithError:"),
                            reinterpret_cast<IMP>(&didCompleteWithError), "v@:@@@");
            class_addMethod(delegateClass,
                            sel_getUid("URLSession:task:willPerformHTTPRedirection:newRequest:completionHandler:"),
                            reinterpret_cast<IMP>(&willPerformHTTPRedirection), "v@:@@@@@");
            objc_registerClassPair(delegateClass);
        }
        return delegateClass;
    }

    static Class getBodyStreamClass()
    {
        static Class bodyStreamClass = nullptr;
        if (bodyStreamClass == nullptr)
        {
            Class parentClass = objc_getClass("NSInputStream");
            bodyStreamClass   = objc_allocateClassPair(parentClass, "SCHttpClientBodyStream_Dynamic", 0);
            class_addIvar(bodyStreamClass, "sc_operation", sizeof(void*), alignof(void*), "^v");
            class_addIvar(bodyStreamClass, "sc_status", sizeof(long), alignof(long), "q");
            class_addIvar(bodyStreamClass, "sc_delegate", sizeof(void*), alignof(void*), "@");
            class_addMethod(bodyStreamClass, sel_getUid("open"), reinterpret_cast<IMP>(&bodyStreamOpen), "v@:");
            class_addMethod(bodyStreamClass, sel_getUid("close"), reinterpret_cast<IMP>(&bodyStreamClose), "v@:");
            class_addMethod(bodyStreamClass, sel_getUid("read:maxLength:"), reinterpret_cast<IMP>(&bodyStreamRead),
                            "q@:^cq");
            class_addMethod(bodyStreamClass, sel_getUid("getBuffer:length:"),
                            reinterpret_cast<IMP>(&bodyStreamGetBuffer), "c@:^@^Q");
            class_addMethod(bodyStreamClass, sel_getUid("hasBytesAvailable"),
                            reinterpret_cast<IMP>(&bodyStreamHasBytesAvailable), "c@:");
            class_addMethod(bodyStreamClass, sel_getUid("streamStatus"), reinterpret_cast<IMP>(&bodyStreamStatus),
                            "q@:");
            class_addMethod(bodyStreamClass, sel_getUid("streamError"), reinterpret_cast<IMP>(&bodyStreamError), "@@:");
            class_addMethod(bodyStreamClass, sel_getUid("delegate"), reinterpret_cast<IMP>(&bodyStreamDelegate), "@@:");
            class_addMethod(bodyStreamClass, sel_getUid("setDelegate:"), reinterpret_cast<IMP>(&bodyStreamSetDelegate),
                            "v@:@");
            class_addMethod(bodyStreamClass, sel_getUid("propertyForKey:"),
                            reinterpret_cast<IMP>(&bodyStreamPropertyForKey), "@@:@");
            class_addMethod(bodyStreamClass, sel_getUid("setProperty:forKey:"),
                            reinterpret_cast<IMP>(&bodyStreamSetProperty), "c@:@@");
            class_addMethod(bodyStreamClass, sel_getUid("scheduleInRunLoop:forMode:"),
                            reinterpret_cast<IMP>(&bodyStreamScheduleInRunLoop), "v@:@@");
            class_addMethod(bodyStreamClass, sel_getUid("removeFromRunLoop:forMode:"),
                            reinterpret_cast<IMP>(&bodyStreamRemoveFromRunLoop), "v@:@@");
            objc_registerClassPair(bodyStreamClass);
        }
        return bodyStreamClass;
    }

    static id createDelegate(SC::HttpClientOperation& operation)
    {
        id delegate = sc_objc_msgSend<id>(reinterpret_cast<id>(getDelegateClass()), sel_getUid("alloc"));
        delegate    = sc_objc_msgSend<id>(delegate, sel_getUid("init"));
        setDelegateOperation(delegate, &operation);
        return delegate;
    }

    static id createBodyStream(SC::HttpClientOperation& operation)
    {
        id stream       = sc_objc_msgSend<id>(reinterpret_cast<id>(getBodyStreamClass()), sel_getUid("alloc"));
        stream          = sc_objc_msgSend<id>(stream, sel_getUid("init"));
        Ivar statusIvar = class_getInstanceVariable(getBodyStreamClass(), "sc_status");
        setBodyStreamOperation(stream, &operation);
        *reinterpret_cast<long*>(reinterpret_cast<char*>(stream) + ivar_getOffset(statusIvar)) = NSStreamStatusNotOpen;
        return stream;
    }

    static void setDelegateOperation(id delegate, SC::HttpClientOperation* operation)
    {
        Ivar ivar = class_getInstanceVariable(getDelegateClass(), "sc_operation");
        *reinterpret_cast<SC::HttpClientOperation**>(reinterpret_cast<char*>(delegate) + ivar_getOffset(ivar)) =
            operation;
    }

    static void setBodyStreamOperation(id stream, SC::HttpClientOperation* operation)
    {
        Ivar ivar = class_getInstanceVariable(getBodyStreamClass(), "sc_operation");
        *reinterpret_cast<SC::HttpClientOperation**>(reinterpret_cast<char*>(stream) + ivar_getOffset(ivar)) =
            operation;
    }

    static SC::HttpClientOperation* getOperation(id self)
    {
        Ivar ivar = class_getInstanceVariable(getDelegateClass(), "sc_operation");
        return *reinterpret_cast<SC::HttpClientOperation**>(reinterpret_cast<char*>(self) + ivar_getOffset(ivar));
    }

    static SC::HttpClientOperation* getBodyStreamOperation(id self)
    {
        Ivar ivar = class_getInstanceVariable(getBodyStreamClass(), "sc_operation");
        return *reinterpret_cast<SC::HttpClientOperation**>(reinterpret_cast<char*>(self) + ivar_getOffset(ivar));
    }

    static long& getBodyStreamStatusRef(id self)
    {
        Ivar ivar = class_getInstanceVariable(getBodyStreamClass(), "sc_status");
        return *reinterpret_cast<long*>(reinterpret_cast<char*>(self) + ivar_getOffset(ivar));
    }

    static id& getBodyStreamDelegateRef(id self)
    {
        Ivar ivar = class_getInstanceVariable(getBodyStreamClass(), "sc_delegate");
        return *reinterpret_cast<id*>(reinterpret_cast<char*>(self) + ivar_getOffset(ivar));
    }

    static void bodyStreamOpen(id self, SEL) { getBodyStreamStatusRef(self) = NSStreamStatusOpen; }

    static void bodyStreamClose(id self, SEL) { getBodyStreamStatusRef(self) = NSStreamStatusClosed; }

    static long bodyStreamRead(id self, SEL, char* buffer, unsigned long maxLength)
    {
        SC::HttpClientOperation* operation = getBodyStreamOperation(self);
        if (operation == nullptr)
        {
            getBodyStreamStatusRef(self) = NSStreamStatusError;
            return -1;
        }

        Result       bodyError(true);
        bool         endReached = false;
        const size_t bytesRead =
            operation->readRequestBodyChunk({buffer, static_cast<size_t>(maxLength)}, bodyError, endReached);
        if (not bodyError)
        {
            getBodyStreamStatusRef(self) = NSStreamStatusError;
            return -1;
        }
        if (endReached)
        {
            getBodyStreamStatusRef(self) = NSStreamStatusAtEnd;
            return 0;
        }
        getBodyStreamStatusRef(self) = NSStreamStatusOpen;
        return static_cast<long>(bytesRead);
    }

    static bool bodyStreamGetBuffer(id, SEL, char**, unsigned long*) { return false; }

    static bool bodyStreamHasBytesAvailable(id self, SEL)
    {
        const long status = getBodyStreamStatusRef(self);
        return status != NSStreamStatusAtEnd and status != NSStreamStatusClosed and status != NSStreamStatusError;
    }

    static long bodyStreamStatus(id self, SEL) { return getBodyStreamStatusRef(self); }

    static id bodyStreamError(id, SEL) { return nullptr; }

    static id bodyStreamDelegate(id self, SEL) { return getBodyStreamDelegateRef(self); }

    static void bodyStreamSetDelegate(id self, SEL, id delegate) { getBodyStreamDelegateRef(self) = delegate; }

    static id bodyStreamPropertyForKey(id, SEL, id) { return nullptr; }

    static bool bodyStreamSetProperty(id, SEL, id, id) { return false; }

    static void bodyStreamScheduleInRunLoop(id, SEL, id, id) {}

    static void bodyStreamRemoveFromRunLoop(id, SEL, id, id) {}

    static void didReceiveResponse(id self, SEL, id, id, id urlResponse, id completionHandler)
    {
        SC::HttpClientOperation* operation = getOperation(self);
        if (operation == nullptr or operation->currentResponse == nullptr)
        {
            return;
        }

        SC::HttpClientResponse& response = *operation->currentResponse;
        response.statusCode         = static_cast<int>(sc_objc_msgSend<long>(urlResponse, sel_getUid("statusCode")));
        response.negotiatedProtocol = SC::HttpClientResponse::Protocol::Http11;
        response.redirectCount =
            reinterpret_cast<SC::HttpClientOperation::Internal*>(operation->storage)->redirectCount;

        id          url            = sc_objc_msgSend<id>(urlResponse, sel_getUid("URL"));
        id          absoluteString = sc_objc_msgSend<id>(url, sel_getUid("absoluteString"));
        const char* urlCStr        = absoluteString != nullptr
                                         ? sc_objc_msgSend<const char*>(absoluteString, sel_getUid("UTF8String"))
                                         : nullptr;
        if (urlCStr != nullptr)
        {
            (void)operation->copyResponseEffectiveUrl(
                SC::StringSpan::fromNullTerminated(urlCStr, operation->currentRequest.url.getEncoding()));
        }

        id            headers = sc_objc_msgSend<id>(urlResponse, sel_getUid("allHeaderFields"));
        id            allKeys = sc_objc_msgSend<id>(headers, sel_getUid("allKeys"));
        unsigned long count   = sc_objc_msgSend<unsigned long>(allKeys, sel_getUid("count"));

        size_t       appended = 0;
        char*        dest     = const_cast<char*>(response.headers.data());
        const size_t capacity = response.headers.sizeInBytes();
        for (unsigned long idx = 0; idx < count; ++idx)
        {
            id key   = sc_objc_msgSend<id>(allKeys, sel_getUid("objectAtIndex:"), idx);
            id value = sc_objc_msgSend<id>(headers, sel_getUid("objectForKey:"), key);

            const char* keyCStr = sc_objc_msgSend<const char*>(key, sel_getUid("UTF8String"));
            const char* valCStr = sc_objc_msgSend<const char*>(value, sel_getUid("UTF8String"));
            if (keyCStr == nullptr or valCStr == nullptr)
            {
                continue;
            }
            const size_t keyLen = strlen(keyCStr);
            const size_t valLen = strlen(valCStr);
            if (appended + keyLen + valLen + 4 > capacity)
            {
                continue;
            }
            memcpy(dest + appended, keyCStr, keyLen);
            appended += keyLen;
            dest[appended++] = ':';
            dest[appended++] = ' ';
            memcpy(dest + appended, valCStr, valLen);
            appended += valLen;
            dest[appended++] = '\r';
            dest[appended++] = '\n';
        }
        response.headersLength = appended;
        operation->enqueueResponseHead();

        struct BlockLiteral
        {
            void* isa;
            int   flags;
            int   reserved;
            void (*invoke)(void*, long);
        };
        auto block = reinterpret_cast<BlockLiteral*>(completionHandler);
        block->invoke(block, 1);
    }

    static void didReceiveData(id self, SEL, id, id, id data)
    {
        SC::HttpClientOperation* operation = getOperation(self);
        if (operation == nullptr)
        {
            return;
        }

        const void*   bytes  = sc_objc_msgSend<const void*>(data, sel_getUid("bytes"));
        unsigned long length = sc_objc_msgSend<unsigned long>(data, sel_getUid("length"));
        if (length == 0)
        {
            return;
        }

        const SC::Result enqueueRes =
            operation->enqueueResponseDataCopy({reinterpret_cast<const char*>(bytes), length});
        if (not enqueueRes)
        {
            operation->enqueueError(enqueueRes);
            auto& internal = *reinterpret_cast<SC::HttpClientOperation::Internal*>(operation->storage);
            if (internal.task != nullptr)
            {
                sc_objc_msgSend<void>(internal.task, sel_getUid("cancel"));
            }
            return;
        }
    }

    static void willPerformHTTPRedirection(id self, SEL, id, id, id, id newRequest, id completionHandler)
    {
        SC::HttpClientOperation* operation = getOperation(self);

        struct BlockLiteral
        {
            void* isa;
            int   flags;
            int   reserved;
            void (*invoke)(void*, id);
        };
        auto block = reinterpret_cast<BlockLiteral*>(completionHandler);
        if (operation != nullptr and operation->isAutomaticRedirectEnabled())
        {
            auto& internal = *reinterpret_cast<SC::HttpClientOperation::Internal*>(operation->storage);
            if (internal.redirectCount >= operation->currentRequest.options.redirect.maxRedirects)
            {
                block->invoke(block, nullptr);
            }
            else
            {
                internal.redirectCount += 1;
                block->invoke(block, newRequest);
            }
        }
        else
        {
            block->invoke(block, nullptr);
        }
    }

    static void didCompleteWithError(id self, SEL, id, id, id error)
    {
        SC::HttpClientOperation* operation = getOperation(self);
        if (operation == nullptr)
        {
            return;
        }
        if (error != nullptr)
        {
            auto& internal = *reinterpret_cast<SC::HttpClientOperation::Internal*>(operation->storage);
            if (internal.cancelRequested)
            {
                operation->enqueueError(Result::Error("HttpClient: request cancelled"));
            }
            else
            {
                operation->enqueueError(Result::Error("HttpClient: NSURLSession error"));
            }
        }
        else
        {
            operation->enqueueResponseComplete();
        }
    }
};

SC::HttpClient::HttpClient()
{
    static_assert(sizeof(Internal) <= sizeof(storage), "Apple HttpClient storage too small");
    static_assert(alignof(Internal) <= alignof(uint64_t), "Apple HttpClient alignment mismatch");
    SC::placementNew(*reinterpret_cast<Internal*>(storage));
}

SC::HttpClient::~HttpClient()
{
    if (initialized)
    {
        (void)close();
    }
    reinterpret_cast<Internal*>(storage)->~Internal();
}

SC::Result SC::HttpClient::platformInit() { return Result(true); }
SC::Result SC::HttpClient::platformClose() { return Result(true); }

SC::HttpClientOperation::HttpClientOperation()
{
    static_assert(sizeof(Internal) <= sizeof(storage), "Apple HttpClientOperation storage too small");
    static_assert(alignof(Internal) <= alignof(uint64_t), "Apple HttpClientOperation alignment mismatch");
    SC::placementNew(*reinterpret_cast<Internal*>(storage));
}

SC::HttpClientOperation::~HttpClientOperation()
{
    if (initialized)
    {
        (void)close();
    }
    reinterpret_cast<Internal*>(storage)->~Internal();
}

SC::Result SC::HttpClientOperation::platformInit() { return Result(true); }

SC::Result SC::HttpClientOperation::platformClose()
{
    auto& internal           = *reinterpret_cast<Internal*>(storage);
    internal.cancelRequested = true;

    // NSURLSession teardown is asynchronous. Clear raw callback targets first so callbacks from a previous
    // operation cannot race into a later stack-reused HttpClientOperation during the next test section.
    if (internal.delegate != nullptr)
    {
        HttpClientAppleCallbacks::setDelegateOperation(internal.delegate, nullptr);
    }
    if (internal.bodyStream != nullptr)
    {
        HttpClientAppleCallbacks::setBodyStreamOperation(internal.bodyStream, nullptr);
    }

    if (internal.task != nullptr)
    {
        sc_objc_msgSend<void>(internal.task, sel_getUid("cancel"));
#if !__has_feature(objc_arc)
        sc_objc_msgSend<void>(internal.task, sel_getUid("release"));
#endif
        internal.task = nullptr;
    }
    if (internal.session != nullptr)
    {
        sc_objc_msgSend<void>(internal.session, sel_getUid("invalidateAndCancel"));
#if !__has_feature(objc_arc)
        sc_objc_msgSend<void>(internal.session, sel_getUid("release"));
#endif
        internal.session = nullptr;
    }
    if (internal.bodyStream != nullptr)
    {
        sc_objc_msgSend<void>(internal.bodyStream, sel_getUid("close"));
#if !__has_feature(objc_arc)
        sc_objc_msgSend<void>(internal.bodyStream, sel_getUid("release"));
#endif
        internal.bodyStream = nullptr;
    }
#if !__has_feature(objc_arc)
    if (internal.delegate != nullptr)
    {
        sc_objc_msgSend<void>(internal.delegate, sel_getUid("release"));
    }
#endif
    internal.delegate        = nullptr;
    internal.cancelRequested = false;
    return Result(true);
}

SC::Result SC::HttpClientOperation::platformCancel()
{
    auto& internal           = *reinterpret_cast<Internal*>(storage);
    internal.cancelRequested = true;
    if (internal.task != nullptr)
    {
        sc_objc_msgSend<void>(internal.task, sel_getUid("cancel"));
    }
    return Result(true);
}

SC::Result SC::HttpClientOperation::platformStart()
{
    auto& internal           = *reinterpret_cast<Internal*>(storage);
    internal.cancelRequested = false;
    internal.redirectCount   = 0;

    SC_TRY_MSG(currentRequest.options.tls.verifyPeer, "HttpClient: Apple custom TLS settings not supported");
    SC_TRY_MSG(currentRequest.options.tls.caCertificatesPath.sizeInBytes() == 0,
               "HttpClient: Apple custom TLS settings not supported");

    id configurationClass = reinterpret_cast<id>(objc_getClass("NSURLSessionConfiguration"));
    id configuration      = sc_objc_msgSend<id>(configurationClass, sel_getUid("ephemeralSessionConfiguration"));
    sc_objc_msgSend<void>(configuration, sel_getUid("setHTTPShouldSetCookies:"), false);

    internal.delegate = HttpClientAppleCallbacks::createDelegate(*this);
    id sessionClass   = reinterpret_cast<id>(objc_getClass("NSURLSession"));
    internal.session = sc_objc_msgSend<id>(sessionClass, sel_getUid("sessionWithConfiguration:delegate:delegateQueue:"),
                                           configuration, internal.delegate, nullptr);
#if !__has_feature(objc_arc)
    internal.session = sc_objc_msgSend<id>(internal.session, sel_getUid("retain"));
#endif

    id stringClass = reinterpret_cast<id>(objc_getClass("NSString"));
    id urlString   = sc_objc_msgSend<id>(stringClass, sel_getUid("stringWithBytes:length:encoding:"),
                                         currentRequest.url.toCharSpan().data(), currentRequest.url.sizeInBytes(),
                                         NSUTF8StringEncoding);
    SC_TRY_MSG(urlString != nullptr, "HttpClient: failed creating NSURL string");

    id urlClass = reinterpret_cast<id>(objc_getClass("NSURL"));
    id url      = sc_objc_msgSend<id>(urlClass, sel_getUid("URLWithString:"), urlString);
    SC_TRY_MSG(url != nullptr, "HttpClient: invalid NSURL");

    id requestClass = reinterpret_cast<id>(objc_getClass("NSMutableURLRequest"));
    id requestObj   = sc_objc_msgSend<id>(requestClass, sel_getUid("requestWithURL:"), url);

    id methodString = sc_objc_msgSend<id>(reinterpret_cast<id>(objc_getClass("NSString")),
                                          sel_getUid("stringWithUTF8String:"), "GET");
    switch (currentRequest.method)
    {
    case HttpClientRequest::HttpGET:
        methodString = sc_objc_msgSend<id>(reinterpret_cast<id>(objc_getClass("NSString")),
                                           sel_getUid("stringWithUTF8String:"), "GET");
        break;
    case HttpClientRequest::HttpPOST:
        methodString = sc_objc_msgSend<id>(reinterpret_cast<id>(objc_getClass("NSString")),
                                           sel_getUid("stringWithUTF8String:"), "POST");
        break;
    case HttpClientRequest::HttpPUT:
        methodString = sc_objc_msgSend<id>(reinterpret_cast<id>(objc_getClass("NSString")),
                                           sel_getUid("stringWithUTF8String:"), "PUT");
        break;
    case HttpClientRequest::HttpHEAD:
        methodString = sc_objc_msgSend<id>(reinterpret_cast<id>(objc_getClass("NSString")),
                                           sel_getUid("stringWithUTF8String:"), "HEAD");
        break;
    case HttpClientRequest::HttpDELETE:
        methodString = sc_objc_msgSend<id>(reinterpret_cast<id>(objc_getClass("NSString")),
                                           sel_getUid("stringWithUTF8String:"), "DELETE");
        break;
    case HttpClientRequest::HttpPATCH:
        methodString = sc_objc_msgSend<id>(reinterpret_cast<id>(objc_getClass("NSString")),
                                           sel_getUid("stringWithUTF8String:"), "PATCH");
        break;
    case HttpClientRequest::HttpOPTIONS:
        methodString = sc_objc_msgSend<id>(reinterpret_cast<id>(objc_getClass("NSString")),
                                           sel_getUid("stringWithUTF8String:"), "OPTIONS");
        break;
    }
    sc_objc_msgSend<void>(requestObj, sel_getUid("setHTTPMethod:"), methodString);

    if (currentRequest.options.timeouts.requestTimeoutMs > 0)
    {
        sc_objc_msgSend<void>(requestObj, sel_getUid("setTimeoutInterval:"),
                              static_cast<double>(currentRequest.options.timeouts.requestTimeoutMs) / 1000.0);
    }

    for (size_t idx = 0; idx < currentRequest.headers.sizeInElements(); ++idx)
    {
        id headerName  = sc_objc_msgSend<id>(reinterpret_cast<id>(objc_getClass("NSString")),
                                             sel_getUid("stringWithBytes:length:encoding:"),
                                             currentRequest.headers[idx].name.toCharSpan().data(),
                                             currentRequest.headers[idx].name.sizeInBytes(), NSUTF8StringEncoding);
        id headerValue = sc_objc_msgSend<id>(reinterpret_cast<id>(objc_getClass("NSString")),
                                             sel_getUid("stringWithBytes:length:encoding:"),
                                             currentRequest.headers[idx].value.toCharSpan().data(),
                                             currentRequest.headers[idx].value.sizeInBytes(), NSUTF8StringEncoding);
        sc_objc_msgSend<void>(requestObj, sel_getUid("setValue:forHTTPHeaderField:"), headerValue, headerName);
    }

    const uint64_t declaredBodySize =
        currentRequest.body.isStreamed() ? currentRequest.body.sizeInBytes : currentRequest.body.bytes.sizeInBytes();
    if (declaredBodySize > 0 and not httpClientAppleHasHeader(currentRequest, SC::StringSpan("Content-Length")))
    {
        const size_t encodedLength = httpClientAppleWriteUnsigned(declaredBodySize, backendScratch);
        SC_TRY_MSG(encodedLength > 0, "HttpClient: backend scratch too small for Content-Length");
        id headerName  = sc_objc_msgSend<id>(reinterpret_cast<id>(objc_getClass("NSString")),
                                             sel_getUid("stringWithUTF8String:"), "Content-Length");
        id headerValue = sc_objc_msgSend<id>(reinterpret_cast<id>(objc_getClass("NSString")),
                                             sel_getUid("stringWithBytes:length:encoding:"), backendScratch.data(),
                                             encodedLength, NSUTF8StringEncoding);
        sc_objc_msgSend<void>(requestObj, sel_getUid("setValue:forHTTPHeaderField:"), headerValue, headerName);
    }
    if (currentRequest.body.isStreamed() and not httpClientAppleHasHeader(currentRequest, SC::StringSpan("Expect")))
    {
        id headerName  = sc_objc_msgSend<id>(reinterpret_cast<id>(objc_getClass("NSString")),
                                             sel_getUid("stringWithUTF8String:"), "Expect");
        id headerValue = sc_objc_msgSend<id>(reinterpret_cast<id>(objc_getClass("NSString")),
                                             sel_getUid("stringWithUTF8String:"), "");
        sc_objc_msgSend<void>(requestObj, sel_getUid("setValue:forHTTPHeaderField:"), headerValue, headerName);
    }

    if (currentRequest.body.bytes.sizeInBytes() > 0)
    {
        id dataClass = reinterpret_cast<id>(objc_getClass("NSData"));
        id bodyData  = sc_objc_msgSend<id>(dataClass, sel_getUid("dataWithBytes:length:"),
                                           currentRequest.body.bytes.data(), currentRequest.body.bytes.sizeInBytes());
        sc_objc_msgSend<void>(requestObj, sel_getUid("setHTTPBody:"), bodyData);
    }
    else if (currentRequest.body.isStreamed())
    {
        internal.bodyStream = HttpClientAppleCallbacks::createBodyStream(*this);
        SC_TRY_MSG(internal.bodyStream != nullptr, "HttpClient: failed creating request body stream");
        sc_objc_msgSend<void>(requestObj, sel_getUid("setHTTPBodyStream:"), internal.bodyStream);
    }

    internal.task = sc_objc_msgSend<id>(internal.session, sel_getUid("dataTaskWithRequest:"), requestObj);
#if !__has_feature(objc_arc)
    internal.task = sc_objc_msgSend<id>(internal.task, sel_getUid("retain"));
#endif
    SC_TRY_MSG(internal.task != nullptr, "HttpClient: failed creating NSURLSessionDataTask");
    sc_objc_msgSend<void>(internal.task, sel_getUid("resume"));
    return Result(true);
}
