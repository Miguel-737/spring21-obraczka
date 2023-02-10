#include <err.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <ctype.h>

#define BUFFER_SIZE 10
#define MAX_SIZE 4001

// struct longstr
// {
//   char str[10];
//   int readch;
//   struct longstr* next;
// };

/**
   Converts a string to an 16 bits unsigned integer.
   Returns 0 if the string is malformed or out of the range.
 */
uint16_t strtouint16(char number[]) {
  char *last;
  long num = strtol(number, &last, 10);
  if (num <= 0 || num > UINT16_MAX || *last != '\0') {
    return 0;
  }
  return num;
}

/**
   Creates a socket for listening for connections.
   Closes the program and prints an error message on error.
 */
int create_listen_socket(uint16_t port) {
  struct sockaddr_in addr;
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) {
    err(EXIT_FAILURE, "socket error");
  }

  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htons(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(listenfd, (struct sockaddr*)&addr, sizeof addr) < 0) {
    err(EXIT_FAILURE, "bind error");
  }

  if (listen(listenfd, 500) < 0) {
    err(EXIT_FAILURE, "listen error");
  }

  return listenfd;
}

int geterror (int option){
  switch(option){
    case EACCES :
      return 403;
    case ENOENT :
      return 404;
    default :
      return 500;
  }
}

void err_response(int code, int conn_fd, bool body){
  if (code == 200 || code == 201 || code == 0){
    return;
  }
  send(conn_fd, "HTTP/1.1 ", 9, 0);
  switch(code){
    case 400 :
      send(conn_fd, "400 Bad Request\r\n", 17, 0);
      if (body){
        send(conn_fd, "Content-Length: 12\r\n", 20, 0);
        send(conn_fd, "\r\nBad Request\n", 14, 0);
      }
      break;
    case 403 :
      send(conn_fd, "403 Forbidden\r\n", 15, 0);
      if (body){
        send(conn_fd, "Content-Length: 10\r\n", 20, 0);
        send(conn_fd, "\r\nForbidden\n", 12, 0);
      }
      break;
    case 404 :
      send(conn_fd, "404 Not Found\r\n", 15, 0);
      if (body){
        send(conn_fd, "Content-Length: 12\r\n", 20, 0);
        send(conn_fd, "\r\nNot Found\n", 14, 0);
      }
      break;
    case 500 :
      send(conn_fd, "500 Internal Server Error\r\n", 27, 0);
      if (body){
        send(conn_fd, "Content-Length: 22\r\n", 20, 0);
        send(conn_fd, "\r\nInternal Server Error\n", 24, 0);
      }
      break;
    case 501 :
      send(conn_fd, "501 Not Implemented\r\n", 21, 0);
      if (body){
        send(conn_fd, "Content-Length: 16\r\n", 20, 0);
        send(conn_fd, "\r\nNot Implemented\n", 18, 0);
      }
      break;
  }
  if (!body){
    send(conn_fd, "Content-Length: 0\r\n\r\n", 21, 0);
  }
}

int putfile(char* filepath, int length, int conn_fd){
  //error checking
  //lacking permissions
  //path lead to a directory

  //will retain code 404 if the file/directory
  //does not exist prior the put request
  int code = 0;

  struct stat info;
  if (stat(filepath, &info) == -1){
    code = geterror(errno);
    if (code != 404){
      return code;
    }
  }
  else if (S_ISDIR(info.st_mode)) {
    return 403;
  }

  char buffer[BUFFER_SIZE];
  if(recv(conn_fd, buffer, BUFFER_SIZE, MSG_PEEK) == 0){
    return 0;
  }

  int file_fd = open(filepath, O_RDWR | O_CREAT | O_TRUNC, 0666);

  //read recieved data until content length is satisifed
  while(length>0){
    int got = recv(conn_fd, buffer, BUFFER_SIZE, 0);
    if (got == 0){
      close(file_fd);
      return 0;
    }
    write(file_fd, buffer, got);
    length -= got;
  }

  //close file save
  close(file_fd);

  //if file was created
  if (code == 404) {
    send(conn_fd, "HTTP/1.1 201 Created\r\n", 22, 0);
    send(conn_fd, "Content-Length: 8\r\n\r\n", 21, 0);
    send(conn_fd, "Created\n", 8, 0);
    return 201;
  }
  //otherwise
  send(conn_fd, "HTTP/1.1 200 OK\r\n", 17, 0);
  send(conn_fd, "Content-Length: 3\r\n\r\n", 21, 0);
  send(conn_fd, "OK\n", 3, 0);
  return 200;
}

int headfile (char* filepath, int conn_fd){
  //error checking
  //lacking permissions
  //file does not exist
  //path lead to a directory
  struct stat info;
  if (stat(filepath, &info) == -1){
      int code = geterror(errno);
      return code;
  }
  else if (S_ISDIR(info.st_mode)) {
    return 403;
  }

  //add header information
  send(conn_fd, "HTTP/1.1 200 OK\r\n", 17, 0);
  send(conn_fd, "Content-Length: ", 16, 0);

  int size = info.st_size;
  char strval[1001];
  sprintf(strval, "%d", size); 

  //extrating int to string 
  send(conn_fd, strval, strlen(strval), 0);
  send(conn_fd, "\r\n\r\n", 4, 0);
  return 200;
}

int getfile(char* filepath, int conn_fd){
  //hands error checking and header task
  //to headfile
  int head_status = headfile (filepath, conn_fd);
  if (head_status != 200)
    return head_status;

  int file_fd = open(filepath, O_RDONLY);

  int filled = 1;
  char buffer[BUFFER_SIZE];
  while (filled){
    filled = read(file_fd, buffer, BUFFER_SIZE);
    send(conn_fd, buffer, filled, 0);
  }
  //end of message
  close(file_fd);
  return 200;
}

void discardbody(int conn_fd, int length){
  if (length < 1)
    return;

  int discarded = 0;
  char buffer[1];
  while (discarded < length){
    if(recv(conn_fd, buffer, 1, 0) == 0)
      return;
    discarded++;
  }
}

bool process_request(int conn_fd){
  //stores complete line
  char first[MAX_SIZE];

  char command[MAX_SIZE];
  char filepath[MAX_SIZE];
  char buffer[1];
  buffer[0] = ' ';

  //for number of spaces occupied in line
  int filled = 0;
  //read the first line entirely into a buffer
  while (buffer[0] != '\n')
  {
    int got = recv(conn_fd, buffer, 1, 0);
    if (got == 1 && filled < MAX_SIZE -1){
      first[filled] = buffer[0];
      ++filled;
    }
    else if (got == 0){
      return true;
    }
  }
  first[filled] = '\0';

  //get the context-length value if available
  //and read all data before the body

  //will store positive number if length 
  //is found
  int length = -1;
  bool CL = false;
  char line[MAX_SIZE];
  char numStore[MAX_SIZE];

  //to contain header component
  filled = 0;

  //to find end of header
  int sets = 1;

  //read all data before body 
  //extract context length if it exist
  while(sets < 2){
    int got = recv(conn_fd, buffer, 1, 0);
    if (got == 0){
      return true;
    }

    if (!CL){
      line[filled] = buffer[0];
      filled++;

      if (buffer[0] == '\n'){
        line[filled] = '\0';
        if (sscanf(line, "Content-Length: %s\r\n", numStore) != 1){
          length = -1;
          filled = 0;
        }
        else {
          bool invalid = false;
          for(size_t i = 0; i < strlen(numStore); i++){
            if(!isdigit(numStore[i])){
              invalid = true;
              break;
            }
          }
          if (!invalid){
            sscanf(numStore, "%d", &length);
          }
          CL = true;
        }
      } 
    }

    //to find end of head
    if(buffer[0] == '\n') sets++;
    else if (buffer[0] != '\r') sets = 0;
  }

  //check for bad request
  bool bad = false;

  //attempt extract command and file object
  if (sscanf( first, "%s /%s", command, filepath) < 2){
    bad = true;
  }

  //check if filepath a is exactly 15 characters
  if (!bad && strlen(filepath) != 15){
      bad = true;
  }

  //check if filepath alphanumeric 
  if (!bad){
    for (size_t i = 0; i < strlen(filepath); i++){
      if (!isalnum(filepath[i])){
        bad = true;
        break;
      }
    }
  }

  //call appropriate fuction
  //match requested command or
  //send an error response
  char head[] = "HEAD\0";
  char put[] = "PUT\0";
  char get[] = "GET\0";

  //============PUT COMMAND=============
  if (!strcmp(command, put)){
    //it is a bad request if:
    //the context-length doesn't exist
    //the context-length is a negative number
    //the context-length is not a number
    //it fails other minimum requirements
    if (length < 0 || !CL || bad){
      err_response(400, conn_fd, true);
      return true;
    }
    
    int result = putfile(filepath, length, conn_fd);
    //if connection is lost, close the connection on
    //server side
    if (result == 0) {
      return true;
    }//if error occurred read remaining boby
    else if (result != 200 && result != 201){
      discardbody(conn_fd, length);
    }

    //send error message if error occured
    err_response(result, conn_fd, true);
  }//============HEAD COMMAND============
  else if(!strcmp(command, head)){
    //it is a bad request if:
    //the context-length does exist
    //it fails other minimum requirements
    if(CL || bad){
      err_response(400, conn_fd, false);
      return true;
    }

    //execute request
    err_response(headfile(filepath, conn_fd), 
                 conn_fd, false);
  }//============GET COMMAND=============
  else if(!strcmp(command, get)){
    //it is a bad request if:
    //the context-length does exist
    //it fails other minimum requirements
    if(CL || bad){
      err_response(400, conn_fd, true);
      return true;
    }

    //execute request
    err_response(getfile(filepath, conn_fd), 
                 conn_fd, true);
  }//===========UNIMPLEMENTED============
  else {
    //it is a bad request if:
    //if the context-length exist:
    //  the context-length is a negative number
    //  the context-length is not a number
    //it fails other minimum requirements
    if ((length < 0 && CL) || bad){
      err_response(400, conn_fd, true);
      return true;
    }
    err_response(501, conn_fd, true);

    //discard remaining body
    discardbody(conn_fd, length);
  }
  
  //if there is no bad request was made
  //then return false
  return false;
}

void handle_connection(int connfd) {
  char buffer[BUFFER_SIZE];

  //while the client hasn't cut the connection or
  //the last response was not not a 400 error
  while (recv(connfd, buffer, 10, MSG_PEEK) > 0)
  {
    if(process_request(connfd)){
      break;
    }
  }
  
  // when done, close socket
  close(connfd);
}

int main(int argc, char *argv[]) {
  int listenfd;
  uint16_t port;

  if (argc != 2) {
    errx(EXIT_FAILURE, "wrong arguments: %s port_num", argv[0]);
  }
  port = strtouint16(argv[1]);
  if (port == 0) {
    errx(EXIT_FAILURE, "invalid port number: %s", argv[1]);
  }
  listenfd = create_listen_socket(port);

  while(1) {
    int connfd = accept(listenfd, NULL, NULL);
    if (connfd < 0) {
      warn("accept error");
      continue;
    }
    handle_connection(connfd);
  }
  return EXIT_SUCCESS;
}
