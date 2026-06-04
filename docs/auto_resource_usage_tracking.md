# Alloy Barrier & Sync Model

### Split Model

- ResourceUse: what a pass/work item promises it will do.
- ResourceState: portable stage/access/layout tuple.
- ResourceBarrier: explicit transition from one state to another.
- Backend mapping: sync2/enhanced barrier first; DX12 legacy state as lossy fallback.

```c++
// Alloy sync model abstraction:

enum class ResourceAccess {
    None,

    IndirectArgumentRead,
    VertexBufferRead,
    IndexBufferRead,
    ConstantBufferRead,

    ShaderResourceRead,   // SRV / sampled texture / readonly structured buffer
    UnorderedAccess,      // UAV / storage image-buffer access, conservatively read-write

    RenderTarget,
    DepthStencilRead,
    DepthStencilWrite,

    CopySource,
    CopyDest,

    AccelerationStructureRead,
    AccelerationStructureWrite,

    Present,
};

enum class PipelineStage {
    None,

    // Umbrella scopes / coarse aliases.
    AllCommands,
    AllGraphics,
    AllShaders,

    // Common explicit stages.
    DrawIndirect,
    VertexInput,  // vertex + index fetch
    VertexShader,
    MeshShader,   // task/amplification + mesh
    FragmentShader,
    DepthStencil,
    ColorOutput,
    ComputeShader,
    RayTracing,
    Copy,
    BuildAS,
};

enum class TextureLayout {
    Undefined,          // valid only as old layout / discard
    General,
    ShaderReadOnly,
    Storage,
    ColorAttachment,
    DepthStencilReadOnly,
    DepthStencilWrite,
    CopySource,
    CopyDest,
    ResolveSource,
    ResolveDest,
    Present,
};
```
### Usage Declarations

The user uses following interface on `ICommandList` to declare resource dependencies via barriers

We're limiting the barrier usages to outside of render/compute/transfer passes. If a use case for intra-pass dependencies arises, we can add support for that via:
- **render pass**: attachment dependencies only, no layout transition
- **compute pass**: full barrier-like dependency, allow layout transition
- **transfer pass**: full barrier-like dependency, allow layout transition

```c++
// State description
// Buffer
struct ResourceState {
    PipelineStageMask stages;
    ResourceAccessMask access;
};
// Texture
struct TextureState {
    PipelineStageMask stages;
    ResourceAccessMask access;
    TextureLayout layout;
};

// This keeps call sites clean:
//
// TextureState{
//     .stages = PipelineStage::FragmentShader,
//     .access = ResourceAccess::ShaderResourceRead,
//     .layout = TextureLayout::ShaderReadOnly,
// };

// The barrier interface on command list looks like:

struct BufferBarrierOp {
    common::sp<IBuffer> buffer;
    ResourceState from;
    ResourceState to;
};

struct TextureBarrierOp {
    common::sp<ITextureView> texture;
    TextureState from;
    TextureState to;
};

using BarrierOp = std::variant<
    BufferBarrierOp,
    TextureBarrierOp>;

void Barrier(std::span<const BarrierOp> barrierOps);
```

### Backend Mapping

Vulkan sync2 maps directly:

- PipelineStage -> VkPipelineStageFlags2
- ResourceAccess -> VkAccessFlags2
- TextureLayout -> VkImageLayout
- buffer use -> VkBufferMemoryBarrier2
- texture use -> VkImageMemoryBarrier2
- memory-only dependency -> VkMemoryBarrier2

DX12 enhanced barrier also maps directly:

- PipelineStage -> D3D12_BARRIER_SYNC
- ResourceAccess -> D3D12_BARRIER_ACCESS
- TextureLayout -> D3D12_BARRIER_LAYOUT
- buffers -> D3D12_BUFFER_BARRIER
- textures -> D3D12_TEXTURE_BARRIER
- global memory dependency -> D3D12_GLOBAL_BARRIER

DX12 legacy fallback is lossy:

- stage masks mostly collapse.
- texture layout/access collapses into D3D12_RESOURCE_STATES.
- read-to-read barriers are dropped.

That fallback is fine for compatibility, but not the semantic baseline. The semantic baseline should be sync2/enhanced barrier.

Metal doesn't have inter-pass barriers, and only has a limited selection of pipeline stages and no layout/access flags.

Important rules to bake into the model:

- Present is valid only for swapchain textures.
- ColorAttachment* requires ColorOutput.
- Shader accesses require at least one shader stage.
- Descriptor heap binding does not retain resources or imply barriers; ResourceUse/DescriptorRangeUse does.

# Auto resource usage tracking 

Vulkan and DX12 require explicit resource barriers and layout transitions. Core Alloy exposes explicit barriers as the native, predictable model. Automatic resource usage tracking is an optional convenience layer on top of Alloy:

```c++
auto trackedDevice = alloy_sp(new ResourceTrackingDevice(graphicsDevice));
```

The tracking layer wraps `IGraphicsDevice`, `ICommandQueue`, `ICommandList`, pass encoders, resource factory outputs, and companion objects. It records usage declarations and emits explicit Alloy barriers for the native objects it wraps.

Tracked and untracked objects should not be mixed implicitly. A tracked queue/list must either receive resources created/imported through the tracking device, or the user must explicitly import/assume an initial state for external resources.

Within a command list, resource usages are registered per-pass. `ICommandList::End()` semantically seals the command list and validates that no more commands or usage declarations can be added.

There are two materialization paths:

- If all usage declarations are concrete `UseResource` declarations, the tracking layer may record/materialize the native command list at `End()`. Queue-entry transitions are still emitted at `Submit`, because they depend on the destination queue timeline state.
- If the command list contains `UseDescriptorRange` declarations, there is no record-time descriptor snapshot. `End()` only seals the symbolic command list. Actual native command recording/replay that depends on the resolved resource footprint happens at `Submit`, after descriptor ranges are expanded.

Each command list has an "expected resource states" list. During `ICommandQueue::Submit`, the tracking queue resolves pending usage declarations, compares them with the current per-queue resource status, and inserts transition helper command lists around the submitted command lists where necessary.

Alloy uses a "timeline" concept to track resource states. Each command queue is a timeline and have its own resource state list. State list within each timeline is standalone and will only sync between "sync points":
- A queue-owned `AutoEvent` signal registers a **send point**. Other timelines can fetch the resource state snapshot at this point.
- Waiting on an `AutoEvent` registers a **fetch point**. The issuing timeline can read the snapshot when it needs the relevant resources.
- A submission will alter the resource state within the timeline and create a **write point**. *If the resource doesn't exist in current timeline, this will behave like a implicit fetch point to fetch from the CPU timeline - more on this later*

Regular `IEvent` objects remain execution sync primitives only. Signaling or waiting on a regular `IEvent` does not transfer resource-state knowledge.

## Timeline Model

Each timeline owns a resource [state list](#state-list) keyed by `{ SyncPoint inst, SyncPoint value }` pair. This can be implemented as 2 64bit values such as `{ void* inst, uint64_t value }`. A submission can create a key looks like `{ nullptr, submissionIdx }`. All states are updated at *API invoke time* instead of actual GPU time as a low-cost resonably accurate extrapolate.

Submission is a very unique sync point creator:
1. Before any command lists actually submitted to GPU, a special resource transition command list *may* be inserted.
2. Backends *passively* track a "last completed submission index" to know the execution state of transition command lists and recycle completed ones. This happens during the `Submit` interface call, and is the first step.

This works to our advantage - we'll know the latest completed resource states up to the retired queue generation, plus an API-extrapolated tail for queued work. In a real application there can be numerous event waits and submissions. If we track every state from app start to finish, the state list will be unnecessarily bloated. Instead, we can only keep a "window" of states between selected sync point: from completed ground truth to latest API extrapolation. Anything older than the ground truth will be discarded. This also behaves correctly in terms of command execution: syncing to old event values should be treated **carefully**: you must carefully examine and make sure later mutators won't break your assumptions of resource states. A carefully designed API calling sequence that works on bare DX12/Vulkan will also work here: the worst case is syncing to a discarded sync point, and we provide the ground truth as a replacement. see [status list trimming](#state-trimming)

## CPU Timeline

CPU timeline is a special timeline: resources create and destroy on this timeline, and it is always in sync with API extrapolation. It is also where API calls are made. To maintain semantic correctness, CPU timeline won't track stale status like other timelines. Resource states will be transferred to relative timelines at `Submit`, and got back by waiting on the queue-owned `AutoEvent`. During this period the resource state is unknown to CPU timeline.

Unknown tracker state is not the same as `TextureLayout::Undefined`. `Undefined` is a real discard transition for texture contents. Unknown tracker state means the tracking layer cannot prove the current state. Fetching an unknown state should be a validation error unless the user explicitly imports, assumes, or discards the state.

Resource destruction on CPU timeline will also broadcast to other timelines and remove resource state history from them. This is **NOT** thread safe, and requires resource state known to CPU timeline, othewise the resource is effectively "in-use" by GPU, destroying such resources is a API violation on **ALL BACKENDS**.

## Event and Synchronization

The tracking layer uses specialized `AutoEvent`s for resource-state synchronization. Each tracked command queue owns one `AutoEvent`. Users can get this event and wait/query it, but cannot create arbitrary `AutoEvent`s.

`AutoEvent` rules:

- An `AutoEvent` is bound to one owner queue/timeline.
- Only the owner queue can signal its `AutoEvent`.
- Event values are monotonically increasing within that `AutoEvent`; they do not need to be globally atomic across all events.
- A signaled `(AutoEvent*, value)` identifies a resource-state snapshot for the owner queue at that generation.
- Waiting on an `AutoEvent` orders GPU execution and transfers resource-state knowledge from the signaled snapshot to the waiting timeline.
- Waiting on an `AutoEvent` from the CPU transfers state knowledge back to the CPU timeline before the wait returns.

Regular `IEvent` remains supported as execution-only synchronization. A tracked queue may signal or wait on a regular `IEvent`, but that wait does not fetch resource states and that signal does not publish a state snapshot. If resources cross queues through regular `IEvent`s, the user must use explicit barriers, explicit state requests, or an import/assume-state operation to keep tracking coherent.

## Thread Safety

CPU timeline thread safety:

- On **Vulkan backend** we use VMA. VMA interfaces that interacts with `allocator` is thread safe, which means resource creation/destruction is thread safe. VMA interfaces that interacts with `allocation` is not thread safe. *Note: the Vulkan interface itself is NOT thread safe. VMA uses locks to synchronize internally*

- On **DX12 backend**, the native DX12 resource creation interfaces from `ID3D12Device` are thread safe, individual allocations is not thread safe.

- On **Metal backend**, similar to DX12, the native metal APIs on `MTLDevice` is thread safe, anything below it is not.

For alloy implementation, we not only need the resource creation/destruction, but also update their timeline status using sync points. This will all mutate the CPU state list. We decide to make alloy resource interfaces **NOT** thread safe because if you make creation/destruction of them thread safe, you will also pay the price in `Signal`, `Wait` and `Submit` which are non-threadsafe interfaces.

Resource destruction implications: 
- Alloy uses reference counted object lifetime management scheme. Resources aren't being destroyed untll all its strong refs are dropped, so it is hard to predict where will the resource be destroyed.
- Unlike creation, which only mutates the CPU timeline's state table, destruction mutates all timelines' state tables. We can't bind the destruction to "one specific thread"

We will use a weak ref scheme here: the trackable resources will itself create a weak ref status block (which is also ref counted) and holds a ref to it. the status list will hold ref to that status block, not the resource itself. On resource destruction, it will mark the status block invalid and release its ref. Based on observation:

> a resource will only enter other timelines via sync points. The first entry point is always a `Submit`. 

We can have following strategy: during `Submit` where we shrink the status list, we also clear all invalid weak refs. During `Wait` where we pull in resource states, we skip all invalid refs. During `Signal` this will always be a no-op. See [state list](#state-list)

Note: 
1. `AutoEvent`, although not a trackable resource, is referenced by multiple timelines and should use a similar weak-status scheme to handle destruction.
2. Queue operations are not thread safe, meaning either all queues should be operated on single thread, or add external synchronization on all queue operation points. However event readback is thread safe and atomic, so it's safe to use in polling style.

## State List

### State Tracking

- **Write point** will update current status
- **Send point** will "snapshot" current status
- **Fetch point** will get "snapshot'-ed status from corresponding **send point**

Not all resources' state will be changed per send point, so to save both storage space and update overhead, we use a generational tracking method:

- Each `Submit` is a new generation. Generation == submission fence.
- Each resource maintain a queue of `{ generation, status }` pairs. If it's state isn't changed for a generation, the simply skip adding the pair.
- Send points are maintained as a queue of `{ key, generation }` pairs. Normally naturally ordered by generation due to the linearity of timelines. Due to the trimming this queue isn't expected to be long, so visiting won't take much time.

Fetching is simple: get generation from key, walk target resource's queue new to old, pick first state where its generation <= key generation. Fetcher will only fetch resources that it needs. Won't fetch the whole resource status list.

### State Trimming

It's both unnecessary and wasteful to keep a resource's state tracked across it's full lifecycle. Any states that is older than current completed submission index's can be dropped. We do that inside `Submit` where we read back the complete fence and recycle the transition command buffers. We do following 3 things:

1. Drop all invalid resources from queue
2. Walk resource state queues old to new, pop entry where a) queue have >1 entries and b) state generation older than current completed generation.
3. Walk send point queue old to new, pop entry where a) key is invalid (`AutoEvent` destroyed) or b) send point generation older than current completed generation.

## Example Sequence

```
Timelines

POI idx          #1 #2 #3  #4   #5  #6      #7      #8
GfxQ     |--------------+-<<1>>/////+{====}-[*]----<2>-------
                        |           |               |
XferQ    |----------[-]{=====}-[/]-<1>-----------------------
                     |  |                           |
CPU      |-------[-]-+--+-<<2>>/////////////////////+-[*]----
 1. Res#1 created ^  ^  ^ 3. Submit rndr cmd        ^
                     |                              |
           2. Submit xfer cmd           4. CPU wait satisfied

Resource states:
  [-]: Generic/Undefined state
  [/]: Transfer dst
  [*]: Shader resource

Legends:
  <NUM>: AutoEvent signal. NUM is event value
  <<NUM>>/// : Wait for AutoEvent. NUM is wait value. 
               Before satisfied the timeline becomes "///"
               to indicate blockage
  {===}: Resource transition command list

```

- **POI #1**: Resource 1 created. State logged to CPU timeline
- **POI #2**: `Submit` to transfer queue. 

    Actual API calling sequence: `Submit`. The transfer queue advances/signals its `AutoEvent` to value 1 for this submission.

    Resource 1's state is transferred to transfer queue timeline and purged from CPU timeline. API extrapolation: although the transfer queue hasn't finished its command yet, transfer queue's status list still records resource 1's state being "transfer destination". This happens at `Submit` API calling time and is ahead of ground truth GPU status.

    The `AutoEvent` signal creates a send point for the transfer queue timeline.

- **POI #3**: `Submit` to graphics queue.

    Actual API calling sequence: 

    1. Encode wait on transfer queue `AutoEvent` value 1.
    2. `Submit`.
    3. The graphics queue advances/signals its `AutoEvent` to value 2 for this submission.


    Waiting on the transfer queue `AutoEvent` creates a fetch point on graphics queue. No resource state fetching has happened yet.

    Resource 1 usage is declared in the submission. graphics queue perform status fetch on current timeline -> encounters the fetch point -> fetch resource state from peer send point. Resource 1's state now enters graphics queue timeline.

    The graphics queue `AutoEvent` signal creates a send point for the graphics queue timeline.

    API extrapolation: currently both the wait and the command list haven't been executed on GPU (because transfer queue hasn't finished yet), graphics queue's status list still records resource 1's state being "shader resource" and 2 steps ahead of ground truth GPU status (which is still generic/undefined).

    Status list isolation: CPU timeline: no resource 1; Transfer queue timeline: resource 1 in "transfer destination" status; Graphics queue timeline: resource 1 in "shader resource" status.

- **POI #4**: Graphics queue begins to wait for the transfer queue `AutoEvent`; Transfer queue completed the transition command, now resource state ground truth is "transfer destination"; CPU thread waits on graphics queue `AutoEvent` value 2.

- **POI #5**: Graphics queue blocked on event wait; CPU thread blocked on event wait

- **POI #6**: Transfer queue completed and signaled its `AutoEvent` with value 1. Graphics queue unblocked and starts executing transition command buffer; CPU thread still blocked on event wait

- **POI #7**: Graphics queue completed transition command. now resource state ground truth is "shader resource";

- **POI #8**: Graphics queue completed and signaled its `AutoEvent` with value 2; CPU thread unblocks. Before the wait call exits, CPU thread executes fetch point and fetch from corresponding send point. Now resource 1's state is returned to CPU timeline as "shader resource"

## Bindless

Alloy bindless T1/T2 allows mutable resources and large descriptor heaps. Descriptor heap binding by itself does not tell the tracking layer which resources the shader will touch. Applications must provide usage declarations for bindless work.

Automatic tracking is intentionally a lightweight convenience utility. It is appropriate for simple tools, upload paths, debug UI, small examples, and non-performance-critical workloads. Large bindless heaps and complex rendering pipelines should use the explicit barrier model.

Tracked command lists may record two kinds of usage declarations:

```c++
void UseResource(common::sp<IBuffer> buffer, ResourceState state);
void UseResource(common::sp<ITextureView> texture, TextureState state);

void UseDescriptorRange(
    common::sp<IDescriptorHeap> heap,
    DescriptorRange range,
    PipelineStageMask stages,
    ResourceAccessMask access);
```

All usage declarations are resolved at `Submit`.

- `UseResource` is already concrete; submit-time resolution just retains and inserts the concrete resource state request.
- `UseDescriptorRange` is expanded at submit by reading descriptor metadata from the referenced heap range and producing concrete resource state requests.
- The submitted tracking record retains the resolved resources/views/ranges until the owning queue's `AutoEvent` reaches the submission value.

There is no record-time descriptor snapshot mode. Submit-time resolution (`AtSubmit`) is the only descriptor resolution mode. This keeps pre-recorded command lists reusable while making the cost visible at submission time, where the transition helper command lists are already recorded.

For command lists containing `UseDescriptorRange`, `End()` only semantically seals the recording. Submit-time descriptor expansion produces the concrete resource footprint, freezes the referenced descriptor entries, computes the required barriers, and then materializes the native command recording for that submission.

### Descriptor Freezing

Mutable descriptor entries can only be updated while they are not in flight. A descriptor heap or mutable resource-set entry becomes frozen when `Submit` resolves a usage declaration that references it. It remains frozen until the owner queue's `AutoEvent` reaches the submission value.

Updating a frozen descriptor entry is a validation error. The tracking layer should not silently block on CPU, because that can introduce surprising stalls.

This rule applies whether the command list is freshly recorded or long-standing and pre-recorded:

- A pre-recorded command list may declare that it uses heap `H` range `[N, M)`.
- Each submit resolves the current contents of that range.
- Those entries are frozen for that submitted work.
- If the application wants to submit the same command list with different resources in the same slots before the previous submission retires, it must use different descriptor slots, a different heap/page, or explicit barriers.

### Precision and Cost

Descriptor range declarations are convenient but can over-barrier. If the shader dynamically indexes a huge range, the application must either:

- declare the exact range it can touch,
- conservatively declare a larger range and accept the cost, or
- use the explicit barrier model.

Concrete `UseResource` declarations are the precise fallback. They work regardless of which heap slot or resource set refers to the resource, and are the preferred form for long-lived command lists whose resource footprint is fixed.

Intra-pass barrier support:

- **Vulkan**: Only have render pass. Barriers within pass is limited only to attachments.
- **Metal**: Have render/compute/transfer passes. Barriers within pass can sync between any stages.
- **DX12**: No passes, immediate mode.

To abstract over all those backends, Alloy uses the following model: for render passes, only insert barriers for attachment dependencies inside the pass. Other `UseResource` / `UseDescriptorRange` declarations are pushed to before the beginning of the pass. For compute and transfer passes, all usage declarations can insert corresponding barriers.


## Dedicated Transfer Queue

Dedicated transfer queues are normally DMA backed queues that can only do transfer commands. They have limited support for resource states (normally only "generic", "transfer source" and "transfer destination"). To be able to utilise such queue, alloy need explicit resource state transition interface on command list.

Alloy provides following interface for command list/queue:

```c++
void RequestResourceState(Resource, State);
```
