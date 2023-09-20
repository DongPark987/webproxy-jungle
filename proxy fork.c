#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

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
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
int echo(char *host, char *path, char *port, int fd);






void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
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
  printf("여기까지: %s\n", uri);
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);
  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs, host, port);

  printf("두잇 프록시\n");
  echo(host, cgiargs, port, fd);
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
  {
    strcpy(host, tmp); // 포트넘버 없음
  }
  else
  {
    ptr = index(tmp, ':'); // 포트넘버 있음
    *ptr = '\0';
    strcpy(host, tmp);     // 호스트
    strcpy(port, ptr + 1); // 포트넘버
  }

  return 1;
}

int main(int argc, char **argv)
{
  int listenfd, connfd, pid;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command-line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);

  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    pid = fork();
    switch (pid)
    {
    case 0 /* constant-expression */:
      Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE,
                  port, MAXLINE, 0);
      printf("Accepted connection from (%s, %s)\n", hostname, port);
      doit(connfd);
      Close(connfd);
      exit(0);
    }
  }
}

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
  char message[MAXLINE] = "";
  sprintf(message, "GET %s HTTP/1.1\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\nConnection: close\r\nProxy-Connection: close\r\nHost: %s\r\n\r\n", path, host);
  // printf("입력: %s\n", message);
  Rio_writen(clientfd, message, strlen(message));
  // printf("응답\n");
  size = Rio_readn(clientfd, buf, 100000);

  Close(clientfd);
  Rio_writen(fd, buf, size);
  // printf("%s\n", buf);
}
