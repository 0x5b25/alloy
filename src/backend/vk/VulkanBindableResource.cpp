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
        vkDestroyDescriptorSetLayout(vkDev->LogicalDev(), _dsl, nullptr);
    }
    
    sp<ResourceLayout> VulkanResourceLayout::Make(
        const sp<VulkanDevice>& dev,
        const Description& desc
    ){
        VkDescriptorSetLayoutCreateInfo dslCI{};
        dslCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

        auto& elements = desc.elements;
        std::vector<VkDescriptorType> _descriptorTypes {elements.size()};
        std::vector<VkDescriptorSetLayoutBinding> bindings {elements.size()};

        std::uint32_t uniformBufferCount = 0;
        std::uint32_t uniformBufferDynamicCount = 0;
        std::uint32_t sampledImageCount = 0;
        std::uint32_t samplerCount = 0;
        std::uint32_t storageBufferCount = 0;
        std::uint32_t storageBufferDynamicCount = 0;
        std::uint32_t storageImageCount = 0;

        unsigned dynamicBufferCount = 0;

        for (unsigned i = 0; i < elements.size(); i++)
        {
            bindings[i].binding = i;
            bindings[i].descriptorCount = 1;
            VkDescriptorType descriptorType = VK::priv::VdToVkDescriptorType(elements[i].kind, elements[i].options);
            bindings[i].descriptorType = descriptorType;
            bindings[i].stageFlags = VK::priv::VdToVkShaderStages(elements[i].stages);
            if (elements[i].options.dynamicBinding) {
                dynamicBufferCount += 1;
            }

            _descriptorTypes[i] = descriptorType;

            switch (descriptorType)
            {
                case VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLER:
                    samplerCount += 1;
                    break;
                case VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                    sampledImageCount += 1;
                    break;
                case VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                    storageImageCount += 1;
                    break;
                case VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    uniformBufferCount += 1;
                    break;
                case VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                    uniformBufferDynamicCount += 1;
                    break;
                case VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                    storageBufferCount += 1;
                    break;
                case VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                    storageBufferDynamicCount += 1;
                    break;
            }
        }

        VK::priv::DescriptorResourceCounts drcs {};
            drcs.uniformBufferCount = uniformBufferCount;
            drcs.uniformBufferDynamicCount = uniformBufferDynamicCount;
            drcs.sampledImageCount = sampledImageCount;
            drcs.samplerCount = samplerCount;
            drcs.storageBufferCount = storageBufferCount;
            drcs.storageBufferDynamicCount = storageBufferDynamicCount;
            drcs.storageImageCount = storageImageCount;

        dslCI.bindingCount = elements.size();
        dslCI.pBindings = bindings.data();

        VkDescriptorSetLayout rawDsl;
        VK_CHECK(vkCreateDescriptorSetLayout(dev->LogicalDev(), &dslCI, nullptr, &rawDsl));

        auto dsl = new VulkanResourceLayout(dev, desc);
        dsl->_dsl = rawDsl;
        dsl->_dynamicBufferCount = dynamicBufferCount;
        dsl->_drcs = drcs;

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

        VkDescriptorSetLayout dsl = vkLayout->GetHandle();
        //_descriptorCounts = vkLayout.DescriptorResourceCounts;
        auto descriptorAllocationToken = dev->AllocateDescriptorSet(dsl);

        auto& boundResources = desc.boundResources;
        auto descriptorWriteCount = boundResources.size();
        std::vector<VkWriteDescriptorSet> descriptorWrites(descriptorWriteCount);
        std::vector<VkDescriptorBufferInfo> bufferInfos(descriptorWriteCount);
        std::vector<VkDescriptorImageInfo> imageInfos(descriptorWriteCount);

        assert(descriptorWriteCount == vkLayout->GetDesc().elements.size());

        //std::vector<sp<BindableResource>> _refCounts;
        std::unordered_set<VulkanTexture*> texReadOnly, texRW;
        for (int i = 0; i < descriptorWriteCount; i++)
        {
            auto& elem = vkLayout->GetDesc().elements[i];
            auto type = elem.kind;

            descriptorWrites[i].sType = VkStructureType::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[i].descriptorCount = 1;
            descriptorWrites[i].descriptorType = VdToVkResourceKind(type, elem.options.dynamicBinding, elem.options.writable);
            descriptorWrites[i].dstBinding = i;
            descriptorWrites[i].dstSet = descriptorAllocationToken.GetHandle();

            using _ResKind = IBindableResource::ResourceKind;
            switch(type){
                case _ResKind::UniformBuffer:
                case _ResKind::StorageBuffer: {
                    auto* range = PtrCast<BufferRange>(boundResources[i].get());
                    auto* rangedVkBuffer = static_cast<const VulkanBuffer*>(range->GetBufferObject());
                    bufferInfos[i].buffer = rangedVkBuffer->GetHandle();
                    bufferInfos[i].offset = range->GetOffsetInBytes();
                    bufferInfos[i].range = range->GetSizeInBytes();
                    descriptorWrites[i].pBufferInfo = &bufferInfos[i];
                    //_refCounts.push_back(boundResources[i]);
                } break;

                case _ResKind::Texture:{
                    auto* vkTexView = PtrCast<VulkanTextureView>(boundResources[i].get());
                    imageInfos[i].imageView = vkTexView->GetHandle();
                    if(elem.options.writable){
                        imageInfos[i].imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL;
                    } else {
                        imageInfos[i].imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    }
                    descriptorWrites[i].pImageInfo = &imageInfos[i];

                    auto vkTex = PtrCast<VulkanTexture>(vkTexView->GetTarget().get());
                    texReadOnly.insert(vkTex);
                    //_sampledTextures.Add(Util.AssertSubtype<Texture, VkTexture>(texView.Target));
                    //_refCounts.push_back(boundResources[i]);
                }break;


                case _ResKind::Sampler:{
                    auto* sampler = PtrCast<VulkanSampler>(boundResources[i].get());
                    imageInfos[i].sampler = sampler->GetHandle();
                    descriptorWrites[i].pImageInfo = &imageInfos[i];
                    //_refCounts.push_back(boundResources[i]);
                }break;
            }
            
        }

        vkUpdateDescriptorSets(dev->LogicalDev(), descriptorWriteCount, descriptorWrites.data(), 0, nullptr);
        
        auto descSet = new VulkanResourceSet(dev, std::move(descriptorAllocationToken), desc);
        descSet->_texReadOnly = std::move(texReadOnly);
        descSet->_texRW = std::move(texRW);

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


