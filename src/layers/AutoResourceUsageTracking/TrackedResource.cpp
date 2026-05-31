#include "TrackedResource.hpp"

#include "TrackingDevice.hpp"

#include <cassert>
#include <utility>

namespace alloy::layers::AutoResourceUsageTracking
{

    TrackedTexture::TrackedTexture(
        common::sp<TrackingDevice> dev,
        common::sp<ITexture> texture
    )
        : _dev(dev)
        , _inner(texture.release())
    {
        assert(_dev);
        assert(_inner);
    }

    TrackedTexture::~TrackedTexture() {
    }

    WeakTrackedRef<ITexture> TrackedTexture::GetTrackedRef() const {
        return WeakTrackedRef<ITexture>(_inner);
    }

    common::sp<ITexture> TrackedTexture::GetInnerTexture() const {
        return common::ref_sp(_inner.get());
    }

    void* TrackedTexture::GetNativeHandle() const {
        return _inner->GetNativeHandle();
    }

    void TrackedTexture::SetDebugName(const std::string& name) {
        _inner->SetDebugName(name);
    }

    void TrackedTexture::WriteSubresource(
        std::uint32_t mipLevel,
        std::uint32_t arrayLayer,
        Point3D dstOrigin,
        Size3D writeSize,
        const void* src,
        std::uint32_t srcRowPitch,
        std::uint32_t srcDepthPitch) {
        _inner->WriteSubresource(
            mipLevel,
            arrayLayer,
            dstOrigin,
            writeSize,
            src,
            srcRowPitch,
            srcDepthPitch);
    }

    void TrackedTexture::ReadSubresource(
        void* dst,
        std::uint32_t dstRowPitch,
        std::uint32_t dstDepthPitch,
        std::uint32_t mipLevel,
        std::uint32_t arrayLayer,
        Point3D srcOrigin,
        Size3D readSize) {
        _inner->ReadSubresource(
            dst,
            dstRowPitch,
            dstDepthPitch,
            mipLevel,
            arrayLayer,
            srcOrigin,
            readSize);
    }

    ITexture::SubresourceLayout TrackedTexture::GetSubresourceLayout(
        std::uint32_t mipLevel,
        std::uint32_t arrayLayer,
        SubresourceAspect aspect) {
        return _inner->GetSubresourceLayout(mipLevel, arrayLayer, aspect);
    }

    TrackedBuffer::TrackedBuffer(
        common::sp<TrackingDevice> dev,
        common::sp<IBuffer> buffer
    )
        : _dev(dev)
        , _inner(buffer.get())
    {
        assert(_dev);
        assert(_inner);
    }

    TrackedBuffer::~TrackedBuffer() {
    }

    WeakTrackedRef<IBuffer> TrackedBuffer::GetTrackedRef() const {
        return WeakTrackedRef<IBuffer>(_inner);
    }

    common::sp<IBuffer> TrackedBuffer::GetInnerBuffer() const {
        return common::ref_sp(_inner.get());
    }

    void* TrackedBuffer::MapToCPU() {
        return _inner->MapToCPU();
    }

    void TrackedBuffer::UnMap() {
        _inner->UnMap();
    }

    std::uint64_t TrackedBuffer::GetNativeHandle() const {
        return _inner->GetNativeHandle();
    }

    void TrackedBuffer::SetDebugName(const std::string& name) {
        _inner->SetDebugName(name);
    }

    std::string TrackedBuffer::GetDebugName() {
        return _inner->GetDebugName();
    }


    TrackedSCTexView::TrackedSCTexView(
        common::sp<TrackedSwapChain> sc,
        common::sp<TrackedTexture> tex,
        common::sp<ITextureView> view
    )
        : TrackedTexView(std::move(tex), std::move(view))
        ,_sc(std::move(sc))
    { }


    TrackedSwapChain::TrackedSwapChain(
        common::sp<TrackingDevice> dev,
        common::sp<ISwapChain> sc
    )
        : _dev(dev)
        , _inner(sc)
        , _proxies(sc->GetDesc().backBufferCnt)
    {
        // proxy textures are lazy-initialized in GetBackBuffer()
    }

    TrackedSwapChain::~TrackedSwapChain() {

    }

    common::sp<ITextureView> TrackedSwapChain::GetBackBuffer() {
        auto currIdx = _inner->GetBackBufferIndex();
        auto currBackBuffer = _inner->GetBackBuffer();

        auto proxySlot = _proxies[currIdx];


        if(!proxySlot) {

            auto trackedTex = common::make_sp<TrackedTexture>(
                _dev,
                currBackBuffer->GetTextureObject()
            );

            //Let's create a proxy wrapper
            proxySlot = trackedTex;
        }

        
        return common::make_sp<TrackedSCTexView>(
            common::ref_sp(this),
            proxySlot,
            std::move(currBackBuffer)
        );
    }


    void TrackedSwapChain::Resize(
        std::uint32_t width,
        std::uint32_t height
    ) {
        // On all backends, before we can resize (basically destroy 
        // and re-create resources) we must release all outstanding 
        // references. We should drop out references as well

        for(auto& entry : _proxies) entry = {};

        _inner->Resize(width, height);
    }


} // namespace alloy::layers::AutoResourceUsageTracking
