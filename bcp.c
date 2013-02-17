/*
** bcp.c -- broadcast copy -- Jordan Wilberding -- (C) 2012-2013

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <libgen.h>

#define BROADCAST_PORT 4950   // default udp port
#define BCP_CODE 3141593      // have a unique code to verify broadcast
#define BCP_TCP_PORT 10789    // default tcp port
#define BACKLOG 10            // how many pending connections queue will hold
#define MAXBUFLEN 1024        // buffer size for packets
#define MAXNAMELEN 255        // max filename length

int file_exists (char *filename)
{
  struct stat buffer;
  return (stat (filename, &buffer) == 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void listener(char *ip, int *port)
{
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;
  int numbytes;
  struct sockaddr_storage their_addr;
  char buf[MAXBUFLEN];
  socklen_t addr_len;
  uint32_t packet[2];
  char port_s[100];

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  snprintf(port_s, 100, "%d", BROADCAST_PORT);

  if ((rv = getaddrinfo(NULL, port_s, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  // loop through all the results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
                         p->ai_protocol)) == -1) {
      perror("listener: socket");
      continue;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("listener: bind");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "listener: failed to bind socket\n");
    exit(2);
  }

  freeaddrinfo(servinfo);

  addr_len = sizeof their_addr;

  int done = 0;

  while (!done) {
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
                             (struct sockaddr *)&their_addr, &addr_len)) == -1) {
      perror("recvfrom");
      exit(1);
    }
    else if (numbytes == sizeof(packet)) {
      //"listener: got packet from %s\n",
             inet_ntop(their_addr.ss_family,
                       get_in_addr((struct sockaddr *)&their_addr),
                       ip, INET6_ADDRSTRLEN);

      memcpy(&packet, buf, sizeof(packet));

      if (ntohl(packet[0]) == BCP_CODE) {
        *port = ntohl(packet[1]);
        done = 1;
      }
    }
  }

  close(sockfd);
}

void broadcaster()
{
  int sockfd;
  struct sockaddr_in their_addr;
  struct hostent *he;
  int numbytes;
  int broadcast = 1;

  if ((he=gethostbyname("255.255.255.255")) == NULL) {  // get the host info
    perror("gethostbyname");
    exit(1);
  }

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }

  // this call is what allows broadcast packets to be sent:
  if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast,
                 sizeof broadcast) == -1) {
    perror("setsockopt (SO_BROADCAST)");
    exit(1);
  }

  their_addr.sin_family = AF_INET;     // host byte order
  their_addr.sin_port = htons(BROADCAST_PORT); // short, network byte order
  their_addr.sin_addr = *((struct in_addr *)he->h_addr);
  memset(their_addr.sin_zero, '\0', sizeof their_addr.sin_zero);

  uint32_t packet[2];
  packet[0] = htonl((uint32_t)BCP_CODE);
  packet[1] = htonl((uint32_t)BCP_TCP_PORT);

  if ((numbytes=sendto(sockfd, &packet, sizeof(packet), 0,
                       (struct sockaddr *)&their_addr, sizeof their_addr)) == -1) {
    perror("sendto");
    exit(1);
    }

  close(sockfd);
}

void sigchld_handler(int s)
{
  while(waitpid(-1, NULL, WNOHANG) > 0);
}

ssize_t recv_full(int fd, void *buf, ssize_t len)
{
  char *p = buf;
  ssize_t recvd;
  for (recvd = 1; len > 0 && recvd > 0; len -= recvd, p += recvd)
    recvd = recv(fd, p, len, 0);
  return p - (char *)buf;
}

void server(int port)
{
  int sockfd, new_fd;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;
  socklen_t sin_size;
  struct sigaction sa;
  int yes=1;
  char s[INET6_ADDRSTRLEN];
  int rv;
  int numbytes;
  char buf[MAXBUFLEN];
  char filename[MAXNAMELEN];
  uint32_t filename_size;
  FILE *ft;
  size_t total;
  char port_s[100];
  char overwrite;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  snprintf(port_s, 100, "%d", port);

  if ((rv = getaddrinfo(NULL, port_s, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  // loop through all the results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
                         p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                   sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue;
    }

    break;
  }

  if (p == NULL)  {
    fprintf(stderr, "server: failed to bind\n");
    exit(2);
  }

  freeaddrinfo(servinfo);

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }

  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  sin_size = sizeof their_addr;
  new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
  if (new_fd == -1) {
    perror("accept");
  }

  inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
  printf("Incoming connection from: %s\n", s);

  numbytes = recv_full(new_fd, &filename_size, sizeof(filename_size));
  filename_size = ntohl(filename_size);

  if (numbytes < sizeof(filename_size)) {
    printf("Protocol error, exiting.\n");
    exit(1);
  }

  numbytes = recv_full(new_fd, filename, filename_size);
  filename[filename_size] = '\0';

  if (numbytes < filename_size) {
    printf("Protocol error, exiting.\n");
    exit(1);
  }

  if (file_exists(filename)) {
    printf("%s already exists. Overwrite? Y/n: ", filename);
    scanf("%c", &overwrite);

    if (overwrite == 'n') {
      close(new_fd);
      close(sockfd);
      exit(0);
    }
  }

  ft = fopen (filename, "wb");

  if (ft == NULL) {
    perror("Cannon open file");
    exit(1);
  }

  total = 0;
  while((numbytes = recv(new_fd, buf, MAXBUFLEN, 0)) > 0) {
    total += numbytes;
    printf("\rReceive: %zu", total);
    fwrite(&buf, 1, numbytes, ft);
  }

  printf("\nFile received: %s\n", filename);

  fclose(ft);

  close(new_fd);
  close(sockfd);
}

void client(char *ip, int *port, char *path)
{
  int sockfd;//, numbytes;
  char buf[MAXBUFLEN];
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];
  int size;
  FILE *ft;
  uint32_t filename_size;
  size_t total;
  char port_s[100];
  char *filename;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  snprintf(port_s, 100, "%d", *port);

  if ((rv = getaddrinfo(ip, port_s, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  // loop through all the results and connect to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
                         p->ai_protocol)) == -1) {
      perror("client: socket");
      continue;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("client: connect");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    exit(2);
  }

  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);

  freeaddrinfo(servinfo);

  ft = fopen(path, "rb");

  if (ft == NULL) {
    perror("Cannot open file");
    exit(1);
  }

  filename = basename(path);
  filename_size = strlen(filename);
  filename_size = htonl(filename_size);

  memcpy(buf, &filename_size, sizeof(filename_size));
  memcpy(&buf[sizeof(filename_size)], filename, strlen(filename));

  if (send(sockfd, buf, sizeof(int)+strlen(filename), 0) == -1)
    perror("send");

  total = 0;
  while (!feof(ft)) {
    size = fread(&buf, 1, MAXBUFLEN, ft);
    total += size;

    printf("\rSent: %zu", total);

    if (send(sockfd, buf, size, 0) == -1)
      perror("send");
  }

  printf("\nFile sent.\n");

  close(sockfd);
  fclose(ft);
}

int main(int argc, char *argv[])
{
  int port;
  char ip[INET6_ADDRSTRLEN];

  if (argc > 1) {
    printf("Listening for request..\n");
    listener(ip, &port);
    printf("Sending file to: %s:%d\n", ip, port);
    client(ip, &port, argv[1]);
  }
  else {
    printf("Requesting file..\n");
    if (!fork()) { // this is the child process
      server(BCP_TCP_PORT);
    }
    else {
      sleep(1);
      broadcaster();
      int status;
      wait(&status);
    }
  }

  return 0;
}
