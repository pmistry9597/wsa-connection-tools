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
	std::mutex recv_mutex; // remains locked by recv thread, used by destructor to wait for thread to close
	std::mutex send_mutex; // remains locked by send thread, used by destructor to wait for thread to close

	// wsabuf for receving msgs
	WSABUF recvBuf;
	// function that will run whenever a message is received
	std::function<void()> recvEvent;
	// function that will run on connection close
	std::function<void()> closeEvent;
	// ip and port stored here
	std::string str_ip_address; int int_port;
	// function that will be run in seperate thread to send messages in the sendQueue
	std::condition_variable send_cv; // to wakeup sendWorker when sendQueue is not empty
	void sendWorker();
	void recvWorker();
public:
	bool null;
	// constructor takes in a connected socket, size of buffer when receiving messages, and event to be run on message receive (no event by default)
	Connection(SOCKET connection, int bufsize, const std::function<void()>& recvEvent = nullptr);
	Connection();
	~Connection();
	void push_msg(std::string msg); //push message to be sent
	std::string pop_msg(); // return most recent msg - empty string if no message
	void close(); // end connections
	void attach_recvEvent(const std::function<void()>& recvEvent); // set event to be run on msg receive
	void attach_closeEvent(const std::function<void()>& closeEvent); // set event to be run on connection close
	bool msg_present(); // returns if messages available to be read from the connections
	bool waitForMessage(); // true if connection is still useful for operations
	bool is_alive(); // if this object is still useful for operations
	std::string ip_address();
	int port();
	bool connect(std::string ip_address, int port, int bufsize); // blocking connect request - will not return until a result is gotten, either failure or successful connection
	int msg_count(); // return number of msgs currently in queue
	static bool start_winsock(); // starts fundamental winsock resources - must run before any networking can happen
};

#endif