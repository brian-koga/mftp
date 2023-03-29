// Brian Koga
// CS 360 Final PRoject
// Server program
// Takes commands such as 'C' 'L' 'G' 'P' 'Q' that execute cd, ls, get, put,
// quit. if there is data to send, it sends it to client

#include "mftp.h"


// creates a socket that connects to localhost
// input: port number, if input == -1, a default port is used
// output: socket file descriptor
int createSocket(int port) {
  if(port == -1) {
    port = PORT_NUMBER;
  }
  int socketfd;
  socketfd = socket(AF_INET, SOCK_STREAM, 0);

  int ret = setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
  if(ret == -1) {
    return(-1);
  }

  struct sockaddr_in servAddr;

  memset(&servAddr, 0, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_port = htons(port);
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

  if(bind(socketfd, (struct sockaddr*) &servAddr, sizeof(struct sockaddr_in))) {
    fprintf(stderr, "Error: %s\n", strerror(errno));
    fflush(stderr);
    return(-1);
  }
  return(socketfd);
}

// Sends a mesage to the client on listenfd, of the form E<err>\n
int errorResponse(int listenfd, char *err) {
  int actual = write(listenfd, "E", 1);
  actual = write(listenfd, err, strlen(err));
  char c = '\n';
  write(listenfd, &c, 1);
  if(actual != strlen(err)) {
    printf("Could not send error message to client\n");
    fflush(stdout);
    return(-1);
  }
  return(0);
}

// function that returns 1 if pathname is a directory, 0 otherwise
int isDir (char *pathName) {
    struct stat area, *s= &area;
    return (stat(pathName, s) == 0) && S_ISDIR(s->st_mode);
  }

// function that returns 1 if pathname is a file, 0 otherwise
int isFile (char *pathName) {
    struct stat area, *s= &area;
    return (stat(pathName, s) == 0) && S_ISREG(s->st_mode);
  }

// main program, creates a socket with default port number
// starts the server that listens for and accepts connections (backlog of 4)
// when a connection is made the process forks and lets the child handle the
// connection, while the parent logs the connection and returns to listening
int main(int argc, char **argv) {
  int listenfd, socketfd;
  socketfd = createSocket(-1);

  if(socketfd == -1) {
    printf("Fatal error binding socket -> %s\n", strerror(errno));
    exit(-1);
  }

  listen(socketfd, 4);
  int length = sizeof(struct sockaddr_in);
  struct sockaddr_in clientAddr;
  // Listen loop
  while(1) {
    listenfd = accept(socketfd, (struct sockaddr *) &clientAddr, &length);
    int forkRet = fork();
    if(forkRet == 0) {
      // child
      int dataSocketfd, dataListenfd;
      int dataSocketExists = 0;
      int myID = getpid();
      char command;
      char arg[50];
      char c;
      int index;
      // command listen loop
      while(1) {
        int actual = read(listenfd, &command, 1);

        // the socket must have been closed
        if(actual == 0) {
          printf("Child %d: Control Socket EOF detected, exiting.\n", myID);
          printf("Child %d: Fatal error, exiting\n", myID);
          fflush(stdout);
          exit(0);
        }

        //****** establish data connection command *******
        if(command == 'D') { // client wishes to establish a data connection
          read(listenfd, &c, 1); // consume new line ?? EOF??

          dataSocketfd = createSocket(0); // the 0 means "choose a port"

          fflush(stdout);
          if(dataSocketfd == -1) {
            //error occurred
            fprintf(stderr, "Error: %s\n", strerror(errno));
            fflush(stderr);
            errorResponse(listenfd, strerror(errno));
          }

          // get port of datasocket
          struct sockaddr_in dataServAddr;
          memset(&dataServAddr, 0, sizeof(dataServAddr));
          int servAddrLength = sizeof(dataServAddr);

          getsockname(dataSocketfd, (struct sockaddr*)&dataServAddr, &servAddrLength);

          int assignedPort = ntohs(dataServAddr.sin_port);

          // save the port as a string
          char portAsString[9];
          int charactersPrinted = snprintf(portAsString, 8, "%d", assignedPort);
          portAsString[charactersPrinted] = '\0';

          write(listenfd, "A", 1); // acknowledge
          // write the port for the child to make the data connection
          write(listenfd, &portAsString, strlen(portAsString));
          write(listenfd, "\n", 1);

          // listen for and accept the clients connection
          listen(dataSocketfd, 1);
          int dataSocketLength = sizeof(struct sockaddr_in);
          struct sockaddr_in clientDataAddr;
          dataListenfd = accept(dataSocketfd, (struct sockaddr *) &clientDataAddr, &dataSocketLength);
          dataSocketExists = 1;


          fflush(stdout);

        //****** change directory command *******
        } else if(command == 'C') {
          // read in the path to change to
          read(listenfd, &c, 1);
          index = 0;
          while(c != '\n') {
            arg[index] = c;
            index++;
            read(listenfd, &c, 1);
          }
          arg[index] = '\0';
          // record error to stdout and client
          if(chdir(arg) == -1) {
            // log cd failure
            printf("Child %d: cd to %s failed with error %s\n", myID, arg, strerror(errno));
            errorResponse(listenfd, strerror(errno));
          } else {
            // log cd action
            printf("Child %d: Changed current directory to %s\n", myID, arg);
            write(listenfd, "A\n", 2); // acknowledge
          }

        //****** List Directory command *******
        } else if(command == 'L') {
          int status;
          read(listenfd, &c, 1); // consume new line ??
          // check if data connection exists
          if(dataSocketExists) {
            write(listenfd, "A\n", 2); // acknowledge
            int forkReturn = fork();
            if(forkReturn > 0) {
              //parent
              wait(&status);
              //close data socket
              close(dataListenfd);
              close(dataSocketfd);
              dataSocketExists = 0;

            } else if(forkReturn == 0) {
              //child will handle the ls
              //close stdout
              close(1);
              // set fd 1 to dataListenfd
              dup(dataListenfd);
              execlp("ls", "ls", "-la", NULL);
              // unlikely, but ls may fail, communicate this to client
              errorResponse(listenfd, "Unable to run ls command");
              exit(-1);
            } else {
              // error
              printf("Error: unable to fork for ls command.\n");
              fflush(stdout);
              errorResponse(listenfd, "Unable to run ls command");
            }
          } else {
            errorResponse(listenfd, "No data connection established");
          }

        //****** Get a file command *******
        } else if(command == 'G') {
          int status;
          // read in the path to get
          read(listenfd, &c, 1);
          index = 0;
          while(c != '\n') {
            arg[index] = c;
            index++;
            read(listenfd, &c, 1);
          }
          arg[index] = '\0';

          // log read action
          printf("Child %d: Reading file %s\n", myID, arg);
          fflush(stdout);
          if(isDir(arg) == 0) {
            // check if data connection exists
            if(dataSocketExists) {
              int fd = open(arg, O_RDONLY, 0);
              if(fd > -1) {
                write(listenfd, "A\n", 2); // acknowledge
                // successfully opened file
                // log transmission action
                printf("Child %d: Transmitting file %s to client\n", myID, arg);
                fflush(stdout);
                char nextCharacter;
                int actual = read(fd, &nextCharacter, 1);
                // loop until read returns 0, meaning the end of file has been reached
                while(actual) {
                  write(dataListenfd, &nextCharacter, 1);
                  actual = read(fd, &nextCharacter, 1);
                }
                close(fd);
                close(dataListenfd);
                close(dataSocketfd);
                dataSocketExists = 0;

              } else { // error opening file
                errorResponse(listenfd, strerror(errno));
              }
            } else { // data connection not established
              errorResponse(listenfd, "No data connection established");
            }

          } else { // no file
            errorResponse(listenfd, "File is a directory");
          }


        //****** Put a file command *******
        } else if(command == 'P') {

          // get path from client
          // try to create file, if can't send error
          // else read in file contents

          int status;
          // read in the path to put
          read(listenfd, &c, 1);
          index = 0;
          while(c != '\n') {
            arg[index] = c;
            index++;
            read(listenfd, &c, 1);
          }
          arg[index] = '\0';

          // log write action
          printf("Child %d: Writing file %s\n", myID, arg);
          fflush(stdout);
          if(isFile(arg) == 0) { // no file with same name exists
            // check if data connection exists
            if(dataSocketExists) {
              int fd = open(arg, O_CREAT | O_RDWR);
              if(fd > -1) {
                write(listenfd, "A\n", 2); // acknowledge
                // successfully created file
                // log transmission action
                printf("Child %d: Receiving file %s from client\n", myID, arg);
                fflush(stdout);

                // read file from client
                char nextCharacter;
                int charsRead = read(dataListenfd, &nextCharacter, 1);
                // loop until read returns 0, meaning the end of file has been reached
                while(charsRead) {
                  write(fd, &nextCharacter, 1);
                  charsRead = read(dataListenfd, &nextCharacter, 1);
                }

                close(fd);
                close(dataListenfd);
                close(dataSocketfd);
                dataSocketExists = 0;

              } else { // error opening file
                errorResponse(listenfd, strerror(errno));
              }
            } else { // data connection not established
              errorResponse(listenfd, "No data connection established");
            }
          } else { // file already exists
            errorResponse(listenfd, "File exists");
          }


        //****** quit command *******
        } else if(command == 'Q') { // client wishes to exit
          read(listenfd, &c, 1);

          write(listenfd, "A\n", 2); // acknowledge
          printf("Child %d: Quitting\n", myID);
          fflush(stdout);
          exit(0);
        } else {
            errorResponse(listenfd, "Command not recognized\n");
          exit(1);
        }
      }

    } else {
      // parent

      //Get hostname
      char hostName[NI_MAXHOST];
      int hostEntry;

      if(hostEntry = getnameinfo((struct sockaddr*)&clientAddr, sizeof(clientAddr),
                  hostName, sizeof(hostName), NULL,0, NI_NUMERICSERV)) {
                    fprintf(stderr, "Error: %s\n", gai_strerror(hostEntry));
                    fflush(stderr);
                    exit(1);
                  }
      // print client hostname and pid
      printf("Child %d: Connection accepted from host %s\n", forkRet, hostName);
      fflush(stdout);

      // wait with no hangup, in case other children have exited
      waitpid(-1, NULL, WNOHANG);
    }
  }
}
