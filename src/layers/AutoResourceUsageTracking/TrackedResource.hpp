#pragma once

#include "alloy/Buffer.hpp"
#include "alloy/Texture.hpp"
#include "alloy/SwapChain.hpp"
#include "alloy/common/RefCnt.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <concepts>

namespace alloy::layers::AutoResourceUsageTracking
{
    class TrackingDevice;

    // Weak ref style resource "alive"-ness tracker
    // Important: 
    //     1. The control block should only be created when strong reference to resource
    //      is present in current scope.
    //     2. This should only be used as a "alive" indicator. Never get resource handle
    //      from this tracker.
    template<std::derived_from <common::RefCntBase> T>
    struct TrackedCtrlBlk : public common::NVRefCnt<TrackedCtrlBlk<T>> {

        std::atomic<uint32_t> _strongCount{1};
        std::atomic<T*> _object;

        TrackedCtrlBlk(T* resource)
            : _object(resource) 
        { }

        void RefStrong() {
            assert(_strongCount.load(std::memory_order_relaxed) > 0);
            _strongCount.fetch_add(1, std::memory_order_relaxed);
        }

        bool TryRefStrong() {
            uint32_t count = _strongCount.load(std::memory_order_acquire);

            while (count != 0) {
                if (_strongCount.compare_exchange_weak(
                        count,
                        count + 1,
                        std::memory_order_acquire,
                        std::memory_order_relaxed)) {
                    return true;
                }
            }

            return false;
        }

        void UnrefStrong() {
            if (_strongCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                T* object = _object.exchange(nullptr, std::memory_order_acq_rel);
                object->unref();
            }
        }

        T* GetObject() const {
            auto strongRefCnt = _strongCount.load(std::memory_order_acquire);
            if(strongRefCnt)
                return _object.load(std::memory_order_acquire);
            else
                return nullptr;
        }

        bool IsValid() const {
            auto strongRefCnt = _strongCount.load(std::memory_order_acquire);
            return strongRefCnt > 0;
        }

    };
    
    template<std::derived_from <common::RefCntBase> T>
    struct StrongTrackedRef {
        common::sp<TrackedCtrlBlk<T>> _ctrl;

        StrongTrackedRef() {}

        StrongTrackedRef(T* obj)
            : _ctrl(common::make_sp<TrackedCtrlBlk<T>>(obj))
        { }

        StrongTrackedRef(const common::sp<TrackedCtrlBlk<T>>& ctrl)
            : _ctrl(ctrl)
        { }

        StrongTrackedRef(StrongTrackedRef<T>&& that) 
            : _ctrl(that._ctrl.release())
        { }

        StrongTrackedRef(const StrongTrackedRef<T>& that) {
            auto pCtrl = that._ctrl.get();
            if(pCtrl) {
                pCtrl->ref();
            }

            this->reset(pCtrl);
        }

        ~StrongTrackedRef() {
            reset();
        }

        void reset(TrackedCtrlBlk<T>* ptr = nullptr) {
            auto ctrl = std::move(_ctrl);
            _ctrl.reset(ptr);
            if (ctrl) {
                ctrl->UnrefStrong();
            }
        }

        TrackedCtrlBlk<T>* release() {
            return _ctrl.release();
        }

        StrongTrackedRef<T>& operator=(StrongTrackedRef<T>&& that) {
            reset(that.release());
            return *this;
        }
        
        StrongTrackedRef<T>& operator=(const StrongTrackedRef<T>& that) {
            if (this != &that) {
                auto pCtrl = that._ctrl.get();
                if(pCtrl) {
                    pCtrl->ref();
                }

                this->reset(pCtrl);
            }
            return *this;
        }

        template <typename U>
        bool operator==(const StrongTrackedRef<U>& that){ return this->_ctrl == that._ctrl;}


        T& operator*() const {
            assert((bool)this);
            return *_ctrl->_object;
        }

        explicit operator bool() const { return _ctrl && _ctrl->GetObject() != nullptr; }

        T* get() const { return _ctrl ? _ctrl->GetObject() : nullptr; }
        T* operator->() const { return _ctrl ? _ctrl->GetObject() : nullptr; }

        template<typename U>
        bool operator==(const StrongTrackedRef<U>& that) const {
            return _ctrl == that._ctrl;
        }

    };

    template<std::derived_from <common::RefCntBase> T>
    struct WeakTrackedRef {
        common::sp<TrackedCtrlBlk<T>> _ctrl;

        WeakTrackedRef() {}

        WeakTrackedRef(const StrongTrackedRef<T>& strongRef)
            : _ctrl(strongRef._ctrl) { } 

        explicit operator bool() const { return IsValid(); }
        bool IsValid() const {
            return _ctrl && _ctrl->IsValid();
        }

        StrongTrackedRef<T> Lock() const {
            if (!_ctrl) {
                return {};
            }

            if (!_ctrl->TryRefStrong()) {
                return {};
            }

            return StrongTrackedRef<T>(_ctrl);
        }

        
        template<typename U>
        bool operator==(const WeakTrackedRef<U>& that) const {
            return _ctrl == that._ctrl;
        }
    };

    class TrackedTexture : public ITexture {
        
        // Hold a reference to a device so it won't die
        common::sp<TrackingDevice> _dev;
        StrongTrackedRef<ITexture> _inner;

    public:
        explicit TrackedTexture(
            common::sp<TrackingDevice> dev,
            common::sp<ITexture> texture
        );
        ~TrackedTexture() override;

        // Wrapper specific
        WeakTrackedRef<ITexture> GetTrackedRef() const;
        common::sp<ITexture> GetInnerTexture() const;

        // Delegate to _innner
        virtual const Description& GetDesc() const override { return _inner->GetDesc(); }
        void* GetNativeHandle() const override;
        void SetDebugName(const std::string& name) override;

        void WriteSubresource(
            std::uint32_t mipLevel,
            std::uint32_t arrayLayer,
            Point3D dstOrigin,
            Size3D writeSize,
            const void* src,
            std::uint32_t srcRowPitch,
            std::uint32_t srcDepthPitch) override;

        void ReadSubresource(
            void* dst,
            std::uint32_t dstRowPitch,
            std::uint32_t dstDepthPitch,
            std::uint32_t mipLevel,
            std::uint32_t arrayLayer,
            Point3D srcOrigin,
            Size3D readSize) override;

        SubresourceLayout GetSubresourceLayout(
            std::uint32_t mipLevel,
            std::uint32_t arrayLayer,
            SubresourceAspect aspect = SubresourceAspect::Color) override;
    };

    static_assert(__is_base_of(alloy::common::RefCntBase, alloy::layers::AutoResourceUsageTracking::TrackedTexture));
    static_assert(std::derived_from<alloy::layers::AutoResourceUsageTracking::TrackedTexture, alloy::common::RefCntBase>);

    class TrackedTexView : public ITextureView {
        common::sp<TrackedTexture> _target;
        StrongTrackedRef<ITextureView> _inner;
    public:

        TrackedTexView(
            common::sp<TrackedTexture> tex,
            common::sp<ITextureView> view
        )
            : _target(std::move(tex))
            , _inner(view.get())
        {
            view.get()->ref();
        }

        
        WeakTrackedRef<ITextureView> GetTrackedRef() const {
            return {_inner};
        }

        common::sp<ITextureView> GetInner() const {
            return common::ref_sp(_inner.get());
        }
        
        virtual const Description& GetDesc() const { return _inner->GetDesc(); }
        
        virtual common::sp<ITexture> GetTextureObject() const {
            return _target;
        }
    
    };

    // We don't need cross submission buffer state for now.
    // This class remains dormant
    class TrackedBuffer : public IBuffer {
        common::sp<TrackingDevice> _dev;
        StrongTrackedRef<IBuffer> _inner;

    public:
        explicit TrackedBuffer(
            common::sp<TrackingDevice> dev,
            common::sp<IBuffer> buffer
        );
        ~TrackedBuffer() override;

        // Wrapper specific
        WeakTrackedRef<IBuffer> GetTrackedRef() const;
        common::sp<IBuffer> GetInnerBuffer() const;

        // Delegate to _innner
        
        virtual const Description& GetDesc() const override { return _inner->GetDesc(); }
        void* MapToCPU() override;
        void UnMap() override;
        std::uint64_t GetNativeHandle() const override;
        void SetDebugName(const std::string& name) override;
        std::string GetDebugName() override;
    };

    class TrackedSwapChain;
    class TrackedRT;

    class TrackedSCTexView : public TrackedTexView {
        common::sp<TrackedSwapChain> _sc;
    public:
        TrackedSCTexView(
            common::sp<TrackedSwapChain> sc,
            common::sp<TrackedTexture> tex,
            common::sp<ITextureView> view
        );
    };

    class TrackedSwapChain : public ISwapChain {
        common::sp<TrackingDevice> _dev;
        common::sp<ISwapChain> _inner;

        // Proxy textures to wrap the back buffers to make them trackable
        std::vector<common::sp<TrackedTexture>> _proxies;

    public:

        TrackedSwapChain(
            common::sp<TrackingDevice> dev,
            common::sp<ISwapChain> sc
        );

        ~TrackedSwapChain() override;

        //Delegates
        virtual common::sp<ITextureView> GetBackBuffer() override;

        virtual const Description& GetDesc() const override {
            return _inner->GetDesc();
        }
        // Get the current backbuffer index
        virtual uint32_t GetBackBufferIndex() override {
            return _inner->GetBackBufferIndex();
        }
        virtual void Resize(
            std::uint32_t width,
            std::uint32_t height
        ) override;

        virtual bool IsSyncToVerticalBlank() const override {
            return _inner->IsSyncToVerticalBlank();
        }
        virtual void SetSyncToVerticalBlank(bool sync) override {
            _inner->SetSyncToVerticalBlank(sync);
        }

        virtual std::uint32_t GetWidth() const override {
            return _inner->GetWidth();
        }
        virtual std::uint32_t GetHeight() const override {
            return _inner->GetHeight();
        }

    };

} // namespace alloy::layers::AutoResourceUsageTracking

namespace std {

    template <typename T>
    using WTrackedRef = alloy::layers::AutoResourceUsageTracking::WeakTrackedRef<T>;

    
    template <typename T>
    using STrackedRef = alloy::layers::AutoResourceUsageTracking::StrongTrackedRef<T>;

    template <typename T>
    struct hash<WTrackedRef<T>> {
        size_t operator()(const WTrackedRef<T> &record) const {
            return hash<void*>()(record._ctrl.get());
        }
    };

    
    template <typename T>
    struct hash<STrackedRef<T>> {
        size_t operator()(const STrackedRef<T> &record) const {
            return hash<void*>()(record._ctrl.get());
        }
    };
}
