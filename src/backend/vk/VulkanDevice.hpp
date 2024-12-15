#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include "veldrid/common/RefCnt.hpp"

#include "veldrid/GraphicsDevice.hpp"
#include "veldrid/SyncObjects.hpp"
#include "veldrid/Buffer.hpp"
#include "veldrid/SwapChain.hpp"
#include "veldrid/CommandQueue.hpp"

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

    class VulkanCommandQueue : public CommandQueue {

        VkQueue _q;

    public:

        VulkanCommandQueue(VkQueue q)
            : _q(q)
        { }

        //virtual ~VulkanCommandQueue() override {
        //    _q->Release();
        //}

        //virtual bool WaitForSignal(std::uint64_t timeoutNs) = 0;
        //bool WaitForSignal() {
        //    return WaitForSignal((std::numeric_limits<std::uint64_t>::max)());
        //}
        //virtual bool IsSignaled() const = 0;

        //virtual void Reset() = 0;

        VkQueue GetHandle() const {return _q;}

        virtual void EncodeSignalFence(Fence* fence, uint64_t value) override;

        virtual void EncodeWaitForFence(Fence* fence, uint64_t value) override;

        virtual void SubmitCommand(CommandList* cmd) override;

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

                std::uint32_t supportsDepthClip : 1;

            };
            std::uint32_t value;
        };

    private:

        sp<_VkCtx> _ctx;

        PhyDevInfo _phyDev;
        VkDevice _dev;
        VmaAllocator _allocator;
        _CmdPoolMgr _cmdPoolMgr;
        VK::priv::_DescriptorPoolMgr _descPoolMgr;

        VulkanCommandQueue* _gfxQ;
        VulkanCommandQueue* _copyQ;
        VulkanCommandQueue* _computeQ;

        VkSurfaceKHR _surface;
        bool _isOwnSurface;
        VulkanResourceFactory _resFactory;

        AdapterInfo _adpInfo;

        Features _features;
        GraphicsApiVersion _apiVer;

        //std::string _devName, _devVendor;
        //std::string _drvName, _drvInfo;
        GraphicsDevice::Features _commonFeat;

        VulkanDevice();

    public:

        ~VulkanDevice();

        static sp<GraphicsDevice> Make(
            const GraphicsDevice::Options& options,
            SwapChainSource* swapChainSource = nullptr
        );


    public:
        
        virtual void* GetNativeHandle() const override { return _dev; }

        virtual const GraphicsDevice::AdapterInfo& GetAdapterInfo() const override { return _adpInfo; }
        virtual const GraphicsDevice::Features& GetFeatures() const override { return _commonFeat; }

        virtual ResourceFactory* GetResourceFactory() override { return &_resFactory; };

        const PhyDevInfo& GetPhyDevInfo() const {return _phyDev;}
        const VkDevice& LogicalDev() const {return _dev;}
        const VkPhysicalDevice& PhysicalDev() const {return _phyDev.handle;}

        const VkSurfaceKHR& Surface() const {return _surface;}

        const VkQueue& GraphicsQueue() const {return _gfxQ->GetHandle();}

        const VmaAllocator& Allocator() const {return _allocator;}

        //TODO: temporary querier
        bool SupportsFlippedYDirection() const {return _features.supportsMaintenance1;}

        const Features& GetVkFeatures() const {return _features; }

    private:


    public:
        sp<_CmdPoolContainer> GetCmdPool() { return _cmdPoolMgr.GetOnePool(); }
        VK::priv::_DescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout);
    //Interface
    public:

        //virtual void SubmitCommand(const CommandList* cmd) override;
        virtual SwapChain::State PresentToSwapChain(
            const std::vector<Semaphore*>& waitSemaphores,
            SwapChain* sc) override;

        virtual CommandQueue* GetGfxCommandQueue() override;
        virtual CommandQueue* GetCopyCommandQueue() override;

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

        VkSemaphore  _timelineSem;

        VulkanDevice* _Dev() const {
            return static_cast<VulkanDevice*>(dev.get());
        }

        VulkanFence(const sp<GraphicsDevice>& dev) : Fence(dev) {}

    public:

        ~VulkanFence();

        static sp<Fence> Make(
            const sp<VulkanDevice>& dev
        );

        const VkSemaphore& GetHandle() const { return _timelineSem; }
    
        virtual uint64_t GetCurrentValue() override;
        virtual void SignalFromCPU(uint64_t signalValue) override;
        virtual bool WaitFromCPU(uint64_t expectedValue, uint32_t timeoutMs) override;
        bool WaitFromCPU(uint64_t expectedValue) {
            return WaitFromCPU(expectedValue, (std::numeric_limits<std::uint32_t>::max)());
        }

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

