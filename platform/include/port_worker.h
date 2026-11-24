#pragma once

// The exit path the background workers (Discord, LiveSplit, update check) share.

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace PortWorker {

// Sets `stop`, wakes the worker and gives it two seconds to set `workerDone`; one still inside a
// blocking call after that is detached rather than holding up the exit. The runtime it works on
// is therefore leaked by its owner, never destroyed.
template < typename Runtime >
void Stop(Runtime& runtime) {
  {
    std::lock_guard< std::mutex > lock(runtime.mutex);
    runtime.stop = true;
  }
  runtime.wake.notify_all();
  if (!runtime.worker.joinable()) {
    return;
  }
  bool done;
  {
    std::unique_lock< std::mutex > lock(runtime.mutex);
    done = runtime.wake.wait_for(lock, std::chrono::seconds(2), [&runtime] { return runtime.workerDone; });
  }
  if (done) {
    runtime.worker.join();
  } else {
    runtime.worker.detach();
  }
}

} // namespace PortWorker
