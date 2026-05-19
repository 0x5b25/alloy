# Road to Bindless

## Intent

Alloy currently uses a fixed resource binding model:

- `IResourceLayout` declares the shader resource shape.
- `IResourceSet` is created with resources matching that shape.
- Existing sets are effectively immutable; changing resources means creating a new set.

This note records the planned path from that model to bindless resource access. The goal is
not to expose every Vulkan descriptor feature. Alloy should collapse backend fragmentation
into a small set of binding tiers that are easy for engine code to reason about.

## Tiered Binding Model

Replace `AdapterInfo::Capabilities::supportBindless` with a coarse binding model:

```cpp
enum class ResourceBindingModel {
    FixedBindings,       // T0
    DescriptorIndexing,  // T1
    DescriptorHeap       // T2
};
```

Non-uniform resource indexing remains a separate advanced feature bit. A backend can support
T1 descriptor arrays without promising that per-lane divergent descriptor indices are cheap or
available.

### T0: Fixed Bindings

T0 is the current alloy model.

- Resource layouts are fixed.
- Resource sets are created with resources matching the layout.
- Resource set entries are not updated after creation.
- Descriptor arrays are not dynamically indexed.
- All declared resource entries must be populated.

T0 still allows ordinary dynamic indexing inside buffer contents. For example, a shader may
read `vertices[vertexIndex]` or `perObjectData[objectIndex]` from a bound `StructuredBuffer`.
That is data indirection, not descriptor/resource bindless.

### T1: Descriptor Indexing

T1 keeps the existing `ResourceLayout + ResourceSet` model, but allows declared descriptor
arrays to be sparse and mutable.

T1 is advertised only when the backend supports the complete alloy-level contract:

- Resource layouts still exist.
- Resource sets still exist.
- `bindingCount` is fixed capacity.
- Descriptor array entries may be dynamically indexed.
- Descriptor array entries may be partially populated.
- Resource set entries may be updated after creation.
- Updates may happen after a resource set has been bound or submitted.
- There is no automatic descriptor snapshot or versioning contract.
- The application owns hazards from overwriting entries that pending GPU work may read.
- There is no global `ResourceDescriptorHeap[]` / `SamplerDescriptorHeap[]` shader ABI.

For Vulkan this maps to Vulkan 1.2 descriptor indexing features such as descriptor arrays,
partially bound bindings, update-after-bind-capable bindings, and descriptor set updates. For
DX12 and Metal it maps to rewriting descriptor table / argument table entries under the same
user-synchronized entry hazard rules.

T1 deliberately does not include variable descriptor count allocation. In Vulkan, variable
descriptor count is an allocation optimization with awkward layout constraints, and it has no
clear corresponding concept in DX12 or Metal. Alloy uses fixed capacities for T1.

### T2: Descriptor Heap

T2 is the full bindless target.

- Shaders use a global heap-style ABI, such as SM 6.6 `ResourceDescriptorHeap[]` and
  `SamplerDescriptorHeap[]`.
- Resource access is driven by integer descriptor indices stored in material, scene, or other
  GPU records.
- The backend exposes standalone descriptor heap/table allocation.
- Descriptor handles become incrementable or index-like values.
- Vulkan requires `VK_EXT_descriptor_buffer` for T2.
- Vulkan does not provide a T2 fallback without `VK_EXT_descriptor_buffer`; descriptor-indexing
  global arrays remain a T1 engine convention.
- DX12 maps to native shader-visible descriptor heaps.
- Metal maps to argument buffer table storage.

T2 requires a larger shader remapping design and is intentionally not part of the T1 work.

## T2 Public API Direction (Draft)

T2 should not be a larger `IMutableResourceSet`. It introduces descriptor heap storage that
allocates shader-visible integer indices. Higher-level engine systems, such as material
tables and scene records, store those indices in GPU data.

Alloy exposes two descriptor heap object types:

- A resource descriptor heap for uniform buffers, read-only storage buffers, read-write
  storage buffers, sampled textures, and storage textures.
- A sampler descriptor heap for samplers.

The portable shader ABI tokens are 32-bit descriptor indices relative to the currently bound
resource descriptor heap or sampler descriptor heap. CPU-side wrappers may carry type
information for validation, but GPU records should store plain `uint32_t` values.

Conceptually:

```cpp
enum class ResourceDescriptorType {
    UniformBuffer,
    ReadOnlyStorageBuffer,
    ReadWriteStorageBuffer,
    SampledTexture,
    StorageTexture
};

struct ResourceDescriptorIndex {
    static constexpr uint32_t Invalid = 0xffffffffu;
    uint32_t value = Invalid;
};

struct SamplerDescriptorIndex {
    static constexpr uint32_t Invalid = 0xffffffffu;
    uint32_t value = Invalid;
};

struct ResourceDescriptorRange {
    ResourceDescriptorType type;
    uint32_t firstIndex;
    uint32_t count;

    ResourceDescriptorIndex At(uint32_t offset) const;
};

struct SamplerDescriptorRange {
    uint32_t firstIndex;
    uint32_t count;

    SamplerDescriptorIndex At(uint32_t offset) const;
};

struct ResourceDescriptorHeapDescription {
    uint32_t capacity;
};

struct SamplerDescriptorHeapDescription {
    uint32_t capacity;
};

struct ResourceDescriptorWrite {
    uint32_t firstIndex;
    ResourceDescriptorType type;
    std::span<const common::sp<IBindableResource>> resources;
};

struct ResourceDescriptorClear {
    uint32_t firstIndex;
    uint32_t count;
    ResourceDescriptorType type;
};

struct SamplerDescriptorWrite {
    uint32_t firstIndex;
    std::span<const common::sp<ISampler>> samplers;
};

struct SamplerDescriptorClear {
    uint32_t firstIndex;
    uint32_t count;
};

class IResourceDescriptorHeap : public common::RefCntBase {
public:
    virtual const ResourceDescriptorHeapDescription& GetDesc() const = 0;

    virtual ResourceDescriptorRange Allocate(
        ResourceDescriptorType type,
        uint32_t count,
        uint32_t alignment = 1) = 0;
    virtual void Free(const ResourceDescriptorRange& range) = 0;

    virtual void Write(std::span<const ResourceDescriptorWrite> writes) = 0;
    virtual void Clear(std::span<const ResourceDescriptorClear> clears) = 0;

    virtual void* GetNativeHandle() const { return nullptr; }
};

class ISamplerDescriptorHeap : public common::RefCntBase {
public:
    virtual const SamplerDescriptorHeapDescription& GetDesc() const = 0;

    virtual SamplerDescriptorRange Allocate(uint32_t count, uint32_t alignment = 1) = 0;
    virtual void Free(const SamplerDescriptorRange& range) = 0;

    virtual void Write(std::span<const SamplerDescriptorWrite> writes) = 0;
    virtual void Clear(std::span<const SamplerDescriptorClear> clears) = 0;

    virtual void* GetNativeHandle() const { return nullptr; }
};
```

`ResourceDescriptorType` is deliberately separate from `IBindableResource::ResourceKind`.
T0/T1 resource layouts can infer read/write interpretation from the layout slot, but T2
writes do not have a layout slot. The write must say whether a storage buffer or texture is
read-only or read-write so backends can create the correct native descriptor. Samplers use a
separate heap type and therefore do not need a selectable descriptor type.

Descriptor heap creation is a sibling factory API beside resource-set creation. Descriptor
heap binding is encoder-level state that T2-compatible resource-set binding must respect:

```cpp
ResourceFactory::CreateResourceDescriptorHeap(resourceDesc);
ResourceFactory::CreateSamplerDescriptorHeap(samplerDesc);

IRenderCommandEncoder::SetDescriptorHeaps(
    const common::sp<IResourceDescriptorHeap>& resourceHeap,
    const common::sp<ISamplerDescriptorHeap>& samplerHeap);
IComputeCommandEncoder::SetDescriptorHeaps(
    const common::sp<IResourceDescriptorHeap>& resourceHeap,
    const common::sp<ISamplerDescriptorHeap>& samplerHeap);
```

The binding call makes descriptor indices meaningful for subsequent draw or dispatch work on
that encoder. It takes one resource descriptor heap and one sampler descriptor heap because
the shader-visible ABI has two heap families. If a shader uses only one family, the other
binding can be null or omitted by an overload. Engines normally bind one global resource heap
and one global sampler heap for the frame, but Alloy should allow switching heaps as long as
the application understands that all stored indices are interpreted relative to the newly
bound heaps.

### T2 Pipeline Layout And Mixed Bindings

T2 does not remove the top-level resource layout. The layout still owns the backend binding
ABI: the DX12 root signature, the Vulkan pipeline layout, and Metal's first-level argument
buffer or root signature. A T2-capable layout may combine ordinary bindful slots with global
descriptor heap access.

Bindful resources remain declared in `IResourceLayout::Description::shaderResources`.
Heap-indexed resources are not declared as individual shader resource slots; they are enabled
by descriptor heap usage flags on the layout. This allows mixed shaders where hot frame/pass
constants remain bindful while material, texture, and scene indirection uses global heaps.

Conceptually:

```cpp
struct DescriptorHeapPipelineOptions {
    bool useResourceDescriptorHeap;
    bool useSamplerDescriptorHeap;
};
```

The exact type name is still open, but the placement should be layout-owned rather than only
pipeline-owned. A later cleanup could split this into a true `IPipelineLayout` if
`IResourceLayout` becomes too resource-set-specific.

Shader-facing HLSL stays heap-native:

```hlsl
ConstantBuffer<FrameConstants> frame : register(b0, space0);

StructuredBuffer<InstanceData> instances =
    ResourceDescriptorHeap[scene.instanceBufferIndex];

uint textureIndex = NonUniformResourceIndex(material.baseColorTexture);
Texture2D<float4> baseColor = ResourceDescriptorHeap[textureIndex];

SamplerState samplerState =
    SamplerDescriptorHeap[material.baseColorSampler];
```

`NonUniformResourceIndex` remains gated by the separate non-uniform resource indexing
capability. A T2 backend may support heap indexing without promising cheap or legal per-lane
divergent indices.

The command encoder has only one active resource descriptor heap and one active sampler
descriptor heap at a time. `SetDescriptorHeaps(resourceHeap, samplerHeap)` binds that heap
pair for all subsequent T2 heap-indexed access, and for any bindful descriptor table ranges
that are represented inside those heaps. Binding a T2 descriptor heap pair must not be
treated as independent from resource-set binding.

For mixed T2 layouts, bindful resource-set binding may fill ordinary root/layout slots, but
it must not switch the native shader-visible heap pair. A backend may support bindful slots
by using root constants, root descriptors, direct buffer bindings, or descriptor-table ranges
allocated from the currently bound T2 heaps. A classic T0/T1 resource set that owns private
shader-visible descriptor storage is not compatible with a T2 heap-bound encoder unless the
backend can bind it without changing the active heap pair.

### T2 Lifetime And Hazard Contract

Resource and sampler descriptor heap writes are legal after heap binding and after
command-list submission in the same narrow storage sense as T1 mutable resource-set writes.
Alloy does not provide automatic descriptor snapshots, copy-on-write heap versions, or
retirement tracking.

The application owns these hazards:

- Overwriting, clearing, freeing, or reusing a descriptor entry that pending GPU work may
  read.
- Destroying or releasing a resource whose descriptor may still be read by pending GPU work.
- Reusing a descriptor index that is still stored in in-flight GPU records.

`IResourceDescriptorHeap` and `ISamplerDescriptorHeap` should keep strong references to
resources written into live entries. Overwriting, clearing, or freeing an entry drops the
previous reference. If pending GPU work may still read the previous descriptor, the
application must keep the old resource and old descriptor contents alive by delaying the
overwrite/free or by using a retired-frame allocator.

`Clear` is for releasing Alloy references and optionally installing backend null or dummy
descriptors for debugging. Reading a cleared or never-written descriptor is not portable and
is not part of the T2 contract.

Heap allocation is intentionally simple. Alloy can provide basic range allocators so small
users can call `Allocate` and `Free` directly. Engines that need frame retirement, stable
material handles, or streaming-friendly reuse can allocate coarse ranges and suballocate
inside them.

T2 descriptor heaps do not imply resource barriers, texture layout transitions, residency
management, or bindless material policy. Those remain engine responsibilities layered above
Alloy.

### T2 Backend Mapping

DX12:

- Resource heaps map to shader-visible `CBV_SRV_UAV` descriptor heaps.
- Sampler heaps map to shader-visible sampler descriptor heaps.
- Pipeline creation enables direct heap indexing root signature flags for the heaps the
  shader uses.
- `SetDescriptorHeaps` maps to `ID3D12GraphicsCommandList::SetDescriptorHeaps`.
- Writes create CBV/SRV/UAV or sampler descriptors at the allocated CPU heap indices.

Vulkan:

- Require `VK_EXT_descriptor_buffer`; without it the Vulkan backend must not advertise T2.
- Descriptor writes materialize backend descriptor bytes and copy them into descriptor-buffer
  storage.
- Command encoding binds descriptor buffers and offsets for the resource and sampler heaps.
- Descriptor-indexing-backed global arrays remain a valid T1 engine strategy, but they are
  not part of the Alloy T2 contract.

Metal:

- Resource and sampler heaps map to argument buffer table storage.
- Writes update argument table entries and retain referenced Alloy resources.
- Binding installs the resource and sampler argument tables for the active encoder.
- Shader conversion maps heap-style access to the corresponding argument-buffer arrays.

### T2 Capability And Limits

`ResourceBindingModel::DescriptorHeap` should be advertised only when the backend implements
the complete T2 contract: resource and sampler descriptor heap creation, allocation, writes,
command binding, pipeline heap ABI selection, and shader remapping. A backend should not
report T2 merely because the native API has one relevant feature bit. For Vulkan, the
required feature is specifically `VK_EXT_descriptor_buffer`; descriptor indexing alone is T1.

Adapter limits should grow explicit heap limits before the API lands:

```cpp
struct DescriptorHeapLimits {
    uint32_t maxResourceDescriptors;
    uint32_t maxSamplerDescriptors;
    uint32_t maxBoundResourceHeaps;
    uint32_t maxBoundSamplerHeaps;
};
```

The expected portable command-binding limit is one resource heap and one sampler heap at a
time. Higher native limits can remain backend-specific until there is a cross-backend use
case.

### T2 Open Questions

- Whether `IResourceDescriptorHeap` and `ISamplerDescriptorHeap` should own the small range
  allocators, or whether Alloy should expose only fixed heap storage plus write APIs and
  leave all allocation to engines.
- Exact placement of `DescriptorHeapPipelineOptions` in the pipeline descriptions.
- Whether a future `IPipelineLayout` should replace the current use of `IResourceLayout` for
  push constants and binding model selection.
- Whether debug builds should install dummy descriptors on `Clear`, and which dummy resources
  are required per descriptor type.
- How much validation can catch descriptor index reuse while GPU records are still in flight.
- Whether mixed bindful resources need a T2-compatible resource-set allocation mode, or
  whether the first implementation should limit mixed bindful paths to root constants, root
  descriptors, and direct buffer bindings that do not consume shader-visible heap state.
- Whether T1 and T2 renderer descriptor managers can share one lifetime/retirement policy
  while keeping separate shader-access frontend code.

## T1 Public API Direction

The current positional `IResourceSet::Description::boundResources` interface is too brittle
for sparse descriptor arrays. T1 updates should target resource layout slots, not raw shader
binding numbers.

Conceptually:

```cpp
struct ResourceSetWrite {
    uint32_t layoutSlot;
    uint32_t firstArrayElement;
    std::span<common::sp<IBindableResource>> resources;
};
```

`layoutSlot` refers to an entry in `IResourceLayout::Description::shaderResources`, or to a
future opaque slot handle derived from that entry. The backend maps that slot to native
binding information.

Creation continues to support the existing positional path for T0 and immutable resource
sets. T1 uses a sibling resource-set interface instead of extending `IResourceSet`, so old
bind entry points cannot accidentally accept mutable sets:

```cpp
class IMutableResourceSet : public common::RefCntBase {
    virtual void Update(std::span<const ResourceSetWrite> writes) = 0;
};

CreateMutableResourceSet(layout, initialWrites);
SetGraphicsMutableResourceSet(mutableResourceSet);
SetComputeMutableResourceSet(mutableResourceSet);
```

T1 updates are legal after bind and after command-list submission. Alloy does not guarantee
that an already submitted command list observes old descriptor contents. If an update
overwrites an entry that pending GPU work may read, the application must provide the required
synchronization or avoid reusing that entry until the relevant work retires.

## Update-After-Bind Contract

T1 allows update-after-bind in the narrow descriptor-storage sense: the CPU may rewrite the
descriptor storage owned by a mutable resource set even after that set has been bound or after
command lists using it have been submitted. The contract is entry-based, not set-based. Merely
touching descriptor heap, descriptor table, or argument buffer storage that is referenced by
pending work is not forbidden by Alloy.

There is still no portable automatic snapshot model. DX12 and Metal do not automatically
version descriptor table or argument buffer storage, and Vulkan update-after-bind features
only make descriptor writes legal; they do not make overwriting an entry safe while pending
GPU work may read that entry. If descriptor memory is overwritten while the GPU may still read
the overwritten entry, the application owns the hazard.

A portable automatic versioning system would need copy-on-write resource-set versions:

```text
ResourceSet current version is in flight.
Update allocates or clones a new backend descriptor version.
Future binds use the new version.
Old versions retire by GPU fence.
```

That is useful, but it is not a small T1 requirement. It is deferred. The T1 contract instead
allows user-synchronized updates and leaves the door open to a future automatic versioning design.

## Null Descriptors

T1 does not require null descriptors. Partially populated arrays mean unpopulated entries are
legal as long as the shader does not access them.

Backends may use null descriptors or engine-provided dummy resources for robustness and
debugging, but this is not part of the advertised T1 contract.

## Shader-Facing T1 Model

T1 shaders use declared descriptor arrays, not global descriptor heaps.

Example:

```hlsl
StructuredBuffer<SomeStruct> g_PerObjectAttr[] : register(t0, space4);

SomeStruct LoadAttr(uint bufferIndex, uint objectIndex)
{
    return g_PerObjectAttr[bufferIndex][objectIndex];
}
```

If `bufferIndex` may vary per lane, the shader should use the non-uniform indexing path and
the adapter must advertise the corresponding advanced feature:

```hlsl
uint idx = NonUniformResourceIndex(bufferIndex);
return g_PerObjectAttr[idx][objectIndex];
```

Even if HLSL uses an unbounded array syntax, alloy still imposes a fixed capacity through
the resource layout:

```cpp
shaderResources[slot].kind = ResourceKind::StorageBuffer;
shaderResources[slot].bindingSlot = 0;
shaderResources[slot].bindingSpace = 4;
shaderResources[slot].bindingCount = MaxPerObjectAttrBuffers;
```

The shader-visible register and space remain layout-owned. This distinguishes T1 from T2,
where shaders index `ResourceDescriptorHeap[]` directly.

## Vulkan T1 Mapping

The Vulkan backend should implement T1 first.

Layout creation:

- Continue using `VkDescriptorSetLayout`.
- Create descriptor array bindings with fixed `descriptorCount`.
- Enable partially bound binding flags for T1 array bindings.
- Enable update-after-bind flags for mutable T1 bindings so `vkUpdateDescriptorSets` remains
  legal after bind/submission.
- If the required update-after-bind features are unavailable, do not advertise the relaxed T1
  update contract unless another backend strategy provides equivalent behavior.

Resource set creation:

- Allocate ordinary `VkDescriptorSet` objects from the descriptor pool.
- Apply sparse initial writes.
- Leave unwritten entries unpopulated when the binding is partially bound.
- Keep immutable `IResourceSet` on the dense positional creation path.
- Use `IMutableResourceSet` for sparse writes and later updates.

Updates:

- Translate `layoutSlot + firstArrayElement` to `dstBinding + dstArrayElement`.
- Emit `vkUpdateDescriptorSets`.
- Do not reject updates only because the resource set may be in flight.
- The application owns hazards from rewriting descriptor entries that pending GPU work may
  read.

Shader remapping:

- Keep the current per-layout set/binding remapping strategy.
- Do not enable heap-style `use_heap` remapping for T1.
- Preserve declared array capacities and dynamic indexing semantics.
- Treat non-uniform descriptor indexing as gated by a separate adapter capability.

## DX12 and Metal T1 Mapping

DX12 and Metal should eventually advertise T1 under the same user-synchronized update
contract.

DX12:

- All DX12 resource binding tiers can advertise Alloy T1.
- Mutable resource sets own descriptor table ranges.
- Updates rewrite descriptors in the range.
- Binding still uses descriptor table starts.
- On tier 1 hardware, descriptor table entries that would otherwise be unpopulated are
  initialized or rewritten to null/default descriptors.
- On tier 2 hardware, combined CBV/SRV/UAV heap entries that would otherwise be unpopulated
  are initialized or rewritten to null descriptors.
- On tier 3 hardware, null/unpopulated writes may be a no-op that only drops Alloy resource
  references.
- Updates are allowed after bind/submission.
- The application owns hazards from rewriting descriptor entries that pending GPU work may
  read.

Metal:

- Mutable resource sets own argument table storage.
- Updates rewrite argument table entries.
- Binding writes table addresses into the root argument buffer.
- Updates are allowed after bind/submission.
- The application owns hazards from rewriting argument table entries that pending GPU work may
  read.

This should be mechanical after the Vulkan T1 contract has been validated.

## Task List

1. [x] Replace `supportBindless` with `ResourceBindingModel`.
2. [x] Add a separate non-uniform descriptor indexing capability.
3. [x] Extend resource layout descriptions to mark descriptor-array-capable slots.
4. [x] Add `IMutableResourceSet`, slot-based initial writes, and update writes.
    - [x] Add sibling `IMutableResourceSet` API with slot-based initial writes and update writes.
    - [x] Add separate command-list bind entry points for mutable resource sets.
    - [x] Keep immutable `IResourceSet` on the dense positional creation and bind path.
5. [x] Implement Vulkan T1 layout creation with fixed-size partially bound descriptor arrays.
6. [x] Implement Vulkan mutable resource-set sparse initial writes and update writes.
    - [x] Cache Vulkan `layoutSlot -> descriptor set/binding` lookup in `VulkanResourceLayout`.
7. [ ] Add debug diagnostics for obvious entry rewrite hazards where practical.
8. [x] Add a small shader smoke that dynamically indexes a declared descriptor array.
    - [x] Validate Vulkan T1 with a real renderer path.
9. [ ] After Vulkan is stable, implement DX12 and Metal T1 with the same public contract.
    - [x] Add explicit DX12 and Metal mutable resource-set factory/bind stubs.
    - [x] Implement DX12 mutable resource sets under the same user-synchronized update contract.
    - [ ] Implement Metal mutable resource sets under the same user-synchronized update contract.
    - [ ] Revisit whether DX12 and Metal should advertise `DescriptorHeap` before T2 APIs exist.
10. [x] Draft the T2 descriptor-heap public API direction.
11. [ ] Convert the T2 draft into public headers and backend stubs.

## Open Follow-Ups

- Exact names and placement for `ResourceBindingModel` and the non-uniform indexing bit.
- Whether `layoutSlot` should remain an integer index or become an opaque handle.
- How much debug diagnostics can infer about descriptor entry rewrite hazards.
- Whether dummy descriptors should be installed in debug builds for easier failure diagnosis.
- When to promote the T2 descriptor-heap draft into public API changes.
