// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLO_BRILLO_MESSAGE_LOOPS_BASE_MESSAGE_LOOP_H_
#define LIBBRILLO_BRILLO_MESSAGE_LOOPS_BASE_MESSAGE_LOOP_H_

// BaseMessageLoop is a brillo::MessageLoop implementation based on
// base::MessageLoopForIO. This allows to mix new code using
// brillo::MessageLoop and legacy code using base::MessageLoopForIO in the
// same thread and share a single main loop. This disadvantage of using this
// class is a less efficient implementation of CancelTask() for delayed tasks
// since base::MessageLoopForIO doesn't provide a way to remove the event.

#include <map>
#include <memory>

#include <base/location.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/time/time.h>

#include <brillo/brillo_export.h>
#include <brillo/message_loops/message_loop.h>

namespace brillo {

class BRILLO_EXPORT BaseMessageLoop : public MessageLoop {
 public:
  explicit BaseMessageLoop(base::MessageLoopForIO* base_loop);
  ~BaseMessageLoop() override;

  // MessageLoop overrides.
  TaskId PostDelayedTask(const tracked_objects::Location& from_here,
                         const base::Closure& task,
                         base::TimeDelta delay) override;
  using MessageLoop::PostDelayedTask;
  TaskId WatchFileDescriptor(const tracked_objects::Location& from_here,
                             int fd,
                             WatchMode mode,
                             bool persistent,
                             const base::Closure& task) override;
  using MessageLoop::WatchFileDescriptor;
  bool CancelTask(TaskId task_id) override;
  bool RunOnce(bool may_block) override;
  void Run() override;
  void BreakLoop() override;

  // Returns a callback that will quit the current message loop. If the message
  // loop is not running, an empty (null) callback is returned.
  base::Closure QuitClosure() const;

 private:
  // Called by base::MessageLoopForIO when is time to call the callback
  // scheduled with Post*Task() of id |task_id|, even if it was canceled.
  void OnRanPostedTask(MessageLoop::TaskId task_id);

  // Called from the message loop when the IOTask should run the scheduled
  // callback. This is a simple wrapper of IOTask::OnFileReadyPostedTask()
  // posted from the BaseMessageLoop so it is deleted when the BaseMessageLoop
  // goes out of scope since we can't cancel the callback otherwise.
  void OnFileReadyPostedTask(MessageLoop::TaskId task_id);

  // Return a new unused task_id.
  TaskId NextTaskId();

  struct DelayedTask {
    tracked_objects::Location location;

    MessageLoop::TaskId task_id;
    base::Closure closure;
  };

  std::map<MessageLoop::TaskId, DelayedTask> delayed_tasks_;

  class IOTask : public base::MessageLoopForIO::Watcher {
   public:
    IOTask(const tracked_objects::Location& location,
           BaseMessageLoop* loop,
           MessageLoop::TaskId task_id,
           int fd,
           base::MessageLoopForIO::Mode base_mode,
           bool persistent,
           const base::Closure& task);

    const tracked_objects::Location& location() const { return location_; }

    // Used to start/stop watching the file descriptor while keeping the
    // IOTask entry available.
    bool StartWatching();
    void StopWatching();

    // Called from the message loop as a PostTask() when the file descriptor is
    // available, scheduled to run from OnFileReady().
    void OnFileReadyPostedTask();

    // Cancel the IOTask and returns whether it was actually canceled, with the
    // same semantics as MessageLoop::CancelTask().
    bool CancelTask();

   private:
    tracked_objects::Location location_;
    BaseMessageLoop* loop_;

    // These are the arguments passed in the constructor, basically forwarding
    // all the arguments passed to WatchFileDescriptor() plus the assigned
    // TaskId for this task.
    MessageLoop::TaskId task_id_;
    int fd_;
    base::MessageLoopForIO::Mode base_mode_;
    bool persistent_;
    base::Closure closure_;

    base::MessageLoopForIO::FileDescriptorWatcher fd_watcher_;

    // Tells whether there is a pending call to OnFileReadPostedTask().
    bool posted_task_pending_{false};

    // base::MessageLoopForIO::Watcher overrides:
    void OnFileCanReadWithoutBlocking(int fd) override;
    void OnFileCanWriteWithoutBlocking(int fd) override;

    // Common implementation for both the read and write case.
    void OnFileReady();

    DISALLOW_COPY_AND_ASSIGN(IOTask);
  };

  std::map<MessageLoop::TaskId, IOTask> io_tasks_;

  // Flag to mark that we should run the message loop only one iteration.
  bool run_once_{false};

  // The last used TaskId. While base::MessageLoopForIO doesn't allow to cancel
  // delayed tasks, we handle that functionality by not running the callback
  // if it fires at a later point.
  MessageLoop::TaskId last_id_{kTaskIdNull};

  // The pointer to the libchrome base::MessageLoopForIO we are wrapping with
  // this interface.
  base::MessageLoopForIO* base_loop_;

  // The RunLoop instance used to run the main loop from Run().
  base::RunLoop* base_run_loop_{nullptr};

  // We use a WeakPtrFactory to schedule tasks with the base::MessageLoopForIO
  // since we can't cancel the callbacks we have scheduled there once this
  // instance is destroyed.
  base::WeakPtrFactory<BaseMessageLoop> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(BaseMessageLoop);
};

}  // namespace brillo

#endif  // LIBBRILLO_BRILLO_MESSAGE_LOOPS_BASE_MESSAGE_LOOP_H_