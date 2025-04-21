#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include "nova/concurrency/sp_broadcast_queue.h"
#include "nova/exp/sp_broadcast_queue.h"

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

void StaticBroadcastQueueDemo() {
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

// Demo for dynamically allocated broadcast queue
void BroadcastQueueDemo() {
  constexpr size_t REQUESTED_CAPACITY = 1000;  // Will be rounded up to 1024
  constexpr size_t NUM_CONSUMERS = 3;
  constexpr size_t NUM_MESSAGES = 100;

  std::cout << "\n---------- Broadcast Queue Demo ----------\n" << std::endl;

  // Create dynamic broadcast queue
  nova::SPBroadcastQueue<MarketData> queue(REQUESTED_CAPACITY);

  // Atomic variables for synchronization
  std::atomic<bool> producer_done{false};
  std::atomic<size_t> consumers_ready{0};
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
              // Use Value and manually advance position to demonstrate
              // alternate API
              if (i == 0 && position % 10 == 0) {
                // Consumer 0 occasionally uses Value() + manually advancing
                MarketData data = queue.Value(position);
                position++;

                // Calculate latency
                int64_t now = GetCurrentTimestamp();
                int64_t latency = now - data.timestamp;
                total_latency += latency;
                consumer_stat.messages_processed++;
                consumer_stat.last_position = position;

                // Output sample data
                if (consumer_stat.messages_processed % 10 == 0) {
                  std::cout << "Consumer " << i << " read using Value(): ";
                  data.Print();
                }
              }
              // Use TryPop for consumer 1 to demonstrate that API
              else if (i == 1) {
                MarketData data;
                if (queue.TryPop(position, data)) {
                  // Calculate latency
                  int64_t now = GetCurrentTimestamp();
                  int64_t latency = now - data.timestamp;
                  total_latency += latency;
                  consumer_stat.messages_processed++;
                  consumer_stat.last_position = position;

                  // Output sample data
                  if (consumer_stat.messages_processed % 10 == 0) {
                    std::cout << "Consumer " << i << " read using TryPop(): ";
                    data.Print();
                  }
                }
              }
              // Use Pop for consumer 2 (default behavior)
              else {
                MarketData data = queue.Pop(position);

                // Calculate latency
                int64_t now = GetCurrentTimestamp();
                int64_t latency = now - data.timestamp;
                total_latency += latency;
                consumer_stat.messages_processed++;
                consumer_stat.last_position = position;

                // Output sample data
                if (consumer_stat.messages_processed % 10 == 0) {
                  std::cout << "Consumer " << i << " read using Pop(): ";
                  data.Print();
                }
              }

              // Simulate processing delay, different for each consumer
              std::this_thread::sleep_for(std::chrono::milliseconds(1 + i));
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

  std::cout << "Starting dynamic broadcast queue demo: 1 producer, "
            << NUM_CONSUMERS
            << " consumers, requested capacity=" << REQUESTED_CAPACITY
            << ", actual capacity=" << queue.capacity() << std::endl;

  // Create producer thread
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

      // Sometimes use Emplace instead of Push
      if (i % 3 == 0) {
        // Demonstrate Emplace API
        queue.Emplace(data.timestamp, data.instrument_id, data.price,
                      data.volume, data.side);
        if (i % 10 == 0) {
          std::cout << "Producer emplaced: timestamp=" << data.timestamp
                    << ", instrument=" << data.instrument_id << std::endl;
        }
      } else {
        // Use regular Push API
        queue.Push(data);
        if (i % 10 == 0) {
          std::cout << "Producer pushed: ";
          data.Print();
        }
      }

      // Simulate production interval (faster than in static demo)
      std::this_thread::sleep_for(std::chrono::milliseconds(3));
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
  std::cout << "\nDynamic broadcast queue demo results:" << std::endl;
  for (const auto& stat : stats) {
    stat.Print();
  }

  // Check for queue overflow
  for (const auto& stat : stats) {
    if (stat.messages_processed < NUM_MESSAGES) {
      std::cout << "Warning: Consumer " << stat.consumer_id
                << " only processed " << stat.messages_processed << "/"
                << NUM_MESSAGES
                << " messages - possible overflow or processing issue"
                << std::endl;
    }
  }

  // Check total message count
  uint64_t total_processed = 0;
  for (const auto& stat : stats) {
    total_processed += stat.messages_processed;
  }

  std::cout << "Total processed messages: " << total_processed
            << " (expected: " << NUM_MESSAGES * NUM_CONSUMERS << ")"
            << std::endl;

  if (total_processed == NUM_MESSAGES * NUM_CONSUMERS) {
    std::cout
        << "Success: All messages were properly broadcast to all consumers"
        << std::endl;
  }
}

// Demo for the experimental unified interface broadcast queue
void ExpBroadcastQueueDemo() {
  constexpr size_t QUEUE_CAPACITY = 1024;
  constexpr size_t NUM_CONSUMERS = 3;
  constexpr size_t NUM_MESSAGES = 100;

  std::cout << "\n---------- Experimental Broadcast Queue Demo ----------\n"
            << std::endl;

  // Create static broadcast queue from experimental namespace
  nova::exp::StaticSPBroadcastQueue<MarketData, QUEUE_CAPACITY> static_queue;

  // Create dynamic broadcast queue from experimental namespace
  nova::exp::SPBroadcastQueue<MarketData> dynamic_queue(QUEUE_CAPACITY / 2);

  std::cout << "Created queues with unified interface:" << std::endl;
  std::cout << "  - Static queue capacity: " << static_queue.capacity()
            << std::endl;
  std::cout << "  - Dynamic queue capacity: " << dynamic_queue.capacity()
            << std::endl;

  // Atomic variables for synchronization
  std::atomic<bool> producer_done{false};
  std::atomic<size_t> static_consumers_ready{0};
  std::atomic<size_t> dynamic_consumers_ready{0};

  // Statistics for both queue types
  std::vector<ConsumerStats> static_stats(NUM_CONSUMERS);
  std::vector<ConsumerStats> dynamic_stats(NUM_CONSUMERS);

  // Initialize consumer statistics
  for (size_t i = 0; i < NUM_CONSUMERS; ++i) {
    static_stats[i].consumer_id = static_cast<int>(i);
    static_stats[i].messages_processed = 0;
    static_stats[i].last_position = 0;
    static_stats[i].average_latency_us = 0.0;

    dynamic_stats[i].consumer_id = static_cast<int>(i);
    dynamic_stats[i].messages_processed = 0;
    dynamic_stats[i].last_position = 0;
    dynamic_stats[i].average_latency_us = 0.0;
  }

  // Create static queue consumer threads
  std::vector<std::thread> static_consumers;
  for (size_t i = 0; i < NUM_CONSUMERS; ++i) {
    static_consumers.emplace_back([&static_queue, &producer_done,
                                   &static_consumers_ready, &static_stats,
                                   i]() {
      ConsumerStats& consumer_stat = static_stats[i];
      uint64_t position = 0;  // Each consumer has its own reading position
      double total_latency = 0.0;

      // Notify main thread that consumer is ready
      static_consumers_ready++;

      // Consumer logic
      while (!producer_done || !static_queue.Empty(position)) {
        if (!static_queue.Empty(position)) {
          // Get data
          MarketData data = static_queue.Pop(position);

          // Calculate latency
          int64_t now = GetCurrentTimestamp();
          int64_t latency = now - data.timestamp;

          // Update statistics
          total_latency += latency;
          consumer_stat.messages_processed++;
          consumer_stat.last_position = position;

          // Output every 10 messages to avoid excessive output
          if (consumer_stat.messages_processed % 10 == 0 && i == 0) {
            std::cout << "Static consumer " << i << " processed: ";
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

  // Create dynamic queue consumer threads
  std::vector<std::thread> dynamic_consumers;
  for (size_t i = 0; i < NUM_CONSUMERS; ++i) {
    dynamic_consumers.emplace_back([&dynamic_queue, &producer_done,
                                    &dynamic_consumers_ready, &dynamic_stats,
                                    i]() {
      ConsumerStats& consumer_stat = dynamic_stats[i];
      uint64_t position = 0;  // Each consumer has its own reading position
      double total_latency = 0.0;

      // Notify main thread that consumer is ready
      dynamic_consumers_ready++;

      // Consumer logic
      while (!producer_done || !dynamic_queue.Empty(position)) {
        if (!dynamic_queue.Empty(position)) {
          // Demonstrate unified interface with different methods
          MarketData data;
          if (i == 0) {
            // First consumer uses Pop
            data = dynamic_queue.Pop(position);
          } else {
            // Other consumers use TryPop
            if (!dynamic_queue.TryPop(position, data)) {
              std::this_thread::yield();
              continue;
            }
          }

          // Calculate latency
          int64_t now = GetCurrentTimestamp();
          int64_t latency = now - data.timestamp;

          // Update statistics
          total_latency += latency;
          consumer_stat.messages_processed++;
          consumer_stat.last_position = position;

          // Output every 10 messages to avoid excessive output
          if (consumer_stat.messages_processed % 10 == 0 && i == 0) {
            std::cout << "Dynamic consumer " << i << " processed: ";
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
  while (static_consumers_ready < NUM_CONSUMERS ||
         dynamic_consumers_ready < NUM_CONSUMERS) {
    std::this_thread::yield();
  }

  std::cout
      << "Starting experimental broadcast queue demo with unified interface"
      << std::endl;
  std::cout << "  - " << NUM_CONSUMERS << " consumers for static queue"
            << std::endl;
  std::cout << "  - " << NUM_CONSUMERS << " consumers for dynamic queue"
            << std::endl;

  // Create producer threads - one for each queue type
  std::thread static_producer([&static_queue, &producer_done]() {
    // Generate and broadcast messages
    for (size_t i = 0; i < NUM_MESSAGES; ++i) {
      // Create market data
      MarketData data{.timestamp = GetCurrentTimestamp(),
                      .instrument_id = static_cast<int32_t>(1000 + (i % 5)),
                      .price = 100.0 + (i % 10) * 0.05,
                      .volume = static_cast<int32_t>(10 + (i % 5) * 10),
                      .side = (i % 2 == 0) ? 'B' : 'A'};

      // Publish to static queue
      static_queue.Push(data);

      if (i % 20 == 0) {
        std::cout << "Static producer published: ";
        data.Print();
      }

      // Simulate production interval
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::cout << "Static producer finished publishing " << NUM_MESSAGES
              << " messages" << std::endl;
  });

  std::thread dynamic_producer([&dynamic_queue, &producer_done]() {
    // Generate and broadcast messages with slight delay to differentiate from
    // static
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    for (size_t i = 0; i < NUM_MESSAGES; ++i) {
      // Create market data
      MarketData data{.timestamp = GetCurrentTimestamp(),
                      .instrument_id = static_cast<int32_t>(
                          2000 + (i % 5)),  // Different range to distinguish
                      .price = 200.0 + (i % 10) * 0.05,
                      .volume = static_cast<int32_t>(20 + (i % 5) * 10),
                      .side = (i % 2 == 0) ? 'B' : 'A'};

      // Use Emplace sometimes to demonstrate unified interface
      if (i % 3 == 0) {
        dynamic_queue.Emplace(data.timestamp, data.instrument_id, data.price,
                              data.volume, data.side);

        if (i % 20 == 0) {
          std::cout << "Dynamic producer emplaced item " << i << std::endl;
        }
      } else {
        // Publish to dynamic queue
        dynamic_queue.Push(data);

        if (i % 20 == 0) {
          std::cout << "Dynamic producer published: ";
          data.Print();
        }
      }

      // Simulate production interval
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::cout << "Dynamic producer finished publishing " << NUM_MESSAGES
              << " messages" << std::endl;
    producer_done = true;  // Signal all consumers to finish when done
  });

  // Wait for producers to finish
  static_producer.join();
  dynamic_producer.join();

  // Wait for all consumers to finish
  for (auto& consumer : static_consumers) {
    consumer.join();
  }

  for (auto& consumer : dynamic_consumers) {
    consumer.join();
  }

  // Output statistics
  std::cout << "\nExperimental static broadcast queue results:" << std::endl;
  for (const auto& stat : static_stats) {
    stat.Print();
  }

  std::cout << "\nExperimental dynamic broadcast queue results:" << std::endl;
  for (const auto& stat : dynamic_stats) {
    stat.Print();
  }

  // Check if all messages were received
  bool all_static_received = true;
  bool all_dynamic_received = true;

  for (const auto& stat : static_stats) {
    if (stat.messages_processed != NUM_MESSAGES) {
      all_static_received = false;
      std::cout << "Warning: Static consumer " << stat.consumer_id
                << " only processed " << stat.messages_processed << "/"
                << NUM_MESSAGES << " messages" << std::endl;
    }
  }

  for (const auto& stat : dynamic_stats) {
    if (stat.messages_processed != NUM_MESSAGES) {
      all_dynamic_received = false;
      std::cout << "Warning: Dynamic consumer " << stat.consumer_id
                << " only processed " << stat.messages_processed << "/"
                << NUM_MESSAGES << " messages" << std::endl;
    }
  }

  if (all_static_received && all_dynamic_received) {
    std::cout
        << "Success: All consumers received all messages from both queue types"
        << std::endl;
  }
}

int main() {
  StaticBroadcastQueueDemo();
  BroadcastQueueDemo();
  ExpBroadcastQueueDemo();  // Add the experimental demo
  return 0;
}