#include<stdio.h>
#include<pthread.h>
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>
#include<time.h>

static void* func(void* arg)
{
	printf("p ID:%d,tnew ID:%ld\r\n",getpid(),pthread_self());
	for(int i = 0;i < 10;i++)
	{
		printf("hello!\r\n");
		sleep(1);
	}
	return NULL;
}

int main(int argc,char *argv[])
{
	if(argc > 1)
	{
		printf("argv[0]:%s\r\n",argv[0]);
	}
	int fd,ret,dfd;
	ssize_t size;
	off_t off;
	pthread_t tid;
	char bufwrite[1024] = {"bestwishs mike"};
	char bufread[1024] = {0};

	ret = pthread_create(&tid,NULL,func,NULL);
	if(0 != ret)
	{
		perror("create error");
	}	
	printf("p ID:%d,t ID:%ld\r\n",getpid(),pthread_self());
	umask(0);

	fd = open("my.txt",O_EXCL);
	if(-1 == fd)
	{
		perror("check");
	}

    printf("fd:%d\r\n",fd);

    dfd = dup(fd);
    if(-1 == dfd)
    {
        perror("duplicate error");
    }

    printf("dfd:%d\r\n",dfd);

	fd = open("my.txt",O_CREAT|O_RDWR,0666);
	if(-1 == fd)
	{
		perror("open error");
		return fd;
	}

	sleep(5);

	printf("open file director:%d\r\n",fd);

	size = read(fd,bufread,sizeof(bufread));
	if(-1 == size)
	{
		perror("read error");
	}

	printf("bufread:%s\r\n",bufread);

	sleep(5);

	printf("move to file end!\r\n");
	off = lseek(fd,0,SEEK_END);
	if(-1 == off)
	{
		perror("lseek error");
	}
	printf("file size:%ld\r\n",off);

	off = lseek(fd,0,SEEK_SET);
	if(-1 == off)
	{
		perror("lseek error");
	}

	size = write(fd,bufwrite,sizeof(bufwrite));
	if(-1 == size)
	{
		perror("write error");
		return size;
	}

	off = lseek(fd,0,SEEK_SET);
	if(-1 == off)
	{
		perror("lseek error");
	}

	memset(bufread,0,sizeof(bufread));

	size = read(fd,bufread,sizeof(bufread));
	if(-1 == size)
	{
		perror("read error");
	}

	printf("bufread:%s\r\n",bufread);

	sleep(5);

	off = lseek(fd,0,SEEK_CUR);
	if(-1 == off)
	{
		perror("lseek error");
	}
	printf("current pos:%ld\r\n",off);

	sleep(5);

	printf("move to file end!\r\n");
	off = lseek(fd,0,SEEK_END);
	if(-1 == off)
	{
		perror("lseek error");
	}
	printf("file size:%ld\r\n",off);

	sleep(5);

	ret = close(fd);
	if(-1 == ret)
	{
		perror("close error");
	}

	sleep(5);

	return 0;
}
