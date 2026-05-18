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

## Open Follow-Ups

- Exact names and placement for `ResourceBindingModel` and the non-uniform indexing bit.
- Whether `layoutSlot` should remain an integer index or become an opaque handle.
- How much debug diagnostics can infer about descriptor entry rewrite hazards.
- Whether dummy descriptors should be installed in debug builds for easier failure diagnosis.
- When to start T2 descriptor heap design after T1 stabilizes.
