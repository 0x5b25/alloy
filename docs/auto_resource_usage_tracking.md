# Auto resource usage tracking 

Vulkan and DX12 requires explicit resource barriers and layout transitions. Alloy has a built-in automatic resource usage tracking utility and will auto insert barriers.

Within a command list, resource usages are registered per-pass. Alloy uses a lazy-init pattern at `ICommandList::End()` to replay the command recording and insert barriers where necessary.

Each command list have a "expected resource states" list, during `ICommandQueue::Submit`, alloy will compare the current per-queue resource status with the given expected states and insert a transition helper command list before the submitted one, to transition resources to expected states.

Alloy uses a "timeline" concept to track resource states. Each command queue is a timeline and have its own resource state list. State list within each timeline is standalone and will only sync between "sync points":
- An `SignalEvent` registers a **send point**. Other timelines can fetch resource state "snapshot" at this point.
- A `WaitForEvent` registers a **fetch point**. Issuing timeline will read the snapshot.
- A submission will alter the resource state within the timeline and create a **write point**. *If the resource doesn't exist in current timeline, this will behave like a implicit fetch point to fetch from the CPU timeline - more on this later*

The `IEvent` behaves like a sync primitive between different timelines. On all backends this is a 64bit integer. It's value is **globally atomic** and only allows increment. A `Reset` behaves like a desrtoy-then-create. After that the event should be treated as a new object.

## Timeline Model

Each timeline owns a resource [state list](#state-list) keyed by `{ SyncPoint inst, SyncPoint value }` pair. This can be implemented as 2 64bit values such as `{ void* inst, uint64_t value }`. A submission can create a key looks like `{ nullptr, submissionIdx }`. All states are updated at *API invoke time* instead of actual GPU time as a low-cost resonably accurate extrapolate.

Submission is a very unique sync point creator:
1. Before any command lists actually submitted to GPU, a special resource transition command list *may* be inserted.
2. Backends *passively* track a "last completed submission index" to know the execution state of transition command lists and recycle completed ones. This happens during the `Submit` interface call, and is the first step.

This works to our advantage - we'll **always** know the latest actual resource states on GPU. Albeit not the most up-to-date, it still serves as a "ground truth". In a real application there can be numerous event signal/waits and submissions. If we track every state from app start to finish, the state list will be unnecessarily bloated. Instead, we can only keep a "window" of states between selected sync point: from ground truth to latest API extrapolation. Anything older than the ground truth will be discarded. This also behaves correctly in terms of command execution: syncing to old event values should be treated **carefully**: you must carefully examine and make sure later mutators won't break your assumptions of resource states. A carefully designed API calling sequence that works on bare DX12/Vulkan will also work here: the worst case is syncing to a discarded sync point, and we provide the ground truth as a replacement. see [status list trimming](#state-trimming)

## CPU Timeline

CPU timeline is a special timeline: resources create and destroy on this timeline, and it is always in sync with API extrapolation. It is also where API calls are made. To maintain semantic correctness, CPU timeline won't track stale status like other timelines. Resource states will be transferred to relative timelines at `Submit`, and got back at `WaitForEvent`. During this period the resource state is "unknown" to CPU timeline. Anyone trying to fetch the resource's state from CPU timeline within this period will get "UNDEFINED" and effectively discard the content.

Resource destruction on CPU timeline will also broadcast to other timelines and remove resource state history from them. This is **NOT** thread safe, and requires resource state known to CPU timeline, othewise the resource is effectively "in-use" by GPU, destroying such resources is a API violation on **ALL BACKENDS**.

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
1. `IEvent` although is not a trackable resource, is also being referenced by multiple timelines thus use similar scheme to handle destruction.
2. Queue operations are not thread safe, meaning either all queues should be operated on single thread, or add external synchronization on all queue operation points. However `IEvent` readback is thread safe and atomic, so it's safe to use in polling style.

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
3. Walk send point queue old to new, pop entry where a) key is invalid (IEvent destroyed) or b) send point generation older than current completed generation.

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
  <NUM>: Event signal. NUM is event value
  <<NUM>>/// : Wait for event. NUM is wait value. 
               Before satisfied the timeline becomes "///"
               to indicate blockage
  {===}: Resource transition command list

```

- **POI #1**: Resource 1 created. State logged to CPU timeline
- **POI #2**: `Submit` to transfer queue. 

    Actual API calling sequence: `Submit` then `SignalEvent` with value 1.

    Resource 1's state is transferred to transfer queue timeline and purged from CPU timeline. API extrapolation: although the transfer queue hasn't finish its command yet, transfer queue's status list still record resource 1's state being "transfer destination". This happens at `Submit` API calling time and is ahead of ground truth GPU status.

    `SignalEvent` creates a send point and registers to `IEvent`.

- **POI #3**: `Submit` to graphics queue.

    Actual API calling sequence: 

    1. Encode `WaitForEvent` with value 1
    2. `Submit`.
    3. Encode `SignalEvent` with value 2.


    `WaitForEvent` create a fetch point on graphics queue. No resource state fetching happened.

    Resource 1 usage is declared in the submission. graphics queue perform status fetch on current timeline -> encounters the fetch point -> fetch resource state from peer send point. Resource 1's state now enters graphics queue timeline.

    `SignalEvent` creates a send point and registers to `IEvent`.

    API extrapolation: currently both the wait and the command list haven't been executed on GPU (because transfer queue hasn't finished yet), graphics queue's status list still record resource 1's state being "shader resource" and 2 steps ahead of round truth GPU status (which is still generic/undefined).

    Status list isolation: CPU timeline: no resource 1; Transfer queue timeline: resource 1 in "transfer destination" status; Graphics queue timeline: resource in "shader resource: state

- **POI #4**: Graphics queue begin to wait for event; Transfer queue completed the transition command, now resource state ground truth is "transfer destination"; CPU thread called `WaitForEvent` with value 2 and begin to wait for event.

- **POI #5**: Graphics queue blocked on event wait; CPU thread blocked on event wait

- **POI #6**: Transfer queue completed and signaled event with value 1. Graphics queue unblocked and starts executing transition command buffer; CPU thread still blocked on event wait

- **POI #7**: Graphics queue completed transition command. now resource state ground truth is "shader resource";

- **POI #8**: Graphics queue completed and signal event with value 2; CPU thread unblocks. Before `WaitForEvent` call exits, CPU thread executes fetch point and fetch from corresponding send point. Now resource 1's state is returned to CPU timeline as "shader resource"

## Bindless

Alloy bindless T1/T2 allows mutable resources and the auto tracking system won't have the insights of resource usage like T0 fixed resource set. Application must provide the resource usage info in order to let auto barrier insertion to work.

Alloy provides following interface for all pass encoders:

```c++
void UseResource(ResourceUsage, PipelineStage);
```

Intra-pass barrier support:

- **Vulkan**: Only have render pass. Barriers within pass is limited only to attachments.
- **Metal**: Have render/compute/transfer passes. Barriers within pass can sync between any stages.
- **DX12**: No passes, immediate mode.

To abstract over all those backends, alloy use following model: For render passes, only inserts barriers for `UseResource` on attachments. Other `UseResource` calls will be pushed to before the beginning of the pass. For compute and transfer passes, all `UseResource` will insert corresponding barriers.


## Dedicated Transfer Queue

Dedicated transfer queues are normally DMA backed queues that can only do transfer commands. They have limited support for resource states (normally only "generic", "transfer source" and "transfer destination"). To be able to utilise such queue, alloy need explicit resource state transition interface on command list.

Alloy provides following interface for command list/queue:

```c++
void RequestResourceState(Resource, State);
```
