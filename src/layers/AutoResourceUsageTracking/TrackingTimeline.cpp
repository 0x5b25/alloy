#include "TrackingTimeline.hpp"

#include <cassert>

namespace alloy::layers::AutoResourceUsageTracking {
    
    void TextureLayoutLog::Add(uint64_t gen, TextureLayout layout) {
        assert(log.empty() || (log.back().gen < gen));

        if(!log.empty()) {
            auto& latestEntry = log.back();
            if(latestEntry.layout == layout)
                return;
            //Update state that has the same generation
            if(latestEntry.gen == gen) {
                latestEntry.layout = layout;
                return;
            }
        }

        log.emplace_back(gen, layout);
    }

    TextureLayout TextureLayoutLog::Get(uint64_t gen) const {
        // Traverse new to old (back to front)
        for(auto it = log.rbegin(); it != log.rend(); ++it) {
            auto thisGen = it->gen;
            if(thisGen <= gen) {
                return it->layout;
            }
        }

        // The generation was dropped.
        // we return the oldest possible layout.
        // This is semantically correct at backend API level.
        if(logTrimmed) {
            assert(!log.empty());
            return log.front().layout;
        }

        // Return undefined to implicit discard resource content
        // because either this resource is unknown to us 
        // or the resource enters this timeline after
        // the specified sync point generation.
        // This is semantically correct at backend API level.
        return TextureLayout::Undefined;
    }

    TextureLayout TextureLayoutLog::GetLatest() const {
        assert(!log.empty());
        return log.back().layout;
    }

    void TextureLayoutLog::Trim(uint64_t gen) {
        if(log.empty()) return; // No work to do

        if(log.size() == 1) { // Single entry. Just compare and modify
            auto& entry = log.back();
            if(entry.gen <= gen) {
                logTrimmed = true;
                entry.gen = gen;
            }
            return;
        }

        // Multi entry, must find the "crossing point" where prev entry
        // is older than or equal to trim gen, and current entry is
        // newer than trim gen

        bool popped = false;
        TextureLayout poppedLayout;

        // Traverse old to new (front to back)
        while(!log.empty()) {
            auto& currEntry = log.front();
            if(currEntry.gen <= gen) {
                popped = true;
                poppedLayout = currEntry.layout;
                log.pop_front();
            } else {
                break;
            }
        }

        //We encountered the "crossing point" 
        if(popped) {
            logTrimmed = true;
            log.emplace_front(gen, poppedLayout);
        }
    }

    void GPUTimeline::_CheckAndClearInvalidRefs() {
        auto it = _textures.cbegin();
        while(it != _textures.cend()) {
            if(!it->first)  it = _textures.erase(it);
            else            ++it;
        }
    }

    void GPUTimeline::_Trim(uint64_t gen) {
        for(auto& entry : _textures) {
            entry.second.Trim(gen);
        }
    }

    void GPUTimeline::RegisterTextureState(
        WeakTrackedRef<ITextureView> ref, 
        TextureLayout layout
    ) {
        auto [it, inserted] = _textures.try_emplace (
            std::move(ref)
        );

        it->second.Add(_lastSubmittedFence, layout);
    }

    void GPUTimeline::PushStatesToCPUTimeline(
        CPUTimeline& dst,
        uint64_t fenceVal
    ) const {
        for(auto& entry : _textures) {
            if(!entry.first) continue;
            auto  stat = entry.second.Get(fenceVal);
            dst.RegisterTextureState(entry.first, stat);
        }
    }

    
    void GPUTimeline::PushStatesToGPUTimeline(
        GPUTimeline& dst,
        uint64_t fenceVal
    ) const {
        for(auto& entry : _textures) {
            if(!entry.first) continue;
            auto  stat = entry.second.Get(fenceVal);
            dst.RegisterTextureState(entry.first, stat);
        }
    }

} // namespace name

