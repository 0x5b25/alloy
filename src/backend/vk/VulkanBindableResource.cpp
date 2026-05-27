#include "VulkanBindableResource.hpp"

#include "alloy/common/Common.hpp"

#include <vector>
#include <stdexcept>

#include "VkTypeCvt.hpp"
#include "VkCommon.hpp"
#include "VulkanDevice.hpp"
#include "VulkanTexture.hpp"

namespace alloy::vk
{
    using _ResKind = IBindableResource::ResourceKind;
    VkDescriptorType VdToVkResourceKind(IBindableResource::ResourceKind kind, bool dynamic, bool writable){
        switch (kind)
        {
        case _ResKind::UniformBuffer: {
            if(dynamic) return VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            else        return VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        case _ResKind::StorageBuffer:{
            if(dynamic) return VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            else        return VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }

        case _ResKind::Texture: {
            if(dynamic) return VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            else        return VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        }
        case _ResKind::Sampler: return VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLER;
        default:return VkDescriptorType::VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }
    }

    VulkanResourceLayout::~VulkanResourceLayout(){
        for(auto& s : _sets)
            VK_DEV_CALL(_dev, 
                vkDestroyDescriptorSetLayout(_dev->LogicalDev(), s.layout, nullptr));
    }
    
    common::sp<IResourceLayout> VulkanResourceLayout::Make(
        const common::sp<VulkanDevice>& dev,
        const Description& desc
    ){
        
        const bool useDescriptorIndexing =
                    dev->GetAdapter().GetAdapterInfo().resourceBindingModel
                        != ResourceBindingModel::FixedBindings;

        std::vector<PushConstantInfo> pushConstants;
        uint32_t pushConstantSize = 0;

        for(auto& pc : desc.pushConstants){
            pushConstants.push_back({
                .bindingSlot = pc.bindingSlot,
                .bindingSpace = pc.bindingSpace,
                .sizeInDwords = pc.sizeInDwords,
                .offsetInDwords = pushConstantSize
            });
            pushConstantSize += pc.sizeInDwords;
        }

        auto& elements = desc.shaderResources;
        
        std::vector<ResourceSetInfo> sets;
        std::vector<VulkanResourceLayout::SlotLocation> slotLocations(elements.size());
        // Also builds the 
        // desc.shaderResources -> ResSet::Desc.boundResources index mapping 
        {
            uint32_t linearBase = 0;
            for(uint32_t i = 0; i < elements.size(); i++) {
                const auto& e = elements[i];
                auto vkType = VdToVkDescriptorType(e.kind, e.options);

                ResourceSetInfo* set = nullptr;
                uint32_t setIdx = 0;
                for(setIdx = 0; setIdx < sets.size(); ++setIdx) {
                    if(sets[setIdx].type == vkType) {
                        set = &sets[setIdx];
                        break;
                    }
                }

                if(!set) {
                    set = &sets.emplace_back();
                    set->type = vkType;
                }

                auto bindingCnt = set->bindings.size();
                auto& b = set->bindings.emplace_back();


                slotLocations[i].setIndexAllocated = setIdx;
                slotLocations[i].bindingAllocated = bindingCnt;
                slotLocations[i].linearResourceOffset = linearBase;

                b.regIdxDesignated = e.bindingSlot;
                b.bindSlotAllocated = bindingCnt;
                b.regSpaceDesignated = e.bindingSpace;
                b.bindSetAllocated = setIdx;
                b.indexInShaderResources = i;

                set->elementCnt += e.bindingCount;
                linearBase += e.bindingCount;
            }
        }

        // Create the layouts
        for(uint32_t i = 0; i < sets.size(); i++) {
            auto& s = sets[i];

            std::vector<VkDescriptorSetLayoutBinding> bindings;
                    bindings.reserve(s.bindings.size());
            
            std::vector<VkDescriptorBindingFlags> bindingFlags;
                    bindingFlags.reserve(s.bindings.size());

            for(const auto& b : s.bindings) {

                const auto& bindingDesc = desc.shaderResources[
                    b.indexInShaderResources
                ];

                assert(s.type ==
                    VdToVkDescriptorType(bindingDesc.kind, bindingDesc.options));

                auto& binding = bindings.emplace_back();
            
                binding.binding = b.bindSlotAllocated;
                binding.descriptorCount = bindingDesc.bindingCount;
                binding.descriptorType = s.type;
                binding.stageFlags = VdToVkShaderStages(bindingDesc.stages);


                VkDescriptorBindingFlags flags = 0;
                if(useDescriptorIndexing) {
                    flags |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                                VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
                                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
                }
                bindingFlags.push_back(flags);
            }

            
            VkDescriptorSetLayoutCreateInfo dslCI{};
            dslCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            dslCI.bindingCount = bindings.size();
            dslCI.pBindings = bindings.data();
            if(useDescriptorIndexing) {
                dslCI.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
            }

            VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCI {};
            if(useDescriptorIndexing) {
                bindingFlagsCI.sType =
                    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
                bindingFlagsCI.bindingCount = bindingFlags.size();
                bindingFlagsCI.pBindingFlags = bindingFlags.data();
                dslCI.pNext = &bindingFlagsCI;
            }

            VK_CHECK(VK_DEV_CALL(dev, 
                vkCreateDescriptorSetLayout(dev->LogicalDev(), &dslCI, nullptr, &s.layout)));
        }

        auto dsl = new VulkanResourceLayout(dev, desc);
        dsl->_sets = std::move(sets);
        dsl->_slotLocations = std::move(slotLocations);
        dsl->_pushConstants = std::move(pushConstants);
        dsl->_pushConstantSize = pushConstantSize;

        return common::sp<IResourceLayout>(dsl);
    }

    namespace {
        struct VulkanSetSlotLocation {
            VkDescriptorSet set;
            uint32_t binding;
        };

        // Maps an alloy resource layout slot to the concrete Vulkan descriptor
        // set and binding used by this resource set allocation.
        //
        // layout:
        //     Vulkan layout that owns the precomputed slot-to-set metadata.
        // sets:
        //     Descriptor sets allocated for the resource set, in flattened
        //     Vulkan set-index order.
        // layoutSlot:
        //     Index into IResourceLayout::Description::shaderResources.
        VulkanSetSlotLocation FindSlotLocation(
            VulkanResourceLayout* layout,
            const std::vector<_DescriptorSet>& sets,
            uint32_t layoutSlot
        ) {
            auto location = layout->GetSlotLocation(layoutSlot);
            return {
                .set = sets.at(location.setIndexAllocated).GetHandle(),
                .binding = location.bindingAllocated
            };
        }

        std::vector<IMutableResourceSet::WriteBinding> MakeWritesFromBoundResources(
            const IResourceSet::Description& desc
        ) {
            std::vector<IMutableResourceSet::WriteBinding> writes;
            if(desc.boundResources.empty()) {
                return writes;
            }

            auto& layoutDesc = desc.layout->GetDesc();
            writes.reserve(layoutDesc.shaderResources.size());

            uint32_t resIdx = 0;
            for(uint32_t layoutSlot = 0;
                layoutSlot < layoutDesc.shaderResources.size();
                ++layoutSlot)
            {
                auto& slotDesc = layoutDesc.shaderResources[layoutSlot];
                if(resIdx + slotDesc.bindingCount > desc.boundResources.size()) {
                    throw std::invalid_argument(
                        "ResourceSet boundResources does not match ResourceLayout capacity.");
                }

                auto& write = writes.emplace_back();
                write.layoutSlot = layoutSlot;
                write.firstArrayElement = 0;
                write.resources.reserve(slotDesc.bindingCount);
                for(uint32_t i = 0; i < slotDesc.bindingCount; ++i) {
                    write.resources.push_back(desc.boundResources[resIdx++]);
                }
            }

            if(resIdx != desc.boundResources.size()) {
                throw std::invalid_argument(
                    "ResourceSet boundResources contains more entries than ResourceLayout requires.");
            }

            return writes;
        }
        uint32_t GetRequiredBoundResourceCount(const IResourceLayout::Description& layoutDesc) {
            uint32_t requiredBoundResourceCount = 0;
            for(auto& slotDesc : layoutDesc.shaderResources) {
                requiredBoundResourceCount += slotDesc.bindingCount;
            }

            return requiredBoundResourceCount;
        }
    }


    VulkanResourceSetBase::VulkanResourceSetBase(
        const common::sp<VulkanDevice>& dev,
        const common::sp<VulkanResourceLayout>& layout,
        bool isMutable
    )
        : _dev(dev)
        , _layout(layout)
        , _boundResources(GetRequiredBoundResourceCount(_layout->GetDesc()))
        , _isMutable(isMutable)
    { }


    void VulkanResourceSetBase::AllocateDescriptorSets() {
        auto& sets = _layout->GetResSetInfo();
        for(auto& s : sets) {
            auto descriptorAllocationToken = _dev->AllocateDescriptorSet(
                s.layout,
                s.type,
                s.elementCnt,
                false,
                _isMutable
            );
            _descSet.emplace_back(std::move(descriptorAllocationToken));
        }
        
    }

    common::sp<IResourceSet> VulkanResourceSet::Make(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
    ){
        auto requiredBoundResourceCount =
            GetRequiredBoundResourceCount(desc.layout->GetDesc());
        if(desc.boundResources.size() != requiredBoundResourceCount) {
            throw std::invalid_argument(
                "Vulkan ResourceSet requires boundResources to exactly match ResourceLayout capacity.");
        }

        auto descSet = new VulkanResourceSet(dev, desc);
        descSet->AllocateDescriptorSets();

        if(!desc.boundResources.empty()) {
            auto writes = MakeWritesFromBoundResources(desc);
            descSet->UpdateInternal(writes);
        }

        return common::sp(descSet);
    }

    VulkanResourceSet::VulkanResourceSet(
        const common::sp<VulkanDevice>& dev,
        const Description& desc
    ) 
        : IResourceSet()
        , VulkanResourceSetBase(dev, common::SPCast<VulkanResourceLayout>(desc.layout), false)
    { }

    VulkanResourceSet::~VulkanResourceSet(){

    }

    common::sp<IMutableResourceSet> VulkanMutableResourceSet::Make(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
    ){
        if(dev->GetAdapter().GetAdapterInfo().resourceBindingModel
               == ResourceBindingModel::FixedBindings)
        {
            throw std::runtime_error("Vulkan mutable ResourceSet requires DescriptorIndexing support.");
        }

        auto descSet = new VulkanMutableResourceSet(dev, desc);
        descSet->AllocateDescriptorSets();

        if(!desc.initialWrites.empty()) {
            descSet->UpdateInternal(desc.initialWrites);
        }

        return common::sp<IMutableResourceSet>(descSet);
    }

    VulkanMutableResourceSet::VulkanMutableResourceSet(
        const common::sp<VulkanDevice>& dev,
        const Description& desc
    )
        : IMutableResourceSet()
        , VulkanResourceSetBase(dev, common::SPCast<VulkanResourceLayout>(desc.layout), true)
    { }

    VulkanMutableResourceSet::~VulkanMutableResourceSet(){

    }

    void VulkanResourceSetBase::UpdateInternal(
        const std::span<const IMutableResourceSet::WriteBinding>& writes
    ) {
        auto& slotDescs = _layout->GetDesc().shaderResources;

        assert(_boundResources.size() == GetRequiredBoundResourceCount(_layout->GetDesc()));

        for(auto& write : writes) {
            if(write.layoutSlot >= slotDescs.size()) {
                throw std::out_of_range("ResourceSet write layoutSlot is out of range.");
            }

            auto& slotDesc = slotDescs[write.layoutSlot];
            if(write.firstArrayElement + write.resources.size() > slotDesc.bindingCount) {
                throw std::out_of_range("ResourceSet write exceeds layout slot bindingCount.");
            }
            if(write.resources.empty()) {
                continue;
            }

            auto location = FindSlotLocation(
                _layout.get(),
                _descSet,
                write.layoutSlot);

            std::vector<VkDescriptorBufferInfo> bufferInfos;
            std::vector<VkDescriptorImageInfo> imageInfos;
            VkWriteDescriptorSet descriptorWrite {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET
            };

            descriptorWrite.dstSet = location.set;
            descriptorWrite.dstBinding = location.binding;
            descriptorWrite.dstArrayElement = write.firstArrayElement;
            descriptorWrite.descriptorCount =
                static_cast<uint32_t>(write.resources.size());
            descriptorWrite.descriptorType =
                VdToVkDescriptorType(slotDesc.kind, slotDesc.options);

            using _ResKind = IBindableResource::ResourceKind;
            switch(slotDesc.kind) {
                case _ResKind::UniformBuffer:
                case _ResKind::StorageBuffer:
                    bufferInfos.resize(write.resources.size());
                    descriptorWrite.pBufferInfo = bufferInfos.data();
                    break;
                case _ResKind::Texture:
                case _ResKind::Sampler:
                    imageInfos.resize(write.resources.size());
                    descriptorWrite.pImageInfo = imageInfos.data();
                    break;
            }

            for(uint32_t i = 0; i < write.resources.size(); ++i) {
                if(write.resources[i] == nullptr) {
                    throw std::invalid_argument("ResourceSet write contains a null resource.");
                }

                switch(slotDesc.kind) {
                    case _ResKind::UniformBuffer:
                    case _ResKind::StorageBuffer: {
                        const auto* range = PtrCast<BufferRange>(write.resources[i].get());
                        const auto* rangedVkBuffer =
                            PtrCast<VulkanBuffer>(range->GetBufferObject().get());
                        bufferInfos[i].buffer = rangedVkBuffer->GetHandle();
                        bufferInfos[i].offset = range->GetShape().GetOffsetInBytes();
                        bufferInfos[i].range = range->GetShape().GetSizeInBytes();
                    } break;

                    case _ResKind::Texture: {
                        const auto* vkTexView =
                            PtrCast<VulkanTextureView>(write.resources[i].get());
                        imageInfos[i].imageView = vkTexView->GetHandle();
                        imageInfos[i].imageLayout = slotDesc.options.writable
                            ? VK_IMAGE_LAYOUT_GENERAL
                            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                        auto vkTex =
                            PtrCast<VulkanTexture>(vkTexView->GetTextureObject().get());
                        if(slotDesc.options.writable) {
                            _texRW.insert(vkTex);
                        } else {
                            _texReadOnly.insert(vkTex);
                        }
                    } break;

                    case _ResKind::Sampler: {
                        auto* sampler = PtrCast<VulkanSampler>(write.resources[i].get());
                        imageInfos[i].sampler = sampler->GetHandle();
                    } break;
                }
                auto linearBase = _layout->GetSlotLocation(write.layoutSlot).linearResourceOffset;

                _boundResources[
                    linearBase + write.firstArrayElement + i] = write.resources[i];
            }

            VK_DEV_CALL(_dev,
                vkUpdateDescriptorSets(
                    _dev->LogicalDev(),
                    1,
                    &descriptorWrite,
                    0,
                    nullptr));
        }
    }

    IBindableResource* VulkanResourceSetBase::GetBoundResource(
        uint32_t layoutSlot,
        uint32_t firstArrayElement
    ) {
        auto linearBase = _layout->GetSlotLocation(layoutSlot).linearResourceOffset;

        return _boundResources[linearBase + firstArrayElement].get();
    }

    void VulkanMutableResourceSet::Update(
        const std::span<const WriteBinding>& writes
    ) {
        UpdateInternal(writes);
    }

    //void VulkanResourceSet::VisitElements(ElementVisitor visitor) {
    //    VulkanResourceLayout* vkLayout = static_cast<VulkanResourceLayout*>(description.layout.get());
    //
    //    auto& boundResources = description.boundResources;
    //    auto descriptorWriteCount = boundResources.size();
    //    assert(descriptorWriteCount == vkLayout->GetDesc().elements.size());
    //
    //    for (int i = 0; i < descriptorWriteCount; i++)
    //    {
    //        auto& elem = vkLayout->GetDesc().elements[i];
    //        auto type = elem.kind;
    //    }
    //
    //}

} // namespace alloy


