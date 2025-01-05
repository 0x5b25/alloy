#include "D3DBarrierCompatibility.hpp"

#include <vector>
#include <unordered_map>

#include <d3d12.h>
#include <dxgi1_4.h> //Guaranteed by DX12

namespace alloy::dxc
{
    
struct {
    D3D12_BARRIER_SYNC syncScope;
    D3D12_BARRIER_ACCESS allowedAccess;
}
static const
accessAllowedInSyncScope [] = {
    {D3D12_BARRIER_SYNC_NONE, D3D12_BARRIER_ACCESS_NO_ACCESS},
};

} // namespace alloy::dxc
