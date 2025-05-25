//
// SPSCQueue Demo
//

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
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
  std::cout << "\n=== SPSCQueue Ping-Pong Test ===\n" << std::endl;

  // Create two queues, A->B and B->A
  nova::static_impl::SPSCQueue<int64_t, 16> queue_a_to_b;
  nova::static_impl::SPSCQueue<int64_t, 16> queue_b_to_a;

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
  std::cout << "\n=== SPSCQueue Producer-Consumer Pattern ===\n" << std::endl;

  nova::static_impl::SPSCQueue<TradeData, 32> trade_queue;

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

// Demo 3: Task class for TaskProcessor implementation
class Task {
 public:
  // Default constructor creates an empty task
  Task() = default;

  // Create task from any callable object
  template <typename F,
            typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, Task>>>
  Task(F&& f)
      : impl_(
            std::make_shared<TaskModel<std::decay_t<F>>>(std::forward<F>(f))) {}

  // Move constructor
  Task(Task&& other) noexcept = default;

  // Move assignment
  Task& operator=(Task&& other) noexcept = default;

  // Copy constructor and assignment (needed for SPSCQueue)
  Task(const Task&) = default;
  Task& operator=(const Task&) = default;

  // Check if task is valid
  explicit operator bool() const noexcept {
    return static_cast<bool>(impl_);
  }

  // Execute the task
  void operator()() {
    if (impl_) {
      impl_->execute();
    }
  }

 private:
  // Abstract interface for task concept
  struct TaskConcept {
    virtual ~TaskConcept() = default;
    virtual void execute() = 0;
  };

  // Concrete implementation for specific function types
  template <typename F>
  struct TaskModel : TaskConcept {
    explicit TaskModel(F&& f) : func_(std::forward<F>(f)) {}
    void execute() override {
      func_();
    }
    F func_;
  };

  // Store the task implementation with shared pointer to allow copying
  std::shared_ptr<TaskConcept> impl_;
};

// Demo 3: TaskProcessor class that uses SPSCQueue to pass tasks between threads
class TaskProcessor {
 public:
  // Constructor - creates a queue and starts a consumer thread
  explicit TaskProcessor(std::size_t capacity = 1024)
      : queue_(capacity), running_(true) {
    consumer_thread_ = std::thread(&TaskProcessor::process_tasks, this);
    consumer_id_ = consumer_thread_.get_id();
  }

  // Destructor - stops processor and joins thread
  ~TaskProcessor() {
    shutdown();
    if (consumer_thread_.joinable()) {
      consumer_thread_.join();
    }
  }

  // Non-copyable and non-movable
  TaskProcessor(const TaskProcessor&) = delete;
  TaskProcessor& operator=(const TaskProcessor&) = delete;
  TaskProcessor(TaskProcessor&&) = delete;
  TaskProcessor& operator=(TaskProcessor&&) = delete;

  // Submit a task to the queue, returns true if successful
  template <typename F>
  bool submit(F&& task) {
    return queue_.TryPush(Task(std::forward<F>(task)));
  }

  // Submit a task, blocking until it succeeds
  template <typename F>
  void submit_blocking(F&& task) {
    Task t(std::forward<F>(task));
    while (!queue_.TryPush(t)) {
      std::this_thread::yield();
    }
  }

  // Shutdown the processor
  void shutdown() {
    running_.store(false, std::memory_order_release);
  }

  // Get the consumer thread ID
  std::thread::id get_consumer_id() const {
    return consumer_id_;
  }

 private:
  // Task processing loop
  void process_tasks() {
    while (running_.load(std::memory_order_acquire)) {
      Task* task = queue_.Front();
      if (task) {
        (*task)();
        queue_.Pop();
      } else {
        std::this_thread::yield();
      }
    }
  }

 private:
  // Task queue
  nova::SPSCQueue<Task> queue_;
  std::thread consumer_thread_;
  std::thread::id consumer_id_;
  std::atomic<bool> running_;
};

// Demo 3: Task scheduling between threads using SPSCQueue
void TaskProcessorDemo() {
  std::cout << "\n=== SPSCQueue Task Processor Demo ===\n" << std::endl;

  // Create a task processor with capacity 1024
  TaskProcessor processor(1024);

  std::cout << "Main thread ID: " << std::this_thread::get_id() << std::endl;
  std::cout << "Consumer thread ID: " << processor.get_consumer_id()
            << std::endl;
  std::cout << "Submitting 10 numbered tasks..." << std::endl;

  // Submit 10 tasks
  for (int i = 0; i < 10; ++i) {
    processor.submit([i]() {
      std::cout << "Executing task " << i
                << " on thread ID: " << std::this_thread::get_id() << std::endl;

      // Simulate different processing times
      std::this_thread::sleep_for(std::chrono::milliseconds(100 + i * 20));

      std::cout << "Completed task " << i << std::endl;
    });

    // Short pause between submissions
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  // Submit a calculation task that will return a result through atomic variable
  std::atomic<int> result{0};
  processor.submit_blocking([&result]() {
    std::cout << "Executing calculation task..." << std::endl;

    // Simulate complex calculation
    int sum = 0;
    for (int i = 1; i <= 100; ++i) {
      sum += i;
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Store result
    result.store(sum, std::memory_order_relaxed);
    std::cout << "Calculation complete, sum = " << sum << std::endl;
  });

  // Wait for all tasks to complete
  std::cout << "Waiting for all tasks to complete..." << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(3));

  // Print calculation result
  std::cout << "Calculation result: " << result.load() << std::endl;

  // Submit a verification task that depends on previous calculation
  processor.submit([calculated = result.load()]() {
    std::cout << "Verifying calculated sum " << calculated << "..."
              << std::endl;
    int expected = (100 * 101) / 2;  // Gauss formula n(n+1)/2
    if (calculated == expected) {
      std::cout << "Verification successful!" << std::endl;
    } else {
      std::cout << "Verification failed! Expected: " << expected << std::endl;
    }
  });

  // Wait for final task to complete
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Shutdown the task processor
  std::cout << "Shutting down task processor..." << std::endl;
  processor.shutdown();

  std::cout << "Task Processor demo completed" << std::endl;
}

int main() {
  std::cout << "SPSCQueue Demo" << std::endl;

  // Run Demo 1: Ping-Pong test
  PingPongDemo();

  // Run Demo 2: Producer-Consumer pattern
  ProducerConsumerDemo();

  // Run Demo 3: Task Processor
  TaskProcessorDemo();

  return 0;
}
