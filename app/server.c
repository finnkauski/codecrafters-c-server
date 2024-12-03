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

typedef struct {
  char *key;
  char *value;
} Header;

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
    char buffer[BUFFER_SIZE];

    int valread = read(client_fd, buffer, BUFFER_SIZE);

    if (valread < 0) {
      printf("Read failed: %s \n", strerror(errno));
      return 1;
    }

    // Parse headers
    bool first = true;
    int n_headers = 0;
    char method[16], path[256], protocol[16];
    char *token, *string;
    string = &buffer[0];
    Header headers[100];
    while ((token = strsep(&string, "\r\n")) != NULL) {
      if (first) {
        // Parse method, path and protocol.
        sscanf(token, "%s %s %s", method, path, protocol);
        printf("DEBUG: Request:\r\n\"\"\"\r\n%s\"\"\"\r\n", buffer);
        printf("DEBUG: Method: %s\n", method);
        printf("DEBUG: Path: %s\n", path);
        printf("DEBUG: Protocol: %s\n", protocol);
        first = false;
      } else if (strcmp(token, "") != 0) {
        printf("DEBUG: Header \"%s\"", token);
        char *key, *value;
        key = strsep(&token, ": ");
        value = &token[1];
        printf("-> key: %s, value: %s\n", key, value);

        Header header;
        header.key = key;
        header.value = value;
        headers[n_headers++] = header;
      }
    }

    // Echo Path
    char *response;
    if (strncmp(path, "/echo/", 6) == 0) {
      char *echo_string = &path[6];
      printf("DEBUG: Echo string: %s\n", echo_string);
      char response_buffer[BUFFER_SIZE];
      sprintf(response_buffer,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/plain\r\n"
              "Content-Length: %ld\r\n\r\n"
              "%s",
              strlen(echo_string), echo_string);
      response = &response_buffer[0];
    } else if (strcmp(path, "/user-agent") == 0) {
      char *user_agent;
      for (int i = 0; i < n_headers; i++) {
        if (strcmp(headers[i].key, "User-Agent") == 0) {
          user_agent = headers[i].value;
          printf("DEBUG: Found user-agent: %s\n", headers[i].value);
          break;
        }
      }

      char response_buf[100];
      sprintf(response_buf,
              "HTTP/1.1 200 OK\r\nContent-Length: %lu\r\nContent-Type: "
              "text/plain\r\n\r\n%s",
              strlen(user_agent), user_agent);
      response = &response_buf[0];
    } else if (strcmp(path, "/") == 0) {
      char *identifier = "Art Server";
      char response_buf[100];
      sprintf(response_buf,
              "HTTP/1.1 200 OK\r\nContent-Length: %lu\r\nContent-Type: "
              "text/plain\r\n\r\n%s",
              strlen(identifier), identifier);
      response = &response_buf[0];
    } else {
      response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nContent-Type: "
                 "text/plain\r\n\r\n";
    }

    printf("DEBUG: Response:\r\n\"\"\"\r\n%s\n\"\"\"\r\n", response);
    int bytes_sent = send(client_fd, response, strlen(response), 0);
  }

  close(server_fd);

  printf("Shutdown...");

  return 0;
}
