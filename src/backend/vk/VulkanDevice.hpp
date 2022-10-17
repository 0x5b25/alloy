#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include "veldrid/common/RefCnt.hpp"

#include "veldrid/GraphicsDevice.hpp"
#include "veldrid/Fence.hpp"
#include "veldrid/Buffer.hpp"
#include "veldrid/SwapChain.hpp"

#include <deque>
#include <map>
#include <thread>
#include <mutex>

#include "VkDescriptorPoolMgr.hpp"

class _VkCtx;

namespace Veldrid
{
    class VulkanBuffer;
    class VulkanDevice;

    //Manage command pools, to achieve one command pool per thread
    class _CmdPoolMgr {
        friend class VulkanDevice;
        
    public:

        struct Container : public RefCntBase {
            VkCommandPool pool;
            _CmdPoolMgr* mgr;
            std::thread::id boundID;

            ~Container() {
                mgr->_ReleaseCmdPoolHolder(this);
            }

        };

    private:
        VkDevice _dev;
        std::uint32_t _queueFamily;

        std::deque<VkCommandPool> _freeCmdPools;
        std::map<std::thread::id, Container*> _threadBoundCmdPools;
        std::mutex _m_cmdPool;

        void _ReleaseCmdPoolHolder(Container* holder);

        sp<Container> _AcquireCmdPoolHolder();

    public:
        _CmdPoolMgr() { }

        ~_CmdPoolMgr() {
            //Threoretically there should be no bound command pools,
            // i.e. all command buffers holded by threads should be released
            // then the VulkanDevice can be destroyed.
            assert(_threadBoundCmdPools.empty());
            for (auto p : _freeCmdPools) {
                vkDestroyCommandPool(_dev, p, nullptr);
            }
        }

        void Init(VkDevice dev, std::uint32_t queueFamily){
            _dev = dev; _queueFamily = queueFamily;
        }

        sp<Container> GetOnePool() { return _AcquireCmdPoolHolder(); }
    };

    class VulkanDevice : public GraphicsDevice {

    public:
        union Features {
            struct {
                std::uint32_t hasComputeCap : 1;
                std::uint32_t hasUniqueCpyQueue : 1;
                std::uint32_t hasUniqueComputeQueue : 1;

                std::uint32_t supportsDebug : 1;
                std::uint32_t supportsPresent : 1;
                std::uint32_t supportsGetMemReq2 : 1;
                std::uint32_t supportsDedicatedAlloc : 1;

                std::uint32_t supportsMaintenance1 : 1;
                std::uint32_t supportsDrvPropQuery : 1;

            };
            std::uint32_t value;
        };

    private:

        sp<_VkCtx> _ctx;

        VkPhysicalDevice _phyDev;
        VkDevice _dev;
        VmaAllocator _allocator;
        _CmdPoolMgr _cmdPoolMgr;
        _DescriptorPoolMgr _descPoolMgr;

        VkQueue _queueGraphics, _queueCopy, _queueCompute;

        VkSurfaceKHR _surface;

        Features _features;
        GraphicsApiVersion _apiVer;

        std::string _devName, _devVendor;
        std::string _drvName, _drvInfo;
        GraphicsDevice::Features _commonFeat;

        VulkanDevice() = default;

    public:

        ~VulkanDevice();

        static sp<GraphicsDevice> Make(
            const GraphicsDevice::Options& options,
            SwapChainSource* swapChainSource = nullptr
            );


    public:
        virtual const std::string& DeviceName() const override { return _devName; }
        virtual const std::string& VendorName() const override { return ""; }
        virtual const GraphicsApiVersion ApiVersion() const override { return _apiVer; }
        virtual const GraphicsDevice::Features& GetFeatures() const override { return _commonFeat; }

        virtual ResourceFactory* GetResourceFactory() override { return nullptr; };


        const VkDevice& LogicalDev() const {return _dev;}
        const VkPhysicalDevice& PhysicalDev() const {return _phyDev;}

        const VkSurfaceKHR& Surface() const {return _surface;}

        const VkQueue& GraphicsQueue() const {return _queueGraphics;}

        const VmaAllocator& Allocator() const {return _allocator;}


    private:


    public:
        sp<_CmdPoolMgr::Container> GetCmdPool() { return _cmdPoolMgr.GetOnePool(); }
        _DescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout){
            return _descPoolMgr.Allocate(layout);
        }

    };

    class VulkanBuffer : public Buffer{

    private:
        VkBuffer _buffer;
        VmaAllocation _allocation;

        //VkBufferUsageFlags _usages;
        //VmaMemoryUsage _allocationType;

        VulkanDevice* _Dev() const {
            return reinterpret_cast<VulkanDevice*>(dev.get());
        }

        VulkanBuffer(
            const sp<VulkanDevice>& dev,
            const Buffer::Description& desc
        ) : 
            Buffer(sp<GraphicsDevice>{dev}, desc)
        {}

    public:

        virtual ~VulkanBuffer();

        virtual std::string GetDebugName() const override {
            return "<VK_DEVBUF>";
        }

        static sp<Buffer> Make(
            const sp<VulkanDevice>& dev,
            const Buffer::Description& desc
        );

        VkBuffer GetHandle() const {return _buffer;}

    };

    class VulkanFence : public Fence {

    private:

        VkFence _fence;

        VulkanDevice* _Dev() const {
            return reinterpret_cast<VulkanDevice*>(dev.get());
        }

        VulkanFence(const sp<VulkanDevice>& dev) : Fence(dev) {}

    public:

        ~VulkanFence();

        static sp<Fence> Make(
            const sp<VulkanDevice>& dev,
            bool signaled = false
        );

        const VkFence& Handle() const { return _fence; }

        bool IsSignaled() const override;

        void Reset() override;

    };
    
} // namespace Veldrid

