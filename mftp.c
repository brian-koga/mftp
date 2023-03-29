// Brian Koga
// CS 360 Final Project
// Client program
// includes commands for ls, remote ls, cd, remote cd, get a file from server
// show a file from server and put a file on server, there is also an exit command

#include "mftp.h"

char response[50];
char* hostString;

// creates a socket that connects to the host if port is NULL, a default port is used
// output: socket file descriptor
int createSocket(char* host, char* port) {
  if(port == NULL) {
    port = PORT_NUMBER_STR;
  }
  int sockfd;

  // set up server address
  struct addrinfo hints, *actualdata;
  memset(&hints, 0, sizeof(hints));
  int err;

  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_INET;
  err = getaddrinfo(host, port, &hints, &actualdata);
  if(err) {
    fprintf(stderr, "Error: %s\n", gai_strerror(err));
    exit(1);
  }
  sockfd = socket(actualdata->ai_family, actualdata->ai_socktype, 0);

  // attempt to connect to server
  if(connect(sockfd, actualdata->ai_addr, actualdata->ai_addrlen) < 0) {
    // failed to connect to server
    printf("Error: %s\n", strerror(errno));
    exit(1);
  } else {
    return(sockfd);
  }
}

// Receives a response from the server connected with socketfd
// The message will either start with an A or an E
// If A, and there is no message return a status of 0, meaning success
// If A, and there is more to message, record it in response and return 0
// If E, record error message, and return a status of -1 meaning error
int getResponse(int socketfd) {
  char c;
  int status = 0;
  int index = 0;
  int actual = read(socketfd, &c, 1);
  // check for unexpected server socket closure
  if(actual == 0) {
    printf("Error: control socket closed unexpectedly\n");
    exit(-1);
  }

  // response will either be E or A
  // if A, set status to 0, if next char is new line and return "A"
  if(c == 'A') {
    // first char was A
    status = 0;
    read(socketfd, &c, 1);
    if(c == '\n') {
      return status;
    }
  } else {
    // first char was E
    read(socketfd, &c, 1);
    status = -1;
  }
  // either E and rest of message is the error message
  // or A and rest of message is the port from a 'D' command
  while(c != '\n') {
    response[index] = c;
    index++;
    read(socketfd, &c, 1);
  }
  response[index] = '\0';
  return status;
}


// Creates a data socket connection
// input: the socket that has the port number to create the data connection
// output: the data connection socket file descriptor
int createDataConnection(int socketfd) {
  int dataSocketfd = -1;
  // send dataconnection command to server
  int actual = write(socketfd, "D\n", 2);
  int status = getResponse(socketfd);
  if(status == -1) {
    return(status);
  }
  // if status is not -1, then the socket was successfully created,
  // and the port number is in response

  dataSocketfd = createSocket(hostString, response);
  fflush(stdout);
  // wait for acknowledgement
  return(dataSocketfd);
}

// function that returns 1 if pathname is a file, 0 otherwise
int isFile (char *pathName) {
    struct stat area, *s= &area;
    return (stat(pathName, s) == 0) && S_ISREG(s->st_mode);
  }

// function that returns 1 if pathname is a directory, 0 otherwise
int isDir (char *pathName) {
    struct stat area, *s= &area;
    return (stat(pathName, s) == 0) && S_ISDIR(s->st_mode);
  }

// runs the ls command on the client
// uses fork and exec to display the results of ls 20 lines at a time
int runls() {
  int fd[2];
  pipe(fd);
  int status;
  int s_in = dup(0); // "save" stdin
  if(fork()) {
      //parent will read from pipe
      close(fd[1]);
      //close stdin
      close(0);
      //"reset" stdin to fd read
      dup(fd[0]);
      //wait for child to run
      wait(&status);
      // try to execute, file descriptor 0 is the out part of fd
      execlp("more", "more", "-20", NULL);

      // will only happen upon failure, so restore stdin
      close(fd[0]);
      dup2(s_in, 0);
      close(s_in);
      dprintf(2, "%s\n", strerror(errno));
      return(-1);
    } else {
      //child will write to pipe
      close(fd[0]);
      //close stdout
      close(1);
      //"reset" stdout to fd write
      dup(fd[1]);
      // try to execute, file descriptor 1 is the in part of fd
      execlp("ls", "ls", "-la", NULL);
      close(fd[1]);
      exit(0);
    }
}


// runClient loop, prompts user for commands/arguments and runs
// the appropriate command, or informs the user an unrecognized
// command was received
int runClient(int socketfd) {
  while(1) {
    printf("MFTP> ");
    fflush(stdout);
    // read the input into a buffer
    int numRead;
    char buf[100] = {0};
    int index = 0;
    int cont = 1;
    int newWord = 1;
    char c;
    numRead = read(0, &c, 1);

    while(numRead > 0 && cont) {
      if(isspace(c) == 0) {
        //Meaningful character
        buf[index] = c;
        index++;
        newWord = 0;
      } else {
        // space
        if(newWord == 0) {
          // first whitespace encountered after word
          // add a space so that any tab is reduced to  a single space
          buf[index] = ' ';
          index++;
          newWord = 1;
        } // want to ignore multiple spaces
      }
      numRead = read(0, &c, 1);
      //if new line was read, close loop
      if(c == '\n') {
        cont = 0;
      }
    }

    // parse into tokens
    char* input[2];
    input[0] = strtok(buf, " ");
    input[1] = strtok(NULL, " ");


    char* command = input[0];

    //****** exit command *******
    if(strcmp(command, "exit") == 0) { // send exit command to server, then exit
      char c = 'Q';
      // send quit command to server
      int actual = write(socketfd, &c, 1);
      c = '\n';
      write(socketfd, &c, 1);
      // check acknowledgement
      int res = getResponse(socketfd);
      if(res == 0) {
        // exit message acknowledged
        exit(0);
      } else {
        // error
        printf("Error: %s\n", response);
        fflush(stdout);
      }

    // ****** cd command **********
    } else if(strcmp(command, "cd") == 0) { // change local directory
      if(input[1] == NULL) {
        printf("Command error: expecting a parameter\n");
      } else {
        if(chdir(input[1]) == -1) {
          printf("Change directory: %s\n", strerror(errno));
        }
      }

    // ******** rcd command **********
    } else if(strcmp(command, "rcd") == 0) {
      if(input[1] == NULL) {
        printf("Command error: expecting a parameter\n");
      } else {
        char c = 'C';
        int actual = write(socketfd, &c, 1);
        actual = write(socketfd, input[1], strlen(input[1]));
        c = '\n';
        write(socketfd,&c, 1);

        int res = getResponse(socketfd);
        if(res == -1) {
          // error
          printf("Error response from server: %s\n", response);
          fflush(stdout);
        }
      }
    // *********** ls command *************
    } else if(strcmp(command, "ls") == 0) {
      int status;
      int forkRet = fork();
      if(forkRet == 0) {
        runls();
      } else {
        // parent will wait for child
        wait(&status);
      }

    // *********** rls command *************
    } else if(strcmp(command, "rls") == 0) {
      printf("Command was rls\n");
      int status;
      int dataSocketfd = createDataConnection(socketfd);

      /// send L command
      int actual = write(socketfd, "L\n", 2);
      int stat = getResponse(socketfd);
      if(dataSocketfd != -1) {
        if(fork()) {
            //wait for child to run
            wait(&status);
            // done with rls command
            // close dataSocket
            close(dataSocketfd);
          } else {
            //child
            //close stdin
            close(0);
            //"reset" stdin to dataSocketfd
            dup(dataSocketfd);
            // try to execute, file descriptor 0 is the dataSocketfd
            execlp("more", "more", "-20", NULL);
            // will only happen upon failure
            dprintf(2, "%s\n", strerror(errno));
            exit(-1);
          }
      } else {
        //error creating data socket
        printf("Error response from server: %s\n", response);
        fflush(stdout);
      }

    // *********** get command *************
    } else if(strcmp(command, "get") == 0) {

      if(input[1] == NULL) {
        printf("Command error: expecting a parameter\n");
      } else {
        //Get last component of pathname
        char* fileName;
        char* newFileName = strtok(input[1], "/");
        // if no '/', then fileName is just pathname
        if(newFileName == NULL) {
          fileName = newFileName;
        }
        // separate pathname into tokens until the final one is found
        while(newFileName != NULL) {
          fileName = newFileName;
          newFileName = strtok(NULL, "/");
        }

        int dataSocketfd = createDataConnection(socketfd);

        // make sure data socket was created successfully
        if(dataSocketfd != -1) {
          // send G command
          char c = 'G';
          int actual = write(socketfd, &c, 1);
          actual = write(socketfd, input[1], strlen(input[1]));
          c = '\n';
          write(socketfd, &c, 1);
          // get response from server
          int res = getResponse(socketfd);
          if(res == -1) {
            // error
            printf("Error response from server: %s\n", response);
            fflush(stdout);
          } else {
            //file exists on server


            if(isFile(fileName)) {
              // local file of same name
              printf("Open/creating local file: File exists\n");
              fflush(stdout);
            } else {
              int fd = open(fileName, O_CREAT | O_RDWR);
              if(fd > -1) {
                // read file from server
                char nextCharacter;
                int charsRead = read(dataSocketfd, &nextCharacter, 1);
                // loop until read returns 0, meaning the end of file has been reached
                while(charsRead) {
                  write(fd, &nextCharacter, 1);
                  charsRead = read(dataSocketfd, &nextCharacter, 1);
                }
                close(fd);
                close(dataSocketfd);
              } else {
                // could not open file
                printf("Open/creating local file: Could not create file\n");
                fflush(stdout);
              }
            }
          }
        }
      }


    // *********** show command *************
    } else if(strcmp(command, "show") == 0) {
      if(input[1] == NULL) {
        printf("Command error: expecting a parameter\n");
      } else {
        int status;
        int dataSocketfd = createDataConnection(socketfd);

        // make sure data socket was created successfully
        if(dataSocketfd != -1) {
          // send G command, then path of file to get, then new line
          char c = 'G';
          int actual = write(socketfd, &c, 1);
          actual = write(socketfd, input[1], strlen(input[1]));
          c = '\n';
          write(socketfd,&c, 1);
          // get response from server
          int res = getResponse(socketfd);
          if(res == -1) {
            // error
            printf("Error response from server: %s\n", response);
            fflush(stdout);
          } else {

            int forkRet = fork();
            if(forkRet == 0) {
              //child will handle the show
              // close stdin
              close(0);
              // "reset" stdin to to the data socket fd
              dup(dataSocketfd);
              // try to execute, file descriptor 0 is the dataSocketfd
              execlp("more", "more", "-20", NULL);
              // will only happen upon failure
              dprintf(2, "%s\n", strerror(errno));
              exit(-1);
            } else {
              // parent will wait for child
              wait(&status);
              // close data socket
              close(dataSocketfd);
            }
          }
        }
      }
    // *********** put command *************
    } else if(strcmp(command, "put") == 0) {
      if(input[1] == NULL) {
        printf("Command error: expecting a parameter\n");
      } else {
        // first, check if file path exists and is a regular file
        // Then send request to server
        // wait for acknowledgement response
        // if A, send file
        // if  E, display error

        //Get last component of pathname
        char* fileName;
        char* newFileName = strtok(input[1], "/");
        // if no '/', then fileName is just pathname
        if(newFileName == NULL) {
          fileName = newFileName;
        }
        // separate pathname into tokens until the final one is found
        while(newFileName != NULL) {
          fileName = newFileName;
          newFileName = strtok(NULL, "/");
        }


        if(isDir(input[1])) {
          // local file does not exist (or is not a normal file)
          printf("Local file %s is a directory, command ignored\n", input[1]);
          fflush(stdout);
        } else {
          int fd = open(fileName, O_RDONLY, 0);
          if(fd > -1) {
            int dataSocketfd = createDataConnection(socketfd);

            // make sure data socket was created successfully
            if(dataSocketfd != -1) {
              // send P command, then the name of the file to create, then a new line
              char c = 'P';
              int actual = write(socketfd, &c, 1);
              actual = write(socketfd, fileName, strlen(fileName));
              c = '\n';
              write(socketfd, &c, 1);
              // get response from server
              int res = getResponse(socketfd);
              if(res == -1) {
                // error
                printf("Error response from server: %s\n", response);
                fflush(stdout);
              } else {
                // Server is ready to receive, no errors locally or on server
                // write file to server
                char nextCharacter;
                int charsRead = read(fd, &nextCharacter, 1);
                // loop until read returns 0, meaning the end of file has been reached
                while(charsRead) {
                  write(dataSocketfd, &nextCharacter, 1);
                  charsRead = read(fd, &nextCharacter, 1);
                }
                close(fd);
                close(dataSocketfd);
              }
            }
          } else {
            // could not open file
            printf("Opening file for reading: %s\n", strerror(errno));
            fflush(stdout);
          }
        }
      }
    } else {
      printf("Command '%s' is unknown - ignored\n", command);
    }
  }
}

// main function, sets up the server address then attecpts to connect
int main(int argc, char **argv) {
  int socketfd = createSocket(argv[1], NULL);
  hostString = argv[1];
  if(socketfd == -1) {
    printf("Error connecting to server %s\n", argv[1]);
  } else {
    printf("Connected to server %s\n", argv[1]);
    runClient(socketfd);
  }


}
