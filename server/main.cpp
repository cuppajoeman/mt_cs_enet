#include <condition_variable>
#include <enet.h>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <stdio.h>
#include <thread>

int num_input_snapshots_processed = 0;

struct InputSnapshot {
  int mock_data;
};

template <typename T> class ThreadSafeQueue {
public:
  // Default constructor
  ThreadSafeQueue() = default;

  // Delete copy constructor and assignment operator for safety
  ThreadSafeQueue(const ThreadSafeQueue &other) = delete;
  ThreadSafeQueue &operator=(const ThreadSafeQueue &other) = delete;

  // Push an item onto the queue
  void push(const T &item) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(item);
    cond_var_.notify_one();
  }

  // Pop an item from the queue
  T pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_var_.wait(lock, [this] { return !queue_.empty(); });
    T item = queue_.front();
    queue_.pop();
    return item;
  }

  // Return the front item without removing it
  T front() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
      throw std::runtime_error("Queue is empty");
    }
    return queue_.front();
  }

  // Check if the queue is empty
  bool empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
  }

  // Get the current size of the queue
  size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

private:
  std::queue<T> queue_;
  mutable std::mutex mutex_;
  std::condition_variable cond_var_;
};

int main() {
  if (enet_initialize() != 0) {
    printf("An error occurred while initializing ENet.\n");
    return 1;
  }

  ENetAddress address;

  address.host = ENET_HOST_ANY; /* Bind the server to the default localhost. */
  address.port = 7777;          /* Bind the server to port 7777. */

#define MAX_CLIENTS 32

  /* create a server */
  ENetHost *server = enet_host_create(&address, MAX_CLIENTS, 2, 0, 0);

  if (server == NULL) {
    printf("An error occurred while trying to create an ENet server host.\n");
    return 1;
  }

  printf("Started a server...\n");

  ThreadSafeQueue<InputSnapshot> input_queue;

  std::function<void()> receive_loop = [&server, &input_queue]() {
    while (true) {
      ENetEvent event;
      /* Wait up to 1000 milliseconds for an event. (WARNING: blocking) */
      while (enet_host_service(server, &event, 1000) > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
          printf("A new client connected from %x:%u.\n",
                 event.peer->address.host, event.peer->address.port);
          /* Store any relevant client information here. */
          // event.peer->data = "Client information";
          break;

        case ENET_EVENT_TYPE_RECEIVE:
          if (event.packet->dataLength == sizeof(InputSnapshot)) {
            InputSnapshot input_snapshot =
                *reinterpret_cast<InputSnapshot *>(event.packet->data);
            input_queue.push(input_snapshot);
          }
          /* Clean up the packet now that we're done using it. */
          enet_packet_destroy(event.packet);
          break;

        case ENET_EVENT_TYPE_DISCONNECT:
          printf("%s disconnected.\n", event.peer->data);
          /* Reset the peer's client information. */
          event.peer->data = NULL;
          break;

        case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
          printf("%s disconnected due to timeout.\n", event.peer->data);
          /* Reset the peer's client information. */
          event.peer->data = NULL;
          break;

        case ENET_EVENT_TYPE_NONE:
          break;
        }
      }
    }
  };

  std::function<void()> process_loop = [&input_queue]() {
    while (true) {
      while (!input_queue.empty()) {
        InputSnapshot popped_input_snapshot = input_queue.pop();
        // do stuff
        num_input_snapshots_processed += 1;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  };

  std::function send_loop = [&server]() {
    while (true) {
      ENetPacket *packet =
          enet_packet_create(&num_input_snapshots_processed, sizeof(int), 0);
      enet_host_broadcast(server, 0, packet);
      enet_host_flush(server);
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  };

  std::thread receive_thread(receive_loop);
  receive_thread.detach();

  std::thread process_thread(process_loop);
  process_thread.detach();

  std::thread send_thread(send_loop);
  send_thread.join(); // don'ut continue on, aka, don't destroy the server yet.

  enet_host_destroy(server);
  enet_deinitialize();
  return 0;
}
