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
- Updates are not allowed while the resource set version may be in flight.
- There is no update-after-bind contract.
- There is no global `ResourceDescriptorHeap[]` / `SamplerDescriptorHeap[]` shader ABI.

For Vulkan this maps to Vulkan 1.2 descriptor indexing features such as descriptor arrays,
partially bound bindings, and descriptor set updates. For DX12 and Metal it maps to rewriting
descriptor table / argument table entries under the same lifetime rules.

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
- Vulkan prefers `VK_EXT_descriptor_buffer`.
- Vulkan can fall back to descriptor-indexing-backed heap emulation if needed.
- DX12 maps to native shader-visible descriptor heaps.
- Metal maps to argument buffer table storage.

T2 requires a larger shader remapping design and is intentionally not part of the T1 work.

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

T1 updates are valid only before the resource set is used by the GPU, or after all GPU work
that used the previous contents has retired.

## No Update-After-Bind Contract

Vulkan has update-after-bind features, but they do not provide a portable automatic snapshot
model. DX12 and Metal also do not automatically version descriptor table or argument buffer
storage. If descriptor memory is overwritten while the GPU may still read it, the engine owns
the hazard.

A portable automatic versioning system would need copy-on-write resource-set versions:

```text
ResourceSet current version is in flight.
Update allocates or clones a new backend descriptor version.
Future binds use the new version.
Old versions retire by GPU fence.
```

That is useful, but it is not a small T1 requirement. It is deferred. The T1 contract instead
forbids updating in-flight resource sets, with debug validation where practical.

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
- Use update-after-bind flags only if a later design explicitly adds a portable versioning
  policy. Do not use them as the baseline T1 contract.

Resource set creation:

- Allocate ordinary `VkDescriptorSet` objects from the descriptor pool.
- Apply sparse initial writes.
- Leave unwritten entries unpopulated when the binding is partially bound.
- Keep immutable `IResourceSet` on the dense positional creation path.
- Use `IMutableResourceSet` for sparse writes and later updates.

Updates:

- Translate `layoutSlot + firstArrayElement` to `dstBinding + dstArrayElement`.
- Emit `vkUpdateDescriptorSets`.
- Reject updates to resource sets whose previous contents may still be in flight.

Shader remapping:

- Keep the current per-layout set/binding remapping strategy.
- Do not enable heap-style `use_heap` remapping for T1.
- Preserve declared array capacities and dynamic indexing semantics.
- Treat non-uniform descriptor indexing as gated by a separate adapter capability.

## DX12 and Metal T1 Mapping

DX12 and Metal should eventually advertise T1 under the same restricted lifetime contract.

DX12:

- Mutable resource sets own descriptor table ranges.
- Updates rewrite descriptors in the range.
- Binding still uses descriptor table starts.
- No update while in flight unless a future versioning system exists.

Metal:

- Mutable resource sets own argument table storage.
- Updates rewrite argument table entries.
- Binding writes table addresses into the root argument buffer.
- No update while in flight unless a future versioning system exists.

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
7. [ ] Add debug validation for updates to in-flight mutable resource sets.
8. [ ] Add a small shader smoke that dynamically indexes a declared descriptor array.
    - [ ] Validate Vulkan T1 with a real renderer path.
9. [ ] After Vulkan is stable, implement DX12 and Metal T1 with the same public contract.
    - [x] Add explicit DX12 and Metal mutable resource-set factory/bind stubs.
    - [ ] Implement DX12 mutable resource sets under the same no-update-while-in-flight contract.
    - [ ] Implement Metal mutable resource sets under the same no-update-while-in-flight contract.
    - [ ] Revisit whether DX12 and Metal should advertise `DescriptorHeap` before T2 APIs exist.

## Open Follow-Ups

- Exact names and placement for `ResourceBindingModel` and the non-uniform indexing bit.
- Whether `layoutSlot` should remain an integer index or become an opaque handle.
- How command submission records resource-set usage for in-flight update validation.
- Whether dummy descriptors should be installed in debug builds for easier failure diagnosis.
- When to start T2 descriptor heap design after T1 stabilizes.
