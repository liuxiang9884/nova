#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include "nova/concurrency/sp_broadcast_queue.h"

// Data structure for broadcasting
struct MarketData {
  int64_t timestamp;
  int32_t instrument_id;
  double price;
  int32_t volume;
  char side;  // 'B' for bid, 'A' for ask

  void Print() const {
    std::cout << "MarketData: timestamp=" << timestamp
              << ", instrument=" << instrument_id << ", price=" << std::fixed
              << std::setprecision(2) << price << ", volume=" << volume
              << ", side=" << side << std::endl;
  }
};

// Statistics structure
struct ConsumerStats {
  int consumer_id;
  uint64_t messages_processed;
  uint64_t last_position;
  double average_latency_us;

  void Print() const {
    std::cout << "Consumer " << consumer_id << ": processed "
              << messages_processed << " messages, avg latency: " << std::fixed
              << std::setprecision(2) << average_latency_us << " μs"
              << std::endl;
  }
};

// Time utility function
int64_t GetCurrentTimestamp() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::high_resolution_clock::now().time_since_epoch())
      .count();
}

void BroadcastQueueDemo() {
  constexpr size_t QUEUE_CAPACITY = 1024;
  constexpr size_t NUM_CONSUMERS = 3;
  constexpr size_t NUM_MESSAGES = 100;

  // Create broadcast queue
  nova::StaticSPBroadcastQueue<MarketData, QUEUE_CAPACITY> queue;

  // Atomic variables for synchronization
  std::atomic<bool> producer_done{false};
  std::atomic<std::size_t> consumers_ready{0};
  std::vector<ConsumerStats> stats(NUM_CONSUMERS);

  // Initialize consumer statistics
  for (size_t i = 0; i < NUM_CONSUMERS; ++i) {
    stats[i].consumer_id = static_cast<int>(i);
    stats[i].messages_processed = 0;
    stats[i].last_position = 0;
    stats[i].average_latency_us = 0.0;
  }

  // Create consumer threads
  std::vector<std::thread> consumers;
  for (size_t i = 0; i < NUM_CONSUMERS; ++i) {
    consumers.emplace_back(
        [&queue, &producer_done, &consumers_ready, &stats, i]() {
          ConsumerStats& consumer_stat = stats[i];
          uint64_t position = 0;  // Each consumer has its own reading position
          double total_latency = 0.0;

          // Notify main thread that consumer is ready
          consumers_ready++;

          // Consumer logic
          while (!producer_done || queue.HasAvailable(position)) {
            if (queue.HasAvailable(position)) {
              // Get data
              MarketData data = queue.Pop(position);

              // Calculate latency (current time - data generation time)
              int64_t now = GetCurrentTimestamp();
              int64_t latency = now - data.timestamp;

              // Update statistics
              total_latency += latency;
              consumer_stat.messages_processed++;
              consumer_stat.last_position = position;

              // Simulate processing delay, different for each consumer
              std::this_thread::sleep_for(std::chrono::milliseconds(1 + i));

              // Output every 10 messages to avoid excessive output
              if (consumer_stat.messages_processed % 10 == 0) {
                std::cout << "Consumer " << i << " processed: ";
                data.Print();
              }
            } else {
              // Brief sleep when no new data
              std::this_thread::yield();
            }
          }

          // Calculate average latency
          if (consumer_stat.messages_processed > 0) {
            consumer_stat.average_latency_us =
                total_latency / consumer_stat.messages_processed;
          }
        });
  }

  // Wait for all consumers to be ready
  while (consumers_ready < NUM_CONSUMERS) {
    std::this_thread::yield();
  }

  std::cout << "Starting broadcast queue demo: 1 producer, " << NUM_CONSUMERS
            << " consumers, queue capacity=" << QUEUE_CAPACITY << std::endl;

  // Producer thread
  std::thread producer([&queue, &producer_done]() {
    // Generate and broadcast messages
    for (size_t i = 0; i < NUM_MESSAGES; ++i) {
      // Create market data
      MarketData data{
          .timestamp = GetCurrentTimestamp(),
          .instrument_id = static_cast<int32_t>(
              1000 + (i % 5)),  // Cycle through 5 different contracts
          .price = 100.0 + (i % 10) * 0.05,  // Small price fluctuations
          .volume =
              static_cast<int32_t>(10 + (i % 5) * 10),  // Different quantities
          .side = (i % 2 == 0) ? 'B' : 'A'  // Alternate between bid and ask
      };

      // Publish to queue
      queue.Push(data);

      if (i % 10 == 0) {
        std::cout << "Producer published: ";
        data.Print();
      }

      // Simulate production interval
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::cout << "Producer finished publishing " << NUM_MESSAGES << " messages"
              << std::endl;
    producer_done = true;
  });

  // Wait for producer to finish
  producer.join();

  // Wait for all consumers to finish
  for (auto& consumer : consumers) {
    consumer.join();
  }

  // Output statistics
  std::cout << "\nBroadcast queue demo results:" << std::endl;
  for (const auto& stat : stats) {
    stat.Print();
  }

  // Verify all consumers received all messages
  bool all_received = true;
  for (const auto& stat : stats) {
    if (stat.messages_processed != NUM_MESSAGES) {
      all_received = false;
      std::cout << "Warning: Consumer " << stat.consumer_id
                << " only processed " << stat.messages_processed << "/"
                << NUM_MESSAGES << " messages" << std::endl;
    }
  }

  if (all_received) {
    std::cout << "Success: All consumers received all " << NUM_MESSAGES
              << " messages" << std::endl;
  }
}

int main() {
  BroadcastQueueDemo();
  return 0;
}