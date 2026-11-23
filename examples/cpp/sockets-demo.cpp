// sockets-demo - BSD sockets wrapped in C++ (RAII + exceptions) on the
// Waveshare ESP32-P4 v2 board.
//
//   sockets-demo get HOST [PORT] [PATH]    plain HTTP GET (for HTTPS with
//                                          certificate checks use https-get)
//   sockets-demo eco [PORT] [MAX]          TCP echo server, one thread per
//                                          client; stops after MAX clients
//                                          (0 = never).  Default port 7000.
//
// Try the server from the Mac:   nc stamp-p4.local 7000
#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>

// Network failure with the errno (or resolver) reason.
struct NetError : std::runtime_error {
	explicit NetError(const std::string &what) : std::runtime_error(what) {}
	static NetError from_errno(const std::string &what)
	{
		return NetError(what + ": " + std::strerror(errno));
	}
};

// Owns one file descriptor; closed by the destructor, also when an
// exception unwinds.  Move-only.
class Socket {
public:
	Socket() = default;
	explicit Socket(int fd) : fd_(fd) {}
	Socket(const Socket &) = delete;
	Socket &operator=(const Socket &) = delete;
	Socket(Socket &&o) noexcept : fd_(std::exchange(o.fd_, -1)) {}
	Socket &operator=(Socket &&o) noexcept
	{
		if (this != &o) {
			reset();
			fd_ = std::exchange(o.fd_, -1);
		}
		return *this;
	}
	~Socket() { reset(); }

	int fd() const { return fd_; }

	static Socket connect_to(const std::string &host, const std::string &port, int timeout_s)
	{
		addrinfo hints{};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		addrinfo *res = nullptr;
		int rc = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
		if (rc != 0)
			throw NetError("cannot resolve " + host + ": " + gai_strerror(rc));
		std::string last = "no address";
		for (addrinfo *a = res; a; a = a->ai_next) {
			Socket s(::socket(a->ai_family, a->ai_socktype, a->ai_protocol));
			if (s.fd_ < 0) {
				last = std::strerror(errno);
				continue;
			}
			s.set_timeout(timeout_s);
			if (::connect(s.fd_, a->ai_addr, a->ai_addrlen) == 0) {
				freeaddrinfo(res);
				return s;
			}
			last = std::strerror(errno);
		}
		freeaddrinfo(res);
		throw NetError("cannot connect to " + host + ":" + port + ": " + last);
	}

	static Socket listen_on(int port)
	{
		Socket s(::socket(AF_INET, SOCK_STREAM, 0));
		if (s.fd_ < 0)
			throw NetError::from_errno("socket");
		int one = 1;
		setsockopt(s.fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
		sockaddr_in a{};
		a.sin_family = AF_INET;
		a.sin_port = htons(static_cast<uint16_t>(port));
		a.sin_addr.s_addr = htonl(INADDR_ANY);
		if (::bind(s.fd_, reinterpret_cast<sockaddr *>(&a), sizeof(a)) < 0)
			throw NetError::from_errno("bind port " + std::to_string(port));
		if (::listen(s.fd_, 4) < 0)
			throw NetError::from_errno("listen");
		return s;
	}

	Socket accept_one(std::string &peer) const
	{
		sockaddr_in a{};
		socklen_t len = sizeof(a);
		int c = ::accept(fd_, reinterpret_cast<sockaddr *>(&a), &len);
		if (c < 0)
			throw NetError::from_errno("accept");
		char ip[INET_ADDRSTRLEN] = "?";
		inet_ntop(AF_INET, &a.sin_addr, ip, sizeof(ip));
		peer = std::string(ip) + ":" + std::to_string(ntohs(a.sin_port));
		return Socket(c);
	}

	void send_all(const std::string &data) const
	{
		size_t off = 0;
		while (off < data.size()) {
			ssize_t n = ::send(fd_, data.data() + off, data.size() - off, MSG_NOSIGNAL);
			if (n < 0) {
				if (errno == EINTR)
					continue;
				throw NetError::from_errno("send");
			}
			off += static_cast<size_t>(n);
		}
	}

	// Empty string at end of stream.
	std::string recv_some(size_t max = 1024) const
	{
		std::string buf(max, '\0');
		for (;;) {
			ssize_t n = ::recv(fd_, &buf[0], max, 0);
			if (n >= 0) {
				buf.resize(static_cast<size_t>(n));
				return buf;
			}
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				throw NetError("receive: timed out");
			throw NetError::from_errno("receive");
		}
	}

private:
	void set_timeout(int s)
	{
		timeval tv{s, 0};
		setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	}
	void reset()
	{
		if (fd_ >= 0)
			::close(fd_);
		fd_ = -1;
	}
	int fd_ = -1;
};

static int cmd_get(const std::string &host, const std::string &port, const std::string &path)
{
	Socket s = Socket::connect_to(host, port, 10);
	s.send_all("GET " + path + " HTTP/1.0\r\nHost: " + host +
		   "\r\nUser-Agent: sockets-demo\r\nConnection: close\r\n\r\n");
	std::string resp, chunk;
	while (!(chunk = s.recv_some(2048)).empty())
		resp += chunk;
	size_t eol = resp.find("\r\n");
	size_t hdr_end = resp.find("\r\n\r\n");
	if (eol == std::string::npos || hdr_end == std::string::npos)
		throw std::runtime_error("not an HTTP response (" + std::to_string(resp.size()) + " bytes)");
	std::printf("%s\n", resp.substr(0, eol).c_str());
	std::printf("headers: %zu bytes, body: %zu bytes\n", hdr_end, resp.size() - hdr_end - 4);
	return 0;
}

static int cmd_eco(int port, int max_clients)
{
	Socket srv = Socket::listen_on(port);
	std::printf("echo server on port %d (%s)\n", port,
		    max_clients ? ("stops after " + std::to_string(max_clients) + " clients").c_str()
				: "Ctrl-C to stop");
	std::fflush(stdout);
	std::atomic<int> active{0};
	for (int served = 0; max_clients == 0 || served < max_clients; served++) {
		std::string peer;
		Socket c = srv.accept_one(peer);
		std::printf("client %s connected (%d active)\n", peer.c_str(), ++active);
		std::fflush(stdout);
		bool last = max_clients != 0 && served + 1 == max_clients;
		std::thread t([&active](Socket cs, std::string who) {
			try {
				cs.send_all("eco de la placa: escribi algo\n");
				std::string data;
				while (!(data = cs.recv_some()).empty())
					cs.send_all("eco: " + data);
				std::printf("client %s closed\n", who.c_str());
			} catch (const std::exception &e) {
				std::printf("client %s: %s\n", who.c_str(), e.what());
			}
			--active;
			std::fflush(stdout);
		}, std::move(c), peer);
		if (last)
			t.join();	// let the final client finish before exiting
		else
			t.detach();
	}
	while (active > 0)
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	return 0;
}

int main(int argc, char **argv)
{
	std::signal(SIGPIPE, SIG_IGN);
	std::string cmd = argc > 1 ? argv[1] : "";
	try {
		if (cmd == "get" && argc >= 3)
			return cmd_get(argv[2], argc > 3 ? argv[3] : "80", argc > 4 ? argv[4] : "/");
		if (cmd == "eco")
			return cmd_eco(argc > 2 ? std::stoi(argv[2]) : 7000, argc > 3 ? std::stoi(argv[3]) : 0);
	} catch (const NetError &e) {
		std::fprintf(stderr, "network error: %s\n", e.what());
		return 2;
	} catch (const std::exception &e) {
		std::fprintf(stderr, "error: %s\n", e.what());
		return 1;
	}
	std::fprintf(stderr, "usage: %s get HOST [PORT] [PATH] | eco [PORT] [MAX_CLIENTS]\n", argv[0]);
	return 1;
}
