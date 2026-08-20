/**
 *
 * @file command_queue.hpp
 * @author Gaspard Kirira
 * @brief Thread-safe bounded command queue for Vix Realtime rooms.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_INTERNAL_COMMAND_QUEUE_HPP
#define VIX_REALTIME_INTERNAL_COMMAND_QUEUE_HPP

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string_view>

#include <vix/realtime/room_command.hpp>
#include <vix/realtime/command_queue_status.hpp>

namespace vix::realtime::internal
{
  /**
   * @brief Result returned by a command removal operation.
   */
  struct CommandQueuePopResult
  {
    /** @brief Queue operation status. */
    CommandQueueStatus status{CommandQueueStatus::Empty};

    /** @brief Removed command when the operation succeeded. */
    std::optional<RoomCommand> command{};

    /**
     * @brief Return whether a command was successfully removed.
     *
     * @return True when the result contains a command.
     */
    [[nodiscard]] explicit operator bool() const noexcept
    {
      return status == CommandQueueStatus::Success &&
             command.has_value();
    }
  };

  /**
   * @brief Thread-safe bounded FIFO queue for room commands.
   *
   * Each room owns one command queue. The queue serializes command processing
   * for that room while providing deterministic backpressure when its bounded
   * capacity is reached.
   *
   * Closing the queue is permanent. Commands already present remain available
   * for draining, but no new command may be inserted.
   */
  class CommandQueue
  {
  public:
    /**
     * @brief Construct an empty bounded command queue.
     *
     * @param capacity Maximum number of pending commands.
     *
     * @throws vix::realtime::Error
     *         When the capacity is zero.
     */
    explicit CommandQueue(std::size_t capacity);

    /**
     * @brief Destroy the command queue.
     */
    ~CommandQueue() = default;

    CommandQueue(const CommandQueue &) = delete;
    CommandQueue &operator=(const CommandQueue &) = delete;
    CommandQueue(CommandQueue &&) = delete;
    CommandQueue &operator=(CommandQueue &&) = delete;

    /**
     * @brief Insert a command without waiting.
     *
     * @param command Command to enqueue.
     * @return `Success`, `Full`, or `Closed`.
     *
     * @throws vix::realtime::Error
     *         When the command is invalid.
     */
    [[nodiscard]] CommandQueueStatus try_push(
        RoomCommand command);

    /**
     * @brief Wait for capacity and insert a command.
     *
     * A zero timeout behaves like `try_push()`.
     *
     * @param command Command to enqueue.
     * @param timeout Maximum duration to wait for capacity.
     * @return `Success`, `Closed`, or `Timeout`.
     *
     * @throws vix::realtime::Error
     *         When the command is invalid or the timeout is negative.
     */
    [[nodiscard]] CommandQueueStatus wait_push(
        RoomCommand command,
        std::chrono::milliseconds timeout);

    /**
     * @brief Remove the oldest command without waiting.
     *
     * A closed queue may still return queued commands. It returns `Closed`
     * only after every queued command has been drained.
     *
     * @return Command removal result.
     */
    [[nodiscard]] CommandQueuePopResult try_pop();

    /**
     * @brief Wait for and remove the oldest command.
     *
     * A zero timeout behaves like `try_pop()`.
     *
     * @param timeout Maximum duration to wait for a command.
     * @return Command removal result.
     *
     * @throws vix::realtime::Error
     *         When the timeout is negative.
     */
    [[nodiscard]] CommandQueuePopResult wait_pop(
        std::chrono::milliseconds timeout);

    /**
     * @brief Permanently close the command queue.
     *
     * Closing wakes all waiting producers and consumers. Existing commands
     * remain available for draining.
     */
    void close();

    /**
     * @brief Remove every pending command.
     *
     * @return Number of removed commands.
     */
    std::size_t clear();

    /**
     * @brief Return the number of queued commands.
     *
     * @return Current pending command count.
     */
    [[nodiscard]] std::size_t size() const;

    /**
     * @brief Return the maximum number of pending commands.
     *
     * @return Queue capacity.
     */
    [[nodiscard]] std::size_t capacity() const noexcept;

    /**
     * @brief Return whether the queue currently contains no command.
     *
     * @return True when the queue is empty.
     */
    [[nodiscard]] bool empty() const;

    /**
     * @brief Return whether the queue reached its capacity.
     *
     * @return True when no additional command can be inserted immediately.
     */
    [[nodiscard]] bool full() const;

    /**
     * @brief Return whether the queue was permanently closed.
     *
     * @return True when the queue is closed.
     */
    [[nodiscard]] bool closed() const;

  private:
    /** @brief Maximum number of pending commands. */
    std::size_t capacity_{0};

    /** @brief Pending commands in FIFO order. */
    std::deque<RoomCommand> commands_{};

    /** @brief Protects queue state. */
    mutable std::mutex mutex_{};

    /** @brief Notifies consumers when commands become available. */
    std::condition_variable notEmpty_{};

    /** @brief Notifies producers when capacity becomes available. */
    std::condition_variable notFull_{};

    /** @brief Permanent queue closure state. */
    bool closed_{false};
  };

} // namespace vix::realtime::internal

#endif // VIX_REALTIME_INTERNAL_COMMAND_QUEUE_HPP
