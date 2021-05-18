### Coio
* header-only
* implement io_context with liburing
* use c++20 coroutines
* file IO , socket , http-client (still simple)
* need gcc version > 10

### TODO
* need io_cancel   
* http-client (HTTP/1.1 keep-alive , chunk) 
* use modules rewrite headers  

### URING_FEATURES
* sq poll mode : use less system call (io_uring_enter) , need root permission (use kernel polling thread)
* io poll mode : use busy loop for IO completion query , instead of kernel irq ? (seems like it cannot be mixed used with no-poll IO);
* fast poll feature: eliminates the need to use poll_add / read-write in userspace 

### Problems
* [iouring] sq_thread sometimes will not awake 
* [gcc] unexpected move or copy when co_await

### dependency
* liburing
* c-ares
* gtest (only for build tests)

### example

'''

constexpr uint64_t KB = 1024;
constexpr uint64_t MB = 1024 * 1024;

uint16_t g_server_port{};

future<uint64_t> client(io_context &ctx, uint times) {
  uint64_t bytes_read{};

  try {

    auto conn = connector{};
    conn.set_no_delay();
    co_await conn.connect(ipv4::address{g_server_port});
    auto &sock = conn.socket();
    auto buff = std::vector<std::byte>(KB);

    while (times--) {
      [[maybe_unused]] auto n = co_await sock.send(buff);
      auto m = co_await sock.recv(buff);
      if (m == 0)
        break;
      bytes_read += m;
    }

  } catch (const std::exception &e) {
    std::cout << "exception : " << e.what() << std::endl;
  }

  co_return bytes_read;
}

'''

### pingpong benchmark

I rebuilt '''libhv/echo-servers''' ( see '''coio/bench/libhv''' ).

My environment :
* Intel(R) Core(TM) i5-8500 CPU @ 3.00GHz (6 CPUs), ~3.0GHz
* DDR4 RAM 8GB*2 , 2400MHz Dual-channel
* WSL2 : Ubuntu-20.04 (should enable io_uring kernal config)
* use kernel rebuilt by [nathanchance](https://github.com/nathanchance/WSL2-Linux-Kernel)

Comparation
* port 2001 : libhv 
* port 2003 : asio coroutine 
* port 2004 : a simple io_uring echo server written in C

'''
libhv running on port 2001
coio running on port 2002
asio(coroutine) running on port 2003
cio_uring_echo running on port 2004
io_uring echo server listening for connections on port: 2004

==============2001=====================================
[127.0.0.1:2001] 4 threads 1000 connections run 10s
all connected
all disconnected
total readcount=1784495 readbytes=1827322880
throughput = 174 MB/s

==============2002=====================================
[127.0.0.1:2002] 4 threads 1000 connections run 10s
all connected
all disconnected
total readcount=2263449 readbytes=2317771776
throughput = 221 MB/s

==============2003=====================================
[127.0.0.1:2003] 4 threads 1000 connections run 10s
all connected
all disconnected
total readcount=943662 readbytes=966309888
throughput = 164 MB/s

==============2004=====================================
[127.0.0.1:2004] 4 threads 1000 connections run 10s
all connected
all disconnected
total readcount=2440472 readbytes=2499043328
throughput = 238 MB/s
'''

### reference 
* about stackless coroutine - [duff's device](https://mthli.xyz/coroutines-in-c/)
* [cppcoro](https://github.com/lewissbaker/cppcoro)
* https://kernel.dk/io_uring.pdf
* [libhv](https://github.com/ithewei/libhv)
* https://github.com/frevib/io_uring-echo-server/blob/master/io_uring_echo_server.c
* [rust echo bench](https://github.com/haraldh/rust_echo_bench)