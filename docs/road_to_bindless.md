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
- Vulkan's baseline T2 path maps the heap ABI to descriptor-indexing-backed typed global
  arrays, because Vulkan descriptor layouts remain typed even when descriptor buffers are
  available.
- Vulkan can optionally use `VK_EXT_descriptor_buffer` for descriptor storage and binding, but
  it must still honor `VkDescriptorSetLayout` packing, descriptor sizes, and binding offsets.
- Vulkan should leave room for `VK_EXT_mutable_descriptor_type` as a higher-fidelity resource
  heap path where support is available.
- DX12 maps to native shader-visible descriptor heaps.
- Metal maps to argument buffer table storage.

T2 requires a larger shader remapping design and is intentionally not part of the T1 work.

## T2 Public API Direction

> **Decision (2026-05-30): No hybrid bindful. T2 = global heap + push constants only.**
>
> Earlier drafts of this section described *hybrid bindful*: bindful HLSL slots
> (`register(t0)` etc.) whose descriptors physically live in the global heap, reached via a
> per-range offset UBO (`dxil-spv` `use_heap=true` + `offset_binding`). That is dropped.
>
> Rationale: alloy deliberately exposes **no root-view binding**. Vulkan core has no
> root-descriptor equivalent, and `VK_KHR_push_descriptor` is an NVIDIA-origin extension to
> avoid. alloy's DX12 backend already routes **every** buffer/texture binding through a
> descriptor table; only root *constants* use root-parameter space. So a constant buffer is
> *already* table→descriptor→buffer (one indirection) on both backends — hybrid bindful was
> never saving an indirection. It only added: per-range offset UBOs (sets 2/3), an
> `offset_binding` remapper branch, and a CBV/sampler asymmetry (the `dxil-spv` CBV and
> sampler binding structs have no `offset_binding` field, only SRV/UAV do).
>
> The T2 model is therefore:
> - **Push / root constants** — the ≤128-byte hot path: heap indices, bases, flags, tiny
>   scalars. (`dxil-spv` root-constant → push-constant promotion.)
> - **Global heap** — everything else (SRV, UAV, CBV, sampler), reached by integer index.
>   A T2 "constant buffer" is a heap-resident buffer addressed by a pushed/stored index, not
>   a bound `cbuffer`. SRV/UAV/CBV route to the resource heap (set 0), samplers to the
>   sampler heap (set 1), all with `use_heap=true`.
> - **No offset UBOs, no `offset_binding`, no per-range bindful descriptor state.**
>
> Engine shader authors reach constants via heap indices; push constants carry just the
> indices. This matches the GPU-driven renderer direction (push indices, pull data from
> buffers by index) — see `docs/specs/renderer/resource_model.md`.
>
> Sections below that still describe offset UBOs / hybrid bindful are retained for history
> but superseded by this record.

T2 is not a larger `IMutableResourceSet`. It factors the binding model into independent
objects, each owning one concern:

- **`IResourceLayout`** — schema. Push constants and the convention for accessing the
  global heap. No storage. (No bindful-slot heap offsets — see the decision record above.)
- **`IDescriptorHeap`** — allocator over a backing store (DX12 shader-visible heap, Vk
  descriptor pool or `VkBuffer` via `VK_EXT_descriptor_buffer`). Owns the sub-allocator and
  the global heap binding location. No schema.
- **`IDescriptorRange`** — a sub-allocation in a heap: a base index plus a count. Pure
  bookkeeping; it carries no descriptor storage and no per-set bind state. Its base index is
  what the engine bakes into GPU records / indirect commands.
- **`IPipeline`** — compiled shaders, scoped by a layout.

Compatibility reduces to: layout and heap agree on the heap binding convention; pipelines
compiled against a layout bind against any compatible heap.

The descriptor-storage backing (`VK_EXT_descriptor_buffer` vs descriptor pool) is a
backend-internal choice, not an API change. The same `IDescriptorRange` interface serves
both.

Higher-level engine systems (material tables, scene records) store **32-bit descriptor
indices** relative to the currently bound resource or sampler heap. CPU-side wrappers may
carry type information for validation, but GPU records store plain `uint32_t`.

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

struct DescriptorHeapDescription {
    uint32_t resourceCapacity;   // 0 to disable resource heap on this object
    uint32_t samplerCapacity;    // 0 to disable sampler heap on this object
};

// Heap is allocator + storage. Owns sub-allocation policy and the global
// heap binding location. Knows nothing about schema.
class IDescriptorHeap : public common::RefCntBase {
public:
    virtual const DescriptorHeapDescription& GetDesc() const = 0;

    // Allocate a contiguous range scoped by a layout. The range knows its
    // heap base; the layout supplies the relative offsets and per-set
    // bind-state shape.
    virtual common::sp<IDescriptorRange> AllocateRange(
        const common::sp<IResourceLayout>& layout) = 0;

    // Free-form bindless allocation: hand back a heap index range that the
    // engine writes into GPU records. Not scoped by a layout.
    virtual ResourceDescriptorIndex AllocateResourceIndices(
        ResourceDescriptorType type,
        uint32_t count,
        uint32_t alignment = 1) = 0;
    virtual SamplerDescriptorIndex AllocateSamplerIndices(
        uint32_t count,
        uint32_t alignment = 1) = 0;
    virtual void FreeResourceIndices(ResourceDescriptorIndex first, uint32_t count) = 0;
    virtual void FreeSamplerIndices(SamplerDescriptorIndex first, uint32_t count) = 0;

    // Direct heap writes for engine-issued indices.
    virtual void Write(std::span<const ResourceDescriptorWrite> writes) = 0;
    virtual void Write(std::span<const SamplerDescriptorWrite> writes) = 0;
    virtual void Clear(std::span<const ResourceDescriptorClear> clears) = 0;
    virtual void Clear(std::span<const SamplerDescriptorClear> clears) = 0;

    virtual void* GetNativeHandle() const { return nullptr; }
};

// Range is a layout-scoped sub-allocation: a base index + count. Pure
// bookkeeping — no descriptor storage, no per-set bind state, no offset UBO.
// Writes go to the owning heap by absolute index (base + local).
class IDescriptorRange : public common::RefCntBase {
public:
    virtual const IResourceLayout& GetLayout() const = 0;
    virtual IDescriptorHeap& GetHeap() const = 0;

    // Heap base index of this range. The engine bakes this into GPU records /
    // indirect commands; the shader reaches resources via ResourceDescriptorHeap[base + i].
    virtual uint32_t GetHeapBaseIndex() const = 0;

    virtual void* GetNativeHandle() const { return nullptr; }
};
```

`ResourceDescriptorType` is deliberately separate from `IBindableResource::ResourceKind`.
T0/T1 resource layouts can infer read/write interpretation from the layout slot, but T2
free-form heap writes do not have a layout slot. The write must say whether a storage
buffer or texture is read-only or read-write so backends can create the correct native
descriptor. Samplers use a separate heap family and therefore do not need a selectable
descriptor type.

Heap creation, range allocation, and binding are factory and encoder-level operations:

```cpp
ResourceFactory::CreateDescriptorHeap(heapDesc);

IRenderCommandEncoder::SetDescriptorHeap(const common::sp<IDescriptorHeap>& heap);
IComputeCommandEncoder::SetDescriptorHeap(const common::sp<IDescriptorHeap>& heap);

ICommandList::BeginRenderPass(action, passResourceUsage);
ICommandList::BeginComputePass(passResourceUsage);

IRenderCommandEncoder::SetGraphicsDescriptorRange(
    uint32_t layoutSetIndex, const common::sp<IDescriptorRange>& range);
```

A single heap object owns both resource and sampler families when the backend supports it
(DX12 binds them as a pair anyway; Vulkan can group them into one heap object that owns
both backing sets). Switching heaps mid-pass is legal but treated as a hard barrier — all
prior heap-relative indices are invalidated for subsequent work.

Pass resource usage is deliberately separate from heap binding. `SetDescriptorHeap(s)`
answers "which descriptor tables are visible"; `PassResourceUsage` answers "which resources
may be touched during this pass, and with which stages/access." Metal uses this declaration
to call `useResource(s)` for argument-buffer residency at pass begin. DX12 and Vulkan can use
the same declaration later for transition and UAV/storage barriers; the T2 heap bind itself
does not imply residency or memory ordering.

### T2 Pipeline Layout And Push Constants

T2 does not remove the top-level resource layout. The layout owns the schema: the DX12 root
signature, the Vulkan pipeline layout, and Metal's first-level argument buffer layout. A
T2 layout has exactly two kinds of binding:

1. **Push / root constants** — declared in `IResourceLayout::Description::pushConstants`.
   The ≤128-byte hot path. Carries heap indices, base offsets, flags, tiny scalars.
2. **Global heap access** — the resource heap (set 0) and sampler heap (set 1). Not
   declared as individual shader-resource slots; enabled by a descriptor-heap usage flag on
   the layout. Every SRV/UAV/CBV/sampler the shader reads via `ResourceDescriptorHeap[]` /
   `SamplerDescriptorHeap[]` resolves through these.

There are **no bindful descriptor-table slots** in a T2 layout, and therefore no per-slot
heap-offset assignment and no offset UBO. (See the decision record at the top of this
section.)

Shader-facing HLSL is heap-native, with constants reached by index:

```hlsl
// FrameConstants live in a heap-resident buffer; its index is pushed.
struct PushConstants { uint frameConstantsIndex; uint instanceBufferIndex; };
[[vk::push_constant]] PushConstants pc;

ConstantBuffer<FrameConstants> frame = ResourceDescriptorHeap[pc.frameConstantsIndex];

StructuredBuffer<InstanceData> instances =
    ResourceDescriptorHeap[pc.instanceBufferIndex];

uint textureIndex = NonUniformResourceIndex(material.baseColorTexture);
Texture2D<float4> baseColor = ResourceDescriptorHeap[textureIndex];

SamplerState samplerState =
    SamplerDescriptorHeap[material.baseColorSampler];
```

A T2 "constant buffer" is thus a heap-resident buffer addressed by a pushed (or
scene-record-stored) index, not a bound `cbuffer`. On both backends a heap CBV is
table/heap → descriptor → buffer — the same single indirection a bound `cbuffer` would
cost, since alloy exposes no root-view binding. Nothing is lost by routing it through the
heap.

`NonUniformResourceIndex` remains gated by the separate non-uniform resource indexing
capability. A T2 backend may support heap indexing without promising cheap or legal per-lane
divergent indices.

### `dxil-spv` Remapper For T2

All resource-class remap callbacks route to the global heap; the `(set, binding)` written in
the callback *is* the heap's SPIR-V location (there is no converter-wide "heap set" option):

- **SRV / UAV / CBV** → `set = 0` (resource heap), `binding = 0`, `bindless.use_heap = true`.
  For CBV, set `vulkan.uniform_binding` (not the push-constant union member) unless the
  register matches a declared root constant, in which case promote to push constant.
- **Sampler** → `set = 1` (sampler heap), `binding = 0`, `bindless.use_heap = true`.
- **Root constants** → push constants (existing promotion path).

`offset_binding` is left zeroed on every path. `descriptor_type` still selects SSBO vs
texel vs identity for untyped buffers as today.

### 1+1 Heap Binding Discipline

Alloy enforces the DX12 hardware constraint as the portable contract: at most **one
resource heap and one sampler heap** are bound to an encoder at any time. Switching heaps
mid-pass is legal but treated as a hard reset — all prior heap-relative indices in flight
on that encoder are invalidated for subsequent draws. This matches:

- DX12: `SetDescriptorHeaps` is a hardware barrier, only 1+1 shader-visible heaps allowed.
- Vulkan: a single global heap descriptor set is bound; switching means rebinding.
- Metal: one resource argument table + one sampler argument table per encoder.

Engines that want multiple heaps (persistent + transient + per-domain) bind one at a time,
in heap-grouped passes. The expected GPU-driven pattern is:

```text
for each heap-grouped batch:
  cmd.SetDescriptorHeap(batch.heap);
  cmd.SetPipeline(batch.pipeline);
  cmd.SetPushConstants(batch.indices);         // heap base indices, flags
  cmd.ExecuteIndirect(batch.indirectArgs);     // single command, many objects
```

The number of heap binds per frame is in the tens, not the thousands. Per-object descriptor
indices live in indirect command buffers, not in command stream state.

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
management, or bindless material policy. Pass-level `PassResourceUsage` is the Alloy hook for
declaring those resources when a backend needs the information, but descriptor heap writes
and descriptor heap binds remain storage/binding operations only.

### T2 Backend Mapping

DX12:

- `IDescriptorHeap` owns the shader-visible `CBV_SRV_UAV` heap and (optionally) the sampler
  heap. The sub-allocator is a range allocator over heap indices.
- The DX12 root signature is layout-owned and minimal: the two directly-indexed heap flags
  (`CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED`, `SAMPLER_HEAP_DIRECTLY_INDEXED`) plus root
  constants. No descriptor-table root parameters — there are no bindful tables in a T2
  layout. Pipelines created against the same layout share the rootSig.
- `IDescriptorRange` is a base-index + count carved out of the heap. The engine bakes the
  base into GPU records / indirect data; the shader reads `ResourceDescriptorHeap[base + i]`.
- `ResourceDescriptorHeap[k]` reads whatever is at heap index `k`, whether `k` came from a
  range allocation or from engine-issued free-form index allocation.
- `SetDescriptorHeap` maps to `ID3D12GraphicsCommandList::SetDescriptorHeaps`. The 1+1
  hardware constraint is the source of the portable contract.
- Writes create CBV/SRV/UAV or sampler descriptors at the allocated CPU heap indices via
  `CreateConstantBufferView`/etc.

Vulkan:

- The resource heap is one `VK_DESCRIPTOR_TYPE_MUTABLE_EXT` descriptor set at set 0,
  binding 0; the sampler heap is one `SAMPLER` set at set 1, binding 0. Both are
  variable-count, partially-bound, update-after-bind, sized at allocation to the heap
  capacity. The DSLs are device-universal singletons (variable-count, sized to device
  maxima); heaps and layouts reference them. A single mutable set holds UBO/SSBO/sampled/
  storage-image descriptors, so SRV/UAV/CBV all land in set 0.
- All `ResourceDescriptorHeap[]` access remaps to set 0 with `use_heap=true`;
  `SamplerDescriptorHeap[]` to set 1. No `offset_binding`, no per-range offset UBO — the
  base index is carried in push constants or scene records (see decision record).
- `IDescriptorHeap` is the sub-allocator. For T2 with descriptor pools, this is bookkeeping
  over the mutable set's capacity. For descriptor buffer, it is a real allocator over a
  `VkBuffer`.
- `VK_EXT_descriptor_buffer` is an optional implementation strategy. The heap-as-allocator
  factoring makes the swap an internal backend choice with no API impact: descriptor bytes
  are still laid out according to `VkDescriptorSetLayout`;
  `vkGetDescriptorSetLayoutSizeEXT` and `vkGetDescriptorSetLayoutBindingOffsetEXT` apply
  when descriptor buffer is selected.
- `VK_EXT_mutable_descriptor_type` is the **baseline** T2 backing (it is what collapses
  SRV/UAV/CBV into one heap set, mirroring DX12 heap semantics). `VK_EXT_descriptor_buffer`
  is the optional faster storage strategy layered behind the same heap object.
- Command encoding binds the heap's backing descriptor set (or descriptor-buffer offsets)
  once per pass. There is no per-range bind state — range base indices are supplied to the
  shader via push constants or scene records.

Metal:

- Resource and sampler heaps map to argument buffer table storage.
- Writes update argument table entries and retain referenced Alloy resources.
- Binding installs only the resource and sampler argument tables for the active encoder.
  It must not walk the whole heap and snapshot every referenced resource.
- Resources reachable through bindless argument tables are made resident through
  `PassResourceUsage` at `BeginRenderPass` / `BeginComputePass`, which maps to Metal
  `useResource(s)` for the remaining duration of the pass.
- Shader conversion maps heap-style access to the corresponding argument-buffer arrays.

### T2 Capability And Limits

`ResourceBindingModel::DescriptorHeap` should be advertised only when the backend implements
the complete T2 contract: heap allocation (both layout-scoped ranges and free-form
indices), heap writes, command binding under 1+1 discipline, push-constant support, and
shader remapping from `ResourceDescriptorHeap[]` / `SamplerDescriptorHeap[]` to the global
heap sets.

For Vulkan, descriptor indexing alone is not enough unless the backend also implements the
Alloy heap ABI: heap object allocation, the mutable-type resource heap + sampler heap,
`dxil-spirv` `use_heap` remapping, and the 1+1 binding contract. `VK_EXT_descriptor_buffer`
is an optional faster-storage path, not a standalone capability signal.

Adapter limits should grow explicit heap limits before the API lands:

```cpp
struct DescriptorHeapLimits {
    uint32_t maxResourceDescriptors;
    uint32_t maxSamplerDescriptors;
    // The portable bind count is always 1 resource + 1 sampler heap per encoder.
    // Backend-specific higher limits are not exposed.
};
```

### T2 Open Questions

- Whether `IDescriptorHeap` should own both resource and sampler families in one object
  (DX12-natural) or expose them as separate objects (Vulkan-natural). Current direction:
  unified object with optional sampler family.
- Exact placement of the descriptor-heap usage flag in the pipeline / layout descriptions.
- Whether a future `IPipelineLayout` should replace the current use of `IResourceLayout`
  for push constants and binding model selection.
- Whether debug builds should install dummy descriptors on `Clear`, and which dummy
  resources are required per descriptor type.
- How much validation can catch descriptor index reuse while GPU records are still in
  flight.
- Whether `IDescriptorRange` and the T1 `IMutableResourceSet` should be unified under one
  interface (range = degenerate single-range heap), or kept separate for API clarity.

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
9. [x] After Vulkan is stable, implement DX12 and Metal T1 with the same public contract.
    - [x] Add explicit DX12 and Metal mutable resource-set factory/bind stubs.
    - [x] Implement DX12 mutable resource sets under the same user-synchronized update contract.
    - [x] Implement Metal mutable resource sets under the same user-synchronized update contract.
10. [x] Draft the T2 descriptor-heap public API direction.
11. [x] T2 public headers: `IResourceDescriptorHeap`/`ISamplerDescriptorHeap` indexed
       writes.
12. [ ] T2 backend implementation. **no-hybrid-bindful decision**: per-slot relative-offset assignment are being removed.
    - [x] Vulkan T2 backend: `VulkanResourceDescriptorHeap` / `VulkanSamplerDescriptorHeap`
       (mutable-type, pool-allocated, indexed Write/Clear), free-form bindless writes via heap-direct index allocation.
    - [x] DX12 T2 backend: maps naturally to DX12 descriptor APIs.
    - [x] Metal T2 backend: argument-table heap.
    - [x] Pass-level resource usage declaration for Metal argument-buffer residency.
    - [ ] Revisit whether Metal should always advertise T2 APIs.
17. [ ] T2 functional smoke: heap-indexed texture + heap-indexed material constant buffer
       drawn from one heap with push-constant indices
    - [ ] Vulkan
    - [ ] DX12
    - [ ] Metal

## Open Follow-Ups

- Exact names and placement for `ResourceBindingModel` and the non-uniform indexing bit.
- Whether `layoutSlot` should remain an integer index or become an opaque handle.
- How much debug diagnostics can infer about descriptor entry rewrite hazards.
- Whether dummy descriptors should be installed in debug builds for easier failure diagnosis.
- Whether to unify `IDescriptorRange` and `IMutableResourceSet` once T2 backends ship.
- Backing-store policy: descriptor pool vs `VK_EXT_descriptor_buffer` selection criteria
  (drives sub-allocator implementation choice).
