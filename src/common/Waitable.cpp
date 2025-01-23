
#include "alloy/common/Waitable.hpp"

#include <cassert>

namespace alloy::common {


    void AutoResetLatch::Signal()
    {
        std::scoped_lock locker(mutex_);
        signaled_ = true;
        cv_.notify_one();
    }

    void AutoResetLatch::Reset()
    {
        std::scoped_lock locker(mutex_);
        signaled_ = false;
    }

    void AutoResetLatch::Wait()
    {
        std::unique_lock<std::mutex> locker(mutex_);
        while (!signaled_) {
            cv_.wait(locker);
        }
        signaled_ = false;
    }


    bool AutoResetLatch::WaitWithTimeout(
        std::chrono::steady_clock::duration timeout)
    {
        std::unique_lock<std::mutex> locker(mutex_);

        if (signaled_) {
            signaled_ = false;
            return false;
        }
        // We may get spurious wakeups.
        auto wait_remaining = timeout;
        auto start = std::chrono::steady_clock::now();
        while (true) {
            if (std::cv_status::timeout ==
                cv_.wait_for(
                    locker, wait_remaining)) {
                return true;  // Definitely timed out.
            }

            // We may have been awoken.
            if (signaled_) {
                break;
            }

            // Or the wakeup may have been spurious.
            auto now = std::chrono::steady_clock::now();
            //FML_DCHECK(now >= start);
            auto elapsed = now - start;
            // It's possible that we may have timed out anyway.
            if (elapsed >= timeout) {
                return true;
            }

            // Otherwise, recalculate the amount that we have left to wait.
            wait_remaining = timeout - elapsed;
        }

        signaled_ = false;
        return false;
    }

    bool AutoResetLatch::IsSignaledForTest()
    {
        std::scoped_lock locker(mutex_);
        return signaled_;
    }


    void ManualResetLatch::Signal() {
        std::scoped_lock locker(mutex_);
        signaled_ = true;
        signal_id_++;
        cv_.notify_all();
    }

    void ManualResetLatch::Reset() {
        std::scoped_lock locker(mutex_);
        signaled_ = false;
    }

    void ManualResetLatch::Wait() {
        std::unique_lock<std::mutex> locker(mutex_);

        if (signaled_) {
            return;
        }

        auto last_signal_id = signal_id_;
        do {
            cv_.wait(locker);
        } while (signal_id_ == last_signal_id);
    }

    template <typename ConditionFn>
    bool WaitWithTimeoutImpl(std::unique_lock<std::mutex>* locker,
        std::condition_variable* cv,
        ConditionFn condition,
        std::chrono::steady_clock::duration timeout) {
        //FML_DCHECK(locker->owns_lock());

        if (condition()) {
            return false;
        }

        // We may get spurious wakeups.
        auto wait_remaining = timeout;
        auto start = std::chrono::steady_clock::now();
        while (true) {
            if (std::cv_status::timeout ==
                cv->wait_for(*locker, 
                    wait_remaining)) {
                return true;  // Definitely timed out.
            }

            // We may have been awoken.
            if (condition()) {
                return false;
            }

            // Or the wakeup may have been spurious.
            auto now = std::chrono::steady_clock::now();
            //FML_DCHECK(now >= start);
            auto elapsed = now - start;
            // It's possible that we may have timed out anyway.
            if (elapsed >= timeout) {
                return true;
            }

            // Otherwise, recalculate the amount that we have left to wait.
            wait_remaining = timeout - elapsed;
        }
    }

    bool ManualResetLatch::WaitWithTimeout(std::chrono::steady_clock::duration timeout) {
        std::unique_lock<std::mutex> locker(mutex_);

        auto last_signal_id = signal_id_;
        // Disable thread-safety analysis for the lambda: We could annotate it with
        // |FML_EXCLUSIVE_LOCKS_REQUIRED(mutex_)|, but then the analyzer currently
        // isn't able to figure out that |WaitWithTimeoutImpl()| calls it while
        // holding |mutex_|.
        bool rv = WaitWithTimeoutImpl(
            &locker, &cv_,
            [this, last_signal_id]() {
                // Also check |signaled_| in case we're already signaled.
                return signaled_ || signal_id_ != last_signal_id;
            },
            timeout);
        //FML_DCHECK(rv || signaled_ || signal_id_ != last_signal_id);
        return rv;
    }

    bool ManualResetLatch::IsSignaledForTest() {
        std::scoped_lock locker(mutex_);
        return signaled_;
    }

    void MultiObjectLatch::Wait()
    {
        std::unique_lock<std::mutex> lock(_m);

        while (_launchCnt > 0)
        {
            _cv.wait(lock);
        }
    }

    void MultiObjectLatch::AddLaunch(int count)
    {
        assert(count > 0);

        std::unique_lock<std::mutex> lock(_m);
        _launchCnt += count;
    }

    void MultiObjectLatch::LaunchComplete(int count)
    {
        assert(count > 0);

        std::unique_lock<std::mutex> lock(_m);
        if (_launchCnt > count)
        {
            _launchCnt -= count;
            return;
        }

        _launchCnt = 0;
        _cv.notify_one();
    }

    bool MultiObjectLatch::WaitUntilTimeout(std::chrono::steady_clock::duration timeout)
    {
        using clock = std::chrono::steady_clock;
        std::unique_lock<std::mutex> locker(_m);

        if (_launchCnt < 1)
        {
            return false;
        }

        // We may get spurious wakeups.
        auto wait_remaining = timeout;
        auto start = clock::now();

        while (true)
        {
            if (std::cv_status::timeout ==
                _cv.wait_for(
                    locker, wait_remaining))
            {
                return true; // Definitely timed out.
            }

            // We may have been awoken.
            if (_launchCnt < 1)
            {
                break;
            }

            // Or the wakeup may have been spurious.
            auto now = clock::now();
            auto elapsed = now - start;
            // It's possible that we may have timed out anyway.
            if (elapsed >= timeout)
            {
                return true;
            }

            // Otherwise, recalculate the amount that we have left to wait.
            wait_remaining = timeout - elapsed;
        }

        return false;
    }

    int MultiObjectLatch::QueryLaunchCnt()
    {
        std::unique_lock<std::mutex> lock(_m);
        return _launchCnt;
    }

}