#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include "veldrid/common/RefCnt.hpp"

#include "veldrid/GraphicsDevice.hpp"
#include "veldrid/SyncObjects.hpp"
#include "veldrid/Buffer.hpp"
#include "veldrid/SwapChain.hpp"

#include <deque>
#include <map>
#include <thread>
#include <mutex>

#include "VkDescriptorPoolMgr.hpp"
#include "VulkanResourceFactory.hpp"

class _VkCtx;

namespace Veldrid
{
    class VulkanBuffer;
    class VulkanDevice;
    //class VulkanResourceFactory;

    
    struct PhyDevInfo {
        std::string name;
        VkPhysicalDevice handle;
        uint32_t graphicsQueueFamily; bool graphicsQueueSupportsCompute;
        std::optional<uint32_t> computeQueueFamily;
        std::optional<uint32_t> transferQueueFamily;
    };

    //Manage command pools, to achieve one command pool per thread
    
    struct _CmdPoolContainer;
    class _CmdPoolMgr {
        friend class VulkanDevice;
        friend struct _CmdPoolContainer;
 
    private:
        VkDevice _dev;
        std::uint32_t _queueFamily;

        std::deque<VkCommandPool> _freeCmdPools;
        std::map<std::thread::id, _CmdPoolContainer*> _threadBoundCmdPools;
        std::mutex _m_cmdPool;

        void _ReleaseCmdPoolHolder(_CmdPoolContainer* holder);

        sp<_CmdPoolContainer> _AcquireCmdPoolHolder();

    public:
        _CmdPoolMgr() { }
        ~_CmdPoolMgr() { }

        void Init(VkDevice dev, std::uint32_t queueFamily){
            _dev = dev; _queueFamily = queueFamily;
        }

        void DeInit(){
            //Threoretically there should be no bound command pools,
            // i.e. all command buffers holded by threads should be released
            // then the VulkanDevice can be destroyed.
            assert(_threadBoundCmdPools.empty());
            for (auto p : _freeCmdPools) {
                vkDestroyCommandPool(_dev, p, nullptr);
            }
        }

        sp<_CmdPoolContainer> GetOnePool() { return _AcquireCmdPoolHolder(); }
    };

    struct _CmdPoolContainer : public RefCntBase {
        VkCommandPool pool;
        _CmdPoolMgr* mgr;
        std::thread::id boundID;

        ~_CmdPoolContainer() {
            mgr->_ReleaseCmdPoolHolder(this);
        }

        VkCommandBuffer AllocateBuffer();
        void FreeBuffer(VkCommandBuffer buf);
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

        PhyDevInfo _phyDev;
        VkDevice _dev;
        VmaAllocator _allocator;
        _CmdPoolMgr _cmdPoolMgr;
        _DescriptorPoolMgr _descPoolMgr;

        VkQueue _queueGraphics, _queueCopy, _queueCompute;

        VkSurfaceKHR _surface;
        bool _isOwnSurface;
        VulkanResourceFactory _resFactory;

        Features _features;
        GraphicsApiVersion _apiVer;

        std::string _devName, _devVendor;
        std::string _drvName, _drvInfo;
        GraphicsDevice::Features _commonFeat;

        VulkanDevice();

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

        virtual ResourceFactory* GetResourceFactory() override { return &_resFactory; };

        const PhyDevInfo& GetPhyDevInfo() const {return _phyDev;}
        const VkDevice& LogicalDev() const {return _dev;}
        const VkPhysicalDevice& PhysicalDev() const {return _phyDev.handle;}

        const VkSurfaceKHR& Surface() const {return _surface;}

        const VkQueue& GraphicsQueue() const {return _queueGraphics;}

        const VmaAllocator& Allocator() const {return _allocator;}

        //TODO: temporary querier
        bool SupportsFlippedYDirection() const {return _features.supportsMaintenance1;}

    private:


    public:
        sp<_CmdPoolContainer> GetCmdPool() { return _cmdPoolMgr.GetOnePool(); }
        _DescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout);
    //Interface
    public:

        virtual void SubmitCommand(
            const std::vector<CommandList*>& cmd,
            const std::vector<Semaphore*>& waitSemaphores,
            const std::vector<Semaphore*>& signalSemaphores,
            Fence* fence) override;
        virtual SwapChain::State PresentToSwapChain(
            const std::vector<Semaphore*>& waitSemaphores,
            SwapChain* sc) override;

        //virtual bool WaitForFence(const sp<Fence>& fence, std::uint32_t timeOutNs) override;
        void WaitForIdle() override {vkDeviceWaitIdle(_dev);}
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
            const sp<GraphicsDevice>& dev,
            const Buffer::Description& desc
        ) : 
            Buffer(dev, desc)
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

        const VkBuffer& GetHandle() const {return _buffer;}

        virtual void* MapToCPU();

        virtual void UnMap();

    };

    class VulkanFence : public Fence {

    private:

        VkFence _fence;

        VulkanDevice* _Dev() const {
            return reinterpret_cast<VulkanDevice*>(dev.get());
        }

        VulkanFence(const sp<GraphicsDevice>& dev) : Fence(dev) {}

    public:

        ~VulkanFence();

        static sp<Fence> Make(
            const sp<VulkanDevice>& dev,
            bool signaled = false
        );

        const VkFence& GetHandle() const { return _fence; }

        bool WaitForSignal(std::uint64_t timeoutNs) const override {
            auto res = vkWaitForFences(_Dev()->LogicalDev(), 1, &_fence, 0, timeoutNs);
            return res == VK_SUCCESS;
        }
        bool IsSignaled() const override;

        void Reset() override;

    };
    
    class VulkanSemaphore : public Semaphore {

    private:
        VkSemaphore _sem;

        VulkanSemaphore(const sp<GraphicsDevice>& dev) : Semaphore(dev) {}

    public:
        ~VulkanSemaphore();

        static sp<Semaphore> Make(
            const sp<VulkanDevice>& dev
        );

        const VkSemaphore& GetHandle() const { return _sem; }

    };

} // namespace Veldrid

