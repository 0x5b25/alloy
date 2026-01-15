#pragma once
//This is a experimental workgraph API targeting only DX12 backend
//Vulkan has experimental extensions in AMDX_**
//Metal API don't have this ability at all
//Possible emulation of this functionality on other APIs:
//    https://github.com/HansKristian-Work/vkd3d-proton/blob/workgraphs/docs/workgraphs.md

#include "common/RefCnt.hpp"

namespace alloy {
    
    class IWorkgraphNode : public common::RefCntBase {

    };

    
    class IWorkgraph : public common::RefCntBase {

    };

} // namespace alloy

