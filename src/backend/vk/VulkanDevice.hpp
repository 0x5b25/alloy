#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include "alloy/common/RefCnt.hpp"

#include "alloy/GraphicsDevice.hpp"
#include "alloy/SyncObjects.hpp"
#include "alloy/Buffer.hpp"
#include "alloy/SwapChain.hpp"
#include "alloy/CommandQueue.hpp"

#include <deque>
#include <map>
#include <thread>
#include <mutex>

#include "VkDescriptorPoolMgr.hpp"
#include "VulkanResourceFactory.hpp"

#define VK_DEV_CALL(dev, expr) (dev->GetFnTable(). expr )

namespace alloy::vk
{
    //class _VkCtx;
    class VulkanAdapter;
    class VulkanBuffer;
    class VulkanDevice;
    //class VulkanResourceFactory;
    //Manage command pools, to achieve one command pool per thread
    
    struct _CmdPoolContainer;
    class _CmdPoolMgr {
        friend class VulkanDevice;
        friend struct _CmdPoolContainer;
 
    private:
        VulkanDevice* _dev;
        std::uint32_t _queueFamily;

        std::deque<VkCommandPool> _freeCmdPools;
        std::map<std::thread::id, _CmdPoolContainer*> _threadBoundCmdPools;
        std::mutex _m_cmdPool;

        void _ReleaseCmdPoolHolder(_CmdPoolContainer* holder);

        common::sp<_CmdPoolContainer> _AcquireCmdPoolHolder();

    public:
        _CmdPoolMgr(VulkanDevice* dev, std::uint32_t queueFamily)
            : _dev { dev }
            , _queueFamily { queueFamily }
        { }
        ~_CmdPoolMgr();

        common::sp<_CmdPoolContainer> GetOnePool() { return _AcquireCmdPoolHolder(); }
    };

    struct _CmdPoolContainer : public common::RefCntBase {
        VkCommandPool pool;
        _CmdPoolMgr* mgr;
        std::thread::id boundID;

        ~_CmdPoolContainer() {
            mgr->_ReleaseCmdPoolHolder(this);
        }

        VkCommandBuffer AllocateBuffer();
        void FreeBuffer(VkCommandBuffer buf);
    };

    class VulkanCommandQueue : public ICommandQueue {

        VulkanDevice* _dev;
        _CmdPoolMgr _cmdPoolMgr;
        VkQueue _q;

    public:

        VulkanCommandQueue(VulkanDevice* dev, std::uint32_t queueFamily, VkQueue q);

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

        virtual void EncodeSignalEvent(IEvent* evt, uint64_t value) override;

        virtual void EncodeWaitForEvent(IEvent* evt, uint64_t value) override;

        virtual void SubmitCommand(ICommandList* cmd) override;


        
        virtual common::sp<ICommandList> CreateCommandList() override;

    };

    class VulkanDevice : public IGraphicsDevice, public VulkanResourceFactory {

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

        common::sp<VulkanAdapter> _adp;

        VolkDeviceTable _fnTable;

        //PhyDevInfo _phyDev;
        VkDevice _dev;
        VmaAllocator _allocator;
        //_CmdPoolMgr _cmdPoolMgr;
        _DescriptorPoolMgr _descPoolMgr;

        VulkanCommandQueue* _gfxQ;
        VulkanCommandQueue* _copyQ;
        VulkanCommandQueue* _computeQ;

        //VkSurfaceKHR _surface;
        //bool _isOwnSurface;

        Features _features;

        //std::string _devName, _devVendor;
        //std::string _drvName, _drvInfo;
        IGraphicsDevice::Features _commonFeat;

        VulkanDevice();

    public:

        ~VulkanDevice();

        static common::sp<IGraphicsDevice> Make(
		    const common::sp<VulkanAdapter>& adp,
            const IGraphicsDevice::Options& options
        );


    public:
        
        virtual void* GetNativeHandle() const override { return _dev; }

        //virtual const IGraphicsDevice::AdapterInfo& GetAdapterInfo() const override { return _adpInfo; }
        virtual const IGraphicsDevice::Features& GetFeatures() const override { return _commonFeat; }
        virtual IPhysicalAdapter& GetAdapter() const override;

        virtual ResourceFactory& GetResourceFactory() override { return *this; };

        //const PhyDevInfo& GetPhyDevInfo() const {return _phyDev;}
        const VkDevice& LogicalDev() const {return _dev;}
        //const VkPhysicalDevice& PhysicalDev() const {return _phyDev.handle;}

        //const VkInstance& GetInstance() const;
        const VolkDeviceTable& GetFnTable() const {return _fnTable;}

        //const VkSurfaceKHR& Surface() const {return _surface;}

        const VkQueue& GraphicsQueue() const {return _gfxQ->GetHandle();}

        const VmaAllocator& Allocator() const {return _allocator;}

        //TODO: temporary querier
        bool SupportsFlippedYDirection() const {return _features.supportsMaintenance1;}

        const Features& GetVkFeatures() const {return _features; }

    private:


    public:
        //sp<_CmdPoolContainer> GetCmdPool() { return _cmdPoolMgr.GetOnePool(); }
        _DescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout);
    //Interface
    public:

        //virtual void SubmitCommand(const CommandList* cmd) override;
        virtual ISwapChain::State PresentToSwapChain(
            ISwapChain* sc) override;

        virtual ICommandQueue* GetGfxCommandQueue() override;
        virtual ICommandQueue* GetCopyCommandQueue() override;

        //virtual bool WaitForFence(const sp<Fence>& fence, std::uint32_t timeOutNs) override;
        void WaitForIdle() override { _fnTable.vkDeviceWaitIdle(_dev);}
    };

    class VulkanBuffer : public IBuffer{

    private:
        common::sp<VulkanDevice> _dev;

        VkBuffer _buffer;
        VmaAllocation _allocation;

        //VkBufferUsageFlags _usages;
        //VmaMemoryUsage _allocationType;

        VulkanBuffer(
            const common::sp<VulkanDevice>& dev,
            const IBuffer::Description& desc
        ) 
            : IBuffer(desc)
            , _dev(dev)
        {}

    public:

        virtual ~VulkanBuffer();

        //virtual std::string GetDebugName() const override {
        //    return "<VK_DEVBUF>";
        //}

        static common::sp<IBuffer> Make(
            const common::sp<VulkanDevice>& dev,
            const IBuffer::Description& desc
        );

        const VkBuffer& GetHandle() const {return _buffer;}

        virtual void* MapToCPU();

        virtual void UnMap();

        virtual void SetDebugName(const std::string & name) override {
            // Check for a valid function pointer
	        if (_dev->GetFeatures().commandListDebugMarkers)
	        {
	        	VkDebugMarkerObjectNameInfoEXT nameInfo = {};
	        	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
	        	nameInfo.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT;
	        	nameInfo.object = (uint64_t)_buffer;
	        	nameInfo.pObjectName = name.c_str();
                _dev->GetFnTable().vkDebugMarkerSetObjectNameEXT(_dev->LogicalDev(), &nameInfo);
	        }
        }

    };

    class VulkanFence : public IEvent {

    private:
        common::sp<VulkanDevice> _dev;
        VkSemaphore  _timelineSem;


        VulkanFence(const common::sp<VulkanDevice>& dev) : _dev(dev) {}

    public:

        ~VulkanFence();

        static common::sp<IEvent> Make(
            const common::sp<VulkanDevice>& dev
        );

        const VkSemaphore& GetHandle() const { return _timelineSem; }
    
        virtual uint64_t GetSignaledValue() override;
        virtual void SignalFromCPU(uint64_t signalValue) override;
        virtual bool WaitFromCPU(uint64_t expectedValue, uint32_t timeoutMs) override;
        bool WaitFromCPU(uint64_t expectedValue) {
            return WaitFromCPU(expectedValue, (std::numeric_limits<std::uint32_t>::max)());
        }

    };
    
} // namespace alloy

