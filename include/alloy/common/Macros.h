#pragma once

#define DISABLE_COPY_AND_ASSIGN(Class) \
  Class(const Class &) = delete; \
  Class &operator=(const Class &) = delete



// DLL/.so exports.
#if !defined(VLD_IMPLEMENTATION)
    #define VLD_IMPLEMENTATION 0
#endif
#if !defined(VLD_API)
    #if defined(VLD_DLL)
        #if defined(_MSC_VER)
            #if VLD_IMPLEMENTATION
                #define VLD_API __declspec(dllexport)
            #else
                #define VLD_API __declspec(dllimport)
            #endif
        #else
            #define VLD_API __attribute__((visibility("default")))
        #endif
    #else
        #define VLD_API
    #endif
#endif

//Build config
#if !defined(VLD_DEBUG) && !defined(VLD_RELEASE)
    #ifdef NDEBUG
        #define VLD_RELEASE
    #else
        #define VLD_DEBUG
    #endif
#endif

#if defined(VLD_DEBUG) && defined(VLD_RELEASE)
#  error "cannot define both VLD_DEBUG and VLD_RELEASE"
#elif !defined(VLD_DEBUG) && !defined(VLD_RELEASE)
#  error "must define either VLD_DEBUG or VLD_RELEASE"
#endif

#ifdef VLD_DEBUG
#define DEBUGCODE(code) \
        do{ \
            code; \
        }while(0)
#else
#define DEBUGCODE(code)
#endif

//Platform definitions
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
   //define something for Windows (32-bit and 64-bit, this part is common)
    #define VLD_PLATFORM_WIN32
    #define VLD_PLATFORM VLD_PLATFORM_WIN32
    #ifdef _WIN64
   //define something for Windows (64-bit only)
    #else
   //define something for Windows (32-bit only)
    #endif
#elif __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR
    // iOS, tvOS, or watchOS Simulator
        #define VLD_PLATFORM_IOS_SIM 1
        #define VLD_PLATFORM VLD_PLATFORM_IOS_SIM
    #elif TARGET_OS_MACCATALYST
     // Mac's Catalyst (ports iOS API into Mac, like UIKit).
        #define VLD_PLATFORM_MACCATALYST 1
        #define VLD_PLATFORM VLD_PLATFORM_MACCATALYST
    #elif TARGET_OS_IPHONE
    // iOS, tvOS, or watchOS device
        #define VLD_PLATFORM_IOS 1
        #define VLD_PLATFORM VLD_PLATFORM_IOS
    #elif TARGET_OS_MAC
    // Other kinds of Apple platforms
        #define VLD_PLATFORM_MACOS 1
        #define VLD_PLATFORM VLD_PLATFORM_MACOS
    #else
    #   error "Unknown Apple platform"
    #endif
#elif __ANDROID__
    // Below __linux__ check should be enough to handle Android,
    // but something may be unique to Android.
    #define VLD_PLATFORM_ANDROID
    #define VLD_PLATFORM VLD_PLATFORM_ANDROID
#elif __linux__
    // linux
    #define VLD_PLATFORM_LINUX
    #define VLD_PLATFORM VLD_PLATFORM_LINUX
#elif __unix__ // all unices not caught above
    // Unix
#elif defined(_POSIX_VERSION)
    // POSIX
#else
#   error "Unknown compiler"
#endif

#ifndef VLD_PLATFORM
    #error "Unknown alloy platform"
#endif

