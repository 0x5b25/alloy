#pragma once

#include <glm/glm.hpp>
#include <alloy/alloy.hpp>

template<typename T>
using alloy_sp = alloy::common::sp<T>;

class MeshComponent {

};

class TransformComponent {
    glm::vec3 position, rotatio, scale;
};

class SceneObject {

};

class Scene {
    alloy_sp<alloy::IGraphicsDevice> _dev;

    struct Span {
        uint32_t startOffset, size;
        Span* pNext;
    };

    struct BufferContainer {
        alloy_sp<alloy::IBuffer> buffer;
        Span* pFreeSpans;
    };

    BufferContainer _vertexBuffer, _indexBuffer, _perObjectDataBuffer;


public:

    Scene(alloy_sp<alloy::IGraphicsDevice> dev);
    ~Scene();

    class Iterator {
    public:
        using value_type = SceneObject;
        using pointer = value_type*;
        using reference = value_type&;
    private:
        Scene* _scene;

    public:
        Iterator();
        ~Iterator();

        Iterator(const Iterator& other);
        Iterator(Iterator&& other);

        Iterator& operator=(const Iterator& other);
        Iterator& operator=(Iterator&& other);

        bool operator==(const Iterator& other) const;

        SceneObject& operator*();
        SceneObject const& operator*() const;

        SceneObject* operator&();
        SceneObject const* operator&() const;
        
        SceneObject* operator->();
        SceneObject const* operator->() const;

        Iterator& operator++();  // prefix
        Iterator operator++(int);//postfix
    };

    Iterator begin();
    Iterator end();

    SceneObject& CreateObject();

    void UpdateGPUScene();

};
