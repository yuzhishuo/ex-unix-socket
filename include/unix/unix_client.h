#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <condition_variable>
#include <mutex>

#include "constex.h"

void unix_client(const char* server_path, const char* client_path,
                 std::condition_variable& cond, std::mutex& mutex,
                 std::atomic_bool& atomic) {
  struct sockaddr_un cliun, serun;
  int len;
  int sockfd, n;

  if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    perror("client socket error");
    exit(1);
  }

  // 一般显式调用bind函数，以便服务器区分不同客户端
  memset(&cliun, 0, sizeof(cliun));
  cliun.sun_family = AF_UNIX;
  strcpy(cliun.sun_path, client_path);
  len = offsetof(struct sockaddr_un, sun_path) + strlen(cliun.sun_path);
  unlink(cliun.sun_path);
  if (bind(sockfd, (struct sockaddr*)&cliun, len) < 0) {
    perror("bind error");
    exit(1);
  }

  memset(&serun, 0, sizeof(serun));
  serun.sun_family = AF_UNIX;
  strcpy(serun.sun_path, server_path);
  len = offsetof(struct sockaddr_un, sun_path) + strlen(serun.sun_path);
  if (connect(sockfd, (struct sockaddr*)&serun, len) < 0) {
    perror("connect error");
    exit(1);
  }

  {
    std::unique_lock<std::mutex> lock(mutex);
    atomic.store(true);
    cond.notify_all();
  }
  char buf[] = "123123";
  write(sockfd, buf, strlen(buf));
  write(sockfd, buf, strlen(buf));
  close(sockfd);
  return;
}