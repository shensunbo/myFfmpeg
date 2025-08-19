#pragma once
#include <iostream>
#include <vector>

template <typename  T>
class CircularBuffer {
private:
  std::vector<T> buffer;
  size_t head = 0;
  size_t tail = 0;
  const size_t capacity;

public:
  CircularBuffer(size_t capacity) : buffer(capacity), capacity(capacity) {}

  bool isEmpty() const { return head == tail; }
  bool isFull() const { return (head + 1) % capacity == tail; }

  void enqueue(const T& pkt) {
    if (isFull()) {
      tail = (tail + 1) % capacity; // Overwrite the oldest element
    }
    buffer[head] = pkt;
    head = (head + 1) % capacity;
  }

  bool dequeue(T& pkt) {
    if (isEmpty()) return false;
    pkt = buffer[tail];
    tail = (tail + 1) % capacity;
    return true;
  }
    size_t size() const { return (head - tail + capacity) % capacity; }
};