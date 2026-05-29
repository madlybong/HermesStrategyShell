#pragma once
#include <atomic>
#include <optional>
#include <vector>

namespace LockFree {

template <typename T, size_t Capacity = 1024> class SPSCQueue {
public:
  SPSCQueue() : head_(0), tail_(0) {}

  bool push(const T &item) {
    size_t currentTail = tail_.load(std::memory_order_relaxed);
    size_t nextTail = (currentTail + 1) % Capacity;
    if (nextTail == head_.load(std::memory_order_acquire)) {
      return false; // Full
    }
    buffer_[currentTail] = item;
    tail_.store(nextTail, std::memory_order_release);
    return true;
  }

  std::optional<T> pop() {
    size_t currentHead = head_.load(std::memory_order_relaxed);
    if (currentHead == tail_.load(std::memory_order_acquire)) {
      return std::nullopt; // Empty
    }
    T item = buffer_[currentHead];
    size_t nextHead = (currentHead + 1) % Capacity;
    head_.store(nextHead, std::memory_order_release);
    return item;
  }

  bool empty() const {
    return head_.load(std::memory_order_acquire) ==
           tail_.load(std::memory_order_acquire);
  }

private:
  std::vector<T> buffer_{Capacity};
  std::atomic<size_t> head_;
  std::atomic<size_t> tail_;
};

} // namespace LockFree
