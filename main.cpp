#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>  // for unlink
#include <unix/constex.h>
#include <unix/unix_client.h>

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

std::mutex mut;
std::atomic_bool ato{false};
std::condition_variable cond;
// 3rd party
#include <spdlog/spdlog.h>  // for SPDLOG_ERROR, SPDLOG_INFO

int main(int argc, char** argv) {
  struct sockaddr_un serun;

  int listenfd = socket(AF_UNIX, SOCK_STREAM, 0);

  if (listenfd == -1) {
    perror("socket error");
    exit(1);
  }

  memset(&serun, 0, sizeof(serun));

  serun.sun_family = AF_UNIX;
  strcpy(serun.sun_path, server_path);
  int size = offsetof(struct sockaddr_un, sun_path) + strlen(serun.sun_path);

  unlink(server_path);

  if (bind(listenfd, (struct sockaddr*)&serun, size) == -1) {
    perror("bind error");
    exit(1);
  }

  printf(" UNIX domain socket bound to %s\n", server_path);

  if (listen(listenfd, 5) == -1) {
    perror("listen error");
    exit(1);
  }

  printf("Accepting connections ...\n");

  std::jthread thread{std::bind(unix_client, server_path, client_path,
                                std::ref(cond), std::ref(mut), std::ref(ato))};

  using namespace std::this_thread;
  using namespace std::chrono;
  if (!ato.load()) {
    std::unique_lock<std::mutex> lock(mut);
     SPDLOG_ERROR("ato is false");
    cond.wait(lock);
  } else {
    SPDLOG_ERROR("ato is true");
  }
  while (1) {
    struct sockaddr_un cliun;
    socklen_t size = sizeof(cliun);
    int connfd = accept(listenfd, (struct sockaddr*)&cliun, &size);

    if (connfd == -1) {
      perror("accept error");
      continue;
    }
    std::cout << "Connection established with " << cliun.sun_path << std::endl;

    char buf[1024];
    int count = 1;
    while (1) {
      auto n = read(connfd, buf, 1024);

      if (n < 0) {
        perror("read error");
        break;
      } else if (n == 0) {
        printf("EOF\n");
        break;
      }
      printf("%d, received: %s\n", count, buf);
      count++;
    }
  }
}