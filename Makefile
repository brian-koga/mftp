all: mftpserve.c mftp.c mftp.h
	gcc -o mftpserve mftpserve.c
	gcc -o mftp mftp.c

clean:
	rm -f mftp mftpserve

run: mftp
	./mftp localhost

runServer: mftpserve
	./mftpserve
