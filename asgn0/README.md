# shoulders
Author: Miguel Avalos \
Student ID: 1704078

## Brief Description
- for a every file or standard input indicator provided, prints the first characters up to a maximum length provided by num_bytes

## How to run
     ./shoulders num_bytes [FILES/-]
 
 - num_bytes is a non-negative integer.
 - multiple file names or standard input indicators '-' could be provided
 - if no [FILES/-] arguments exist, then standard input is used as a default source
 
## Faults 
- Errors can result if the '\0' null character is entered in the middle of the arguments 
    - num_bytes validation process fails
    - standard out indicator and filename determination process could be off
    - both processes use strcmp to compare strings
    
- if num_bytes is outside of int range, it will cause an integer overflow
    