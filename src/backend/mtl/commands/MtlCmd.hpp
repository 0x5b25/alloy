#pragma once

struct MtlCmdBase{

    MtlCmdBase* next;

    MtlCmdBase()
        : next(nullptr)
    {

    }

    virtual ~MtlCmdBase(){}

    virtual RecordCmd()

};

