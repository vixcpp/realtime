/**
 *
 * @file command_queue.cpp
 * @author Gaspard Kirira
 * @brief Implementation of the Vix Realtime bounded command queue.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/internal/command_queue.hpp>

#include <utility>

#include <vix/realtime/errors.hpp>

namespace vix::realtime::internal
{
  namespace
  {
    /**
     * @brief Validate a blocking queue timeout.
     */
    void validate_timeout(
        std::chrono::milliseconds timeout)
    {
      if (timeout.count() < 0)
      {
        throw Error{
            ErrorCode::InvalidConfiguration,
            "command queue timeout cannot be negative"};
      }
    }

  } // namespace

  CommandQueue::CommandQueue(std::size_t capacity)
      : capacity_(capacity)
  {
    if (capacity_ == 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "command queue capacity must be greater than zero"};
    }
  }

  CommandQueueStatus CommandQueue::try_push(
      RoomCommand command)
  {
    command.validate();

    {
      std::lock_guard<std::mutex> lock{mutex_};

      if (closed_)
      {
        return CommandQueueStatus::Closed;
      }

      if (commands_.size() >= capacity_)
      {
        return CommandQueueStatus::Full;
      }

      commands_.push_back(std::move(command));
    }

    notEmpty_.notify_one();
    return CommandQueueStatus::Success;
  }

  CommandQueueStatus CommandQueue::wait_push(
      RoomCommand command,
      std::chrono::milliseconds timeout)
  {
    command.validate();
    validate_timeout(timeout);

    if (timeout.count() == 0)
    {
      const auto status =
          try_push(std::move(command));

      if (status == CommandQueueStatus::Full)
      {
        return CommandQueueStatus::Timeout;
      }

      return status;
    }

    std::unique_lock<std::mutex> lock{mutex_};

    const bool ready = notFull_.wait_for(
        lock,
        timeout,
        [this]
        {
          return closed_ ||
                 commands_.size() < capacity_;
        });

    if (!ready)
    {
      return CommandQueueStatus::Timeout;
    }

    if (closed_)
    {
      return CommandQueueStatus::Closed;
    }

    commands_.push_back(std::move(command));

    lock.unlock();
    notEmpty_.notify_one();

    return CommandQueueStatus::Success;
  }

  CommandQueuePopResult CommandQueue::try_pop()
  {
    std::optional<RoomCommand> command;

    {
      std::lock_guard<std::mutex> lock{mutex_};

      if (commands_.empty())
      {
        return CommandQueuePopResult{
            closed_
                ? CommandQueueStatus::Closed
                : CommandQueueStatus::Empty,
            std::nullopt};
      }

      command.emplace(
          std::move(commands_.front()));

      commands_.pop_front();
    }

    notFull_.notify_one();

    return CommandQueuePopResult{
        CommandQueueStatus::Success,
        std::move(command)};
  }

  CommandQueuePopResult CommandQueue::wait_pop(
      std::chrono::milliseconds timeout)
  {
    validate_timeout(timeout);

    if (timeout.count() == 0)
    {
      const auto result = try_pop();

      if (result.status == CommandQueueStatus::Empty)
      {
        return CommandQueuePopResult{
            CommandQueueStatus::Timeout,
            std::nullopt};
      }

      return result;
    }

    std::unique_lock<std::mutex> lock{mutex_};

    const bool ready = notEmpty_.wait_for(
        lock,
        timeout,
        [this]
        {
          return closed_ ||
                 !commands_.empty();
        });

    if (!ready)
    {
      return CommandQueuePopResult{
          CommandQueueStatus::Timeout,
          std::nullopt};
    }

    if (commands_.empty())
    {
      return CommandQueuePopResult{
          CommandQueueStatus::Closed,
          std::nullopt};
    }

    std::optional<RoomCommand> command{
        std::move(commands_.front())};

    commands_.pop_front();

    lock.unlock();
    notFull_.notify_one();

    return CommandQueuePopResult{
        CommandQueueStatus::Success,
        std::move(command)};
  }

  void CommandQueue::close()
  {
    {
      std::lock_guard<std::mutex> lock{mutex_};
      closed_ = true;
    }

    notEmpty_.notify_all();
    notFull_.notify_all();
  }

  std::size_t CommandQueue::clear()
  {
    std::size_t removed = 0;

    {
      std::lock_guard<std::mutex> lock{mutex_};

      removed = commands_.size();
      commands_.clear();
    }

    if (removed != 0)
    {
      notFull_.notify_all();
    }

    return removed;
  }

  std::size_t CommandQueue::size() const
  {
    std::lock_guard<std::mutex> lock{mutex_};
    return commands_.size();
  }

  std::size_t CommandQueue::capacity() const noexcept
  {
    return capacity_;
  }

  bool CommandQueue::empty() const
  {
    std::lock_guard<std::mutex> lock{mutex_};
    return commands_.empty();
  }

  bool CommandQueue::full() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return commands_.size() >= capacity_;
  }

  bool CommandQueue::closed() const
  {
    std::lock_guard<std::mutex> lock{mutex_};
    return closed_;
  }

} // namespace vix::realtime::internal
