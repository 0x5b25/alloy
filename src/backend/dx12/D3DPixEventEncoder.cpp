#include "D3DPixEventEncoder.hpp"

#include <cstdint>
#include <vector>

namespace alloy::dxc {


#pragma region PixEventsCommon
//Borrowed from PIXEventsCommon.h

    #define PIX_EVENT_METADATA_NONE                     0x0
    #define PIX_EVENT_METADATA_ON_CONTEXT               0x1
    #define PIX_EVENT_METADATA_STRING_IS_ANSI           0x2
    #define PIX_EVENT_METADATA_HAS_COLOR                0xF0

    enum PIXEventType : std::uint8_t
    {
        PIXEvent_EndEvent       = 0x00,
        PIXEvent_BeginEvent     = 0x01,
        PIXEvent_SetMarker      = 0x02,
    };
    
    //Bits 7-19 (13 bits)
    static constexpr std::uint64_t PIXEventsBlockEndMarker     = 0x00000000000FFF80;

    // V2 events

    // Bits 00..06 (7 bits) - Size in QWORDS
    static constexpr std::uint64_t PIXEventsSizeWriteMask      = 0x000000000000007F;
    static constexpr std::uint64_t PIXEventsSizeBitShift       = 0;
    static constexpr std::uint64_t PIXEventsSizeReadMask       = PIXEventsSizeWriteMask << PIXEventsSizeBitShift;
    static constexpr std::uint64_t PIXEventsSizeMax            = (1ull << 7) - 1ull;

    // Bits 07..11 (5 bits) - Event Type
    static constexpr std::uint64_t PIXEventsTypeWriteMask      = 0x000000000000001F;
    static constexpr std::uint64_t PIXEventsTypeBitShift       = 7;
    static constexpr std::uint64_t PIXEventsTypeReadMask       = PIXEventsTypeWriteMask << PIXEventsTypeBitShift;

    // Bits 12..19 (8 bits) - Event Specific Metadata
    static constexpr std::uint64_t PIXEventsMetadataWriteMask  = 0x00000000000000FF;
    static constexpr std::uint64_t PIXEventsMetadataBitShift   = 12;
    static constexpr std::uint64_t PIXEventsMetadataReadMask   = PIXEventsMetadataWriteMask << PIXEventsMetadataBitShift;

    // Buts 20..63 (44 bits) - Timestamp
    static constexpr std::uint64_t PIXEventsTimestampWriteMask = 0x00000FFFFFFFFFFF;
    static constexpr std::uint64_t PIXEventsTimestampBitShift  = 20;
    static constexpr std::uint64_t PIXEventsTimestampReadMask  = PIXEventsTimestampWriteMask << PIXEventsTimestampBitShift;

    inline std::uint64_t PIXEncodeEventInfo(std::uint64_t timestamp, PIXEventType eventType, std::uint8_t eventSize, std::uint8_t eventMetadata)
    {
        return
            ((timestamp & PIXEventsTimestampWriteMask) << PIXEventsTimestampBitShift) |
            (((std::uint64_t)eventType & PIXEventsTypeWriteMask) << PIXEventsTypeBitShift) |
            (((std::uint64_t)eventMetadata & PIXEventsMetadataWriteMask) << PIXEventsMetadataBitShift) |
            (((std::uint64_t)eventSize & PIXEventsSizeWriteMask) << PIXEventsSizeBitShift);
    }

#pragma endregion

#pragma region Pix3Win
//Borrowed from pix3_win.h


// The following WINPIX_EVENT_* defines denote the different metadata values that have
// been used by tools to denote how to parse pix marker event data. The first two values
// are legacy values used by pix.h in the Windows SDK.
#define WINPIX_EVENT_UNICODE_VERSION 0
#define WINPIX_EVENT_ANSI_VERSION 1

// These values denote PIX marker event data that was created by the WinPixEventRuntime.
// In early 2023 we revised the PIX marker format and defined a new version number.
#define WINPIX_EVENT_PIX3BLOB_VERSION 2
#define WINPIX_EVENT_PIX3BLOB_V2 6345127 // A number that other applications are unlikely to have used before

// For backcompat reasons, the WinPixEventRuntime uses the older PIX3BLOB format when it passes data
// into the D3D12 runtime. It will be updated to use the V2 format in the future.
#define D3D12_EVENT_METADATA WINPIX_EVENT_PIX3BLOB_VERSION


inline void PIXInsertGPUMarkerOnContextForBeginEvent(_In_ ID3D12GraphicsCommandList* commandList, UINT8 eventType, _In_reads_bytes_(size) void* data, UINT size)
{
    UNREFERENCED_PARAMETER(eventType);
    commandList->BeginEvent(WINPIX_EVENT_PIX3BLOB_V2, data, size);
}


inline void PIXInsertGPUMarkerOnContextForSetMarker(_In_ ID3D12GraphicsCommandList* commandList, UINT8 eventType, _In_reads_bytes_(size) void* data, UINT size)
{
    UNREFERENCED_PARAMETER(eventType);
    commandList->SetMarker(WINPIX_EVENT_PIX3BLOB_V2, data, size);
}

inline void PIXInsertGPUMarkerOnContextForEndEvent(_In_ ID3D12GraphicsCommandList* commandList, UINT8, void*, UINT)
{
    commandList->EndEvent();
}

#pragma endregion

#pragma region PixEvents
    static constexpr uint64_t PixEventMaxSizeQwords = 255; //8 bit encoding for eventSize in PIX event header
    static constexpr uint64_t PixEventHeaderSizeQwords = 3; //format: <eventHeader, color, context> <str content>
    static constexpr uint64_t PixEventMaxPayloadSizeQwords = PixEventMaxSizeQwords - PixEventHeaderSizeQwords;

//Borrowed from PIXEvents.h
    void EncodePIXBeginEvent(ID3D12GraphicsCommandList* context, std::uint64_t color, const std::string& str)
    {
        //Small buffer optimization. Use 64 byte stack space
        static constexpr auto StrStoreSpaceQwordsAvail = (8 - PixEventHeaderSizeQwords);
        std::uint64_t buffer[8];

        std::uint64_t* destination = buffer;
        std::uint8_t* pBufferBegin = (std::uint8_t*)buffer;
        std::uint8_t* pBufferEnd = pBufferBegin + sizeof(buffer) - 1;

        //Need to store the null termination
        auto strSizeBytes = std::min(PixEventMaxPayloadSizeQwords * 8, str.size() + 1);
        //String size rounded up to QWORDS
        auto strSizeQwords = (strSizeBytes + 7) / 8;

        std::vector<std::uint64_t> largeBuffer;
        if(StrStoreSpaceQwordsAvail < strSizeQwords) {
            largeBuffer.resize(strSizeQwords + PixEventHeaderSizeQwords, 0);
            destination = largeBuffer.data();
            pBufferBegin = (std::uint8_t*)largeBuffer.data();
            pBufferEnd = pBufferBegin + largeBuffer.size() * 8 - 1;
        }
        
        std::uint8_t eventSizeQwords = strSizeQwords + PixEventHeaderSizeQwords;

        {
            std::uint64_t* eventDestination = destination++;
            *destination++ = color;

            //PIXCopyStringArgumentsWithContext(destination, limit, context, formatString, args...);
            *destination++ = (uintptr_t)context;

            //Clamp if string is too long
            memcpy(destination, str.data(), strSizeBytes);

            //Add null terminator
            *pBufferEnd = 0;

            const std::uint8_t eventMetadata =
                PIX_EVENT_METADATA_ON_CONTEXT |
                PIX_EVENT_METADATA_STRING_IS_ANSI |
                PIX_EVENT_METADATA_HAS_COLOR;

            *eventDestination = PIXEncodeEventInfo(0, PIXEvent_BeginEvent, eventSizeQwords, eventMetadata);

            PIXInsertGPUMarkerOnContextForBeginEvent(
                context,
                PIXEvent_BeginEvent,
                /*data*/pBufferBegin, 
                /*size*/eventSizeQwords * 8
            );

        }
    }

    void EncodePIXEndEvent(ID3D12GraphicsCommandList* context)
    {
        std::uint64_t* destination = nullptr;

        {
            std::uint64_t buffer[8];
            destination = buffer;

            std::uint64_t* eventDestination = destination++;

            const std::uint8_t eventSize = static_cast<const std::uint8_t>(destination - buffer);
            const std::uint8_t eventMetadata = PIX_EVENT_METADATA_NONE;
            *eventDestination = PIXEncodeEventInfo(0, PIXEvent_EndEvent, eventSize, eventMetadata);
            PIXInsertGPUMarkerOnContextForEndEvent(context, PIXEvent_EndEvent, static_cast<void*>(buffer), static_cast<UINT>(reinterpret_cast<BYTE*>(destination) - reinterpret_cast<BYTE*>(buffer)));

        }
    }

    void EncodePIXMarker(ID3D12GraphicsCommandList* context, std::uint64_t color, const std::string& str)
    {
        //Small buffer optimization. Use 64 byte stack space
        static constexpr auto StrStoreSpaceQwordsAvail = (8 - PixEventHeaderSizeQwords);
        std::uint64_t buffer[8];

        std::uint64_t* destination = buffer;
        std::uint8_t* pBufferBegin = (std::uint8_t*)buffer;
        std::uint8_t* pBufferEnd = pBufferBegin + sizeof(buffer) - 1;

        //Need to store the null termination
        auto strSizeBytes = std::min(PixEventMaxPayloadSizeQwords * 8, str.size() + 1);
        //String size rounded up to QWORDS
        auto strSizeQwords = (strSizeBytes + 7) / 8;

        std::vector<std::uint64_t> largeBuffer;
        if(StrStoreSpaceQwordsAvail < strSizeQwords) {
            largeBuffer.resize(strSizeQwords + PixEventHeaderSizeQwords, 0);
            destination = largeBuffer.data();
            pBufferBegin = (std::uint8_t*)largeBuffer.data();
            pBufferEnd = pBufferBegin + largeBuffer.size() * 8 - 1;
        }
        
        std::uint8_t eventSizeQwords = strSizeQwords + PixEventHeaderSizeQwords;


        {
            std::uint64_t* eventDestination = destination++;
            *destination++ = color;

            //PIXCopyStringArgumentsWithContext(destination, limit, context, formatString, args...);
            *destination++ = (uintptr_t)context;

            //Clamp if string is too long
            memcpy(destination, str.data(), strSizeBytes);

            //Add null terminator
            *pBufferEnd = 0;

            const std::uint8_t eventMetadata =
                PIX_EVENT_METADATA_ON_CONTEXT |
                PIX_EVENT_METADATA_STRING_IS_ANSI |
                PIX_EVENT_METADATA_HAS_COLOR;
            *eventDestination = PIXEncodeEventInfo(0, PIXEvent_SetMarker, eventSizeQwords, eventMetadata);
            PIXInsertGPUMarkerOnContextForSetMarker(
                context,
                PIXEvent_SetMarker, 
                /*data*/pBufferBegin, 
                /*size*/eventSizeQwords * 8
            );

        }
    }


#pragma endregion


}
