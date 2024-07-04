// On iOS this is a private API so to use it we must copy at least the API definitions

#ifndef __CFRUNLOOP__
#include <CoreFoundation/CFRunLoop.h>
#endif

#ifndef __CFUUID__
#include <CoreFoundation/CFUUID.h>
#endif

#include <Block.h>
#include <dispatch/dispatch.h>
#include <sys/types.h>

#include <Availability.h>

#pragma once

extern "C"
{

    CF_ASSUME_NONNULL_BEGIN

#pragma pack(push, 2)

    typedef UInt32 FSEventStreamCreateFlags;

    enum
    {

        kFSEventStreamCreateFlagNone       = 0x00000000,
        kFSEventStreamCreateFlagUseCFTypes = 0x00000001,
        kFSEventStreamCreateFlagNoDefer    = 0x00000002,
        kFSEventStreamCreateFlagWatchRoot  = 0x00000004,
        kFSEventStreamCreateFlagIgnoreSelf = 0x00000008,
        kFSEventStreamCreateFlagFileEvents = 0x00000010,
        kFSEventStreamCreateFlagMarkSelf   = 0x00000020,

        kFSEventStreamCreateFlagUseExtendedData = 0x00000040,

        kFSEventStreamCreateFlagFullHistory = 0x00000080,
    };

#define kFSEventStreamEventExtendedDataPathKey CFSTR("path")
#define kFSEventStreamEventExtendedFileIDKey   CFSTR("fileID")
    typedef UInt32 FSEventStreamEventFlags;

    enum
    {
        kFSEventStreamEventFlagNone             = 0x00000000,
        kFSEventStreamEventFlagMustScanSubDirs  = 0x00000001,
        kFSEventStreamEventFlagUserDropped      = 0x00000002,
        kFSEventStreamEventFlagKernelDropped    = 0x00000004,
        kFSEventStreamEventFlagEventIdsWrapped  = 0x00000008,
        kFSEventStreamEventFlagHistoryDone      = 0x00000010,
        kFSEventStreamEventFlagRootChanged      = 0x00000020,
        kFSEventStreamEventFlagMount            = 0x00000040,
        kFSEventStreamEventFlagUnmount          = 0x00000080,
        kFSEventStreamEventFlagItemCreated      = 0x00000100,
        kFSEventStreamEventFlagItemRemoved      = 0x00000200,
        kFSEventStreamEventFlagItemInodeMetaMod = 0x00000400,
        kFSEventStreamEventFlagItemRenamed      = 0x00000800,

        kFSEventStreamEventFlagItemModified      = 0x00001000,
        kFSEventStreamEventFlagItemFinderInfoMod = 0x00002000,
        kFSEventStreamEventFlagItemChangeOwner   = 0x00004000,
        kFSEventStreamEventFlagItemXattrMod      = 0x00008000,
        kFSEventStreamEventFlagItemIsFile        = 0x00010000,

        kFSEventStreamEventFlagItemIsDir          = 0x00020000,
        kFSEventStreamEventFlagItemIsSymlink      = 0x00040000,
        kFSEventStreamEventFlagOwnEvent           = 0x00080000,
        kFSEventStreamEventFlagItemIsHardlink     = 0x00100000,
        kFSEventStreamEventFlagItemIsLastHardlink = 0x00200000,
        kFSEventStreamEventFlagItemCloned         = 0x00400000

    };
    typedef UInt64 FSEventStreamEventId;

    enum
    {
        kFSEventStreamEventIdSinceNow = 0xFFFFFFFFFFFFFFFFULL
    };

    typedef struct __FSEventStream*       FSEventStreamRef;
    typedef const struct __FSEventStream* ConstFSEventStreamRef;
    struct FSEventStreamContext
    {
        CFIndex                                       version;
        void* __nullable                              info;
        CFAllocatorRetainCallBack __nullable          retain;
        CFAllocatorReleaseCallBack __nullable         release;
        CFAllocatorCopyDescriptionCallBack __nullable copyDescription;
    };
    typedef struct FSEventStreamContext FSEventStreamContext;

    typedef CALLBACK_API_C(void, FSEventStreamCallback)(ConstFSEventStreamRef streamRef,
                                                        void* __nullable clientCallBackInfo, size_t numEvents,
                                                        void* eventPaths,
                                                        const FSEventStreamEventFlags* _Nonnull eventFlags,
                                                        const FSEventStreamEventId* _Nonnull eventIds);
    extern FSEventStreamRef __nullable FSEventStreamCreate(CFAllocatorRef __nullable        allocator,
                                                           FSEventStreamCallback            callback,
                                                           FSEventStreamContext* __nullable context,
                                                           CFArrayRef pathsToWatch, FSEventStreamEventId sinceWhen,
                                                           CFTimeInterval latency, FSEventStreamCreateFlags flags);

    extern FSEventStreamRef __nullable FSEventStreamCreateRelativeToDevice(
        CFAllocatorRef __nullable allocator, FSEventStreamCallback callback, FSEventStreamContext* __nullable context,
        dev_t deviceToWatch, CFArrayRef pathsToWatchRelativeToDevice, FSEventStreamEventId sinceWhen,
        CFTimeInterval latency, FSEventStreamCreateFlags flags);

    extern FSEventStreamEventId FSEventStreamGetLatestEventId(ConstFSEventStreamRef streamRef);

    extern dev_t FSEventStreamGetDeviceBeingWatched(ConstFSEventStreamRef streamRef);

    extern CF_RETURNS_RETAINED CFArrayRef FSEventStreamCopyPathsBeingWatched(ConstFSEventStreamRef streamRef);
    extern FSEventStreamEventId           FSEventsGetCurrentEventId(void);

    extern CF_RETURNS_RETAINED CFUUIDRef __nullable FSEventsCopyUUIDForDevice(dev_t dev);

    extern FSEventStreamEventId FSEventsGetLastEventIdForDeviceBeforeTime(dev_t dev, CFAbsoluteTime time);

    extern Boolean FSEventsPurgeEventsForDeviceUpToEventId(dev_t dev, FSEventStreamEventId eventId);

    extern void FSEventStreamRetain(FSEventStreamRef streamRef);

    extern void FSEventStreamRelease(FSEventStreamRef streamRef);

    extern void FSEventStreamScheduleWithRunLoop(FSEventStreamRef streamRef, CFRunLoopRef runLoop,
                                                 CFStringRef runLoopMode)
        API_DEPRECATED("Use FSEventStreamSetDispatchQueue instead.", macos(10.5, 13.0), ios(6.0, 16.0));

    extern void FSEventStreamUnscheduleFromRunLoop(FSEventStreamRef streamRef, CFRunLoopRef runLoop,
                                                   CFStringRef runLoopMode)
        API_DEPRECATED("Use FSEventStreamSetDispatchQueue instead.", macos(10.5, 13.0), ios(6.0, 16.0));

    extern void FSEventStreamSetDispatchQueue(FSEventStreamRef streamRef, dispatch_queue_t __nullable q);

    extern void FSEventStreamInvalidate(FSEventStreamRef streamRef);

    extern Boolean FSEventStreamStart(FSEventStreamRef streamRef);

    extern FSEventStreamEventId FSEventStreamFlushAsync(FSEventStreamRef streamRef);

    extern void FSEventStreamFlushSync(FSEventStreamRef streamRef);

    extern void FSEventStreamStop(FSEventStreamRef streamRef);

    extern void FSEventStreamShow(ConstFSEventStreamRef streamRef);

    extern CF_RETURNS_RETAINED CFStringRef FSEventStreamCopyDescription(ConstFSEventStreamRef streamRef);

#pragma pack(pop)

    CF_ASSUME_NONNULL_END
}
