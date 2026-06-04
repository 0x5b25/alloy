## Command List Interface Reshape

Currently alloy exposes a highly abstracted command list allocation interface: command queues can directly provide command lists. Albeit simple, it brings heavy overheads on command buffer allocation. We need a new, cleaner and more efficient command list abstraction model. 

- **Vulkan strategy**:  Currently alloy employs the "one pool per thread" strategy to manage Vulkan command pools. Command pools on Vulkan is generally "bottomless" and we can keep allocating until the system actually runs out of memory. Command buffers allocated from the pool can be freely reset assuming it's not being used by GPU.

- **DX12 strategy**: Currently alloy employs the "one pool per command list" strategy to manage `ID3D12CommandAllocator`s. DX12 restriction is that 1) only one command list in a pool can be in `active` state and record commands. 2) No guarantee a command lists reset actually frees its space back to the allocator.


## Efficient Command List Abstraction

### 1. Goals
- Provide a **unified command buffer abstraction** across Metal, Vulkan, and DX12.
- Enable **multi-threaded command recording** while keeping submission centralized.
- Hide backend-specific memory management (pools/allocators) from users.
- Maintain **safe, expressive API** for recording passes and commands.

---

### 2. Backend Summaries

| Feature | Metal | Vulkan | DX12 |
|---------|-------|--------|------|
| Memory Ownership | Command buffer owns memory | Command pool owns memory, buffers allocated from it | Command allocator owns memory; command list is thin wrapper |
| Max simultaneous recording | Limited by queue | Multiple buffers per pool | 1 per allocator |
| Freeing | Automatic | Free buffer frees memory | Only Reset allocator |
| Threading | Simple | Complex; per-thread pools recommended | Limited; per-allocator recording |
| Pass Requirement | Yes | Only for graphics; not needed for compute/blit | No passes; logical pass can be simulated |

**Observations:**
- DX12 per-list allocator is heavier if many allocators are allocated but ensures predictable memory use.
- Vulkan can be flexible using per-thread pools.
- Metal is the simplest but still limited in parallelism.

---

### 3. Proposed Abstraction Model

**Core Concepts:**
1. **CommandAllocator** – owns the memory and provides CommandBuilder(s).
2. **CommandBuilder** – records a single command list/buffer; returns an **opaque token** when finalized.
3. **Passes** – RenderPass, ComputePass, BlitPass; support fluent interface within a scope.
4. **Centralized Submitter** – collects tokens from worker threads and submits them to the intended GPU queue.

**C++-style API Skeleton:**

```cpp
class CommandAllocator {
public:
    CommandBuilder BeginCommandList();
    void Reset();
};

class Token {
    /* A "mostly" opaque token to represent a finished command list
     * inside the `CommandAllocator`. Please see "Token Recycling"
     */
}

class CommandBuilder {
public:
    RenderPass BeginRenderPass(const RenderPassDescriptor& desc);
    ComputePass BeginComputePass();
    BlitPass BeginBlitPass();
    Token Finalize(); // opaque handle
};

class RenderPass {
public:
    RenderPass& setPipeline(Pipeline pipeline);
    RenderPass& setResourceSet(ResourceSet set);
    RenderPass& draw(uint32_t vertexCount, uint32_t startVertex = 0);
    RenderPass& drawIndexed(uint32_t indexCount, uint32_t startIndex = 0);
    ...

    void End();
    ~RenderPass();
};

// Other passes

// Worker thread usage
// allocator is requested from queue during worker
// startup
auto cmdBuilder = allocator.BeginCommandList();
auto pass = cmdBuilder.BeginRenderPass(...);
pass.setPipeline(pipeline)
    .setResourceSet(resources)
    .draw(3)
    .drawIndexed(36);
pass.End(); // optional, either RAII'd or auto-ended by builder.Finalize()
Token token = cmdBuilder.Finalize();

// Central submitter
queue.Submit({token1, token2, token3});
```

**Notes:**
- Each worker thread can have its **own allocator** (DX12: per allocator, Vulkan: per-thread pool).
- Multi-buffer recording per thread: request additional allocators instead of batching internally.
- `Finalize()` returns an opaque token for centralized submission.
- RAII ensures passes are closed even in case of exceptions.

---

### 4. Token Recycling
- Tokens should encode allocator ID + buffer index.
- Enables **safe reuse of memory** once GPU has finished execution.
- Centralized submitter maintains list of in-flight tokens per allocator.

---

### 5. Fluent Interface vs Explicit Calls

**Decision:** Hybrid approach
- **Fluent inside passes:** expressive, allows chaining draw/setPipeline calls.
- **Explicit for lifecycle:** BeginCommandList(), Finalize(), Reset() ensures correctness and clarity.
- Avoid per-call status reporting; validation occurs at `Finalize()` or via debug layers.

---

### 6. Memory / Threading Strategy

- **DX12:** One allocator per command list; recording single list per allocator; reset when all lists complete.
- **Vulkan:** Command pool per thread; can provide multiple allocators from the same pool.
- **Metal:** Queue-backed buffers; per-thread abstraction can hide concurrency limits.

**Worker threads**: record independently using their allocators. **Central submission** ensures thread-safe submission to GPU.

---

### 7. Diagram of Flow

```
[Worker Thread 1]         [Worker Thread 2]       ...
      |                         |
      v                         v
CommandAllocator             CommandAllocator
      |                         |
      v                         v
CommandBuilder               CommandBuilder
      |                         |
      v                         v
RenderPass/ComputePass/BlitPass (fluent, RAII)
      |                         |
Finalize() -> token           Finalize() -> token
      \                         /
       \                       /
        \\-------------------//
                 |
                 v
         Central Submitter
                 |
                 v
              GPU Queue
```

---

### 8. Additional Notes
- Avoid internal multi-buffer batching inside allocator to preserve allocator semantics.
- Workers can request multiple allocators if they need parallel-ish recording.
- Backend-specific details are hidden; allocator and builder abstractions unify the interface.
- This design scales well for **frame graph execution**, multi-threaded recording, and centralized submission.
- Metal 4 brings new models to command buffers and command queues:
    1. A new command buffer allocation scheme with `MTL4CommandAllocator` and `MTL4CommandBuffer`s taking an allocator and does not retain resources. Works much like the DX12 model. We might want to support that on Metal 4 capable platforms to lower the parallel command list allocating and encoding overhead
    2. `MTL4CommandQueue` can now take batched submissions and allow submitting to command queue directly instead of per `MTLCommandcBuffer`'s commit. This allows lowering the CPU overhead. 

---

**Document captures:**
- Goals, backend summaries, abstraction model, fluent vs explicit interface, token recycling, threading strategy, and diagram.
- Ready to serve as **implementation blueprint** for cross-platform engines.

