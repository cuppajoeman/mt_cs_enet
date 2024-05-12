#include <enet.h>
#include <functional>
#include <stdio.h>
#include <thread>

int client_counter = 0;

struct InputSnapshot {
  int mock_data;
};

int main() {
  if (enet_initialize() != 0) {
    fprintf(stderr, "An error occurred while initializing ENet.\n");
    return EXIT_FAILURE;
  }

  ENetHost *client = {0};
  client = enet_host_create(NULL /* create a client host */,
                            1 /* only allow 1 outgoing connection */,
                            2 /* allow up 2 channels to be used, 0 and 1 */,
                            0 /* assume any amount of incoming bandwidth */,
                            0 /* assume any amount of outgoing bandwidth */);
  if (client == NULL) {
    fprintf(stderr,
            "An error occurred while trying to create an ENet client host.\n");
    exit(EXIT_FAILURE);
  }

  ENetAddress address;
  ENetEvent event;
  ENetPeer *server_peer;

  /* Connect to some.server.net:1234. */
  enet_address_set_host(&address, "127.0.0.1");
  address.port = 7777;

  /* Initiate the connection, allocating the two channels 0 and 1. */
  server_peer = enet_host_connect(client, &address, 2, 0);
  if (server_peer == NULL) {
    fprintf(stderr, "No available peers for initiating an ENet connection.\n");
    exit(EXIT_FAILURE);
  }

  /* Wait up to 5 seconds for the connection attempt to succeed. */
  if (enet_host_service(client, &event, 5000) > 0 &&
      event.type == ENET_EVENT_TYPE_CONNECT) {
    puts("Connection to some.server.net:1234 succeeded.");
  } else {
    /* Either the 5 seconds are up or a disconnect event was */
    /* received. Reset the peer in the event the 5 seconds   */
    /* had run out without any significant event.            */
    enet_peer_reset(server_peer);
    puts("Connection to some.server.net:1234 failed.");
  }

  // Receive some events
  enet_host_service(client, &event, 5000);

  std::function<void()> send_loop = [client, server_peer]() {
    while (true) {
      InputSnapshot input_snapshot;
      input_snapshot.mock_data = client_counter;
      ENetPacket *packet =
          enet_packet_create(&input_snapshot, sizeof(InputSnapshot), 0);
      enet_peer_send(server_peer, 0, packet);
      enet_host_flush(client);
      client_counter += 1;
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  };

  std::function<void()> receive_loop = [client]() {
    while (true) {
      ENetEvent event;

      /* Wait up to 1000 milliseconds for an event. */
      while (enet_host_service(client, &event, 500) > 0) {
        switch (event.type) {

        case ENET_EVENT_TYPE_RECEIVE:
          if (event.packet->dataLength == sizeof(int)) {
            int server_num_processed_inputs =
                *reinterpret_cast<int *>(event.packet->data);
            printf(
                "Just got that the server has processed %d input snapshots.\n",
                server_num_processed_inputs);
          }
          /* Clean up the packet now that we're done using it. */
          enet_packet_destroy(event.packet);

          break;

        case ENET_EVENT_TYPE_DISCONNECT:
          printf("%s disconnected.\n", event.peer->data);
          /* Reset the peer's client information. */
          event.peer->data = NULL;
        }
      }
    }
  };

  std::thread receive_thread(receive_loop);
  receive_thread.detach();

  std::thread send_thread(send_loop);
  send_thread.join(); // don't continue the code until this finishes, aka don't
                      // continue on until the disconnect.

  // Disconnect
  enet_peer_disconnect(server_peer, 0);

  uint8_t disconnected = false;
  /* Allow up to 3 seconds for the disconnect to succeed
   * and drop any packets received packets.
   */
  while (enet_host_service(client, &event, 3000) > 0) {
    switch (event.type) {
    case ENET_EVENT_TYPE_RECEIVE:
      enet_packet_destroy(event.packet);
      break;
    case ENET_EVENT_TYPE_DISCONNECT:
      puts("Disconnection succeeded.");
      disconnected = true;
      break;
    }
  }

  // Drop connection, since disconnection didn't successed
  if (!disconnected) {
    enet_peer_reset(server_peer);
  }

  enet_host_destroy(client);
  enet_deinitialize();
}
