//
// StaticSPSCQueue Demo
//

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

#include "nova/concurrency/spsc_queue.h"

// Demo 1: Stats structure for Ping-Pong test
struct PingPongStats {
  uint64_t iterations;
  double avg_latency_ns;
  double min_latency_ns;
  double max_latency_ns;
  double ops_per_sec;
};

// Demo 2: Custom data structure for trading
struct TradeData {
  int64_t timestamp;
  int32_t instrument_id;
  double price;
  int32_t quantity;
  char side;  // 'B' for buy, 'S' for sell

  // Print trade data
  void Print() const {
    std::cout << "Trade: "
              << "timestamp=" << timestamp
              << ", instrument_id=" << instrument_id << ", price=" << std::fixed
              << std::setprecision(2) << price << ", quantity=" << quantity
              << ", side=" << side << std::endl;
  }
};

// Demo 1: Ping-Pong test - passing int64_t between two threads
void PingPongDemo() {
  std::cout << "\n=== StaticSPSCQueue Ping-Pong Test ===\n" << std::endl;

  // Create two queues, A->B and B->A
  nova::StaticSPSCQueue<int64_t, 16> queue_a_to_b;
  nova::StaticSPSCQueue<int64_t, 16> queue_b_to_a;

  const int ITERATIONS = 1000000;
  std::atomic<bool> done(false);
  PingPongStats stats = {0, 0.0, std::numeric_limits<double>::max(), 0.0, 0.0};

  std::thread thread_a([&]() {
    // Warm-up phase
    for (int i = 0; i < 1000; ++i) {
      queue_a_to_b.Emplace(i);
      int64_t response;
      while (!queue_b_to_a.TryPop(response)) {
        std::this_thread::yield();
      }
    }

    // Measurement phase
    std::vector<double> latencies;
    latencies.reserve(ITERATIONS);

    auto start_time_total = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; ++i) {
      auto start_time = std::chrono::high_resolution_clock::now();

      // Send message to B
      queue_a_to_b.Emplace(i);

      // Wait for B's response
      int64_t response;
      while (!queue_b_to_a.TryPop(response)) {
        std::this_thread::yield();
      }

      // Verify correct response
      if (response != i) {
        std::cerr << "Error: Received mismatched response: expected=" << i
                  << ", actual=" << response << std::endl;
      }

      auto end_time = std::chrono::high_resolution_clock::now();
      double latency =
          std::chrono::duration<double, std::nano>(end_time - start_time)
              .count();
      latencies.push_back(latency);
    }

    auto end_time_total = std::chrono::high_resolution_clock::now();
    double total_time_sec =
        std::chrono::duration<double>(end_time_total - start_time_total)
            .count();

    // Calculate statistics
    stats.iterations = ITERATIONS;
    stats.ops_per_sec = ITERATIONS / total_time_sec;
    double total = 0.0;
    for (double latency : latencies) {
      total += latency;
      stats.min_latency_ns = std::min(stats.min_latency_ns, latency);
      stats.max_latency_ns = std::max(stats.max_latency_ns, latency);
    }
    stats.avg_latency_ns = total / ITERATIONS;

    done.store(true, std::memory_order_release);
  });

  // Thread B - Responder
  std::thread thread_b([&]() {
    while (!done.load(std::memory_order_acquire)) {
      int64_t request;
      if (queue_a_to_b.TryPop(request)) {
        // Received message from A, respond immediately
        queue_b_to_a.Emplace(request);
      } else {
        std::this_thread::yield();
      }
    }
  });

  thread_a.join();
  thread_b.join();

  // Print results
  std::cout << "Ping-Pong Test Completed:" << std::endl;
  std::cout << "  Iterations: " << stats.iterations << std::endl;
  std::cout << "  Average latency: " << std::fixed << std::setprecision(2)
            << stats.avg_latency_ns << " ns" << std::endl;
  std::cout << "  Minimum latency: " << std::fixed << std::setprecision(2)
            << stats.min_latency_ns << " ns" << std::endl;
  std::cout << "  Maximum latency: " << std::fixed << std::setprecision(2)
            << stats.max_latency_ns << " ns" << std::endl;
  std::cout << "  Operations per second: " << std::fixed << std::setprecision(2)
            << stats.ops_per_sec << " ops/sec" << std::endl;
}

// Demo 2: Producer-Consumer pattern - one thread writing, one thread reading
void ProducerConsumerDemo() {
  std::cout << "\n=== StaticSPSCQueue Producer-Consumer Pattern ===\n"
            << std::endl;

  nova::StaticSPSCQueue<TradeData, 32> trade_queue;

  constexpr int NUM_TRADES = 100;
  std::atomic<bool> producer_done(false);
  std::atomic<int> trades_processed(0);

  // Producer thread - generates trade data
  std::thread producer([&]() {
    std::cout << "Producer: Starting to generate trade data..." << std::endl;

    for (int i = 0; i < NUM_TRADES; ++i) {
      TradeData trade{};
      trade.timestamp =
          std::chrono::system_clock::now().time_since_epoch().count();
      trade.instrument_id = 1000 + (i % 10);  // 10 different instruments
      trade.price = 100.0 + (i % 50) * 0.5;   // Price between 100-125
      trade.quantity = 1 + (i % 10) * 10;     // Quantity between 10-100
      trade.side = (i % 2 == 0) ? 'B' : 'S';  // Alternate buy/sell

      // Add trade to the queue
      while (!trade_queue.TryEmplace(trade)) {
        std::this_thread::yield();  // Wait if queue is full
      }

      // Simulate time interval between trades
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "Producer: Finished generating " << NUM_TRADES << " trades"
              << std::endl;
    producer_done.store(true, std::memory_order_release);
  });

  // Consumer thread - processes trade data
  std::thread consumer([&]() {
    std::cout << "Consumer: Starting to process trade data..." << std::endl;

    while (true) {
      TradeData* trade = trade_queue.Front();

      if (trade) {
        // Process trade data
        trade->Print();
        trade_queue.Pop();
        trades_processed.fetch_add(1, std::memory_order_relaxed);

        // Simulate processing time
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
      } else if (producer_done.load(std::memory_order_acquire)) {
        // Check if producer is done and queue is empty
        if (trades_processed.load(std::memory_order_relaxed) >= NUM_TRADES) {
          break;
        }
        std::this_thread::yield();
      } else {
        // Wait for more data
        std::this_thread::yield();
      }
    }

    std::cout << "Consumer: Finished processing " << trades_processed.load()
              << " trades" << std::endl;
  });

  producer.join();
  consumer.join();
}

int main() {
  std::cout << "StaticSPSCQueue Demo" << std::endl;

  // Run Demo 1: Ping-Pong test
  PingPongDemo();

  // Run Demo 2: Producer-Consumer pattern
  ProducerConsumerDemo();

  return 0;
}
