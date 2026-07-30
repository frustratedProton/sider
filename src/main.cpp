#include "server.hpp"

int main() {
  TcpServer server{6379};
  server.start();

  return 0;
}