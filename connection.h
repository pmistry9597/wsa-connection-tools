#ifndef CONNECTION_H
#define CONNECTION_H

#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <string>

// winsock stuff
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
// end of winsock stuff

class Connection {
	SOCKET connection; // socket handle
	std::queue<std::string> sendQueue; // queue of messages to be sent
	std::mutex sendMutex; // mutex to protect sendQueue
	std::queue<std::string> recvQueue; // queue of received messages
	std::mutex recvMutex; // mutex to protect recvQueue
	std::condition_variable recv_cv; // condition variable to wait for incoming message

	// wsaoverlapped for recv
	WSAOVERLAPPED recvOverlapped;
	WSAOVERLAPPED sendOverlapped; // wsaoverlapped for send
	std::atomic<bool> quit; // true if the worker thread should quit

	// wsabuf for receving msgs
	WSABUF recvBuf;
	// function that will run whenever a message is received
	std::function<void()> recvEvent;
	// ip and port stored here
	std::string str_ip_address; int int_port;
	// function that will be run in seperate thread to send messages in the sendQueue
	std::condition_variable send_cv; // to wakeup sendWorker when sendQueue is not empty
	void sendWorker();
	void recvWorker();
public:
	// constructor takes in a connected socket, size of buffer when receiving messages, and event to be run on message receive (no event by default)
	Connection(SOCKET connection, int bufsize, const std::function<void()>& recvEvent = nullptr);
	~Connection();
	void push_msg(std::string msg);
	std::string pop_msg();
	void close();
	void set_recvEvent(const std::function<void()>& recvEvent);
	bool msg_present();
	bool waitForMessage();
	bool is_alive();
	std::string ip_address();
	int port();
	static bool start_winsock();
};

#endif