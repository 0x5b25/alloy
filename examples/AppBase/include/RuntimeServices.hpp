

class ITimeService {

public:
    virtual float GetElapsedSeconds() = 0;
    virtual float GetDeltaSeconds() = 0;
    virtual float GetFixedUpdateDeltaSeconds() = 0;

};


class IRuntimeService {

public:
    virtual ITimeService* GetTimeService() = 0;

};
