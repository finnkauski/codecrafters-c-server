#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 4221
#define BUFFER_SIZE 1024

#define SUCCESS_200 "HTTP/1.1 200 OK\r\n\r\n";

// Example Request:
// GET /index.html HTTP/1.1\r\n
// Host: localhost:4221\r\n
// User-Agent: curl/7.64.1\r\n
// Accept: */*\r\n
// \r\n

int main() {
  // Disable output buffering
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  // You can use print statements as follows for debugging, they'll be visible
  // when running tests.
  printf("Logs from your program will appear here!\n");

  // Uncomment this block to pass the first stage

  int server_fd, client_addr_len;
  struct sockaddr_in client_addr;

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    printf("Socket creation failed: %s...\n", strerror(errno));
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    printf("SO_REUSEADDR failed: %s \n", strerror(errno));
    return 1;
  }

  struct sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(PORT),
      .sin_addr = {htonl(INADDR_ANY)},
  };

  if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
    printf("Bind failed: %s \n", strerror(errno));
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    printf("Listen failed: %s \n", strerror(errno));
    return 1;
  }

  printf("Waiting for a client to connect...\n");
  client_addr_len = sizeof(client_addr);

  while (true) {
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                           (unsigned int *)&client_addr_len);
    printf("Client connected\n");

    // Handling of connection
    // tofree = string = strdup("abc,def,ghi");
    // assert(string != NULL);
    char buffer[BUFFER_SIZE];

    int valread = read(client_fd, buffer, BUFFER_SIZE);

    if (valread < 0) {
      printf("Read failed: %s \n", strerror(errno));
      return 1;
    }

    printf("Received:\r\n\"\"\"\r\n%s\"\"\"\r\n", buffer);
    char method[16], path[256];

    sscanf(buffer, "%s %s", method, path);

    printf("METHOD: %s\n", method);
    printf("PATH: %s\n", path);

    char *reply;
    if (strcmp(method, "/") == 0) {
      reply = SUCCESS_200;
    } else {
      reply = "HTTP/1.1 404 NOT FOUND\r\n\r\n";
    }
    int bytes_sent = send(client_fd, &reply, strlen(reply), 0);
  }

  close(server_fd);

  printf("Shutdown...");

  return 0;
}
