#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "nova/concurrency/broadcast_queue.h"

using BroadcastQueue = nova::static_impl::FlexibleSPBroadcastQueue<1024>;

struct Message {
  int id;
  std::string content;
  std::chrono::steady_clock::time_point timestamp;

  Message(int i, std::string c)
      : id(i),
        content(std::move(c)),
        timestamp(std::chrono::steady_clock::now()) {}
};

struct LargeData {
  double values[10];
  int sequence;

  LargeData(int seq) : sequence(seq) {
    for (int i = 0; i < 10; ++i) {
      values[i] = seq * 10.0 + i;
    }
  }
};

void BasicDemo() {
  std::cout << "=== Basic FlexibleSPBroadcastQueue Demo ===\n";

  BroadcastQueue queue;

  // Producer thread
  std::thread producer([&queue]() {
    std::cout << "Producer: Starting to produce messages...\n";

    for (int i = 0; i < 5; ++i) {
      // Emplace different types
      auto& msg = queue.Emplace<Message>(i, "Hello " + std::to_string(i));
      std::cout << "Producer: Emplaced Message " << msg.id << "\n";

      auto& num = queue.Emplace<int>(i * 100);
      std::cout << "Producer: Emplaced int " << num << "\n";

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Producer: Finished\n";
  });

  // Consumer thread
  std::thread consumer([&queue]() {
    std::cout << "Consumer: Starting to consume...\n";

    // Get current write position as starting point
    auto read_pos = queue.GetCurrentWritePos();
    int messages_read = 0;

    while (messages_read < 10) {
      // Try reading Message
      if (auto msg_opt = queue.TryRead<Message>(read_pos)) {
        auto* msg = *msg_opt;
        std::cout << "Consumer: Read Message id=" << msg->id
                  << ", content=" << msg->content << "\n";
        ++messages_read;
      }
      // Try reading int
      else if (auto int_opt = queue.TryRead<int>(read_pos)) {
        auto* num = *int_opt;
        std::cout << "Consumer: Read int " << *num << "\n";
        ++messages_read;
      } else {
        // No new data, wait briefly
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }

    std::cout << "Consumer: Finished reading " << messages_read
              << " messages\n";
  });

  producer.join();
  consumer.join();
}

void WrapAroundDemo() {
  std::cout << "\n=== Wrap-Around Demo ===\n";

  // Use smaller buffer to trigger wrap-around quickly
  nova::static_impl::FlexibleSPBroadcastQueue<256> small_queue;

  std::atomic<bool> producer_done{false};

  // Producer: fill buffer and cause wrap-around
  std::thread producer([&small_queue, &producer_done]() {
    std::cout << "Producer: Filling small buffer to trigger wrap-around...\n";

    for (int i = 0; i < 20; ++i) {
      auto& data = small_queue.Emplace<LargeData>(i);
      std::cout << "Producer: Emplaced LargeData " << data.sequence
                << " (write_pos: " << small_queue.GetCurrentWritePos() << ")\n";

      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    producer_done = true;
    std::cout << "Producer: Finished (final write_pos: "
              << small_queue.GetCurrentWritePos() << ")\n";
  });

  // Consumer: read with wrap-around handling
  std::thread consumer([&small_queue, &producer_done]() {
    std::cout << "Consumer: Starting to read with wrap-around...\n";

    auto read_pos = small_queue.GetCurrentWritePos();
    int items_read = 0;

    while (!producer_done || items_read < 20) {
      if (auto data_opt = small_queue.TryRead<LargeData>(read_pos)) {
        auto* data = *data_opt;
        std::cout << "Consumer: Read LargeData " << data->sequence
                  << " (read_pos now: " << read_pos << ")\n";
        ++items_read;
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }

    std::cout << "Consumer: Finished reading " << items_read << " items\n";
  });

  producer.join();
  consumer.join();
}

void MultipleReadersDemo() {
  std::cout << "\n=== Multiple Readers Demo ===\n";

  BroadcastQueue queue;
  std::atomic<bool> producer_done{false};

  // Single producer
  std::thread producer([&queue, &producer_done]() {
    std::cout << "Producer: Starting continuous production...\n";

    for (int i = 0; i < 15; ++i) {
      auto& msg = queue.Emplace<Message>(i, "Broadcast " + std::to_string(i));
      std::cout << "Producer: Broadcast Message " << msg.id << "\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    producer_done = true;
    std::cout << "Producer: Finished broadcasting\n";
  });

  // Multiple readers starting at different times
  std::vector<std::thread> readers;

  // Reader 1: starts immediately
  readers.emplace_back([&queue, &producer_done]() {
    std::cout << "Reader1: Starting immediately\n";
    auto read_pos = queue.GetCurrentWritePos();
    int count = 0;

    while (!producer_done || count < 15) {
      if (auto msg_opt = queue.TryRead<Message>(read_pos)) {
        auto* msg = *msg_opt;
        std::cout << "Reader1: Got Message " << msg->id << "\n";
        ++count;
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }

    std::cout << "Reader1: Finished with " << count << " messages\n";
  });

  // Reader 2: starts after delay
  readers.emplace_back([&queue, &producer_done]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "Reader2: Starting after delay\n";

    auto read_pos = queue.GetCurrentWritePos();
    int count = 0;

    while (!producer_done || count < 10) {
      if (auto msg_opt = queue.TryRead<Message>(read_pos)) {
        auto* msg = *msg_opt;
        std::cout << "Reader2: Got Message " << msg->id << "\n";
        ++count;
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }

    std::cout << "Reader2: Finished with " << count << " messages\n";
  });

  // Reader 3: starts very late
  readers.emplace_back([&queue, &producer_done]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "Reader3: Starting very late\n";

    auto read_pos = queue.GetCurrentWritePos();
    int count = 0;

    while (!producer_done || count < 5) {
      if (auto msg_opt = queue.TryRead<Message>(read_pos)) {
        auto* msg = *msg_opt;
        std::cout << "Reader3: Got Message " << msg->id << "\n";
        ++count;
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }

    std::cout << "Reader3: Finished with " << count << " messages\n";
  });

  producer.join();
  for (auto& reader : readers) {
    reader.join();
  }
}

void AlignmentDemo() {
  std::cout << "\n=== Alignment Demo ===\n";

  BroadcastQueue queue;

  // Test different alignment requirements
  struct alignas(16) AlignedStruct {
    double a, b;
    AlignedStruct(double x, double y) : a(x), b(y) {}
  };

  struct alignas(32) HighlyAligned {
    double data[4];
    HighlyAligned(double w, double x, double y, double z) : data{w, x, y, z} {}
  };

  std::thread producer([&queue]() {
    std::cout << "Producer: Testing different alignments...\n";

    // Mix different types with different alignment requirements
    queue.Emplace<char>('A');
    queue.Emplace<AlignedStruct>(1.1, 2.2);
    queue.Emplace<int>(42);
    queue.Emplace<HighlyAligned>(1.0, 2.0, 3.0, 4.0);
    queue.Emplace<double>(3.14159);

    std::cout << "Producer: Finished emplacing aligned data\n";
  });

  std::thread consumer([&queue]() {
    auto read_pos = queue.GetCurrentWritePos();
    int items_read = 0;

    while (items_read < 5) {
      if (auto c_opt = queue.TryRead<char>(read_pos)) {
        std::cout << "Consumer: Read char '" << **c_opt << "'\n";
        ++items_read;
      } else if (auto as_opt = queue.TryRead<AlignedStruct>(read_pos)) {
        auto* as = *as_opt;
        std::cout << "Consumer: Read AlignedStruct(" << as->a << ", " << as->b
                  << ")\n";
        ++items_read;
      } else if (auto i_opt = queue.TryRead<int>(read_pos)) {
        std::cout << "Consumer: Read int " << **i_opt << "\n";
        ++items_read;
      } else if (auto ha_opt = queue.TryRead<HighlyAligned>(read_pos)) {
        auto* ha = *ha_opt;
        std::cout << "Consumer: Read HighlyAligned(" << ha->data[0] << ", "
                  << ha->data[1] << ", " << ha->data[2] << ", " << ha->data[3]
                  << ")\n";
        ++items_read;
      } else if (auto d_opt = queue.TryRead<double>(read_pos)) {
        std::cout << "Consumer: Read double " << **d_opt << "\n";
        ++items_read;
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }

    std::cout << "Consumer: Finished reading aligned data\n";
  });

  producer.join();
  consumer.join();
}

int main() {
  std::cout << "FlexibleSPBroadcastQueue Demo\n";
  std::cout << "=============================\n\n";

  try {
    BasicDemo();
    WrapAroundDemo();
    MultipleReadersDemo();
    AlignmentDemo();

    std::cout << "\n🎉 All demos completed successfully!\n";
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}