#include <stdio.h>
#include <signal.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define NTHREADS 100
#define SBUFSIZE 100

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs, char *host, char *port);
void get_filetype(char *filename, char *filetype);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
int echo(char *host, char *path, char *port, int fd);

// 세마포어
// sbuf
/* $begin sbufc */
/* $begin sbuft */
typedef struct
{
  int *buf;    /* Buffer array */
  int n;       /* Maximum number of slots */
  int front;   /* buf[(front+1)%n] is first item */
  int rear;    /* buf[rear%n] is last item */
  sem_t mutex; /* Protects accesses to buf */
  sem_t slots; /* Counts available slots */
  sem_t items; /* Counts available items */
} sbuf_t;
/* $end sbuft */

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

void *thread(void *vargp);
sbuf_t ssbuf; /* Shared buffer of connected descriptors */

void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE], host[MAXLINE], port[MAXLINE] = "";
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))
  {
    printf("차단\n");
    clienterror(fd, method, "501", "Not implemented",
                "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);
  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs, host, port);
  if (stat(filename, &sbuf) < 0 && !strstr(uri, "http"))
  {
    clienterror(fd, filename, "404", "Not found",
                "Tiny couldn’t find this file");
    return;
  }
  printf("두잇 프록시\n");
  echo(host, cgiargs, port, fd);
}

void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];
  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs, char *host, char *port)
{
  char *ptr;
  // strstr 함수: 문자열 안에 특정 문자열이 있는지 탐색
  printf("파스 시작: %s\n", uri);
  if (!strstr(uri, "cgi-bin") && !strstr(uri, "http"))
  { /* Static content */
    printf("스태틱\n");
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri) - 1] == '/')
      strcat(filename, "adder.html");

    // strcat(filename, "home.html");
    else if (uri[strlen(uri) - 1] == '/')
      strcat(filename, "adder.html");
    return 1;
  }
  else if (strstr(uri, "http"))
  {
    /*프록시 가동*/
    printf("프록시 가동\n");
    // 쿼리로 포인터 이동

    char tmp[MAXLINE] = "";
    sscanf(uri, "http://%s", tmp);

    ptr = index(tmp, '/');
    *ptr = '\0';

    strcpy(cgiargs, "/"); // path 파싱
    strcat(cgiargs, ptr + 1);

    if (!strstr(tmp, ":"))
      // 포트넘버 없음
      strcpy(host, tmp);
    else
    {
      // 포트넘범 있음
      ptr = index(tmp, ':');
      *ptr = '\0';
      strcpy(host, tmp);     // 호스트
      strcpy(port, ptr + 1); // 포트넘버
    }

    // printf("호스트: %s\n", tmp);
    // printf("쿼리: %s\n", cgiargs);

    // ptr = index(uri, '?');
    // if (ptr)
    // {
    //   strcpy(cgiargs, ptr + 1);
    //   *ptr = '\0';
    // }
    // else
    //   strcpy(cgiargs, "");
    // strcpy(filename, ".");
    // strcat(filename, uri);
    // printf("쿼리: %s\n", cgiargs);
    // printf("파일명: %s\n", filename);
    return 2;
  }
}

int main(int argc, char **argv)
{

  signal(SIGPIPE, SIG_IGN); // sigpipe 무시
  int listenfd, connfd, pid;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;

  /* Check command-line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);

  sbuf_init(&ssbuf, SBUFSIZE);
  for (int i = 0; i < NTHREADS; i++) /* Create worker threads */
    Pthread_create(&tid, NULL, thread, NULL);

  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    sbuf_insert(&ssbuf, connfd);
  }
}

// void *thread(void *vargp){
//   Pthread_detach(pthread_self());
//   while (1)
//   {
//     /* code */
//     int connfd = subuf_remove(&sbuf);
//     echo_cnt(connfd);
//   }

// }

int echo(char *host, char *path, char *port, int fd)
{
  printf("에코 가동\n");
  printf("호스트: %s\n", host);
  printf("경로: %s\n", path);
  printf("포트: %s\n", port);

  int clientfd;
  char buf[100000];
  // rio_t rio;
  // host = "www.basics.re.kr";
  // port = "80";
  if (!strcmp(port, ""))
    clientfd = Open_clientfd(host, "80");
  else
    clientfd = Open_clientfd(host, port);

  ssize_t size = 0;
  // char message[] = "GET /main.do HTTP/1.1\r\nHost: www.basics.re.kr\r\n\r\n";
  // char message[] = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
  char message[MAXLINE] = "";
  sprintf(message, "GET %s HTTP/1.0\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\nConnection: close\r\nProxy-Connection: close\r\nHost: %s\r\n\r\n", path, host);
  printf("입력: %s\n", message);
  Rio_writen(clientfd, message, strlen(message));
  printf("응답\n");
  size = Rio_readn(clientfd, buf, 100000);
  // Fputs(buf, stdout);

  // printf("꾸꾸\n%s\n", buf);

  Close(clientfd);
  Rio_writen(fd, buf, size);
  // printf("%s\n", buf);
}

/* Create an empty, bounded, shared FIFO buffer with n slots */
/* $begin sbuf_init */
void sbuf_init(sbuf_t *sp, int n)
{
  sp->buf = Calloc(n, sizeof(int));
  sp->n = n;                  /* Buffer holds max of n items */
  sp->front = sp->rear = 0;   /* Empty buffer iff front == rear */
  Sem_init(&sp->mutex, 0, 1); /* Binary semaphore for locking */
  Sem_init(&sp->slots, 0, n); /* Initially, buf has n empty slots */
  Sem_init(&sp->items, 0, 0); /* Initially, buf has zero data items */
}
/* $end sbuf_init */

/* Clean up buffer sp */
/* $begin sbuf_deinit */
void sbuf_deinit(sbuf_t *sp)
{
  Free(sp->buf);
}
/* $end sbuf_deinit */

/* Insert item onto the rear of shared buffer sp */
/* $begin sbuf_insert */
void sbuf_insert(sbuf_t *sp, int item)
{
  P(&sp->slots);                          /* Wait for available slot */
  P(&sp->mutex);                          /* Lock the buffer */
  sp->buf[(++sp->rear) % (sp->n)] = item; /* Insert the item */
  V(&sp->mutex);                          /* Unlock the buffer */
  V(&sp->items);                          /* Announce available item */
}
/* $end sbuf_insert */

/* Remove and return the first item from buffer sp */
/* $begin sbuf_remove */
int sbuf_remove(sbuf_t *sp)
{
  int item;
  P(&sp->items);                           /* Wait for available item */
  P(&sp->mutex);                           /* Lock the buffer */
  item = sp->buf[(++sp->front) % (sp->n)]; /* Remove the item */
  V(&sp->mutex);                           /* Unlock the buffer */
  V(&sp->slots);                           /* Announce available slot */
  return item;
}
/* $end sbuf_remove */
/* $end sbufc */

// 쓰레드
void *thread(void *vargp)
{
  Pthread_detach(pthread_self());
  while (1)
  {
    signal(SIGPIPE, SIG_IGN);         // sigpipe 무시
    int connfd = sbuf_remove(&ssbuf); /* Remove connfd from buffer */
    doit(connfd);                     /* Service client */
    Close(connfd);
  }
}

// void *thread(void *vargp)
// {
//   int clientfd = *((int *)vargp);
//   Pthread_detach(pthread_self());
//   Free(vargp);
//   doit(clientfd);
//   Close(clientfd);
//   return NULL;
// }
