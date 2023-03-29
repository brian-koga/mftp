# FinalProject-mftp

Fina; project for 360 to create a client server program that allows clients to look at, get and put files onto a server directory.

Initial comments:

4/26/20

mftp.c:
```
Brian Koga
CS 360 Final Project
Client program
includes commands for ls, remote ls, cd, remote cd, get a file from server
show a file from server and put a file on server, there is also an exit command
```

mftpserve.c
```
Brian Koga
CS 360 Final PRoject
Server program
Takes commands such as 'C' 'L' 'G' 'P' 'Q' that execute cd, ls, get, put,
quit. if there is data to send, it sends it to client
```

