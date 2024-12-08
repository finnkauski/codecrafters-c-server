#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
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

typedef struct {
  int client_fd;
  char *folder;
} HandlerArgs;

unsigned char *read_file_to_buffer(char *filename, FILE *fp, size_t *size) {
  unsigned char *buffer = NULL;

  if (!fp) {
    fprintf(stderr, "ERROR: Could not open file %s\n", filename);
    *size = 0;
    return NULL;
  }

  // Move the file pointer to the end and determine file size
  if (fseek(fp, 0, SEEK_END) != 0) {
    fprintf(stderr, "ERROR: Could not seek in file %s\n", filename);
    fclose(fp);
    *size = 0;
    return NULL;
  }

  long file_size = ftell(fp);
  if (file_size < 0) {
    fprintf(stderr, "ERROR: Could not get file size for %s\n", filename);
    fclose(fp);
    *size = 0;
    return NULL;
  }

  // Allocate buffer
  buffer = (unsigned char *)malloc((size_t)file_size + 1);
  if (!buffer) {
    fprintf(stderr, "ERROR: Out of memory while reading %s\n", filename);
    fclose(fp);
    *size = 0;
    return NULL;
  }

  // Return to the beginning of the file
  rewind(fp);

  // Read the file into the buffer
  size_t read_bytes = fread(buffer, 1, (size_t)file_size, fp);
  fclose(fp);

  if (read_bytes != (size_t)file_size) {
    fprintf(stderr, "ERROR: Could not read entire file %s\n", filename);
    free(buffer);
    *size = 0;
    return NULL;
  }

  // If you want a null-terminator for string compatibility
  buffer[file_size] = '\0';

  *size = (size_t)file_size;
  return buffer;
}

void *handle_connection(void *handler_args) {
  printf("Client connected\n");
  char buffer[BUFFER_SIZE];
  char *response;

  HandlerArgs args = *((HandlerArgs *)handler_args);

  // Handling of connection

  int valread = read(args.client_fd, buffer, BUFFER_SIZE);

  if (valread < 0) {
    printf("Read failed: %s \n", strerror(errno));
    return NULL;
  }

  // Parse headers
  bool first = true, headers_parsed = false;
  int n_headers = 0, n_empty = 0;
  char method[16], path[256], protocol[16];
  char *token, *string, *body;
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
      headers_parsed = true;
      printf("DEBUG: Header \"%s\"", token);
      char *key, *value;
      key = strsep(&token, ": ");
      value = &token[1];
      printf("-> key: %s, value: %s\n", key, value);

      Header header;
      header.key = key;
      header.value = value;
      headers[n_headers++] = header;
      n_empty = 0;
    } else if (headers_parsed) {
      // printf("DEBUG: N_empty \"%d\"\n", n_empty);
      if (n_empty == 2) {
        body = string;
        printf("DEBUG: Body \"%s\"\n", body);
        break;
      } else {
        n_empty++;
      }
    }
  }

  // Echo Path
  if ((strncmp(path, "/files/", 6) == 0) && (args.folder != NULL)) {
    char filepath[512]; // Resultant concatenated string (large enough to hold
                        // both strings)
    strncpy(filepath, args.folder, sizeof(filepath));
    filepath[sizeof(filepath) - 1] = '\0'; // Ensure null termination
    filepath[strlen(args.folder)] = '/';

    // Concatenate the second string to the result using strlcat
    if (strlcat(filepath, &path[6], sizeof(filepath)) >= sizeof(filepath)) {
      printf(
          "Resulting string was truncated due to insufficient buffer size.\n");
      return NULL;
    }
    if (strcmp(method, "POST") == 0) {
      FILE *fp = fopen(filepath, "wb");

      int file_size;
      for (int i = 0; i < n_headers; i++) {
        if (strcmp(headers[i].key, "Content-Length") == 0) {
          file_size = atoi(headers[i].value);
          printf("DEBUG: File size for POST request: %d\n", file_size);
          break;
        }
      }

      size_t written = fwrite(body, 1, file_size, fp);

      fclose(fp);
      response = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n";
    } else if (strcmp(method, "GET")) {
      printf("DEBUG: Requested filepath: %s\n", filepath);
      char response_buffer[BUFFER_SIZE + 256];
      FILE *fp = fopen(filepath, "rb");
      if (fp == NULL) {
        printf("DEBUG: File not found: %s.\n", filepath);
        response =
            "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nContent-Type: "
            "text/plain\r\n\r\n";

      } else {
        size_t file_size = 0;
        unsigned char *data = read_file_to_buffer(filepath, fp, &file_size);

        if (data) {
          // Use the data...
          printf("DEBUG: Read %zu bytes from the file.\n", file_size);
        }
        sprintf(response_buffer,
                "HTTP/1.1 200 OK\r\nContent-Length: %lu\r\nContent-Type: "
                "application/octet-stream\r\n\r\n%s",
                file_size, data);

        response = &response_buffer[0];

        // Remember to free when done
        free(data);
      }
    }

  } else if (strncmp(path, "/echo/", 6) == 0) {
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
  int bytes_sent = send(args.client_fd, response, strlen(response), 0);

  return NULL;
}

int main(int argc, char *argv[]) {
  char *folder = NULL;
  if (argc > 1) {
    printf("Starting server with the following arguments: \n");
    for (int i; i < argc; i++) {
      char *argument = argv[i];
      if (strcmp(argument, "--directory") == 0) {
        folder = argv[i + 1];
      }
      printf("  -| \"%s\"\n", argument);
    }
  }

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
    if (client_fd == -1) {
      continue;
    }

    pthread_t new_process;

    HandlerArgs handler_args = {.client_fd = client_fd, .folder = folder};
    pthread_create(&new_process, NULL, handle_connection, &handler_args);
    pthread_detach(new_process);
  }

  close(server_fd);

  printf("Shutdown...");

  return 0;
}
