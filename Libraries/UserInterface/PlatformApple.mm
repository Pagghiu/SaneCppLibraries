#include "PlatformApplication.h"
#include "PlatformResource.h"

#if defined(__APPLE__)
#import <Foundation/Foundation.h>
#include <TargetConditionals.h>
#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif
#endif

const char* SC::PlatformResourceLoader::lookupPathNative(char* buffer, int bufferLength, const char* directory,
                                                         const char* file)
{
    char executablePath[2048] = {0};
    if (executablePath[0] == 0)
    {
        const char* resourcePath = [NSBundle.mainBundle.resourcePath cStringUsingEncoding:kCFStringEncodingUTF8];
        strncpy(executablePath, resourcePath, 2048);
    }
    snprintf(buffer, bufferLength, "%s/%s/%s", executablePath, directory, file);
    return buffer;
}

void SC::PlatformApplication::initNative() {}

void SC::PlatformApplication::openFiles()
{
#if TARGET_OS_OSX
    NSArray*     fileTypes = [NSArray arrayWithObjects:@"jpg", @"jpeg", nil];
    NSOpenPanel* panel     = [NSOpenPanel openPanel];
    [panel setAllowsMultipleSelection:NO];
    [panel setCanChooseDirectories:NO];
    [panel setCanChooseFiles:YES];
    [panel setFloatingPanel:YES];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    panel.allowedFileTypes = fileTypes;
#pragma clang diagnostic pop
    NSInteger result = [panel runModal];
    if (result == NSModalResponseOK)
    {
        // return [panel URLs];
    }
    // return nil;
#endif
}

void SC::PlatformApplication::saveFiles()
{
#if TARGET_OS_OSX
    NSArray*     fileTypes = [NSArray arrayWithObjects:@"jpg", @"jpeg", nil];
    NSSavePanel* panel     = [NSSavePanel savePanel];
    [panel setFloatingPanel:YES];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    panel.allowedFileTypes = fileTypes;
#pragma clang diagnostic pop
    NSInteger result = [panel runModal];
    if (result == NSModalResponseOK)
    {
        // return [panel URLs];
    }
#endif
}
