#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <alloy/alloy.hpp>

#include "Allocators.hpp"

template<typename T>
using alloy_sp = alloy::common::sp<T>;

constexpr uint32_t INVALID_ID = 0xffffffff;

struct Vertex {
    glm::vec3 position;
    glm::vec2 texCoord;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec3 bitangent;
};

// Assume triangle list topology
// CCW front facing
struct Mesh {
    std::vector<Vertex> vertices;
    //std::vector<uint32_t> indices;
};

/* GPU scene representation:
 * struct Vertex {
 *     float3 position;
 *     float2 texCoord;
 *     float3 normal;
 * };
 *
 * Index buffer uses uint32_t   
 * StructuredBuffer<uint> objectIdx; //Match with index buffer.
 *                                   // Used to fetch per object data
 * struct PerObjData {
 *     float4x4 transform;
 *     //Material
 *     float4 color;
 *     float roughness, metallic;
 * };
 * StructuredBuffer<PerObjData> //per object data
 *
 */

struct PerObjData {
    glm::mat4 transform;
    //Material
    glm::vec4 color;
    float roughness, metallic;
};

struct SceneDescriptor{
    glm::vec4 skyBoxSkyColor;
    glm::vec4 skyBoxLightDir;
    glm::vec4 skyBoxLightColor;

    glm::vec4 cameraPos;
    glm::mat4 view;
    glm::mat4 proj;
};

struct MeshComponent {
    uint32_t meshId;

    //Material
    glm::vec4 color;
    float roughness, metallic;

    static MeshComponent CreateDefault() {
        return MeshComponent {
            .meshId = INVALID_ID,
        };
    }
};

struct TransformComponent {
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;

    static TransformComponent CreateDefault() {
        return TransformComponent {
            .position = {},
            .rotation = glm::quat(1,0,0,0),
            .scale = {1,1,1}
        };
    }

    glm::vec3 GetEulerAngles() const {
        return glm::eulerAngles(rotation);
    }

    void SetEulerAngles(const glm::vec3& eulerAngles) {
        rotation = glm::quat(eulerAngles);
    }

    glm::mat4 BuiltMatrix() const {
        auto m = glm::mat4(1);//Identity matrix
        m = glm::translate(m, position);
        m *= glm::mat4_cast(rotation);
        m = glm::scale(m, scale);
        return m;
    }
};

struct SceneObject {
    uint32_t id;
    uint32_t parentId;

    TransformComponent transform;
    MeshComponent mesh;

    static SceneObject CreateDefault() {
        return SceneObject {
            .parentId = INVALID_ID,
            .transform = TransformComponent::CreateDefault(),
            .mesh = MeshComponent::CreateDefault()
        };
    }
};

class Scene {
    alloy_sp<alloy::IGraphicsDevice> _dev;

    static constexpr uint32_t kInvalidId = std::numeric_limits<uint32_t>::max();

    alloy_sp<alloy::IBuffer> _indexBuffer, //No holes allowed. May need to entirely recreate
                             _objIdxBuffer,

                    _vertexBuffer,  //Holds all registered meshes. May have holes after mesh modify
                    _perObjectDataBuffer;

    alloy_sp<alloy::IResourceLayout> _resLayout;
    alloy_sp<alloy::IResourceSet> _resSet;

    struct MeshDataHolder {
        Mesh mesh;
        uint32_t firstVertexIndex, vertexCnt;
        bool isDirty;
    };
    std::vector<MeshDataHolder> _meshes;
    FreeListAllocator<uint32_t> _meshAllocator;

    //std::vector<Material> _materials;
    
    struct SceneObjectHolder {
        SceneObject object;
        bool isAlive;
        bool isTransformDirty;
        bool isMeshDirty;
    };

    std::vector<glm::mat4> _cachedTransforms;

    std::vector<SceneObjectHolder> _objects;
    BitmapAllocator<uint32_t> _objectAlloc;

    uint32_t _meshVertexCnt;
    uint32_t _sceneVertexCnt;

    void _CreateResLayout();
    void _CreateResSet();

public:

    Scene(alloy_sp<alloy::IGraphicsDevice> dev);
    ~Scene();

    class Iterator {
    public:
        using value_type = SceneObject;
        using pointer = value_type*;
        using reference = value_type&;
    private:
        friend class Scene;

        Scene* _scene;
        uint32_t _idx;

        void _FindNextAliveObj() {
            if (!_scene) return;
            while (_idx < _scene->_objects.size()) {
                const auto& currHolder = _scene->_objects[_idx];
                if(currHolder.isAlive) break;

                ++_idx;
            }
        }

        Iterator(Scene* scene)
            : _scene(scene), _idx(0)
        {
            _FindNextAliveObj();
        }

        Iterator(Scene* scene, uint32_t idx)
            : _scene(scene)
            , _idx(idx)
        {}

    public:
        Iterator()
            : _scene(nullptr), _idx(0) {}

        ~Iterator() = default;

        Iterator(const Iterator& other) = default;
        Iterator(Iterator&& other) = default;

        Iterator& operator=(const Iterator& other) = default;
        Iterator& operator=(Iterator&& other) = default;

        bool operator==(const Iterator& other) const;
        //SceneObject& operator*();
        const SceneObject& operator*() const;
        //SceneObject* operator&();
        const SceneObject* operator&() const;
        //SceneObject* operator->();
        const SceneObject* operator->() const;
        Iterator& operator++();  // prefix
        Iterator operator++(int);//postfix
    };

    Iterator begin();
    Iterator end();

    uint32_t AddMesh(Mesh&& mesh);
    Mesh* GetMesh(uint32_t meshID);
    const Mesh* GetMesh(uint32_t meshID) const;
    //uint32_t AddMaterial(Material&& mat);
    //Material* GetMaterial(uint32_t materialID);
    //const Material* GetMaterial(uint32_t materialID) const;

    uint32_t CreateSceneObject();
    void DestroySceneObject(uint32_t objectId);

    const SceneObject* GetSceneObject(uint32_t objectId) const;

    TransformComponent* GetTransformComponent(uint32_t objectId);
    const TransformComponent* GetTransformComponent(uint32_t objectId) const;

    MeshComponent* GetMeshComponent(uint32_t objectId);
    const MeshComponent* GetMeshComponent(uint32_t objectId) const;

    void UpdateGPUScene();

    alloy_sp<alloy::IBuffer> GetVertexBuffer() const { return _vertexBuffer; }
    alloy_sp<alloy::IBuffer> GetIndexBuffer() const { return _indexBuffer; }
    alloy_sp<alloy::IResourceSet> GetResourceSet() const { return _resSet; }
    alloy_sp<alloy::IResourceLayout> GetResourceLayout() const { return _resLayout; }

    uint32_t GetVertexCount() const { return _sceneVertexCnt; }
};
