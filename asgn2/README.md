# httpserver
Author: Miguel Avalos \
Student ID: 1704078

## Brief Description
- sets up an http server that attempts to complete the requests it receive from clients and sending a response after. This server is multi-threaded to handle multiple requests at once. 

## How to run
     ./httpserver port_number [-N threads] [-l log_file]
 
 - port_number must be an integer that is greater than 1024
 - Options:
 - number of threads must be postive integer (>0)
 - logfile can be any string name
 
## Faults  
- If the connection is closed by the client in the middle of writing to the file in a put request, the file could still be overwritten, but a response will not be sent. 
- There is a limit to the file size that a head or get request would respond with. The buffer that contains file size as a string would fit 1000 digits safely. Though, that is enough space to contain any value in the C int range.
- I couldn't use pwrite due the complier saying it's implicitly defined. I checked to see if I had the required packages and added #define _XOPEN_SOURCE 500 from a suggestion online. But, I had no luck. So, used locks for my implementation. It could cause the log file logs to be in a different order. Also, It will also be bottle neck if multiple log entries try to write at the same.

