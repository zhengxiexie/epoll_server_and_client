#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

#define MAXEVENTS 64 //epoll支持的最大的监听事件个数

/*typedef union epoll_data*/
/*{*/
	/*void        *ptr;*/
	/*int          fd;*/
	/*__uint32_t   u32;*/
	/*__uint64_t   u64;*/

/*} epoll_data_t;*/

/*struct epoll_event*/
/*{*/
	/*__uint32_t   events; [> Epoll events <]*/
	/*epoll_data_t data;   [> User data variable <]*/

/*};*/

/*struct addrinfo*/
/*{*/
	/*int              ai_flags;*/
	/*int              ai_family;*/
	/*int              ai_socktype;*/
	/*int              ai_protocol;*/
	/*size_t           ai_addrlen;*/
	/*struct sockaddr *ai_addr;*/
	/*char            *ai_canonname;*/
	/*struct addrinfo *ai_next;*/

/*};*/

/*设置一个socket为非阻塞，如果是监听socket，则accept()为非阻塞
如果其他的socket，则read(), write()为非阻塞*/
static int make_socket_non_blocking (int sfd)
{
	int flags, s;

	flags = fcntl (sfd, F_GETFL, 0);
	if (flags == -1) {
		perror ("fcntl");
		return -1;
	}

	flags |= O_NONBLOCK; //设置非阻塞
	s = fcntl (sfd, F_SETFL, flags);
	if (s == -1) {
		perror ("fcntl");
		return -1;
	}

	return 0;
}

/* 这一步一般不会太大的改变 */
static int create_and_bind (char *port)
{
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s, sfd;

	memset (&hints, 0, sizeof (struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	s = getaddrinfo (NULL, port, &hints, &result);
	if (s != 0) {
		fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next)
	{
		sfd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1)
			continue;

		s = bind (sfd, rp->ai_addr, rp->ai_addrlen);
		if (s == 0) {
			/* 成功绑定 */
			break;
		}

		close (sfd);
	}

	if (rp == NULL) {
		fprintf (stderr, "Could not bind\n");
		return -1;
	}

	freeaddrinfo (result);

	return sfd;
}

int main (int argc, char *argv[])
{
	int sfd, s;
	int efd;
	struct epoll_event event;
	struct epoll_event *events;

	if (argc != 2) {
		fprintf (stderr, "Usage: %s [port]\n", argv[0]);
		exit (EXIT_FAILURE);
	}

	sfd = create_and_bind (argv[1]);
	if (sfd == -1) abort ();

	s = make_socket_non_blocking (sfd);
	if (s == -1) abort ();

	s = listen (sfd, SOMAXCONN);
	if (s == -1) {
		perror ("listen");
		abort ();
	}

	efd = epoll_create1 (0);
	if (efd == -1) {
		perror ("epoll_create");
		abort ();
	}

	/*EPOLLET边缘触发模式, EPOLLIN连接到达，或者有数据来临*/
	event.data.fd = sfd;
	event.events = EPOLLIN | EPOLLET;
	s = epoll_ctl (efd, EPOLL_CTL_ADD, sfd, &event);
	if (s == -1) {
		perror ("epoll_ctl");
		abort ();
	}

	/*这里就是存放所有即将发生的事件, 只要socket上又事件发生，就会放进events集合里*/
	events = calloc (MAXEVENTS, sizeof event);

	/* The event loop */
	while (1)
   	{
		int n, i;

		/*这一步阻塞，直到epoll监听到所有发生的事件，放进events*/
		n = epoll_wait (efd, events, MAXEVENTS, -1);

		/* 进入循环，处理事件 */
		for (i = 0; i < n; i++)
		{

			/*EPOLLERR服务器自己出错，EPOLLHUP对端关闭，EPOLLIN对端有数据*/
			if ((events[i].events & EPOLLERR) ||
					(events[i].events & EPOLLHUP) ||
					(!(events[i].events & EPOLLIN))) {
				fprintf (stderr, "epoll error\n");
				close (events[i].data.fd);
				continue;
			}

			/*如果是监听socket，有事件发生的意思就是说明有一个或者多个新的链接过来了*/
			else if (sfd == events[i].data.fd)
			{
				while (1)
				{
					struct sockaddr in_addr;
					socklen_t in_len;
					int infd;
					char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

					in_len = sizeof in_addr;
					/*因为已经设置了监听socket为非阻塞的了，所以这一步不会阻塞*/
					infd = accept (sfd, &in_addr, &in_len);
					if (infd == -1) {
						if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
							break; //已经accept完所有的链接了
						}
						else {
							perror ("accept");
							break;
						}
					}

					s = getnameinfo (&in_addr, in_len, // 得到所有的链接信息
							hbuf, sizeof hbuf,
							sbuf, sizeof sbuf,
							NI_NUMERICHOST | NI_NUMERICSERV);
					if (s == 0) {
						printf("Accepted connection on descriptor %d "
								"(host=%s, port=%s)\n", infd, hbuf, sbuf);
					}

					/*除了要设置监听socket为非阻塞，还要设置新来的socket为非阻塞，这样后面的read()，write()也为非阻塞了*/
					s = make_socket_non_blocking (infd);
					if (s == -1) abort ();

					event.data.fd = infd;
					event.events = EPOLLIN | EPOLLET;
					/*将新的socket注册到epoll，同时关联到这个socket需要被关注的事件*/
					s = epoll_ctl (efd, EPOLL_CTL_ADD, infd, &event);
					if (s == -1)
					{
						perror ("epoll_ctl");
						abort ();
					}
				}
				continue;
			}
			/*非监听socket上有事件，说明有写数据了，我们这个时候要把所有的数据读完，因为是edge-triggered模式，
			 不读完的话，下次针对余下的数据, 就不再会有事件了*/
			else
			{
				int done = 0;

				while (1)
				{
					ssize_t count;
					char buf[512];

					count = read (events[i].data.fd, buf, sizeof buf); //read()非阻塞
					if (count == -1) {
						if (errno != EAGAIN) //read()非阻塞,目前没有可读数据，不阻塞直接回到主循环
						{
							perror ("read");
							done = 1;
						}
						break;
					}
					else if (count == 0) {//已经读完所有数据
						done = 1;
						break;
					}

					s = write (1, buf, count); // 把buf数据写到终端
					if (s == -1) {
						perror ("write");
						abort ();
					}
				}

				if (done) {
					printf ("Closed connection on descriptor %d\n",
							events[i].data.fd);
					close (events[i].data.fd); // 关闭当前socket会将其移出epoll监听，因为这条链接已经完成任务了嘛
				}
			}
		}
	}

	free (events);
	close (sfd);
	return EXIT_SUCCESS;
}
