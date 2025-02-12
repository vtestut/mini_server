#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

int maxfd = 0, gid = 0;
int id[65536];
char *msg[65536];
fd_set read_set, write_set, current;
char send_buffer[42], recv_buffer[1001];

int extract_message(char **buf, char **msg)
{
	char *newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void err(char *msg)
{
	if(msg)
		write(2, msg, strlen(msg));
	else
	    write(2, "Fatal error", 11);
    write(2, "\n", 1);
	exit(1);
}

void send_to_all(int except, char *msg)
{
    for (int fd = 0; fd <= maxfd; fd++)
    {
        if (FD_ISSET(fd, &write_set) && fd != except)
            send(fd, msg, strlen(msg), 0);
    }
}

void client_arrived(int fd)
{
    id[fd] = gid++;
    msg[fd] = NULL;
    FD_SET(fd, &current);
    sprintf(send_buffer, "server: client %d just arrived\n", id[fd]);
    send_to_all(fd, send_buffer);
    if (fd > maxfd)
        maxfd = fd;
}

void client_left(int fd)
{
    sprintf(send_buffer, "server: client %d just left\n", id[fd]);
    send_to_all(fd, send_buffer);
    free(msg[fd]);
    FD_CLR(fd, &current);
    close(fd);
}

void send_msg(int fd)
{
    char *line;
    int ret;
    while ((ret = extract_message(&msg[fd], &line)) > 0)
    {
        sprintf(send_buffer, "client %d: ", id[fd]);
        send_to_all(fd, send_buffer);
        send_to_all(fd, line);
        free(line);
    }
    if (ret == -1)
        err(NULL);
}

int main(int ac, char **av)
{
    if (ac != 2)
		err("Wrong number of arguments");

    FD_ZERO(&current);
    int serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0)
        err(NULL);
    
	FD_SET(serverfd, &current);
    maxfd = serverfd;
    struct sockaddr_in serveraddr;
    bzero(&serveraddr, sizeof(serveraddr));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(2130706433); // = 127.0.0.1
    serveraddr.sin_port = htons(atoi(av[1]));

    if (bind(serverfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0 || listen(serverfd, 100) < 0)
        err(NULL);

    while (1)
    {
        read_set = write_set = current;

        if (select(maxfd + 1, &read_set, &write_set, NULL, NULL) < 0)
            err(NULL);
        
		for (int fd = 0; fd <= maxfd; fd++)
        {
            if (!FD_ISSET(fd, &read_set)) 
                continue;

            if (fd == serverfd)
            {
                struct sockaddr_in clientaddr;
                socklen_t len = sizeof(clientaddr);
                int clientfd = accept(serverfd, (struct sockaddr *)&clientaddr, &len);
                if (clientfd < 0) continue;
                client_arrived(clientfd);
            }
            else
            {
                int ret = recv(fd, recv_buffer, 1000, 0);
                if (ret <= 0)
                {
                    client_left(fd);
                    continue;
                }
                recv_buffer[ret] = '\0';
                msg[fd] = str_join(msg[fd], recv_buffer);
                if (!msg[fd])
                    err(NULL);
                send_msg(fd);
            }
        }
    }
    return 0;
}