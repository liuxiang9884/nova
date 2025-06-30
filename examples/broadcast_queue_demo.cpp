#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "nova/concurrency/broadcast_queue.h"

// Define message types
enum class MessageType : uint32_t {
  INT_MSG = 1,
  STRING_MSG = 2,
  LARGE_DATA_MSG = 3,
  ALIGNED_MSG = 4
};

using BroadcastQueue =
    nova::static_impl::FlexibleSPBroadcastQueue<MessageType, 1024>;

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

struct alignas(32) AlignedStruct {
  double data[4];
  AlignedStruct(double w, double x, double y, double z) : data{w, x, y, z} {}
};

void BasicDemo() {
  std::cout << "=== Basic FlexibleSPBroadcastQueue Demo ===\n";

  BroadcastQueue queue;
  std::atomic<bool> producer_done{false};

  // Producer thread
  std::thread producer([&queue, &producer_done]() {
    std::cout << "Producer: Starting to produce messages...\n";

    for (int i = 0; i < 5; ++i) {
      // Emplace different types
      auto& msg = queue.Emplace<Message>(MessageType::STRING_MSG, i,
                                         "Hello " + std::to_string(i));
      std::cout << "Producer: Emplaced Message " << msg.id << "\n";

      auto& num = queue.Emplace<int>(MessageType::INT_MSG, i * 100);
      std::cout << "Producer: Emplaced int " << num << "\n";

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    producer_done = true;
    std::cout << "Producer: Finished\n";
  });

  // Consumer thread
  std::thread consumer([&queue, &producer_done]() {
    std::cout << "Consumer: Starting to consume...\n";

    // Wait for first message to be available
    size_t read_pos = queue.GetCurrentWritePos();
    int messages_read = 0;

    while (!producer_done || messages_read < 10) {
      if (auto entry_info = queue.TryRead(read_pos)) {
        switch (entry_info->header->type) {
          case MessageType::STRING_MSG: {
            auto* msg = queue.Get<Message>(*entry_info);
            std::cout << "Consumer: Read Message id=" << msg->id
                      << ", content=" << msg->content << "\n";
            ++messages_read;
            break;
          }
          case MessageType::INT_MSG: {
            auto* num = queue.Get<int>(*entry_info);
            std::cout << "Consumer: Read int " << *num << "\n";
            ++messages_read;
            break;
          }
          default:
            std::cout << "Consumer: Unknown message type\n";
            ++messages_read;
            break;
        }
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
  nova::static_impl::FlexibleSPBroadcastQueue<MessageType, 256> small_queue;

  std::atomic<bool> producer_done{false};

  // Producer: fill buffer and cause wrap-around
  std::thread producer([&small_queue, &producer_done]() {
    std::cout << "Producer: Filling small buffer to trigger wrap-around...\n";

    for (int i = 0; i < 20; ++i) {
      auto& data =
          small_queue.Emplace<LargeData>(MessageType::LARGE_DATA_MSG, i);
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
      if (auto entry_info = small_queue.TryRead(read_pos)) {
        if (entry_info->header->type == MessageType::LARGE_DATA_MSG) {
          auto* data = small_queue.Get<LargeData>(*entry_info);
          std::cout << "Consumer: Read LargeData " << data->sequence
                    << " (read_pos now: " << read_pos;

          // Check if wrap-around occurred
          if (entry_info->entry_pos == 0 && items_read > 0) {
            std::cout << " - WRAP-AROUND DETECTED!";
          }
          std::cout << ")\n";
          ++items_read;
        }
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }

    std::cout << "Consumer: Finished reading " << items_read << " items\n";
  });

  producer.join();
  consumer.join();
}

void MultipleTypesDemo() {
  std::cout << "\n=== Multiple Types Demo ===\n";

  BroadcastQueue queue;
  std::atomic<bool> producer_done{false};

  // Producer with mixed types
  std::thread producer([&queue, &producer_done]() {
    std::cout << "Producer: Starting mixed type production...\n";

    for (int i = 0; i < 12; ++i) {
      switch (i % 4) {
        case 0: {
          auto& msg = queue.Emplace<Message>(MessageType::STRING_MSG, i,
                                             "Mixed " + std::to_string(i));
          std::cout << "Producer: Emplaced Message " << msg.id << "\n";
          break;
        }
        case 1: {
          auto& num = queue.Emplace<int>(MessageType::INT_MSG, i * 10);
          std::cout << "Producer: Emplaced int " << num << "\n";
          break;
        }
        case 2: {
          auto& data = queue.Emplace<LargeData>(MessageType::LARGE_DATA_MSG, i);
          std::cout << "Producer: Emplaced LargeData " << data.sequence << "\n";
          break;
        }
        case 3: {
          auto& aligned = queue.Emplace<AlignedStruct>(
              MessageType::ALIGNED_MSG, i * 1.0, i * 2.0, i * 3.0, i * 4.0);
          std::cout << "Producer: Emplaced AlignedStruct with data[0]="
                    << aligned.data[0] << "\n";
          break;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }

    producer_done = true;
    std::cout << "Producer: Finished mixed production\n";
  });

  // Consumer with type dispatching
  std::thread consumer([&queue, &producer_done]() {
    std::cout << "Consumer: Starting mixed type consumption...\n";

    auto read_pos = queue.GetCurrentWritePos();
    int messages_read = 0;
    int type_counts[5] = {0};  // Count by message type

    while (!producer_done || messages_read < 12) {
      if (auto entry_info = queue.TryRead(read_pos)) {
        switch (entry_info->header->type) {
          case MessageType::STRING_MSG: {
            auto* msg = queue.Get<Message>(*entry_info);
            std::cout << "Consumer: [STRING] Message id=" << msg->id
                      << ", content=" << msg->content << "\n";
            type_counts[1]++;
            break;
          }
          case MessageType::INT_MSG: {
            auto* num = queue.Get<int>(*entry_info);
            std::cout << "Consumer: [INT] Value=" << *num << "\n";
            type_counts[2]++;
            break;
          }
          case MessageType::LARGE_DATA_MSG: {
            auto* data = queue.Get<LargeData>(*entry_info);
            std::cout << "Consumer: [LARGE] LargeData sequence="
                      << data->sequence << ", values[0]=" << data->values[0]
                      << "\n";
            type_counts[3]++;
            break;
          }
          case MessageType::ALIGNED_MSG: {
            auto* aligned = queue.Get<AlignedStruct>(*entry_info);
            std::cout << "Consumer: [ALIGNED] AlignedStruct data=["
                      << aligned->data[0] << ", " << aligned->data[1] << ", "
                      << aligned->data[2] << ", " << aligned->data[3] << "]\n";
            type_counts[4]++;
            break;
          }
          default:
            std::cout << "Consumer: Unknown message type: "
                      << static_cast<uint32_t>(entry_info->header->type)
                      << "\n";
            break;
        }
        ++messages_read;
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }

    std::cout << "Consumer: Finished reading " << messages_read
              << " messages\n";
    std::cout << "  - STRING_MSG: " << type_counts[1] << "\n";
    std::cout << "  - INT_MSG: " << type_counts[2] << "\n";
    std::cout << "  - LARGE_DATA_MSG: " << type_counts[3] << "\n";
    std::cout << "  - ALIGNED_MSG: " << type_counts[4] << "\n";
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
    std::cout << "Producer: Starting broadcast production...\n";

    for (int i = 0; i < 8; ++i) {
      auto& msg = queue.Emplace<Message>(MessageType::STRING_MSG, i,
                                         "Broadcast " + std::to_string(i));
      std::cout << "Producer: Broadcast Message " << msg.id << "\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
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

    while (!producer_done || count < 8) {
      if (auto entry_info = queue.TryRead(read_pos)) {
        if (entry_info->header->type == MessageType::STRING_MSG) {
          auto* msg = queue.Get<Message>(*entry_info);
          std::cout << "Reader1: Got Message " << msg->id << "\n";
          ++count;
        }
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }

    std::cout << "Reader1: Finished with " << count << " messages\n";
  });

  // Reader 2: starts after delay
  readers.emplace_back([&queue, &producer_done]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    std::cout << "Reader2: Starting after delay\n";

    auto read_pos = queue.GetCurrentWritePos();
    int count = 0;

    while (!producer_done || count < 5) {
      if (auto entry_info = queue.TryRead(read_pos)) {
        if (entry_info->header->type == MessageType::STRING_MSG) {
          auto* msg = queue.Get<Message>(*entry_info);
          std::cout << "Reader2: Got Message " << msg->id << "\n";
          ++count;
        }
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }

    std::cout << "Reader2: Finished with " << count << " messages\n";
  });

  producer.join();
  for (auto& reader : readers) {
    reader.join();
  }
}

void AlignmentDemo() {
  std::cout << "\n=== Alignment Demo ===\n";

  BroadcastQueue queue;

  std::cout << "Sizeof and alignment info:\n";
  std::cout << "  int: sizeof=" << sizeof(int) << ", alignof=" << alignof(int)
            << "\n";
  std::cout << "  Message: sizeof=" << sizeof(Message)
            << ", alignof=" << alignof(Message) << "\n";
  std::cout << "  LargeData: sizeof=" << sizeof(LargeData)
            << ", alignof=" << alignof(LargeData) << "\n";
  std::cout << "  AlignedStruct: sizeof=" << sizeof(AlignedStruct)
            << ", alignof=" << alignof(AlignedStruct) << "\n";

  // Test alignment with different types
  auto& i = queue.Emplace<int>(MessageType::INT_MSG, 42);
  auto& msg =
      queue.Emplace<Message>(MessageType::STRING_MSG, 1, "Aligned test");
  auto& aligned = queue.Emplace<AlignedStruct>(MessageType::ALIGNED_MSG, 1.0,
                                               2.0, 3.0, 4.0);

  std::cout << "Memory addresses (checking alignment):\n";
  std::cout << "  int address: " << &i << " (mod " << alignof(int) << " = "
            << (reinterpret_cast<uintptr_t>(&i) % alignof(int)) << ")\n";
  std::cout << "  Message address: " << &msg << " (mod " << alignof(Message)
            << " = " << (reinterpret_cast<uintptr_t>(&msg) % alignof(Message))
            << ")\n";
  std::cout << "  AlignedStruct address: " << &aligned << " (mod "
            << alignof(AlignedStruct) << " = "
            << (reinterpret_cast<uintptr_t>(&aligned) % alignof(AlignedStruct))
            << ")\n";

  // Read back and verify
  auto read_pos = queue.GetCurrentWritePos();
  while (auto entry_info = queue.TryRead(read_pos)) {
    switch (entry_info->header->type) {
      case MessageType::INT_MSG: {
        auto* ptr = queue.Get<int>(*entry_info);
        std::cout << "Read int: " << *ptr << " at address " << ptr << "\n";
        break;
      }
      case MessageType::STRING_MSG: {
        auto* ptr = queue.Get<Message>(*entry_info);
        std::cout << "Read Message: id=" << ptr->id << " at address " << ptr
                  << "\n";
        break;
      }
      case MessageType::ALIGNED_MSG: {
        auto* ptr = queue.Get<AlignedStruct>(*entry_info);
        std::cout << "Read AlignedStruct: data[0]=" << ptr->data[0]
                  << " at address " << ptr << "\n";
        break;
      }
      default:
        break;
    }
  }
}

int main() {
  std::cout << "FlexibleSPBroadcastQueue Comprehensive Demo\n";
  std::cout << "==========================================\n";

  BasicDemo();
  WrapAroundDemo();
  MultipleTypesDemo();
  MultipleReadersDemo();
  AlignmentDemo();

  std::cout << "\n=== All Demos Completed ===\n";
  return 0;
}