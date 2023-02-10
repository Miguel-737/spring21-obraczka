# httpserver
Author: Miguel Avalos \
Student ID: 1704078

## Brief Description
- sets up an http server that attempts to complete the requests it receive from clients and sending a response after.

## How to run
     ./httpserver port_number
 
 - port_number must be an integer that is greater than 1024
 
## Faults 
- There is a limit to the file size that a head or get request would respond with. The buffer that contains file size as a string would fit 1000 digits safely. That is enough space to contain any value in the C int range. 
- If the connection is closed by the client in the middle of writing to the file in a put request, the file will still be overwritten, but a response will not be sent. 
    

