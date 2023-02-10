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
#include <getopt.h>
#define __USE_XOPEN
#include <time.h>

#define BUFFER_SIZE 1000
#define MAX_SIZE    4001
#define FILLED      1
#define INVALID     -1
#define END         -2
#define BLANK       -3
#define EMPTY       -4
#define NOMATCH     -5
#define TOOLARGE    -6
#define ERROR       -7

bool FIFO = true;

typedef struct slot{
  char* file;
  char name[17]; 
  bool empty;
  int length;
  struct tm time;
}slot;

typedef struct c_manage{
  slot* slots;
  int* order;
  int c_slots;
  int file_size;
  int filled;
} c_manage;

/**
   Creates a socket for connecting to a server running on the same
   computer, listening on the specified port number.  Returns the
   socket file descriptor on succes.  On failure, returns -1 and sets
   errno appropriately.
 */
int create_client_socket(uint16_t port) {
  int clientfd = socket(AF_INET, SOCK_STREAM, 0);
  if (clientfd < 0) {
    return -1;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (connect(clientfd, (struct sockaddr*) &addr, sizeof addr)) {
    return -1;
  }
  return clientfd;
}


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

void gettime(struct tm* t, char* source){
  t->tm_hour = 0;
  t->tm_isdst = 0;
  t->tm_mday = 0;
  t->tm_min = 0;
  t->tm_mon = 0;
  t->tm_sec = 0;
  t->tm_wday = 0;
  t->tm_yday = 0;
  t->tm_year = 0;
  strptime(source, "Last-Modified: %a, %d %b %Y %X", t);
}

int strtoint(char* input) {
  for (size_t w = 0; w < strlen(input); w++){
    if(!isdigit(input[w]))
      return INVALID;
  }
  return atoi(input);
}

void err_response(int code, int client_fd, bool body){
  if (code == 200 || code == 201 || code == 0 || code == END){
    return;
  }
  send(client_fd, "HTTP/1.1 ", 9, 0);
  switch(code){
    case 400 :
      send(client_fd, "400 Bad Request\r\n", 17, 0);
      if (body){
        send(client_fd, "Content-Length: 12\r\n", 20, 0);
        send(client_fd, "\r\nBad Request\n", 14, 0);
      }
      break;
    case 403 :
      send(client_fd, "403 Forbidden\r\n", 15, 0);
      if (body){
        send(client_fd, "Content-Length: 10\r\n", 20, 0);
        send(client_fd, "\r\nForbidden\n", 12, 0);
      }
      break;
    case 404 :
      send(client_fd, "404 Not Found\r\n", 15, 0);
      if (body){
        send(client_fd, "Content-Length: 12\r\n", 20, 0);
        send(client_fd, "\r\nNot Found\n", 14, 0);
      }
      break;
    case 500 :
      send(client_fd, "500 Internal Server Error\r\n", 27, 0);
      if (body){
        send(client_fd, "Content-Length: 22\r\n", 20, 0);
        send(client_fd, "\r\nInternal Server Error\n", 24, 0);
      }
      break;
    case 501 :
      send(client_fd, "501 Not Implemented\r\n", 21, 0);
      if (body){
        send(client_fd, "Content-Length: 16\r\n", 20, 0);
        send(client_fd, "\r\nNot Implemented\n", 18, 0);
      }
      break;
  }
  if (!body){
    send(client_fd, "Content-Length: 0\r\n\r\n", 21, 0);
  }
}

int readline(int conn_fd, char* line){
  char buffer[1];
  buffer[0]= ' ';
  int filled = 0;
  while(buffer[0] != '\n'){
    int got = recv(conn_fd, buffer, 1, 0);
    if (got < 1){
      return END;
    }
    else if (buffer[0]!='\r' && buffer[0]!='\n'){
      line[filled++] = buffer[0];
    }
  }

  line[filled] = '\0';

  if (filled <= 2)
    return BLANK;

  return FILLED;
}

bool fullR(int source_fd, int* length, char* first, char* multi_buf, struct tm* times){
  int status = FILLED;
  bool CL = false;
  bool T = false;

  if (readline(source_fd, first) == END){
    return true;
  }

  while(status != BLANK){
    status = readline(source_fd, multi_buf);
    if (status == END) {
      return true;
    }
    if (!CL) {
      if (strstr(multi_buf, "Content-Length:") != NULL){
        bool invalid = false;
        for(size_t i = 16; i < strlen(multi_buf); i++){
          if(!isdigit(multi_buf[i])){
            invalid = true;
            break;
          }
        }
        if (!invalid){
          sscanf(multi_buf, "Content-Length: %d", length);
        }
        CL = true;
      }
    }
  
    if(times != NULL && !T){
      if (strstr(multi_buf, "Last-Modified:") != NULL){
        gettime(times, multi_buf);
      }
    }
  }
  return false;
}

void sortorder(int* arr, int position, int length, int offset){
  for(int i = 0; i < length ; i++){
    if(arr[i] > position)
      arr[i] += offset;
  }
}

bool splitter(int source_fd, int dest_fd, char* store, int bytes){
  int got = 0;
  int left = bytes;
  char* space = store;
  while (left > 0){
    if(left > BUFFER_SIZE)
      got = recv(source_fd, space, BUFFER_SIZE, 0);
    else 
      got = recv(source_fd, space, left, 0);
    if(got == 0){
      return false;
    }
    send(dest_fd, space, got, 0);
    space += got;
    left -= got;
  }
  return true;
}

bool bridge(int source_fd, int dest_fd, int bytes){
  char buffer[BUFFER_SIZE];
  int counter = 1;
  int got = 0;
  bool forward = true;

  while (bytes > 0){
    counter++;
    if (bytes < BUFFER_SIZE){
      got = recv(source_fd, buffer, bytes, 0);
    }
    else {
      got = recv(source_fd, buffer, BUFFER_SIZE, 0);
    }

    if(got == 0)
      return false;

    if(forward) {
      if(send(dest_fd, buffer, got, 0) <= 0){
        return false;
        forward = false;
      }
    }
    bytes -= got;
  }
  return forward;
}

bool uptodate(slot* slt, struct tm* server){
  struct tm* cache = &(slt->time);
  server->tm_isdst = 0;
  cache->tm_isdst = 0;
  
  time_t cache_t = mktime(cache);
  time_t server_t = mktime(server);
  double diff = difftime(cache_t, server_t);
  return diff >= 0.0;
}

int checkcache(char* filename, c_manage* cache) {
  int i = 0;
  slot* slt;
  for(; i<cache->c_slots; i++){
    slt = &(cache->slots[i]);
    if(!(slt->empty)) {
      if (!strcmp(filename, slt->name)){
        return i;
      }
    }
  }
  return NOMATCH;
}

int newentry(int server_fd, int client_fd, char* filename, struct tm* date, int length, c_manage* cache){
  if(cache->file_size < length) {
    return TOOLARGE;
  }
  
  int target = EMPTY;

  if (cache->filled == cache->c_slots) {
    if(FIFO)
      target = 1;
    else
      target = cache->c_slots; 
  }

  int space = 0;
  //search for the most availiable slot
  for(int i = 0; i < cache->c_slots; i++){
    if(cache->order[i] == target){
      space = i;
      break;
    }
  }

  //cache slot
  slot* loc = &(cache->slots[space]);

  //store file in the cache and send to the client
  splitter(server_fd, client_fd, loc->file, length);

  //overwrite filename
  strcpy(loc->name, filename);

  //store file length
  loc->length = length;

  //get file date
  loc->time = *date;
  //settime(&(loc->time), date);

  //indicate as non-empty
  loc->empty = false;

  //increases filled if slot was empty
  if(target == EMPTY){
    cache->filled += 1;
  }

  //give correct order based on policy
  int* order = cache->order;
  if(FIFO){
    if(target != EMPTY)
      sortorder(order, order[space], cache->c_slots, -1);
    order[space] = cache->filled;
  }
  else{
    if(target != EMPTY)
      sortorder(order, order[space], cache->c_slots, -1);
    
    sortorder(order, 0, cache->c_slots, 1);
    order[space] = 1;
  }

  //return space filled
  return space;
}

bool incache(int server_fd, int client_fd, int place,  c_manage* cache, char* multi_buf, char* first, char* file, char* host){
  slot* slt = &(cache->slots[place]);
  int* order = cache->order;
  int length = 0;

  struct tm t;
  fullR(server_fd, &length, first, multi_buf, &t);

  int code = 500;
  //checking for errors
  sscanf(first, "HTTP/1.1 %d", &code);
  if(code == 400){
    //if a bad request don't modify cache
    err_response(code, client_fd, true);
    return true;
  }
  else if(code != 200){
    //if the head request requests an error
    sortorder(order, order[place], cache->c_slots, -1);
    order[place] = EMPTY;
    err_response(code, client_fd, true);
    slt->empty = true;
    return true;
  }
  else if (uptodate(slt, &t)){
    //if the head request shows its up to date 
    //send(client_fd, head, strlen(head), 0);
    char buf[500];
    send(client_fd, "HTTP/1.1 200 OK\r\n", 17, 0);
    sprintf(buf, "Content-Length: %d\r\n\r\n", slt->length);
    send(client_fd, buf, strlen(buf), 0);
    //send(client_fd, "Last-Modified: ", 15, 0);
    //strftime(buf, 500, "%a, %d %b %Y %X %Z\r\n\r\n", &(slt->time));
    //send(client_fd, buf, strlen(buf), 0);
    
    send(client_fd, slt->file, slt->length, 0);

    if(!FIFO){
      sortorder(order, order[place], cache->c_slots, -1);
      sortorder(order, 0, cache->c_slots, 1);
      order[place] = 1;
    }
  }
  else {
    //if the request is out of date
    //send get request on server
    send(server_fd, "GET ", 4, 0);
    send(server_fd, file, strlen(file), 0);
    send(server_fd, " HTTP/1.1\r\nHost: ", 17, 0);
    send(server_fd, host, strlen(host), 0);
    send(server_fd, "\r\n\r\n", 4, 0);

    //recieve server header 
    fullR(server_fd, &length, first, multi_buf, &(slt->time));


    //send server header to client
    send(client_fd, first, strlen(first), 0);
    sprintf(multi_buf, "\r\nContent-Length: %d\r\n\r\n", length);
    send(client_fd, multi_buf, strlen(multi_buf), 0);
    
    //extract content length
    if(length <= cache->file_size) {
      splitter(server_fd, client_fd, slt->file, length);
      slt->length = length;
      if(!FIFO){
        sortorder(order, order[place], cache->c_slots, -1);
        sortorder(order, 0, cache->c_slots, 1);
        order[place] = 1;
      }
    }
    else {
      slt->empty = true;
      bridge(server_fd, client_fd, length);
      sortorder(order, order[place], cache->c_slots, -1);
      order[place] = EMPTY; 
      return true;
    }
  }
  return false;
}

bool process_request(int client_fd, int server_fd, c_manage* cache){
  //MAX_SIZE 4001 bytes
  char first[MAX_SIZE];
  char multi_buf[MAX_SIZE];
  char file_buf[MAX_SIZE];
  char host[MAX_SIZE];
  bool CL = false;
  bool HOST = false;
  int length = -1;

  multi_buf[0] = '\0';
  host[0] = '\0';
  file_buf[0] = '\0';

  //read the first line entirely into a buffer
  if (readline(client_fd, first) == END){
    return true;
  }

  //get the context-length value if available
  //and read all data before the body

  //read all data before body 
  //extract context length if it exists
  //extract host if it exists
  int status = FILLED;
  while(status != BLANK){
    status = readline(client_fd, multi_buf);
    if (status == END) {
      return true;
    }
    if (!CL) {
      if (strstr(multi_buf, "Content-Length:") != NULL){
        bool invalid = false;
        for(size_t i = 16; i < strlen(multi_buf); i++){
          if(!isdigit(multi_buf[i])){
            invalid = true;
            break;
          }
        }
        if (!invalid){
          sscanf(multi_buf, "Content-Length: %d", &length);
        }
        CL = true;
        continue;
      }
    }
    
    if (!HOST){
      if (sscanf(multi_buf, "Host: %s", host) == 1)
        HOST = true;
    }
  }
  
  //attempt to extract command and file
  multi_buf[0] = '\0';
  sscanf(first, "%s %s ", multi_buf, file_buf);

  if(!strcmp(multi_buf, "GET")){
    //check cache
    int position = checkcache(file_buf, cache);
    if (position == NOMATCH){
      //FILE NOT FOUND IN CACHE

      //forward get requests
      send(server_fd, first, strlen(first), 0);
      send(server_fd, "\r\nHost: ", 8, 0);
      send(server_fd, host, strlen(host), 0);
      send(server_fd, "\r\n\r\n", 4, 0);

      //recieve server reponse header
      length = 0;
      struct tm t;
      fullR(server_fd, &length, first, multi_buf, &t);

      //send header to the client
      send(client_fd, first, strlen(first), 0);
      sprintf(multi_buf, "\r\nContent-Length:  %d", length);
      send(client_fd, multi_buf, strlen(multi_buf), 0);
      send(client_fd, "\r\n\r\n", 4, 0);
      
      //filtering out error responses and
      //files that are too big to be stored
      int code = 500;
      sscanf(first, "HTTP/1.1 %d", &code);
      if(code == 400){
        bridge(server_fd, client_fd, length);
        return true;
      }
      else if(code != 200 || 
         cache->c_slots == 0 || length > cache->file_size) 
      {
        bridge(server_fd, client_fd, length);
        return false;
      }
      
      //otherwise store in cache
      position = newentry(server_fd, client_fd, file_buf, &t, length, cache);
    }
    else {
      //FILE FOUND IN CACHE

      //send head request to server
      send(server_fd, "HEAD", 4, 0);
      char* rest = strstr(first, " ");
      if(rest != NULL) {
        send(server_fd, rest, strlen(rest), 0);
      }
      send(server_fd, "\r\nHost: ", 8, 0);
      send(server_fd, host, strlen(host), 0);
      send(server_fd, "\r\n\r\n", 4, 0);

      //see how data is handled
      incache(server_fd, client_fd, position, cache, multi_buf, first, file_buf, host);
    }
  }
  else if(!strcmp(multi_buf, "HEAD")){
    //forward to server
    send(server_fd, first, strlen(first), 0);
    send(server_fd, "\r\nHost: ", 8, 0);
    send(server_fd, host, strlen(host), 0);
    send(server_fd, "\r\n\r\n", 4, 0);

    //forward server response to client
    status = FILLED;
    if(readline(server_fd, first) != FILLED) return true;
    send(client_fd, first, strlen(first), 0);
    send(client_fd, "\r\n", 2, 0);
    while(status == FILLED){
      status = readline(server_fd, multi_buf);
      if(status == END){
        return true;
      }
      send(client_fd, multi_buf, strlen(multi_buf), 0);
      send(client_fd, "\r\n", 2, 0);
    }

    //force close connection if a bad requests is
    //recieved
    if((strstr(first, "HTTP/1.1 400") != NULL)){
        return true;
    }
  }
  else if(!strcmp(multi_buf, "PUT")){
    //for bad requests that don't have a valid
    //context-length to send body
    if(CL == false || length == -1){
      err_response(400, client_fd, true);
      return true;
    }

    //forward to server
    send(server_fd, first, strlen(first), 0);
    sprintf(multi_buf, "\r\nContent-Length: %d\r\n", length);
    send(server_fd, multi_buf, strlen(multi_buf), 0);
    send(server_fd, "Host: ", 6, 0);
    send(server_fd, host, strlen(host), 0);
    send(server_fd, "\r\n\r\n", 4, 0);

    //forward body
    bridge(client_fd, server_fd, length);

    //forward to client
    fullR(server_fd, &length, first, multi_buf, NULL);

    send(client_fd, first, strlen(first), 0);
    sprintf(multi_buf, "\r\nContent-Length: %d\r\n\r\n", length);
    send(client_fd, multi_buf, strlen(multi_buf), 0);
    bridge(server_fd, client_fd, length);

    //force close connection if a bad requests is
    //recieved
    int code = 500;
    sscanf(first, "HTTP/1.1 %d", &code);
    if(code != 201 && code != 200) {
      err_response(code, client_fd, true);
      return true;
    }
  }
  else {
    //for bad requests
    if (strlen(file_buf) != 16 || strstr(first, "HTTP/1.1") == NULL || HOST) {
      err_response(400, client_fd, true);
    }

    for(size_t i = 1; strlen(file_buf); i++){
      if(!isalnum(file_buf[i])){
        err_response(400, client_fd, true);
      }
    }

    err_response(501, client_fd, true);
    return true;
  }
  return false;
}

void handle_connection(int client_fd, int server_port, c_manage* cache) {
  char buffer[10];

  //while the client hasn't cut the connection or
  //the last response was not not a 400 error
  while (recv(client_fd, buffer, 10, MSG_PEEK) > 0){
    //use a different server connection with each request
    int server_fd = create_client_socket(server_port);
    if(server_fd == -1)
      warn("Failed to connect to server: %s\n", strerror(errno));

    if(process_request(client_fd, server_fd, cache)){
      close(server_fd);
      break;
    }
    close(server_fd);
  }
  
  // when done, close client connection
  close(client_fd);
}

int main(int argc, char *argv[]) {
  int listen_fd;
  uint16_t listen_port;
  uint16_t server_port;
  int c_slots = 3;
  int file_size  = 65536;

  //scans options
  int op = 0;
  while ((op = getopt (argc, argv, "c:m:u")) != -1){
    switch (op){
      case 'c' :
        c_slots = strtoint(optarg);
        if (c_slots == INVALID)
          errx(EXIT_FAILURE, "invalid cache size: %s", optarg);
        break;
      case 'm' :
        file_size = strtoint(optarg);
        if (file_size == INVALID)
          errx(EXIT_FAILURE, "invalid max file size: %s", optarg);
        break;
      case 'u' :
        FIFO = false;
        break;
      default:
        warnx("invalid flag: %c", op);
    }
  }

  //checks for valid port_num arguement
  if(optind>= argc){
    errx(EXIT_FAILURE, "Expected port_num after options");
  }

  listen_port = strtouint16(argv[optind]);
  if (listen_port == 0) {
    errx(EXIT_FAILURE, "invalid listen port number: %s", argv[1]);
  }

  server_port = strtouint16(argv[++optind]);
  if (server_port == 0) {
    errx(EXIT_FAILURE, "invalid server port number: %s", argv[2]);
  }

  //creating ports
  listen_fd = create_listen_socket(listen_port);

  c_manage manager;
  slot slots[c_slots];

  int order[c_slots];
  for (int i = 0; i < c_slots; i++){
    order[i] = EMPTY;
    if(file_size != 0) {
      (&slots[i])->file = (char *) malloc(file_size * sizeof(char));
      (&slots[i])->file[0] = '\0';
    }
    else {
      (&slots[i])->file = NULL;
    }
    (&slots[i])->name[0] = '\0';
    (&slots[i])->empty = true;
    (&slots[i])->length = 0;
  }

  (&manager)->slots = *(&slots);
  (&manager)->order = order;
  (&manager)->c_slots = c_slots;
  (&manager)->file_size = file_size;
  (&manager)->filled = 0;

  while(1) {
    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0) {
      warn("accept error");
      continue;
    }

    handle_connection(client_fd, server_port, &manager);
  }

  return EXIT_SUCCESS;
}
