// MachOPatcher.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cassert>
#include <format>
#include <cstring>

//Disable warning of using offsetof() on inherited structs
// add static_assert() to ensure layout
#pragma clang diagnostic ignored "-Winvalid-offsetof"

std::string ReadFile(const std::string& filePath) {

    std::streampos fsize = 0;
    std::ifstream file(filePath, std::ios::binary);

    assert(file);

    //std::vector<uint8_t> buffer{ (std::istreambuf_iterator<char>(file)),
    //             std::istreambuf_iterator<char>() };
    //
    //return buffer;
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    return buffer.str();
}

void WriteFile(const std::string& fileContent, const std::string& filePath) {

    std::ofstream file(filePath, std::ios::binary);

    std::ofstream fs(filePath, std::ios::out | std::ios::binary | std::ios::trunc);
    fs.write(fileContent.data(), fileContent.size());
    fs.close();
}

constexpr uint32_t CPU_TYPE_ABI64 = 0x01000000;
constexpr uint32_t CPU_TYPE_ABI32 = 0x02000000;

enum CPU_TYPE : uint32_t {
    CPU_TYPE_ANY  = 0xffffffff,
    CPU_TYPE_VAX        = 0x1,
    CPU_TYPE_I386       = 0x7,
    CPU_TYPE_ARM        = 0xC,
    CPU_TYPE_POWERPC    = 0x12,
};

// load commands added after OS X 10.1 need to be bitwise ORed with
// LC_REQ_DYLD to be recognized by the dynamic linker(dyld)
// @api private
constexpr uint32_t LC_REQ_DYLD = 0x80000000;

#define FOR_EACH_LOAD_COMMAND_ENUM(V) \
    V(LC_SEGMENT, 0x1) \
    V(LC_SYMTAB, 0x2) \
    V(LC_SYMSEG, 0x3) \
    V(LC_THREAD, 0x4) \
    V(LC_UNIXTHREAD, 0x5) \
    V(LC_LOADFVMLIB, 0x6) \
    V(LC_IDFVMLIB, 0x7) \
    V(LC_IDENT, 0x8) \
    V(LC_FVMFILE, 0x9) \
    V(LC_PREPAGE, 0xa) \
    V(LC_DYSYMTAB, 0xb) \
    V(LC_LOAD_DYLIB, 0xc) \
    V(LC_ID_DYLIB, 0xd) \
    V(LC_LOAD_DYLINKER, 0xe) \
    V(LC_ID_DYLINKER, 0xf) \
    V(LC_PREBOUND_DYLIB, 0x10) \
    V(LC_ROUTINES, 0x11) \
    V(LC_SUB_FRAMEWORK, 0x12) \
    V(LC_SUB_UMBRELLA, 0x13) \
    V(LC_SUB_CLIENT, 0x14) \
    V(LC_SUB_LIBRARY, 0x15) \
    V(LC_TWOLEVEL_HINTS, 0x16) \
    V(LC_PREBIND_CKSUM, 0x17) \
    V(LC_LOAD_WEAK_DYLIB, (LC_REQ_DYLD | 0x18)) \
    V(LC_SEGMENT_64, 0x19) \
    V(LC_ROUTINES_64, 0x1a) \
    V(LC_UUID, 0x1b) \
    V(LC_RPATH, (LC_REQ_DYLD | 0x1c)) \
    V(LC_CODE_SIGNATURE, 0x1d) \
    V(LC_SEGMENT_SPLIT_INFO, 0x1e) \
    V(LC_REEXPORT_DYLIB, (LC_REQ_DYLD | 0x1f)) \
    V(LC_LAZY_LOAD_DYLIB, 0x20) \
    V(LC_ENCRYPTION_INFO, 0x21) \
    V(LC_DYLD_INFO, 0x22) \
    V(LC_DYLD_INFO_ONLY, (LC_REQ_DYLD | 0x22)) \
    V(LC_LOAD_UPWARD_DYLIB, (LC_REQ_DYLD | 0x23)) \
    V(LC_VERSION_MIN_MACOSX, 0x24) \
    V(LC_VERSION_MIN_IPHONEOS, 0x25) \
    V(LC_FUNCTION_STARTS, 0x26) \
    V(LC_DYLD_ENVIRONMENT, 0x27) \
    V(LC_MAIN, (LC_REQ_DYLD | 0x28)) \
    V(LC_DATA_IN_CODE, 0x29) \
    V(LC_SOURCE_VERSION, 0x2a) \
    V(LC_DYLIB_CODE_SIGN_DRS, 0x2b) \
    V(LC_ENCRYPTION_INFO_64, 0x2c) \
    V(LC_LINKER_OPTION, 0x2d) \
    V(LC_LINKER_OPTIMIZATION_HINT, 0x2e) \
    V(LC_VERSION_MIN_TVOS, 0x2f) \
    V(LC_VERSION_MIN_WATCHOS, 0x30) \
    V(LC_NOTE, 0x31) \
    V(LC_BUILD_VERSION, 0x32) \
    V(LC_DYLD_EXPORTS_TRIE, (LC_REQ_DYLD | 0x33)) \
    V(LC_DYLD_CHAINED_FIXUPS, (LC_REQ_DYLD | 0x34)) \
    V(LC_FILESET_ENTRY, (LC_REQ_DYLD | 0x35)) \
    V(LC_ATOM_INFO, 0x36)

#define ENUM_BODY(tag, val) tag = val,

enum LOAD_COMMAND : uint32_t {
    FOR_EACH_LOAD_COMMAND_ENUM(ENUM_BODY)
};


#define ENUM_NAME(tag, val) if(val == in) return #tag ;

std::string GetLoadCommandName(LOAD_COMMAND in) {
    FOR_EACH_LOAD_COMMAND_ENUM(ENUM_NAME)

    return "???";
}

template<typename T, bool bigEndian>
T Read(const void* data) {
    auto byteCnt = sizeof(T);
    T val { };
    for (uint32_t i = 0; i < byteCnt; i++) {
        auto byteIdx = bigEndian ? i : (byteCnt - i - 1);
        val = (val << 8) | ((const uint8_t*)data)[byteIdx];
    }

    return val;
}

constexpr auto ReadDwordBigEndian = Read<uint32_t, true>;
constexpr auto ReadDwordLittleEndian = Read<uint32_t, false>;

template<typename T>
struct MachFileSegment {
    T segHeader;
    const void* pSegStart;
};

struct _MachHeader {
    uint32_t magic;
    uint32_t cpuType;
    uint32_t cpuSubType;
    uint32_t fileType;
    uint32_t loadCommandCnt;
    uint32_t loadCommandSize;
    uint32_t flags;
    uint32_t rsvd;

    bool Read(const void* data) {
        auto pData = (const uint32_t*)data;
        magic           = ReadDwordLittleEndian(pData + 0);
        cpuType         = ReadDwordLittleEndian(pData + 1);
        cpuSubType      = ReadDwordLittleEndian(pData + 2);
        fileType        = ReadDwordLittleEndian(pData + 3);
        loadCommandCnt  = ReadDwordLittleEndian(pData + 4);
        loadCommandSize = ReadDwordLittleEndian(pData + 5);
        flags           = ReadDwordLittleEndian(pData + 6);
        rsvd            = ReadDwordLittleEndian(pData + 7);

        return magic == 0xfeedface || magic == 0xfeedfacf;
    }
};


struct _MachLoadCmdHeader {
    LOAD_COMMAND cmdType;
    uint32_t cmdSize;


    bool Read(const void* data) {
        auto pData = (const uint32_t*)data;
        cmdType = (LOAD_COMMAND)ReadDwordLittleEndian(pData + 0);
        cmdSize = ReadDwordLittleEndian(pData + 1);
        return true;
    }
};

struct MachLoadCommand {
    MachFileSegment<_MachLoadCmdHeader> header;

    std::vector<uint8_t> rawData;

    uint32_t Read(const void* data) {
        auto pData = (const uint32_t*)data;
        header.pSegStart = data;
        header.segHeader.Read(data);
        rawData.resize(header.segHeader.cmdSize);
        memcpy(rawData.data(), data, header.segHeader.cmdSize);

        return header.segHeader.cmdSize;
    }
};


struct MachImage {
    MachFileSegment<_MachHeader> header;

    std::vector<MachLoadCommand> loadCommands;

    bool Read(const void* data) {
        header.pSegStart = data;

        auto pData = (uint8_t*)data;

        if (!header.segHeader.Read(pData)) {
            return false;
        }

        pData += sizeof(_MachHeader);

        for (int i = 0; i < header.segHeader.loadCommandCnt; i++) {

            auto& lc = loadCommands.emplace_back();

            auto lcSize = lc.Read(pData);

            pData += lcSize;
        }

        return true;
    }
};


struct _MachMultiArcFileEntry {
    uint32_t cpuType;
    uint32_t cpuSubType;
    uint32_t fileOffset;
    uint32_t size;
    uint32_t sectionAlignPow2;

    bool Read(const void* data) {
        auto pData = (const uint32_t*)data;
        cpuType          = ReadDwordBigEndian(pData + 0);
        cpuSubType       = ReadDwordBigEndian(pData + 1);
        fileOffset       = ReadDwordBigEndian(pData + 2);
        size             = ReadDwordBigEndian(pData + 3);
        sectionAlignPow2 = ReadDwordBigEndian(pData + 4);

        return true;
    }
};


struct _MachMultiArchHeader {
    uint32_t magic;
    uint32_t numOfBinaries;

    bool Read(const void* data) {
        auto pData = (const uint32_t*)data;
        magic = ReadDwordBigEndian(pData + 0);
        numOfBinaries = ReadDwordBigEndian(pData + 1);

        return magic == 0xcafebabe;
    }
};



struct MachMultiArchImage {
    MachFileSegment<_MachMultiArchHeader> header;

    struct Entry {
        MachFileSegment <_MachMultiArcFileEntry> header;
        MachImage image;
    };

    std::vector<Entry> entries;


    bool Read(const void* data) {
        header.pSegStart = data;
        auto pData = (uint8_t*)data;
        if (!header.segHeader.Read(pData))
            return false;

        pData += sizeof(_MachMultiArchHeader);
        
        for (uint32_t i = 0; i < header.segHeader.numOfBinaries; i++) {

            auto& entry = entries.emplace_back();
            entry.header.pSegStart = pData;
            entry.header.segHeader.Read(pData);

            pData += sizeof(_MachMultiArcFileEntry);
        }

        for (auto& entry : entries) {

            auto pImgData = (uint8_t*)data;
            pImgData += entry.header.segHeader.fileOffset;

            if (!entry.image.Read(pImgData)) {
                return false;
            }
        }

        return true;
    }
};

struct _DylibHeader : public _MachLoadCmdHeader
{
    uint32_t nameOffset;
    uint32_t timestamp;
    uint32_t currentVersion;
    uint32_t compatibilityVersion;


    void Read(const void* data) {
        _MachLoadCmdHeader::Read(data);
        auto pData = (const uint32_t*)((uint8_t*)data + sizeof(_MachLoadCmdHeader));
        nameOffset = ReadDwordLittleEndian(pData + 0);
        timestamp = ReadDwordLittleEndian(pData + 1);
        currentVersion = ReadDwordLittleEndian(pData + 2);
        compatibilityVersion = ReadDwordLittleEndian(pData + 3);
    }
};
static_assert(offsetof(_DylibHeader, nameOffset) == sizeof(_MachLoadCmdHeader));

struct LoadCommand_DYLIB
{
    MachFileSegment<_DylibHeader> header;
    char* pName;
    
    void Read(const void* data) {

        auto pData = (const uint32_t*)data;
        header.pSegStart = data;
        header.segHeader.Read(data);

        pName = (char*)data + header.segHeader.nameOffset;
    }
};

void Dump_DYLIB(const void* data, const char* prefix) {
    LoadCommand_DYLIB lc{};
    lc.Read(data);

    std::cout << prefix << "timestamp: " << lc.header.segHeader.timestamp << "\n";
    std::cout << prefix << "currentVersion: " << lc.header.segHeader.currentVersion << "\n";
    std::cout << prefix << "compatibilityVersion: " << lc.header.segHeader.compatibilityVersion << "\n";
    std::cout << prefix << "name: " << lc.pName << std::endl;
}


#define FOR_EACH_PLATFORM_TYPE_ENUM(V) \
    V( PLATOFRM_macOS, 0x00000001 )\
    V( PLATOFRM_iOS, 0x00000002 )\
    V( PLATOFRM_tvOS, 0x00000003 )\
    V( PLATOFRM_watchOS, 0x00000004 )\
    V( PLATOFRM_bridgeOS, 0x00000005 )\
    V( PLATOFRM_MacCatalyst, 0x00000006 )\
    V( PLATOFRM_iOSSimulator, 0x00000007 )\
    V( PLATOFRM_tvOSSimulator, 0x00000008 )\
    V( PLATOFRM_watchOSSimulator, 0x00000009 )\
    V( PLATOFRM_DriverKit, 0x0000000A )\
    V( PLATOFRM_visionOS, 0x0000000B )\
    V( PLATOFRM_visionOSSimulator, 0x0000000C )

enum PLATFORM_TYPE : uint32_t {
    FOR_EACH_PLATFORM_TYPE_ENUM(ENUM_BODY)
};

std::string GetPlatformTypeName(PLATFORM_TYPE in) {
    FOR_EACH_PLATFORM_TYPE_ENUM(ENUM_NAME)

        return "???";
}

struct _BuildVersionHeader : public _MachLoadCmdHeader {
    PLATFORM_TYPE platformType;
    uint32_t minOSVersion;
    uint32_t minSDKVersion;
    uint32_t numOfTools;

    void Read(const void* data) {

        _MachLoadCmdHeader::Read(data);
        auto pData = (const uint32_t*)((uint8_t*)data + sizeof(_MachLoadCmdHeader));
        platformType = (PLATFORM_TYPE)ReadDwordLittleEndian(pData + 0);
        minOSVersion = ReadDwordLittleEndian(pData + 1);
        minSDKVersion = ReadDwordLittleEndian(pData + 2);
        numOfTools = ReadDwordLittleEndian(pData + 3);
    }
};

static_assert(offsetof(_BuildVersionHeader, platformType) == sizeof(_MachLoadCmdHeader));

struct LoadCommand_BUILD_VERSION
{
    MachFileSegment<_BuildVersionHeader> header;

    void Read(const void* data) {
        auto pData = (const uint32_t*)data;
        header.pSegStart = data;
        header.segHeader.Read(data);
    }
};

void Dump_BUILD_VERSION(const void* data, const char* prefix) {

    LoadCommand_BUILD_VERSION bv{};
    bv.Read(data);

    auto _GetVerStr = [&](uint32_t ver) {
        uint32_t major = (ver >> 16) & 0xff;
        uint32_t minor = (ver >> 8) & 0xff;
        uint32_t patch = (ver) & 0xff;

        return std::format("{}.{}.{}", major, minor, patch);
    };

    std::cout << prefix << "platform: " << GetPlatformTypeName(bv.header.segHeader.platformType) << "\n";
    std::cout << prefix << "minOSVersion: " << _GetVerStr(bv.header.segHeader.minOSVersion) << "\n";
    std::cout << prefix << "minSDKVersion: " << _GetVerStr(bv.header.segHeader.minSDKVersion) << "\n";
    std::cout << prefix << "numOfTools: " << bv.header.segHeader.numOfTools << std::endl;
}

void DumpInfo(const MachImage& img) {
    
    auto arch = img.header.segHeader.cpuType;
    std::cout << "CPU Architecture: ";
    switch (arch & 0xffff) {
    case CPU_TYPE_VAX: std::cout << "VAX"; break;
    case CPU_TYPE_I386: std::cout << "x86"; break;
    case CPU_TYPE_ARM: std::cout << "arm"; break;
    case CPU_TYPE_POWERPC: std::cout << "powerpc"; break;
    default: std::cout << "unknown"; break;
    }

    if (arch & CPU_TYPE_ABI64) {
        std::cout << " 64-bit";
    }

    std::cout << std::endl;


    std::cout << "    load commands: " << img.loadCommands.size();
    std::cout << std::endl;
    for (int i = 0; i < img.loadCommands.size(); i++) {
        auto& lc = img.loadCommands[i];
        std::cout << std::format("        [ #{:3} ] ", i);
        std::cout << GetLoadCommandName(lc.header.segHeader.cmdType);
        std::cout << " size: " << lc.header.segHeader.cmdSize << " bytes";
        std::cout << std::endl;

        if (lc.header.segHeader.cmdType == LC_ID_DYLIB ||
            lc.header.segHeader.cmdType == LC_LOAD_DYLIB) {
            Dump_DYLIB(lc.rawData.data(), "            ");
        }
        else if (lc.header.segHeader.cmdType == LC_BUILD_VERSION) {
            Dump_BUILD_VERSION(lc.rawData.data(), "            ");
        }
    }
}


void PatchOSVersion(const MachImage& img, PLATFORM_TYPE type, uint32_t osVersion) {
    for (auto& lc : img.loadCommands) {
        if (lc.header.segHeader.cmdType == LC_BUILD_VERSION) {
            auto dataPtr = (uint64_t)lc.header.pSegStart;

            auto osTypeOff = offsetof(_BuildVersionHeader, platformType);
            auto osVerOff = offsetof(_BuildVersionHeader, minOSVersion);
            auto sdkVerOff = offsetof(_BuildVersionHeader, minSDKVersion);

            *((uint32_t*)(dataPtr + osTypeOff)) = type;
            *((uint32_t*)(dataPtr + osVerOff)) = osVersion;
            *((uint32_t*)(dataPtr + sdkVerOff)) = osVersion;

            return;
        }
    }
}


void PatchVersionedFrameworkDeps(const MachImage& img) {
    for (auto& lc : img.loadCommands) {
        if (lc.header.segHeader.cmdType == LC_LOAD_DYLIB) {
            auto dataPtr = (uint64_t)lc.header.pSegStart;

            LoadCommand_DYLIB lc_dylib{};
            lc_dylib.Read(lc.header.pSegStart);

            constexpr auto versionPrefix = "/Versions/";
            auto strLen = strlen(lc_dylib.pName);

            auto subStr = strstr(lc_dylib.pName, versionPrefix);

            if (subStr != nullptr) {
                // normally this path looks like "***.framework/Versions/A/***"
                // find out the "A/" part
                auto versionPrefixLen = strlen(versionPrefix);
                auto trail = subStr + versionPrefixLen;
                auto binaryStr = strstr(trail, "/");

                //Copy the remainder
                auto copyLen = strLen - (binaryStr - lc_dylib.pName);
                memcpy(subStr, binaryStr, copyLen);

                //Zero out the version part
                auto versionPartLen = binaryStr - subStr;
                auto newEnd = subStr + copyLen;
                memset(newEnd, 0, versionPartLen);

                continue;
            }
        }
    }
}

constexpr auto IOSVer = (17 << 16) | (0 << 8) | 0;

void PatchForIOS(const MachImage& img) {
    PatchOSVersion(img, PLATOFRM_iOS, IOSVer);
    PatchVersionedFrameworkDeps(img);
}

void PatchForIOSSim(const MachImage& img) {
    PatchOSVersion(img, PLATOFRM_iOSSimulator, IOSVer);
    PatchVersionedFrameworkDeps(img);
}

void PrintUsage() {

    std::cout << "Usage: patcher <original.dylib> <ios/iosSim> <output.dylib>";
}

int main(int argc, char* argv[])
{
    if (argc != 4) {
        std::cout << "Too few arguments. ";
        PrintUsage();
        return 1;
    }

    std::string readPath = argv[1];
    std::string type = argv[2];
    std::string writePath = argv[3];

    bool patchForSim = false;
    if (type == "iosSim") {
        patchForSim = true;
    }
    else if (type != "ios") {
        std::cout << "Unknown platform: " << type << " ";
        PrintUsage();
        return 1;
    }


    std::cout << "reading file" << readPath << "..." << std::endl;
    auto libContent = ReadFile(readPath);


    std::cout << "patching file..." << std::endl;
    MachMultiArchImage img{};

    img.Read(libContent.data());

    for (auto& lib : img.entries) {
        //DumpInfo(lib.image);
        if (patchForSim) {
            PatchForIOSSim(lib.image);

        } else {
            PatchForIOS(lib.image);
        }
    }

    std::cout << "writing file" << writePath << "..." << std::endl;

    WriteFile(libContent, writePath);


    std::cout << "patch done!" << std::endl;

    return 0;

}
