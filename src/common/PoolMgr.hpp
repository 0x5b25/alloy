#pragma once

#include <cstdint>

namespace Veldrid
{
    
    class PoolBase;
    class AllocatorBase;
    class PoolMgrBase;


    class AllocatorBase{
    public:
        struct Token{

        };

    public:

        virtual ~AllocatorBase();

        virtual AllocStat CreateAllocation(AllocationBase** outAlloc) = 0;
    };

    class AllocationBase{

    public:

        virtual ~AllocationBase();
    };

    class PoolBase{

    public:

        virtual ~PoolBase();

        virtual std::uint64_t GetCapacity() = 0;

    };

    

    class PoolMgrBase{


    public:

        virtual ~PoolMgrBase(){}

    protected:

        //Protocols
        //For inheritance
        virtual PoolBase* CreateNewPool() = 0;

    };

} // namespace Veldrid

