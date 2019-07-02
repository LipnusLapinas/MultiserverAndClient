#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>

#define PORT "3490"  // the port users will be connecting to

#define PORT2 "4000"

#define BACKLOG 10     // how many pending connections queue will hold

#define MAXDATASIZE 100

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
	int numbytes;
	char buf[MAXDATASIZE];
    int sockfd, sockfd2, new_fd, new_fd2;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr, their_addr2; // connector's address information
    socklen_t sin_size, sin_size2;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN], s2[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

//naujas socketas

	if ((rv = getaddrinfo(NULL, PORT2, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

	for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd2 = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket2");
            continue;
        }

        if (setsockopt(sockfd2, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt2");
            exit(1);
        }

        if (bind(sockfd2, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd2);
            perror("server: bind2");
            continue;
        }

        break;
    }

	freeaddrinfo(servinfo);

	if (p == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	if (listen(sockfd2, BACKLOG) == -1){
		perror("listen2");
		exit(1);
	}

//baigias

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
		sin_size2 = sizeof their_addr2;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		new_fd2 = accept(sockfd2, (struct sockaddr *)&their_addr2, &sin_size2);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
		if (new_fd2 == -1){
			perror("accept2");
			continue;
		}

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);

		inet_ntop(their_addr2.ss_family,
            get_in_addr((struct sockaddr *)&their_addr2),
            s2, sizeof s2);	
        printf("server: got connection from %s\n", s);
		printf("server: got connection from %s2\n", s2);

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
			close(sockfd2);

			if ((numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0)) == -1) {
				perror("recv");
				exit(1);
			}

			buf[numbytes] = '\0';

			printf("server: received '%s'\n", buf);

			for(int i = 0; i < strlen(buf); i++)
				buf[i] = toupper(buf[i]);

			if (send(new_fd2, buf, strlen(buf), 0) == -1)
				perror("send");
	
            close(new_fd);
			close(new_fd2);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
		close(new_fd2);
    }

    return 0;
}
