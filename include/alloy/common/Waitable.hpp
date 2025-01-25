#pragma once
#include <mutex>
#include <condition_variable>

namespace alloy::common {
	// AutoResetWaitableEvent ------------------------------------------------------

	// An event that can be signaled and waited on. This version automatically
	// returns to the unsignaled state after unblocking one waiter. (This is similar
	// to Windows's auto-reset Event, which is also imitated by Chromium's
	// auto-reset |base::WaitableEvent|. However, there are some limitations -- see
	// |Signal()|.) This class is thread-safe.
	class AutoResetLatch final {
	public:
        AutoResetLatch() = default;
		~AutoResetLatch() = default;

		// Put the event in the signaled state. Exactly one |Wait()| will be unblocked
		// and the event will be returned to the unsignaled state.
		//
		// Notes (these are arguably bugs, but not worth working around):
		// * That |Wait()| may be one that occurs on the calling thread, *after* the
		//   call to |Signal()|.
		// * A |Signal()|, followed by a |Reset()|, may cause *no* waiting thread to
		//   be unblocked.
		// * We rely on pthreads's queueing for picking which waiting thread to
		//   unblock, rather than enforcing FIFO ordering.
		void Signal();

		// Put the event into the unsignaled state. Generally, this is not recommended
		// on an auto-reset event (see notes above).
		void Reset();

		// Blocks the calling thread until the event is signaled. Upon unblocking, the
		// event is returned to the unsignaled state, so that (unless |Reset()| is
		// called) each |Signal()| unblocks exactly one |Wait()|.
		void Wait();

		// Like |Wait()|, but with a timeout. Also unblocks if |timeout| expires
		// without being signaled in which case it returns true (otherwise, it returns
		// false).
		bool WaitWithTimeout(
			std::chrono::steady_clock::duration timeout);

		// Returns whether this event is in a signaled state or not. For use in tests
		// only (in general, this is racy). Note: Unlike
		// |base::WaitableEvent::IsSignaled()|, this doesn't reset the signaled state.
		bool IsSignaledForTest();

	private:
		std::condition_variable cv_;
		std::mutex mutex_;

		// True if this event is in the signaled state.
		bool signaled_ = false;

		//Disable copy and assign
		AutoResetLatch(const AutoResetLatch&) = delete;
		AutoResetLatch& operator=(const AutoResetLatch&) = delete;

	};


	// ManualResetWaitableEvent ----------------------------------------------------

	// An event that can be signaled and waited on. This version remains signaled
	// until explicitly reset. (This is similar to Windows's manual-reset Event,
	// which is also imitated by Chromium's manual-reset |base::WaitableEvent|.)
	// This class is thread-safe.
	class ManualResetLatch final {
	public:
		ManualResetLatch() {}
		~ManualResetLatch() {}

		// Put the event into the unsignaled state.
		void Reset();

		// Put the event in the signaled state. If this is a manual-reset event, it
		// wakes all waiting threads (blocked on |Wait()| or |WaitWithTimeout()|).
		// Otherwise, it wakes a single waiting thread (and returns to the unsignaled
		// state), if any; if there are none, it remains signaled.
		void Signal();

		// Blocks the calling thread until the event is signaled.
		void Wait();

		// Like |Wait()|, but with a timeout. Also unblocks if |timeout| expires
		// without being signaled in which case it returns true (otherwise, it returns
		// false).
		bool WaitWithTimeout(std::chrono::steady_clock::duration timeout);

		// Returns whether this event is in a signaled state or not. For use in tests
		// only (in general, this is racy).
		bool IsSignaledForTest();

	private:
		std::condition_variable cv_;
		std::mutex mutex_;

		// True if this event is in the signaled state.
		bool signaled_ = false;

		// While |std::condition_variable::notify_all()| (|pthread_cond_broadcast()|)
		// will wake all waiting threads, one has to deal with spurious wake-ups.
		// Checking |signaled_| isn't sufficient, since another thread may have been
		// awoken and (manually) reset |signaled_|. This is a counter that is
		// incremented in |Signal()| before calling
		// |std::condition_variable::notify_all()|. A waiting thread knows it was
		// awoken if |signal_id_| is different from when it started waiting.
		unsigned signal_id_ = 0u;

		//Disable copy and assign
		ManualResetLatch(const ManualResetLatch&) = delete;
		ManualResetLatch& operator=(const ManualResetLatch&) = delete;

	};



	/**
	 * \brief Wait for multiple objects(threads)
	 */
	class MultiObjectLatch
	{

		int _launchCnt;
		std::condition_variable _cv;
		std::mutex _m;

	public:

		MultiObjectLatch()
		{
			_launchCnt = 0;
		}

		~MultiObjectLatch()
		{

		}

        void Wait();

		/**
		 * \brief Add new threads into waiting objects
		 * \param count thread count
		 */
        void AddLaunch(int count = 1);

		/**
		 * \brief Notify thread finished. Latch would
		 * be released when no thread is being waited
		 * \param count Finished threads
		 */
        void LaunchComplete(int count = 1);


        bool WaitUntilTimeout(std::chrono::steady_clock::duration timeout);

        int QueryLaunchCnt();

	private:
		MultiObjectLatch(const MultiObjectLatch&) = delete;
		MultiObjectLatch& operator=(const MultiObjectLatch&) = delete;
	};

    
}
