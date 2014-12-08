/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <locale.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */


void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void dostuff(int); /* function prototype */
void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, pid;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;
     struct sigaction sa;          // for signal SIGCHLD

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
     listen(sockfd,5);
     
     clilen = sizeof(cli_addr);
     
     /****** Kill Zombie Processes ******/
     sa.sa_handler = sigchld_handler; // reap all dead processes
     sigemptyset(&sa.sa_mask);
     sa.sa_flags = SA_RESTART;
     if (sigaction(SIGCHLD, &sa, NULL) == -1) {
         perror("sigaction");
         exit(1);
     }
     /*********************************/
     
     while (1) {
         newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
         
         if (newsockfd < 0) 
             error("ERROR on accept");
         
         pid = fork(); //create a new process
         if (pid < 0)
             error("ERROR on fork");
         
         if (pid == 0)  { // fork() returns a value of 0 to the child process
             //printf("I am child %d\n", pid);
             close(sockfd);
             dostuff(newsockfd);
             exit(0);
         }
         else //returns the process ID of the child process to the parent
             close(newsockfd); // parent doesn't need this 
     } /* end of while */
     return 0; /* we never get here */
}

/******** DOSTUFF() *********************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
char* construct_response(char* url, char* type, int* return_size, int* return_header_len);
void parse(char* what_to_get, char* buffer);
int get_file_len(char* file_name);
char* get_content_type(char* what_to_get);
char* get_name(char* what_to_get);

void dostuff (int sock)
{
   int n;
   char buffer[1024];
   char *response;
   int size, header_len;
   char* file_url;
   char* file_type;
   FILE *fp;

   bzero(buffer,1024);
   n = read(sock,buffer,1024);
   if (n < 0) error("ERROR reading from socket"); 

   //Write the message into a output file record.txt in server directory
   fp = fopen("./record.txt", "wb");
   fputs(buffer, fp);
   fclose(fp);
   
   printf("Here is the message: \n%s\n", buffer);

   //Construct the response
   file_url = (char*)malloc(sizeof(char) * 256);
   parse(file_url, buffer);
   file_type = get_content_type(file_url + 1);
   //printf("file_url: %s\nfile_type: %s\n", file_url, file_type);#for debug
   response = construct_response(file_url, file_type, &size, &header_len);
   
   //if (1) {//If the request file type is html, output the file
   //   printf("Here is the html file: \n%s\n", &response[header_len]);
   //}
   
   n = send(sock, response, size + header_len, 0);
  
   if (n < 0) error("ERROR writing to socket");
}
/****************Construct response*****************
 Use the information provided by the parse function, 
 construct the response.
****************************************************/
char* construct_response(char* url, char* type, int* return_size, int* return_header_len)
{
   char *buf;//Used to store file content
   char *header;//The response header
   int size;//The file size
   char date[70];//Used in the Data field in response header
   time_t rawtime;
   struct tm *timeinfo;//Used to get time
   FILE * pFile;

   pFile = fopen(url, "rb");
   if (pFile != NULL && strcmp(url, "./") != 0) 
   {
      //Read file into buf
      fseek(pFile, 0, SEEK_END);
      size = ftell(pFile); //Size of the file
      *return_size = size;
      rewind(pFile);
   
      buf = (char*)malloc(size*sizeof(char));
      fread(buf, size, 1, pFile);

      //Extract the system time
      time(&rawtime);
      timeinfo = localtime(&rawtime);
      strftime(date, sizeof date, "%a, %d %b %Y %T", timeinfo);
      
      //Construct the response header
      header = (char*)malloc((1024+size) * sizeof(char));
      sprintf(header, "HTTP/1.1 200 OK\r\nConnection: close\r\nDate: %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n", date, type, size);
      int header_len = strlen(header); //Length of the response header
      *return_header_len = header_len;

      //Append the file content in the buffer to the header
      int i = 0;
      int j = header_len;
      while (i < size) 
      {
         header[j++] = buf[i++];
      }
      free(buf);
      fclose(pFile);
   }
   else 
   {
      header = (char*)malloc(sizeof(char) * 512);
      sprintf(header, "HTTP/1.1 404 NOT FOUND\r\n\r\n<h1 align=\'center\'>The file does not exist!</h1>");
      *return_size = 0;
      *return_header_len = strlen(header);     
   }
   
   return header;
}
/* URL parser
 * @what_to_get[OUT] output file name. (e.g. index.html, 1.jpeg, 1.gif)
 * @buffer[in] input URL
 */
void parse(char* what_to_get, char* buffer)
{
    int i;
    for(i = 0; buffer[i] != ' '; i++)
        continue;
    i++;
    int j;
    what_to_get[0] = '.';
    for(j = 1; buffer[i] != ' '; j++, i++)
    {
        what_to_get[j] = buffer[i];
    }
    what_to_get[j] = '\0';
}

/* get the type of a file
 * @what_to_get[IN] input file name
 * @return the type of the file (e.g. html, jpeg, gif)
 */
char* get_content_type(char* what_to_get)
{
    char* type_buf = (char*)malloc(256 * sizeof(char));
    int i, j;
    for(i = 0; what_to_get[i] != '.' && what_to_get[i] != '\0'; i++)
        continue;

    if(what_to_get[i] == '\0')
        return type_buf;

    i++;
    for(j = 0; what_to_get[i] != '\0'; i++, j++)
        type_buf[j] = what_to_get[i];
    type_buf[j] = '\0';

    char* content_type;
    content_type = (char*)malloc(sizeof(char) * 256);
    

    if (strcmp(type_buf, "html") == 0 || strcmp(type_buf, "htm") == 0) 
    {
        sprintf(content_type, "text/html");
        //printf("content_type: %s\n", content_type);
    } 
    else if (strcmp(type_buf, "jpg") == 0 || strcmp(type_buf, "gif") == 0 || strcmp(type_buf, "png") == 0) 
    {
        sprintf(content_type, "image/");
        content_type = strcat(content_type, type_buf);
    }
    else if (strcmp(type_buf, "jpeg") == 0)
    {
        sprintf(content_type, "image/jpg");
    }
    else if (strcmp(type_buf, "mp3") == 0)
    {
        sprintf(content_type, "audio/mpeg");
    }
    else if (strcmp(type_buf, "mp4") == 0)
    {
        sprintf(content_type, "video/mp4");
    }
    

    //printf("content_type: %s\n", content_type);#for debug
    return content_type;
}

