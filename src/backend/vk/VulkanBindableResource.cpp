#include "VulkanBindableResource.hpp"

#include "alloy/common/Common.hpp"

#include <vector>

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
        for(auto& b : _bindings)
            for (const auto& info : b.sets) {
                VK_DEV_CALL(_dev, 
                    vkDestroyDescriptorSetLayout(_dev->LogicalDev(), info.layout, nullptr));
            }
    }
    
    common::sp<IResourceLayout> VulkanResourceLayout::Make(
        const common::sp<VulkanDevice>& dev,
        const Description& desc
    ){
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

        //Count the set numbers for each descriptor types
        
        //Register bind info for each kind separately
        std::vector<ResourceBindInfo> resBindings;
        
        //Sort the elements and build the mapping info
        for(uint32_t i = 0; i < elements.size(); i++) {
            auto& e = elements[i];

            VulkanResourceLayout::BindType bindType;

            switch (e.kind)
            {
            case IBindableResource::ResourceKind::UniformBuffer : {
                bindType = VulkanResourceLayout::BindType::UniformBuffer;
            }break;
            case IBindableResource::ResourceKind::StorageBuffer :
            case IBindableResource::ResourceKind::Texture : {
                if(e.options.writable)
                    bindType = VulkanResourceLayout::BindType::ShaderResourceReadWrite;
                else
                    bindType = VulkanResourceLayout::BindType::ShaderResourceReadOnly;

            }break;
            case IBindableResource::ResourceKind::Sampler : {
                bindType = VulkanResourceLayout::BindType::Sampler;
            }break;
            }


            ResourceBindInfo* pBindInfo = nullptr;
            for(auto& b : resBindings) {
                if(bindType == b.type) { 
                    pBindInfo = &b;
                    break;
                }
            }

            if(!pBindInfo) {
                pBindInfo = &resBindings.emplace_back(bindType);
            }
            
            BindSetInfo* pSetInfo = nullptr;
            for(auto& s : pBindInfo->sets) {
                if(e.bindingSpace == s.regSpaceDesignated) { 
                    pSetInfo = &s;
                    break;
                }
            }
            if(!pSetInfo) {
                pSetInfo = &pBindInfo->sets.emplace_back(e.bindingSpace);
            }
            
            pSetInfo->elementIdInList.emplace_back(i);
        }

        // In DX12, each resource types' register spaces are counted separately
        // Alloy is following DX12 designs, but vulkan use shared register 
        // space for all types. We flat them out here
        uint32_t currentSetIdx = 0;

        for(auto& b : resBindings) {
            b.baseSetIndex = currentSetIdx;
            for(auto& s : b.sets) {
                s.setIndexAllocated = currentSetIdx;
                currentSetIdx++;

                //std::vector<VkDescriptorType> _descriptorTypes;
                //    _descriptorTypes.reserve(s.elementInfo.size());
                std::vector<VkDescriptorSetLayoutBinding> bindings;
                    bindings.reserve(s.elementIdInList.size());

                alloy::vk::DescriptorResourceCounts& drcs = s.resourceCounts;
                //    .uniformBufferCount = uniformBufferCount,
                //    .sampledImageCount = sampledImageCount,
                //    .samplerCount = samplerCount,
                //    .storageBufferCount = storageBufferCount,
                //    .storageImageCount = storageImageCount,
                //    .uniformBufferDynamicCount = uniformBufferDynamicCount,
                //    .storageBufferDynamicCount = storageBufferDynamicCount
                //};

                //std::uint32_t uniformBufferCount = 0;
                //std::uint32_t uniformBufferDynamicCount = 0;
                //std::uint32_t sampledImageCount = 0;
                //std::uint32_t samplerCount = 0;
                //std::uint32_t storageBufferCount = 0;
                //std::uint32_t storageBufferDynamicCount = 0;
                //std::uint32_t storageImageCount = 0;

                //unsigned dynamicBufferCount = 0;

                for (auto& elemIdx : s.elementIdInList)
                {
                    auto& e = elements[elemIdx];
                    auto& b = bindings.emplace_back();
                    b.binding = e.bindingSlot;
                    b.descriptorCount = e.bindingCount;
                    VkDescriptorType descriptorType = VdToVkDescriptorType(e.kind, e.options);
                    b.descriptorType = VdToVkDescriptorType(e.kind, e.options);
                    b.stageFlags = VdToVkShaderStages(e.stages);
                    //if (e.options.dynamicBinding) {
                    //    s.dynamicBufferCount += 1;
                    //}

                    switch (descriptorType)
                    {
                        case VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLER:
                            drcs.samplerCount += e.bindingCount;
                            break;
                        case VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                            drcs.sampledImageCount += e.bindingCount;
                            break;
                        case VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                            drcs.storageImageCount += e.bindingCount;
                            break;
                        case VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                            drcs.uniformBufferCount += e.bindingCount;
                            break;
                        case VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                            drcs.uniformBufferDynamicCount += e.bindingCount;
                            break;
                        case VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                            drcs.storageBufferCount += e.bindingCount;
                            break;
                        case VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                            drcs.storageBufferDynamicCount += e.bindingCount;
                            break;
                    }
                }

                VkDescriptorSetLayoutCreateInfo dslCI{};
                dslCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                dslCI.bindingCount = bindings.size();
                dslCI.pBindings = bindings.data();

                VkDescriptorSetLayout rawDsl;
                VK_CHECK(VK_DEV_CALL(dev, 
                    vkCreateDescriptorSetLayout(dev->LogicalDev(), &dslCI, nullptr, &s.layout)));
            }
        }

        auto dsl = new VulkanResourceLayout(dev, desc);
        dsl->_bindings = std::move(resBindings);
        dsl->_pushConstants = std::move(pushConstants);
        dsl->_pushConstantSize = pushConstantSize;
        //dsl->_dynamicBufferCount = dynamicBufferCount;
        //dsl->_drcs = drcs;

        return common::sp<IResourceLayout>(dsl);
    }

    VulkanResourceSet::~VulkanResourceSet(){

    }

    common::sp<IResourceSet> VulkanResourceSet::Make(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
    ){
        VulkanResourceLayout* vkLayout = static_cast<VulkanResourceLayout*>(desc.layout.get());

        auto& bindings = vkLayout->GetBindings();
        auto& boundResources = desc.boundResources;
        auto& resSlotDescs = vkLayout->GetDesc().shaderResources;

        //std::vector<sp<BindableResource>> _refCounts;
        std::unordered_set<VulkanTexture*> texReadOnly, texRW;
        
        std::vector<_DescriptorSet> descSetsAllocated;

        uint32_t resIdx = 0;
        for(auto& b : bindings) {

            for(auto& s : b.sets) {

                auto descriptorAllocationToken = dev->AllocateDescriptorSet(s.layout);

                //Write each entry separately to simplify resource allocation
                for (auto& elemIdx : s.elementIdInList)
                {
                    auto& resSlotDesc = resSlotDescs[elemIdx];
                    auto slotStartIdx = resSlotDesc.bindingSlot;
                    auto slotCnt = resSlotDesc.bindingCount;
                    auto type = resSlotDesc.kind;

                    VkWriteDescriptorSet descriptorWrite {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET
                    };

                    descriptorWrite.descriptorCount = slotCnt;
                    descriptorWrite.descriptorType = VdToVkResourceKind(type, false, resSlotDesc.options.writable);
                    descriptorWrite.dstBinding = slotStartIdx;
                    descriptorWrite.dstSet = descriptorAllocationToken.GetHandle();

                    std::vector<VkDescriptorBufferInfo> bufferInfos;
                    std::vector<VkDescriptorImageInfo> imageInfos;

                    using _ResKind = IBindableResource::ResourceKind;

                    switch(type){
                        case _ResKind::UniformBuffer:
                        case _ResKind::StorageBuffer: {
                            bufferInfos.resize(slotCnt);
                            descriptorWrite.pBufferInfo = bufferInfos.data();
                        } break;

                        case _ResKind::Texture:
                        case _ResKind::Sampler:{
                            imageInfos.resize(slotCnt);
                            descriptorWrite.pImageInfo = imageInfos.data();
                        } break;
                    };

                    for(auto i = 0; i <  slotCnt; i++) {
                        auto pElemRes = boundResources[resIdx++].get();
                        switch(type){
                            case _ResKind::UniformBuffer:
                            case _ResKind::StorageBuffer: {
                                auto* range = PtrCast<BufferRange>(pElemRes);
                                auto* rangedVkBuffer = static_cast<const VulkanBuffer*>(range->GetBufferObject());
                                bufferInfos[i].buffer = rangedVkBuffer->GetHandle();
                                bufferInfos[i].offset = range->GetShape().offsetInElements;
                                bufferInfos[i].range = range->GetShape().elementCount;
                                //_refCounts.push_back(boundResources[i]);
                            } break;

                            case _ResKind::Texture:{
                                auto* vkTexView = PtrCast<VulkanTextureView>(boundResources[elemIdx].get());
                                imageInfos[i].imageView = vkTexView->GetHandle();
                                if(resSlotDesc.options.writable){
                                    imageInfos[i].imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL;
                                } else {
                                    imageInfos[i].imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                                }
                                
                                auto vkTex = PtrCast<VulkanTexture>(vkTexView->GetTextureObject().get());
                                if(resSlotDesc.options.writable)
                                    texRW.insert(vkTex);
                                else
                                    texReadOnly.insert(vkTex);
                                //_sampledTextures.Add(Util.AssertSubtype<Texture, VkTexture>(texView.Target));
                                //_refCounts.push_back(boundResources[i]);
                            }break;


                            case _ResKind::Sampler:{
                                auto* sampler = PtrCast<VulkanSampler>(boundResources[elemIdx].get());
                                imageInfos[i].sampler = sampler->GetHandle();
                                //_refCounts.push_back(boundResources[i]);
                            }break;
                        }
                    }

                    VK_DEV_CALL(dev,
                    vkUpdateDescriptorSets(
                            dev->LogicalDev(), 
                            1, 
                            &descriptorWrite, 
                            0,
                            nullptr
                        )
                    );

                }
                descSetsAllocated.emplace_back(std::move(descriptorAllocationToken));
            }
        }

        
        auto descSet = new VulkanResourceSet(dev, desc);
        descSet->_texReadOnly = std::move(texReadOnly);
        descSet->_texRW = std::move(texRW);
        descSet->_descSet = std::move(descSetsAllocated);

        return common::sp(descSet);
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


