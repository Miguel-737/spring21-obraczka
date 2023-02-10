# httpproxy
Author: Miguel Avalos \
Student ID: 1704078

## Brief Description
- Sets up an http proxy that recieves client requests and when appropriate forward them to the main server. It will cache files of past GET reponses if the file fit in the cache and the reponses weren't errors. When requested, cached files that are up to date will be sent directly to the client instead of forwarding the GET response from httpserver.
## How to run
     ./httpproxy listen_port server_port [-c cache_size] [-m file_size] [-u]
 
 - ports must be an integer that is greater than 1024
 - Options:
 - all number arguments must be a postive integer (>0)
 - c: number of cache slots
 - m: max file size to be stored in bits
 - U: enables the LRU policy
 
## Faults  
- The proxy depends on the main server to send a valid response to it. Otherwise, it could potentially crash.
- There is a limit to the file size that a head or get request would respond with. The buffer that contains file size as a string would fit 500 digits safely. Though, that is enough space to contain any value in the C int range.
- httpproxy does not attempt to free the memory that it has malloc due it being to out of scope for this assignment. 
- httpproxy does not forward (to httpserver) any unimplemented requests and PUT requests without a valid context-length. 
