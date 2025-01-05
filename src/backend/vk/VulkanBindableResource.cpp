#include "VulkanBindableResource.hpp"

#include "veldrid/common/Common.hpp"

#include <vector>

#include "VkTypeCvt.hpp"
#include "VkCommon.hpp"
#include "VulkanDevice.hpp"
#include "VulkanTexture.hpp"

namespace Veldrid
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
        auto vkDev = PtrCast<VulkanDevice>(dev.get());
        for(auto& b : _bindings)
            for (const auto& info : b.sets) {
                vkDestroyDescriptorSetLayout(vkDev->LogicalDev(), info.layout, nullptr);
            }
    }
    
    sp<ResourceLayout> VulkanResourceLayout::Make(
        const sp<VulkanDevice>& dev,
        const Description& desc
    ){
        auto& elements = desc.elements;
        //Count the set numbers for each descriptor types
        

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

        uint32_t currentSetIdx = 0;

        for(auto& b : resBindings) {
            b.baseSetIndex = currentSetIdx;
            for(auto& s : b.sets) {
                s.setIndexAllocated = currentSetIdx;
                currentSetIdx++;

                std::vector<VkDescriptorType> _descriptorTypes {s.elementIdInList.size()};
                std::vector<VkDescriptorSetLayoutBinding> bindings {s.elementIdInList.size()};

                VK::priv::DescriptorResourceCounts& drcs = s.resourceCounts;
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

                for (unsigned i = 0; i < s.elementIdInList.size(); i++)
                {
                    auto& e = elements[s.elementIdInList[i]];
                    bindings[i].binding = i;
                    bindings[i].descriptorCount = 1;
                    VkDescriptorType descriptorType = VK::priv::VdToVkDescriptorType(e.kind, e.options);
                    bindings[i].descriptorType = descriptorType;
                    bindings[i].stageFlags = VK::priv::VdToVkShaderStages(e.stages);
                    //if (e.options.dynamicBinding) {
                    //    s.dynamicBufferCount += 1;
                    //}

                    _descriptorTypes[i] = descriptorType;

                    switch (descriptorType)
                    {
                        case VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLER:
                            drcs.samplerCount += 1;
                            break;
                        case VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                            drcs.sampledImageCount += 1;
                            break;
                        case VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                            drcs.storageImageCount += 1;
                            break;
                        case VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                            drcs.uniformBufferCount += 1;
                            break;
                        case VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                            drcs.uniformBufferDynamicCount += 1;
                            break;
                        case VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                            drcs.storageBufferCount += 1;
                            break;
                        case VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                            drcs.storageBufferDynamicCount += 1;
                            break;
                    }
                }

                VkDescriptorSetLayoutCreateInfo dslCI{};
                dslCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                dslCI.bindingCount = bindings.size();
                dslCI.pBindings = bindings.data();

                VkDescriptorSetLayout rawDsl;
                VK_CHECK(vkCreateDescriptorSetLayout(dev->LogicalDev(), &dslCI, nullptr, &s.layout));
            }
        }

        auto dsl = new VulkanResourceLayout(dev, desc);
        dsl->_bindings = std::move(resBindings);
        //dsl->_dynamicBufferCount = dynamicBufferCount;
        //dsl->_drcs = drcs;

        return sp<ResourceLayout>(dsl);
    }

    VulkanResourceSet::~VulkanResourceSet(){
        auto vkDev = PtrCast<VulkanDevice>(dev.get());

    }

    sp<ResourceSet> VulkanResourceSet::Make(
            const sp<VulkanDevice>& dev,
            const Description& desc
    ){
        VulkanResourceLayout* vkLayout = static_cast<VulkanResourceLayout*>(desc.layout.get());

        auto& bindings = vkLayout->GetBindings();
        auto& boundResources = desc.boundResources;
        auto& elems = vkLayout->GetDesc().elements;

        //std::vector<sp<BindableResource>> _refCounts;
        std::unordered_set<VulkanTexture*> texReadOnly, texRW;
        
        std::vector<Veldrid::VK::priv::_DescriptorSet> descSetsAllocated;

        for(auto& b : bindings) {

            for(auto& s : b.sets) {

                //VkDescriptorSetLayout dsl = vkLayout->GetHandle();
                //_descriptorCounts = vkLayout.DescriptorResourceCounts;
                auto descriptorAllocationToken = dev->AllocateDescriptorSet(s.layout);
                auto descriptorWriteCount = s.elementIdInList.size();
                std::vector<VkWriteDescriptorSet> descriptorWrites(descriptorWriteCount);
                std::vector<VkDescriptorBufferInfo> bufferInfos(descriptorWriteCount);
                std::vector<VkDescriptorImageInfo> imageInfos(descriptorWriteCount);

                //assert(descriptorWriteCount == vkLayout->GetDesc().elements.size());
                for (int i = 0; i < descriptorWriteCount; i++)
                {
                    auto elemIdx = s.elementIdInList[i];
                    auto& elem = elems[elemIdx];
                    auto type = elem.kind;

                    descriptorWrites[i].sType = VkStructureType::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrites[i].descriptorCount = 1;
                    descriptorWrites[i].descriptorType = VdToVkResourceKind(type, false, elem.options.writable);
                    descriptorWrites[i].dstBinding = i;
                    descriptorWrites[i].dstSet = descriptorAllocationToken.GetHandle();

                    using _ResKind = IBindableResource::ResourceKind;
                    switch(type){
                        case _ResKind::UniformBuffer:
                        case _ResKind::StorageBuffer: {
                            auto* range = PtrCast<BufferRange>(boundResources[elemIdx].get());
                            auto* rangedVkBuffer = static_cast<const VulkanBuffer*>(range->GetBufferObject());
                            bufferInfos[i].buffer = rangedVkBuffer->GetHandle();
                            bufferInfos[i].offset = range->GetShape().offsetInElements;
                            bufferInfos[i].range = range->GetShape().elementCount;
                            descriptorWrites[i].pBufferInfo = &bufferInfos[i];
                            //_refCounts.push_back(boundResources[i]);
                        } break;

                        case _ResKind::Texture:{
                            auto* vkTexView = PtrCast<VulkanTextureView>(boundResources[elemIdx].get());
                            imageInfos[i].imageView = vkTexView->GetHandle();
                            if(elem.options.writable){
                                imageInfos[i].imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL;
                            } else {
                                imageInfos[i].imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                            }
                            descriptorWrites[i].pImageInfo = &imageInfos[i];

                            auto vkTex = PtrCast<VulkanTexture>(vkTexView->GetTarget().get());
                            if(elem.options.writable)
                                texRW.insert(vkTex);
                            else
                                texReadOnly.insert(vkTex);
                            //_sampledTextures.Add(Util.AssertSubtype<Texture, VkTexture>(texView.Target));
                            //_refCounts.push_back(boundResources[i]);
                        }break;


                        case _ResKind::Sampler:{
                            auto* sampler = PtrCast<VulkanSampler>(boundResources[elemIdx].get());
                            imageInfos[i].sampler = sampler->GetHandle();
                            descriptorWrites[i].pImageInfo = &imageInfos[i];
                            //_refCounts.push_back(boundResources[i]);
                        }break;
                    }

                }
                descSetsAllocated.emplace_back(std::move(descriptorAllocationToken));

                vkUpdateDescriptorSets(dev->LogicalDev(), descriptorWriteCount, descriptorWrites.data(), 0, nullptr);
            }
        }

        
        auto descSet = new VulkanResourceSet(dev, desc);
        descSet->_texReadOnly = std::move(texReadOnly);
        descSet->_texRW = std::move(texRW);
        descSet->_descSet = std::move(descSetsAllocated);

        return sp(descSet);
    }

    void VulkanResourceSet::VisitElements(ElementVisitor visitor) {
        VulkanResourceLayout* vkLayout = static_cast<VulkanResourceLayout*>(description.layout.get());

        auto& boundResources = description.boundResources;
        auto descriptorWriteCount = boundResources.size();
        assert(descriptorWriteCount == vkLayout->GetDesc().elements.size());

        for (int i = 0; i < descriptorWriteCount; i++)
        {
            auto& elem = vkLayout->GetDesc().elements[i];
            auto type = elem.kind;
        }

    }

} // namespace Veldrid


