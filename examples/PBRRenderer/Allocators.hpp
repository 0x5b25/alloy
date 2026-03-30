#pragma once

#include <bit>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

constexpr auto AlignUp(auto val, auto align) {
    constexpr auto mask = (decltype(val))(align-1);
    return (val + mask - 1) & (~mask);
}

constexpr auto AlignDown(auto val, auto align) {
    constexpr auto mask = (decltype(val))(align-1);
    return val & ~mask;
}

template<typename SizeType = uint32_t>
class FreeListAllocator {
    static_assert(std::is_unsigned_v<SizeType>);
public:
    
    static constexpr SizeType INVALID_VALUE = std::numeric_limits<SizeType>::max();

private:
    struct Node {
        SizeType offset;
        SizeType size;
        bool isFree;
        Node* next;
    };


    Node* _head = nullptr;
    SizeType _capacity = 0;

    static constexpr SizeType _AlignUp(SizeType value, SizeType alignment) {
        return alignment <= 1 ? value : (SizeType)((value + alignment - 1) / alignment * alignment);
    }

    void _DestroyList() {
        while (_head != nullptr) {
            Node* next = _head->next;
            delete _head;
            _head = next;
        }
    }

    void _InsertAfter(Node* prev, Node* node) {
        if (prev == nullptr) {
            node->next = _head;
            _head = node;
            return;
        }

        node->next = prev->next;
        prev->next = node;
    }

    Node* _FindTail(Node** ppPrev = nullptr) const {
        Node* prev = nullptr;
        Node* curr = _head;
        while (curr != nullptr && curr->next != nullptr) {
            prev = curr;
            curr = curr->next;
        }

        if (ppPrev != nullptr) {
            *ppPrev = prev;
        }
        return curr;
    }

public:
    explicit FreeListAllocator(SizeType capacity = 0)
        : _capacity(capacity)
    {
        if (capacity > 0) {
            _head = new Node{ 0, capacity, true, nullptr };
        }
    }

    ~FreeListAllocator() {
        _DestroyList();
    }

    FreeListAllocator(const FreeListAllocator&) = delete;
    FreeListAllocator& operator=(const FreeListAllocator&) = delete;

    FreeListAllocator(FreeListAllocator&& other) noexcept
        : _head(other._head)
        , _capacity(other._capacity)
    {
        other._head = nullptr;
        other._capacity = 0;
    }

    FreeListAllocator& operator=(FreeListAllocator&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        _DestroyList();
        _head = other._head;
        _capacity = other._capacity;

        other._head = nullptr;
        other._capacity = 0;
        return *this;
    }

    void Reset(SizeType capacity) {
        _DestroyList();
        _capacity = capacity;
        _head = capacity > 0 ? new Node{ 0, capacity, true, nullptr } : nullptr;
    }

    SizeType Allocate(SizeType size, SizeType alignment = 1) {
        if (size == 0 || alignment == 0) {
            return INVALID_VALUE;
        }

        Node* prev = nullptr;
        for (Node* curr = _head; curr != nullptr; prev = curr, curr = curr->next) {
            if (!curr->isFree) {
                continue;
            }

            SizeType alignedOffset = _AlignUp(curr->offset, alignment);
            if (alignedOffset < curr->offset) {
                continue;
            }

            SizeType prefixSize = (SizeType)(alignedOffset - curr->offset);
            if (prefixSize > curr->size) {
                continue;
            }

            if (curr->size < size + prefixSize) {
                continue;
            }

            auto remainingAfterPrefix = (SizeType)(curr->size - prefixSize);

            if (prefixSize > 0) {
                Node* prefix = new Node{ curr->offset, prefixSize, true, curr };
                _InsertAfter(prev, prefix);
                prev = prefix;
                curr->offset = alignedOffset;
                curr->size = remainingAfterPrefix;
            }

            
            auto suffixSize = (SizeType)(remainingAfterPrefix - size);

            if (suffixSize) {
                Node* suffix = new Node{
                    (SizeType)(alignedOffset + size),
                    suffixSize,
                    true,
                    curr->next
                };
                curr->next = suffix;
            }

            curr->offset = alignedOffset;
            curr->size = size;
            curr->isFree = false;
            return curr->offset;
        }

        return INVALID_VALUE;
    }

    bool Free(SizeType offset) {
        if (offset == INVALID_VALUE) {
            return false;
        }

        Node* prev = nullptr;
        Node* curr = _head;
        while (curr != nullptr) {
            if (!curr->isFree && curr->offset == offset) {
                break;
            }
            prev = curr;
            curr = curr->next;
        }

        if (curr == nullptr) {
            return false;
        }

        curr->isFree = true;

        if (curr->next != nullptr && curr->next->isFree) {
            Node* next = curr->next;
            curr->size = (SizeType)(curr->size + next->size);
            curr->next = next->next;
            delete next;
        }

        if (prev != nullptr && prev->isFree) {
            prev->size = (SizeType)(prev->size + curr->size);
            prev->next = curr->next;
            delete curr;
        }

        return true;
    }

    SizeType GetCapacity() const {
        return _capacity;
    }

    SizeType GetTrimmableSizeAtEnd() const {
        Node* tail = _FindTail();
        if (tail == nullptr || !tail->isFree) {
            return 0;
        }

        SizeType tailEnd = (SizeType)(tail->offset + tail->size);
        return tailEnd == _capacity ? tail->size : 0;
    }

    void GrowAtEnd(SizeType size) {
        if (size == 0) {
            return;
        }

        if (_head == nullptr) {
            _head = new Node{ 0, size, true, nullptr };
            _capacity = size;
            return;
        }

        Node* tail = _FindTail();
        if (tail->isFree && (SizeType)(tail->offset + tail->size) == _capacity) {
            tail->size = (SizeType)(tail->size + size);
        }
        else {
            tail->next = new Node{ _capacity, size, true, nullptr };
        }

        _capacity = (SizeType)(_capacity + size);
    }

    bool ShrinkAtEnd(SizeType size) {
        if (size == 0) {
            return true;
        }

        SizeType trimmableSize = GetTrimmableSizeAtEnd();
        if (size > trimmableSize) {
            return false;
        }

        Node* prev = nullptr;
        Node* tail = _FindTail(&prev);
        if (tail == nullptr) {
            return false;
        }

        if (size == tail->size) {
            if (prev == nullptr) {
                delete _head;
                _head = nullptr;
            }
            else {
                prev->next = nullptr;
                delete tail;
            }
        }
        else {
            tail->size = (SizeType)(tail->size - size);
        }

        _capacity = (SizeType)(_capacity - size);
        return true;
    }

    static constexpr SizeType InvalidOffset() {
        return INVALID_VALUE;
    }
};


template<typename IndexType = uint32_t>
class BitmapAllocator {
    static_assert(std::is_unsigned_v<IndexType>);

public:
    static constexpr IndexType INVALID_VALUE = std::numeric_limits<IndexType>::max();

private:

    IndexType _capacity = 0;
    std::vector<uint64_t> _words;

public:
    explicit BitmapAllocator(IndexType capacity = 0) {
        Reset(capacity);
    }

    void Reset(IndexType capacity) {
        _capacity = capacity;
        _words.assign(((size_t)capacity + 63u) / 64u, 0ull);
    }

    IndexType Allocate() {
        for (size_t wordIndex = 0; wordIndex < _words.size(); ++wordIndex) {
            auto& w = _words[wordIndex];
            uint64_t freeBits = ~w;
            if (freeBits == 0) {
                continue;
            }

            unsigned long bitIndex = std::countr_zero(freeBits);
            IndexType index = (IndexType)(wordIndex * 64 + bitIndex);
            if (index >= _capacity) {
                break;
            }

            w |= (uint64_t{ 1 } << bitIndex);
            return index;
        }

        return INVALID_VALUE;
    }

    void Free(IndexType index) {
        if (index >= _capacity) return;

        size_t wordIndex = (size_t)(index / 64);
        IndexType bitIndex = (IndexType)(index & 0x3ff);
        uint64_t mask = uint64_t{ 1 } << bitIndex;

        _words[wordIndex] &= ~mask;
    }

    bool IsAllocated(IndexType index) const {
        if (index >= _capacity) {
            return false;
        }

        size_t wordIndex = (size_t)(index / 64);
        IndexType bitIndex = (IndexType)(index % 64);
        return (_words[wordIndex] & (uint64_t{ 1 } << bitIndex)) != 0;
    }

    void Clear() {
        for (uint64_t& word : _words) {
            word = 0;
        }
    }

    IndexType GetCapacity() const {
        return _capacity;
    }

    IndexType GetTrimmableSizeAtEnd() const {
        if (_capacity == 0) {
            return 0;
        }

        const auto reservedBitCnt = AlignUp(_capacity, 64) - _capacity;
        IndexType freeSlotCnt = 0;
        for(auto i = _words.size()-1; i >= 0; --i) {
            uint64_t word = _words[i];
            IndexType freeAtTail = (IndexType)std::countl_zero(word);

            if (freeAtTail == 0) {
                break;
            }

            freeSlotCnt += freeAtTail;
        }

        return freeSlotCnt - reservedBitCnt;
    }

    void GrowAtEnd(IndexType size) {
        if (size == 0) return;
        
        const uint64_t validBitCntInLastWord = _capacity & 0x3ff; // _capacity % 64
        //Clear reserved bits if necessary
        if(validBitCntInLastWord) {
            uint64_t validMask = (1ULL << validBitCntInLastWord) - 1;
            _words.back() &= validMask;
        }

        IndexType oldCapacity = _capacity;
        _capacity = (IndexType)(_capacity + size);
        _words.resize(((size_t)_capacity + 63u) / 64u, 0ull);
    }

    bool ShrinkAtEnd(IndexType size) {
        if (size == 0) {
            return true;
        }

        if (size > GetTrimmableSizeAtEnd()) {
            return false;
        }

        _capacity = (IndexType)(_capacity - size);
        _words.resize(((size_t)_capacity + 63u) / 64u, 0ull);
        return true;
    }

    static constexpr IndexType InvalidIndex() {
        return INVALID_VALUE;
    }
};
