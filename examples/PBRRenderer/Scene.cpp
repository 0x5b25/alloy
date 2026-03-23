#include "Scene.hpp"

#include <cassert>

Scene::Scene(alloy_sp<alloy::IGraphicsDevice> dev)
    : _dev(std::move(dev))
    , _vertexBuffer{}
    , _perObjectDataBuffer{}
{ }

Scene::~Scene() {

}

bool Scene::Iterator::operator==(const Iterator& other) const {
    return _scene == other._scene && _idx == other._idx;
}

SceneObject& Scene::Iterator::operator*() {
    return _scene->_objects[_idx].object;
}

SceneObject const& Scene::Iterator::operator*() const {
    return _scene->_objects[_idx].object;
}

SceneObject* Scene::Iterator::operator&() {
    return _scene ? &_scene->_objects[_idx].object : nullptr;
}

SceneObject const* Scene::Iterator::operator&() const {
    return _scene ? &_scene->_objects[_idx].object : nullptr;
}

SceneObject* Scene::Iterator::operator->() {
    return _scene ? &_scene->_objects[_idx].object : nullptr;
}

SceneObject const* Scene::Iterator::operator->() const {
    return _scene ? &_scene->_objects[_idx].object : nullptr;
}

Scene::Iterator& Scene::Iterator::operator++() {
    if (!_scene) return *this;
    ++_idx;
    _FindNextAliveObj();
    return *this;
}

Scene::Iterator Scene::Iterator::operator++(int) {
    Iterator old = *this;
    ++(*this);
    return old;
}

Scene::Iterator Scene::begin() { return Iterator(this); }

Scene::Iterator Scene::end() { return Iterator(this, _objects.size()); }

uint32_t Scene::AddMesh(Mesh&& mesh) {
    auto id = _meshes.size();
    _meshes.emplace_back(std::move(mesh), 0, true);
    return id;
}

Mesh* Scene::GetMesh(uint32_t meshID) {
    if (meshID >= _meshes.size()) return nullptr;
    auto& holder = _meshes[meshID];
    holder.isDirty = true;
    return &holder.mesh;
}

const Mesh* Scene::GetMesh(uint32_t meshID) const {
    if (meshID >= _meshes.size()) return nullptr;
    return &_meshes[meshID].mesh;
}

//uint32_t Scene::AddMaterial(Material&& mat) {
//    auto id = _materials.size();
//    _materials.emplace_back(std::move(mat));
//    _materialDirty.emplace_back(true);
//    return id;
//}
//
//Material* Scene::GetMaterial(uint32_t materialID) {
//    if (materialID >= _materials.size()) return nullptr;
//    _materialDirty[materialID] = true;
//    return &_materials[materialID];
//}
//
//const Material* Scene::GetMaterial(uint32_t materialID) const {
//    if (materialID >= _materials.size()) return nullptr;
//    return &_materials[materialID];
//}

uint32_t Scene::CreateSceneObject() {
    uint32_t id = _objectAlloc.Allocate();
    if (id == _objectAlloc.INVALID_VALUE) {
        //Not enough slots
        id = _objects.size();
        _objectAlloc.GrowAtEnd(1);
        _objects.emplace_back(SceneObject::CreateDefault(), true, true);
    }

    assert(id < _objects.size());
    auto& holder = _objects[id];

    holder.isAlive = true;
    holder.isDirty = true;
    return id;
}

void Scene::DestroySceneObject(uint32_t objectId) {
    if(objectId > _objects.size()) return;
    auto& objToDestroy = _objects[objectId];
    if(!objToDestroy.isAlive) return;

    for (auto& holder : _objects) {
        if(holder.isAlive && holder.object.parentId == objectId) {
            holder.object.parentId = kInvalidId;
        }
    }

    _objectAlloc.Free(objectId);
    objToDestroy.isAlive = false;
    //Mark slot as dirty to trigger index buffer rebuild
    objToDestroy.isDirty = true;
}

SceneObject* Scene::GetSceneObject(uint32_t objectId) {
    if(objectId > _objects.size()) return nullptr;
    auto& objHolder = _objects[objectId];

    if (!objHolder.isAlive) return nullptr;
    objHolder.isDirty = true;
    return &objHolder.object;
}

SceneObject const* Scene::GetSceneObject(uint32_t objectId) const {
    if(objectId > _objects.size()) return nullptr;
    auto& objHolder = _objects[objectId];

    if (!objHolder.isAlive) return nullptr;
    return &objHolder.object;
}

void Scene::UpdateGPUScene() {
    //For each modified mesh:
    //    is it fit in original space?
    //        if fits, upload modified data, and scale spans accordingly
    //        if not:
    //            1. find free space that is large enough
    //            1a. if no free space availble, resize buffer and put at end
    //            2. upload modified data, change spans accordingly
    bool anyMeshDirty = false;
    {
        bool needNewBuffer = false;
        for(auto& slot : _meshes) {
            if(!slot.isDirty) continue;
            anyMeshDirty = true;
            auto currVertCnt = slot.mesh.vertices.size();

            if(slot.vertexCnt >= currVertCnt) {
                //We have enough space in original position
                slot.vertexCnt = currVertCnt;
                continue;
            }

            //Need new space
            auto newSpaceStart = _meshAllocator.Allocate(currVertCnt);
            if(newSpaceStart == _meshAllocator.INVALID_VALUE) {
                //Construct a new buffer since we need a new buffer anyways
                needNewBuffer = true;
                break;
            }

            slot.firstVertexIndex = newSpaceStart;
            slot.vertexCnt = currVertCnt;
        }

        if(needNewBuffer) {
            //Gather buffer size
            uint32_t totalVertCnt = 0;
            for(auto& slot : _meshes) { 
                totalVertCnt += slot.mesh.vertices.size();
            }

            alloy::IBuffer::Description vbDesc{};
            vbDesc.hostAccess = alloy::HostAccess::PreferDeviceMemory;
            vbDesc.usage.vertexBuffer = 1;
            vbDesc.sizeInBytes = totalVertCnt * sizeof(Vertex);
            auto vb = _dev->GetResourceFactory().CreateBuffer(vbDesc);

            auto pBuffer = (uint8_t*)vb->MapToCPU();
            uint32_t currOffset;
            for(auto& slot : _meshes) {
                auto thisVertCnt = slot.mesh.vertices.size();
                auto copySize = thisVertCnt * sizeof(Vertex);
                auto pDst = pBuffer + currOffset * sizeof(Vertex);

                memcpy(pDst, slot.mesh.vertices.data(), copySize);

                slot.firstVertexIndex = currOffset;
                slot.vertexCnt = thisVertCnt;
                //Mark all as dirty
                slot.isDirty = false;

                currOffset += thisVertCnt;
            }

            vb->UnMap();

            _meshAllocator.Reset(totalVertCnt);
            _meshAllocator.Allocate(totalVertCnt);

            _vertexBuffer = vb;
        }
        else {
            auto pBuffer = (uint8_t*)_vertexBuffer->MapToCPU();
            for(auto& slot : _meshes) { 
                if(!slot.isDirty) continue;
                slot.isDirty = false;

                auto pDst = pBuffer + slot.firstVertexIndex * sizeof(Vertex);
                auto copySize = slot.vertexCnt * sizeof(Vertex);

                memcpy(pDst, slot.mesh.vertices.data(), copySize);
            }
            _vertexBuffer->UnMap();
        }
    }

    //Propagate dirty bits along parent tree
    //For each not visited SceneObject
    //    1. find its highest level parent and keep track of the parent chain
    //    2. mark all the objects along the chain as visited
    //    3. if any of the objects are marked dirty, re-calculate its transform matrix
    //         and update its _cachedTransforms entry
    //    4. calculate all the transform matrices down the chain and update their
    //          _cachedTransforms entries, also mark them as dirty
    bool anyObjSlotDirty = false;
    {
        std::vector<bool> visited(_objects.size(), false);
        for(uint32_t i = 0; i < _objects.size(); ++i) {
            if(visited[i]) continue;
            auto& slot = _objects[i];
            //Objects are just destroyed also considered dirty
            //because we need to rebuild index buffer
            if(slot.isDirty) 
                anyObjSlotDirty = true;
            if(!slot.isAlive) continue;

            //Find the highest not visited parent
            std::vector<uint32_t> inheritChain { };
            auto currObjIdx = i;
            while(currObjIdx != INVALID_ID) {
                if(visited[currObjIdx]) break;
                inheritChain.push_back(currObjIdx);
                currObjIdx = _objects[currObjIdx].object.parentId;
            }

            //Find the first changed parent object
            bool hasChangedObject = false;
            int currIdx = inheritChain.size() - 1;
            while(currIdx >= 0) {
                auto objIdx = inheritChain[currIdx];
                auto& currObjData = _objects[objIdx];
                if(currObjData.isDirty) {
                    hasChangedObject = true;
                    break;
                }
                visited[objIdx] = true;
                currIdx--;
            }

            //Work its way down and update all cached transforms
            if(hasChangedObject) {
                while(currIdx >= 0) {
                    auto objIdx = inheritChain[currIdx];
                    auto& currObjData = _objects[objIdx];
                    currObjData.isDirty = true;
                    visited[objIdx] = true;
                    auto& finalT = _cachedTransforms[objIdx];

                    finalT = currObjData.object.transform.BuiltMatrix();
                    auto parentObjIdx = currObjData.object.parentId;
                    if(parentObjIdx != INVALID_ID) {
                        const auto& parentT = _cachedTransforms[parentObjIdx];
                        finalT = parentT * finalT;
                    }

                    currIdx--;
                }
            }
        }

        //Update PerObjecData buffer
        //do we have enough space?
        bool needNewBuffer = true;
        if(_perObjectDataBuffer) {
            auto reqSize = sizeof(PerObjData) * _objects.size();
            auto& desc = _perObjectDataBuffer->GetDesc();
            if(desc.sizeInBytes >= reqSize) {
                needNewBuffer = false;
            }
        }

        if(needNewBuffer) {
            auto reqSize = sizeof(PerObjData) * _objects.size();
            alloy::IBuffer::Description vbDesc{};
            vbDesc.hostAccess = alloy::HostAccess::PreferDeviceMemory;
            vbDesc.usage.structuredBufferReadOnly = 1;
            vbDesc.sizeInBytes = reqSize;
            _perObjectDataBuffer = _dev->GetResourceFactory().CreateBuffer(vbDesc);
        }

    //For each dirty SceneObject PerObjData
    //    upload modified data (PerObjData as a whole)
        auto pBuffer = (uint8_t*)_perObjectDataBuffer->MapToCPU();

        for(auto i = 0 ; i < _objects.size(); ++i) {
            auto& slot = _objects[i];
            if(!slot.isDirty && !needNewBuffer) continue;

            PerObjData data{};
            if(slot.isAlive) {
                data.transform = _cachedTransforms[i];

                data.color = slot.object.mesh.color;
                data.metallic = slot.object.mesh.metallic;
                data.roughness = slot.object.mesh.roughness;
            }
            
            auto pDst = pBuffer + i * sizeof(PerObjData);

            memcpy(pDst, &data, sizeof(PerObjData));
            //Mark all as dirty
            slot.isDirty = false;
        }

        _perObjectDataBuffer->UnMap();
    }


    //All the newly changed mesh that verts can't fit in original are appended at end
    //Rebuild whole index buffer & objIdx buffer if any of the meshes or SceneObjects
    //are marked dirty
    if(anyMeshDirty || anyObjSlotDirty) {
        //Gather vertex count
        uint32_t vertCnt = 0;
        for(auto& slot : _objects) {
            if(!slot.isAlive) continue;
            auto meshID = slot.object.mesh.meshId;
            const auto& mesh = _meshes[meshID];
            vertCnt += mesh.vertexCnt;
        }

        std::vector<uint32_t> indexData, objIdxData;
        indexData.reserve(vertCnt);
        objIdxData.reserve(vertCnt);
        for(uint32_t i = 0; i < _objects.size(); ++i) {
            auto& slot = _objects[i];
            if(!slot.isAlive) continue;

            auto meshID = slot.object.mesh.meshId;
            if(meshID == INVALID_ID) continue;
            const auto& mesh = _meshes[meshID];

            for(auto vertIdx = 0; vertIdx < mesh.vertexCnt; ++vertIdx) {
                indexData.push_back(vertIdx + mesh.firstVertexIndex);
                objIdxData.push_back(i);
            }
        }

        auto _UpdateBuffer = [&](const std::vector<uint32_t>& data, 
                                 alloy_sp<alloy::IBuffer>& bufferSlot)
        {
            auto reqSize = sizeof(uint32_t) * data.size();
            bool needNewBuffer = true;
            if(bufferSlot) {
                auto& desc = bufferSlot->GetDesc();
                if(desc.sizeInBytes >= reqSize) {
                    needNewBuffer = false;
                }
            }

            if(needNewBuffer) {
                alloy::IBuffer::Description vbDesc{};
                vbDesc.hostAccess = alloy::HostAccess::PreferDeviceMemory;
                vbDesc.usage.indexBuffer = 1;
                vbDesc.usage.structuredBufferReadOnly = 1;
                vbDesc.sizeInBytes = reqSize;
                bufferSlot = _dev->GetResourceFactory().CreateBuffer(vbDesc);
            }

            auto dst = bufferSlot->MapToCPU();
            memcpy(dst, data.data(), reqSize);

            bufferSlot->UnMap();
        };

        _UpdateBuffer(indexData, _indexBuffer);
        _UpdateBuffer(objIdxData, _objIdxBuffer);
    }

}