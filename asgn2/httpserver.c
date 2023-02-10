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
#include <pthread.h>
#include <getopt.h>
#include <semaphore.h>

#define BUFFER_SIZE 1000
#define MAX_SIZE    4001
#define NUM_SIZE    21
#define QTR_SIZE    1001
#define END         -5
#define BLANK       0
#define FILLED      1
#define OTHER       4

typedef struct file_log {
  char* filename;
  sem_t* lock;
}file_log;

typedef struct worker_tools {
  file_log* log;
  int conn_fd;
  sem_t* lock;
  sem_t* waiting;
}worker_tools;

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

int word_length(char* data, int position){
  int i = 0;
  int length = 0;
  int max = strlen(data);
  while (position>0) {
    if(data[i] == ' ' || data[i] == '\n')
      position--;
    else if (i > max)
      return 0;
    i++;
  }

  while (i < max && data[i] != ' ' && data[i] != '\n') {
    length++;
    i++;
  }
  return length;
}

int write_word(char* data, char* buffer, int position, int file_fd){
  int i = 0;
  int length = 0;
  int max = strlen(data);
  buffer[0] = ' '; 
  while (position>0) {
    if(data[i] == ' ' || data[i] == '\n')
      position--;
    else if (i > max)
      return 0;
    i++;
  }

  while (i < max && data[i] != ' ') {
    buffer[0] = data[i];
    write(file_fd, buffer, 1);
    length++;
    i++;
  }
  return length;

}

void writelog(int number, bool fail, file_log* log, char* header, char* host, char* buffer){
  if(log->filename == NULL){
    return;
  }
  sem_wait(log->lock);
  int fd_log = open(log->filename, O_RDWR | O_APPEND);
  if(fail){
    write(fd_log, "FAIL\t", 5);
    write(fd_log, header, strlen(header));
  }
  else {
    write(fd_log, header, word_length(header, 0));
    write(fd_log, "\t", 1);
    write_word(header, buffer, 1, fd_log);
    write(fd_log, "\t", 1);
    write(fd_log, host, strlen(host));
  }

  write(fd_log, "\t", 1);

  if (number == END){
    write(fd_log, "000", 3);
  }
  else {
    sprintf(buffer, "%d", number);
    write(fd_log, buffer, strlen(buffer));
  }
  write(fd_log, "\n", 1);
  close(fd_log);
  sem_post(log->lock);
}

void err_response(int code, int conn_fd, bool body){
  if (code == 200 || code == 201 || code == 0 || code == END){
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
  //check if length is meant to be zero
  if (length != 0){
    if(recv(conn_fd, buffer, BUFFER_SIZE, MSG_PEEK) == 0){
      return 0;
    }
  }

  int file_fd = open(filepath, O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (file_fd == -1){
    return geterror(errno);
  }

  if (length != 0){
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
      return -1*code;
  }
  else if (S_ISDIR(info.st_mode)) {
    return -403;
  }

  int file_fd = open(filepath, O_RDONLY);
  if (file_fd == -1){
    return -1*geterror(errno);
  }

  close(file_fd);

  //add header information
  send(conn_fd, "HTTP/1.1 200 OK\r\n", 17, 0);
  send(conn_fd, "Content-Length: ", 16, 0);

  int size = info.st_size;
  char strval[1001];
  sprintf(strval, "%d", size); 

  //extrating int to string 
  send(conn_fd, strval, strlen(strval), 0);
  send(conn_fd, "\r\n\r\n", 4, 0);
  return size;
}

int getfile(char* filepath, int conn_fd){
  //hands error checking and header task
  //to headfile
  int head_status = headfile (filepath, conn_fd);
  if (head_status < 0)
    return head_status;

  int file_fd = open(filepath, O_RDONLY);

  if (file_fd == -1){
    return -1*geterror(errno);
  }

  int filled = 1;
  char buffer[BUFFER_SIZE];
  while (filled){
    filled = read(file_fd, buffer, BUFFER_SIZE);
    send(conn_fd, buffer, filled, 0);
  }
  //end of message
  close(file_fd);
  return head_status;
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

  if (filled <= 2){
    return BLANK;
  }
  return FILLED;
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

bool process_request(int conn_fd, file_log* log){
  //MAX_SIZE 4001 bytes
  char buffer[MAX_SIZE];
  char first[MAX_SIZE];
  char filepath[17];
  char host[MAX_SIZE];
  bool bad = false;

  //read the first line entirely into a buffer
  if (readline(conn_fd, first) == END){
    writelog(END, true, log, first, NULL, buffer);
    return true;
  }

  //get the context-length value if available
  //and read all data before the body

  //will store positive number if length 
  //is found
  int length = -1;
  bool CL = false;
  bool HOST = false;
  //char numStore[NUM_SIZE];

  //read all data before body 
  //extract context length if it exists
  //extract host if it exists
  int status = FILLED;
  while(status != BLANK){
    status = readline(conn_fd, buffer);
    if (status == END) {
      writelog(END, true, log, first, NULL, buffer);
      return true;
    }
    if (!CL) {
      if (strstr(buffer, "Content-Length:") != NULL){
        bool invalid = false;
        for(size_t i = 16; i < strlen(buffer); i++){
          if(!isdigit(buffer[i])){
            invalid = true;
            break;
          }
        }
        if (!invalid){
          sscanf(buffer, "Content-Length: %d", &length);
        }
        CL = true;
      }

    }
    
    if (!HOST){
      if (sscanf(buffer, "Host: %s", host) == 1)
        HOST = true;
    }
  }

  //checks if filepath is 16 characters including the '/'
  if (word_length(first, 1) != 16){
    writelog(400, true, log, first, NULL, buffer);
    sscanf (first, "%s ", buffer);
    //send body if its not a head command
    if (strcmp(buffer, "HEAD\0")){
      err_response(400, conn_fd, true);
    }
    else {
      err_response(400, conn_fd, false);
    }
    return true;
  }
  
  //check if hostname was detected and http version is 1.1
  if(!HOST || strstr(first, "HTTP/1.1")==NULL){
    bad = true; 
  }

  //attempt extract command and file object
  if (sscanf(first, "%s /%s", buffer, filepath) != 2){
    err_response(400, conn_fd, true);
    writelog(400, true, log, first, NULL, buffer);
    return true;
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
  int result = 0;

  //============PUT COMMAND=============
  if (!strcmp(buffer, put)){
    //it is a bad request if:
    //the context-length doesn't exist
    //the context-length is a negative number
    //the context-length is not a number
    //it fails other minimum requirements
    if (length < 0 || !CL || bad){
      err_response(400, conn_fd, true);
      writelog(400, true, log, first, NULL, buffer);
      return true;
    }
    
    result = putfile(filepath, length, conn_fd);
    //if connection is lost, close the connection on
    //server side
    if (result == 0) {
      writelog(END, true, log, first, NULL, buffer);
      return true;
    }//if error occurred read remaining boby
    else if (result != 200 && result != 201){
      discardbody(conn_fd, length);
      err_response(result, conn_fd, true);
      writelog(result, true, log, first, NULL, buffer);
    }
    else {
      writelog(length, false, log, first, host, buffer);
    }
  }//============HEAD COMMAND============
  else if(!strcmp(buffer, head)){
    //it is a bad request if:
    //the context-length does exist
    //it fails other minimum requirements
    if(CL || bad){
      err_response(400, conn_fd, false);
      writelog(400, true, log, first, NULL, buffer);
      return true;
    }

    result = headfile(filepath, conn_fd);
    //execute request
    if (result < 0){
      err_response(-1*result, conn_fd, false);
      writelog(-1*result, true, log, first, NULL, buffer);
    }
    else{
      writelog(result, false, log, first, host, buffer);
    }
  }//============GET COMMAND=============
  else if(!strcmp(buffer, get)){
    //it is a bad request if:
    //the context-length does exist
    //it fails other minimum requirements
    if(CL || bad){
      err_response(400, conn_fd, true);
      writelog(400, true, log, first, NULL, buffer);
      return true;
    }
    //execute request
    result = getfile(filepath, conn_fd);
    if (result < 0){
      err_response(-1*result, conn_fd, true);
      writelog(-1*result, true, log, first, NULL, buffer);
    }
    else{
      writelog(result, false, log, first, host, buffer);
    }
  }//===========UNIMPLEMENTED============
  else {
    //it is a bad request if:
    //if the context-length exist:
    //  the context-length is a negative number
    //  the context-length is not a number
    //it fails other minimum requirements
    if ((length < 0 && CL) || bad){
      err_response(400, conn_fd, true);
      writelog(400, true, log, first, NULL, buffer);
    }
    else {
      err_response(501, conn_fd, true);
      writelog(501, true, log, first, NULL, buffer);
    }
    return true;
  }
  
  //if there is no bad request was made
  //then return false
  return false;
}

void handle_connection(int connfd, file_log* log) {
  char buffer[10];

  //while the client hasn't cut the connection or
  //the last response was not not a 400 error
  while (recv(connfd, buffer, 10, MSG_PEEK) > 0){
    if(process_request(connfd, log)){
      break;
    }
  }
  
  // when done, close connection
  close(connfd);
}

void* worker(void* object){
  worker_tools* tools = (worker_tools*) object;

  for(;;){
    //wait until given a connection
    sem_wait(tools->lock);

    //recieve request(s), return responses, and 
    //close connection
    handle_connection(tools->conn_fd, tools->log);

    //"becoming available"
    tools->conn_fd = 0;
    sem_post(tools->waiting);
  }
}

int main(int argc, char *argv[]) {
  int listenfd;
  uint16_t port;

  char* logfile = NULL;
  char* threads = NULL;

  //scans options
  int c = 0;
  while ((c = getopt (argc, argv, "N:l:")) != -1){
    switch (c){
      case 'N' :
        threads = optarg;
        break;
      case 'l' :
        logfile = optarg;
        break;
      default:
        warnx("invalid flag: %c", c);
    }
  }

  //checks for valid port_num arguement
  if(optind>= argc){
    errx(EXIT_FAILURE, "Expected port_num after options");
  }
  port = strtouint16(argv[optind]);
  if (port == 0) {
    errx(EXIT_FAILURE, "invalid port number: %s", argv[optind]);
  }

  //default threads
  int workers = 4;
  
  //checks if number of worker threads entered by the user is valid
  if(threads != NULL){
    for (size_t w = 0; w < strlen(threads); w++){
      if(!isdigit(threads[w]))
        errx(EXIT_FAILURE, "invalid number of worker threads: %s", threads);
    }
    workers = atoi(threads);
  }

  //creates port
  listenfd = create_listen_socket(port);

  //log file resources
  sem_t log_lock;
  file_log log;
  (&log)->filename = logfile;
  if(logfile == NULL){
    (&log)->lock = NULL;
  }
  else {
    sem_init(&log_lock, workers, 1);
    (&log)->lock = &log_lock;
    int file_fd = open(logfile, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (file_fd == -1){
      errx(EXIT_FAILURE, "log file '%s' lacks permisssion", logfile);
    }
    close(file_fd);
  }
  
  //will put the dispacher to sleep
  sem_t waiting; 
  sem_init(&waiting, workers, workers);

  //sets up all worker tools
  worker_tools toolboxes[workers];
  sem_t worker_locks[workers];
  pthread_t worker_threads[workers];
  for (int i = 0; i<workers; i++){
      (&(toolboxes[i]))->conn_fd = 0;
      sem_init(&(worker_locks[i]), 2, 0);
      (&(toolboxes[i]))->lock = &(worker_locks[i]);
      (&(toolboxes[i]))->log = &log;
      (&(toolboxes[i]))->waiting = &waiting;
      pthread_create(&(worker_threads[i]), NULL, &worker, &(toolboxes[i]));
  }


  //dispatcher
  while(1) {
    //wait for connection and accept
    int connfd = accept(listenfd, NULL, NULL);
    if (connfd < 0) {
      warn("accept error");
      continue;
    }

    //wait until worker thread is available
    sem_wait(&waiting);

    //give connection (fd) to worker thread
    for(int i = 0; i<workers; i++){
      if ((&toolboxes[i])->conn_fd == 0){
        (&toolboxes[i])->conn_fd = connfd;
        sem_post(&(worker_locks[i]));
        break;
      }
    }
  }
  return EXIT_SUCCESS;
}
