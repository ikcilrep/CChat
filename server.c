#include "headers/errors.h"
#include <arpa/inet.h> //close
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> //strlen
#include <sys/socket.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <sys/types.h>
#include <unistd.h> //close
#define BUFFER_SIZE 256
#define SERVER_ARGUMENTS_TEMPLATE "<PORT>"

int main(int argc, char *argv[]) {
  unsigned int i, max_clients = 100;
  int master_socket, addrlen, new_socket, client_socket[30], activity, valread, opt = 1, max_sd, port, sdi;
  struct sockaddr_in address;

  char buffer[BUFFER_SIZE + 1]; // data buffer of 1K

  // set of socket descriptors
  fd_set readfds;

  if (argc < 2)
    usage(argv[0], SERVER_ARGUMENTS_TEMPLATE);
  port = atoi(argv[1]);

  // initialise all client_socket[] to 0 so not checked
  for (i = 0; i < max_clients; i++)
    client_socket[i] = 0;

  // create a master socket
  if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // set master socket to allow multiple connections
  if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
                 sizeof(opt)) < 0) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  // type of socket created
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  // bind the socket to localhost port 8888
  if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }
  printf("Listener on port %d \n", port);

  // try to specify maximum of 3 pending connections for the master socket
  if (listen(master_socket, 3) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  // accept the incoming connection
  addrlen = sizeof(address);
  puts("Waiting for connections ...");

  while (1) {
    // clear the socket set
    FD_ZERO(&readfds);

    // add master socket to set
    FD_SET(master_socket, &readfds);
    max_sd = master_socket;
    // add child sockets to set
    for (i = 0; i < max_clients; i++) {
      // socket descriptor
      sdi = client_socket[i];

      // if valid socket descriptor then add to read list
      if (sdi > 0)
        FD_SET(sdi, &readfds);

      // highest file descriptor number, need it for the select function
      if (sdi > max_sd)
        max_sd = sdi;
    }

    // wait for an activity on one of the sockets , timeout is NULL ,
    // so wait indefinitely
    activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

    if (activity < 0 && errno != EINTR)
      printf("select error");
    

    // If something happened on the master socket ,
    // then its an incoming connection
    if (FD_ISSET(master_socket, &readfds)) {
      if ((new_socket = accept(master_socket, (struct sockaddr *)&address,
                               (socklen_t *)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
      }

      // inform user of socket number - used in send and receive commands
      printf("New connection, socket fd is %d, ip is : %s, port : %d\n",
             new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

      for (i = 0; i < max_clients; i++) {   
        //if position is empty  
        if( client_socket[i] == 0) {   
            client_socket[i] = new_socket;   
            printf("Adding to list of sockets as %d\n" , i);   
            break;   
        }   
      }   
    }

    // else its some IO operation on some other socket
    for (i = 0; i < max_clients; i++) {
      sdi = client_socket[i];

      if (FD_ISSET(sdi, &readfds)) {
        // Check if it was for closing , and also read the
        // incoming message
        if ((valread = read(sdi, buffer, BUFFER_SIZE)) == 0) {
          // Somebody disconnected , get his details and print
          getpeername(sdi, (struct sockaddr *)&address, (socklen_t *)&addrlen);
          printf("Host disconnected, ip %s, port %d\n",
                 inet_ntoa(address.sin_addr), ntohs(address.sin_port));

          // Close the socket and mark as 0 in list for reuse
          close(sdi);
          client_socket[i] = 0;
        } else {
          buffer[valread] = 0;
          for (unsigned int j = 0; j < max_clients; j++) {
            int sdj = client_socket[j];
            if (i != j && FD_ISSET(sdi, &readfds))
              send(sdj, buffer, strlen(buffer), 0);
          }
        }
      }
    }
  }

  return 0;
}
