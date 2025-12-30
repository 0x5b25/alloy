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
#include <unordered_map>
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
    class VulkanTexture;
    class VulkanDevice;
    class VulkanFence;
    class VulkanCommandList;
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

    class IVkTimeline {
    public:
        struct BufferState{
            VkPipelineStageFlags stage;
            VkAccessFlags2 access;

            bool operator==(const BufferState& other) const {
                return stage == other.stage && access == other.access;
            }
        };

        struct TextureState {
            VkPipelineStageFlags stage;
            VkAccessFlags2 access;
            VkImageLayout layout;

            bool operator==(const TextureState& other) const {
                return stage == other.stage &&
                       access == other.access && 
                       layout == other.layout;
            }
        };

        struct ResourceStates {
            std::unordered_map<VulkanTexture*, VkImageLayout> textures;

            void SyncTo(const ResourceStates& other) {
                for(auto& [k, v] : other.textures)
                    textures.insert_or_assign(k, v);
            }

            void Clear() {
                textures.clear();
            }
        };

        virtual void RemoveResource(VulkanBuffer* buffer) = 0;
        virtual void RemoveResource(VulkanTexture* texture) = 0;

        virtual ResourceStates& GetCurrentState() = 0;

        //Notify that all resources are synced
        // DX12 will sync all resources at submission end
        // Vulkan will sync all resources at semaphore signal
        //virtual void NotifySyncResources() = 0;
    };

    class VulkanCommandQueue : public ICommandQueue, public IVkTimeline {


        VulkanDevice* _dev;
        _CmdPoolMgr _cmdPoolMgr;
        VkQueue _q;

        IVkTimeline::ResourceStates _currentState;

        VkSemaphore  _submissionFence, _presentFence;
        uint64_t _lastSubmittedFence = 0;
        struct TransitionCmdBuf{
            common::sp<_CmdPoolContainer> cmdPool;
            VkCommandBuffer cmdBuf;
            uint64_t completionFenceValue;
        };
        std::deque<TransitionCmdBuf> _transitionCmdBufs;

        void _RecycleTransitionCmdBufs();
    
        //void _InsertBarriers(
        //    const alloy::utils::BarrierActions& barriers);

        void _TransitResourceStatesBeforeSubmit(
            const VulkanCommandList& cmdList);

        void _MarkResourceStatesAfterSubmit(
            const VulkanCommandList& cmdList);

    public:

        VulkanCommandQueue(VulkanDevice* dev, std::uint32_t queueFamily, VkQueue q);

        virtual ~VulkanCommandQueue() override;
        //    _q->Release();
        //}

        //virtual bool WaitForSignal(std::uint64_t timeoutNs) = 0;
        //bool WaitForSignal() {
        //    return WaitForSignal((std::numeric_limits<std::uint64_t>::max)());
        //}
        //virtual bool IsSignaled() const = 0;

        //virtual void Reset() = 0;

        VkQueue GetHandle() const {return _q;}

        /*ICommandQueue implementations*/
        virtual void EncodeSignalEvent(IEvent* evt, uint64_t value) override;

        virtual void EncodeWaitForEvent(IEvent* evt, uint64_t value) override;

        virtual void SubmitCommand(ICommandList* cmd) override;

        virtual common::sp<ICommandList> CreateCommandList() override;

        virtual void* GetNativeHandle() const override {return _q;}
        
        /*IVkTimeline implementations*/
        virtual void RemoveResource(VulkanBuffer* buffer) override {
            //_CleanupSyncPoints();
            //_currentState.buffers.erase(buffer);
            //for(auto& pt : _syncPoints) {
            //    pt.states.buffers.erase(buffer);
            //}
        }
        
        virtual void RemoveResource(VulkanTexture* texture) override {
            //_CleanupSyncPoints();
            _currentState.textures.erase(texture);
            //for(auto& pt : _syncPoints) {
            //    pt.states.textures.erase(texture);
            //}
        }

        ResourceStates& GetCurrentState() override { return _currentState; }

        //Transition texture to present layout if necessary.
        //Will always signal the semaphore for present sync
        VkSemaphore PrepareTextureForPresent(VulkanTexture* tex);

    };

    class VulkanDevice : public IGraphicsDevice
                       , public VulkanResourceFactory
                       , public IVkTimeline
    {

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

        ResourceStates _currentState;

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

        const VkQueue GraphicsQueue() const {return _gfxQ->GetHandle();}

        const VmaAllocator& Allocator() const {return _allocator;}

        //TODO: temporary querier
        bool SupportsFlippedYDirection() const {return _features.supportsMaintenance1;}

        const Features& GetVkFeatures() const {return _features; }

    public:
        ResourceStates& GetCurrentState() override { return _currentState; }

        void RegisterTextureState(const VkImageLayout& request, VulkanTexture* texture) {
            _currentState.textures[texture] = request;
        }

        void RemoveResource(VulkanBuffer* buffer) override {  }
        void RemoveResource(VulkanTexture* texture) override { _currentState.textures.erase(texture); }

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
        std::string _debugName;
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
        { }

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

            _debugName = name;
        }

        
        virtual std::string GetDebugName() override {
            return _debugName;
        }

    };

    class VulkanFence : public IEvent {

    private:
        common::sp<VulkanDevice> _dev;
        VkSemaphore  _timelineSem;

        //map for each timeline : last signaled value
        std::unordered_map<IVkTimeline*, uint64_t> _signalingTimelines;

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

        void RegisterSyncPoint(IVkTimeline* timeline, uint64_t syncValue);
        void SyncTimelineToThis(IVkTimeline* timeline, uint64_t syncValue);

    };
    
} // namespace alloy

