#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <string>

#define PORT 8080

std::vector<int> clients;
std::mutex clients_mutex;

const std::string MODBUS_IP = "127.0.0.1";
const int MODBUS_PORT = 502;

std::vector<std::string> split(const std::string& str) {
  std::vector<std::string> result;
  std::stringstream ss(str);
  std::string word;

  while (ss >> word) {
    result.push_back(word);
  }
  return result;
}

  int read_modbus_register() {
    std::cout << "=== Starting Modbus read ===" << std::endl;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
      std::cerr << "Socket creation failed" << std::endl;
      return -1;
    }
    std::cout << "Socket created: " << sock << std::endl;

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(MODBUS_PORT);

    if (inet_pton(AF_INET, MODBUS_IP.c_str(), &server_addr.sin_addr) <= 0) {
      std::cerr << "Invalid address" << std::endl;
      close(sock);
      return -1;
    }

    std::cout << "Connecting to " << MODBUS_IP << ":" << MODBUS_PORT << std::endl;

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
      perror("connect failed");
      close(sock);
      return -1;
    }

    std::cout << "Connected!" << std::endl;

  // Modbus TCP запрос (Read Holding Register 0, 1 регистр)
  uint8_t request[12] = {
      0x00, 0x01, // Transaction ID
      0x00, 0x00, // Protocol ID
      0x00, 0x06, // Length
      0x01,       // Unit ID
      0x03,       // Function Code (Read Holding Registers)
      0x00, 0x00, // Start Address = 0
      0x00, 0x01  // Quantity = 1
  };

  send(sock, request, sizeof(request), 0);

  uint8_t response[256];
  int len = read(sock, response, sizeof(response));

  close(sock);

  if (len < 9) {
    std::cerr << "Invalid Modbus response\n";
    return -1;
  }

  // Данные начинаются с 9 байта
  int value = (response[9] << 8) | response[10];
  return value;
}

// Рассылка сообщения всем клиентам
void broadcast(const std::string& message, int sender_socket) {
  std::lock_guard<std::mutex> lock(clients_mutex);

  for (int client : clients) {
    if (client != sender_socket) {
      send(client, message.c_str(), message.size(), 0);
    }
  }
}

void handle_client(int client_socket) {
  char buffer[1024];

  while (true) {
    memset(buffer, 0, sizeof(buffer));
    int bytes = read(client_socket, buffer, sizeof(buffer));

    if (bytes <= 0) {
      std::cout << "Client disconnected\n";
      close(client_socket);

      std::lock_guard<std::mutex> lock(clients_mutex);
      clients.erase(std::remove(clients.begin(), clients.end(), client_socket), clients.end());
      break;
    }

    std::string msg(buffer);
    std::cout << "Message: " << msg;

// Убираем \n
    msg.erase(std::remove(msg.begin(), msg.end(), '\n'), msg.end());

    std::vector<std::string> newMsg = split(msg);

    if (newMsg.empty()) {
      continue;
    }

    if (newMsg[0] == "test") {
      int value = read_modbus_register();

      std::string response;
      if (value >= 0) {
        response = "Modbus register 0 value: " + std::to_string(value) + "\n";
      } else {
        response = "Modbus read error\n";
      }

      send(client_socket, response.c_str(), response.size(), 0);
    }
    else if (newMsg[0] == "read" && newMsg.size() >= 3) {
      int addr = std::stoi(newMsg[1]);
      std::string type = newMsg[2];

      int value = read_modbus_register();
      std::string response = "Read " + type + " " + std::to_string(addr) + " = " + std::to_string(value) + "\n";
      send(client_socket, response.c_str(), response.size(), 0);
    }
    else if (newMsg[0] == "write" && newMsg.size() >= 3) {
      int addr = std::stoi(newMsg[1]);
      int val = std::stoi(newMsg[2]);

      std::string response = "Written " + std::to_string(val) + " to HR " + std::to_string(addr) + "\n";
      send(client_socket, response.c_str(), response.size(), 0);
    }
    else {
      broadcast(msg, client_socket);
    }
  }
}


int server() {
  int server_fd, client_socket;
  struct sockaddr_in address;

  server_fd = socket(AF_INET, SOCK_STREAM, 0);

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  bind(server_fd, (struct sockaddr*)&address, sizeof(address));
  listen(server_fd, 10);

  std::cout << "Chat server started on port " << PORT << std::endl;

  while (true) {
    client_socket = accept(server_fd, nullptr, nullptr);

    {
      std::lock_guard<std::mutex> lock(clients_mutex);
      clients.push_back(client_socket);
    }

    std::thread(handle_client, client_socket).detach();
  }

  return 0;
}