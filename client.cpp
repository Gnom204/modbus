#include <iostream>
#include <thread>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <algorithm>

#define PORT 8080

// Поток для получения сообщенийda
void receive_messages(int sock) {
  char buffer[1024];

  while (true) {
    memset(buffer, 0, sizeof(buffer));
    int bytes = read(sock, buffer, sizeof(buffer));

    if (bytes <= 0) {
      std::cout << "Disconnected from server\n";
      break;
    }

    std::cout << buffer;
  }
}

int client() {
  int sock;
  struct sockaddr_in serv_addr;

  sock = socket(AF_INET, SOCK_STREAM, 0);

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);

  inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

  connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

  std::thread(receive_messages, sock).detach();

  std::string message;

  while (true) {
    std::getline(std::cin, message);
    message += "\n";
    send(sock, message.c_str(), message.size(), 0);
  }

  close(sock);
  return 0;
}