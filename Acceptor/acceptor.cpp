#include "acceptor.h"
#include <thread>

void Acceptor::worker() { // will be run by a sepearate thread for accepting
		// create overlapped socket to listen on
		// bind SOCKADDR_IN for the port
		// listen with max_listen specified
		// start listening operation
		// while loop
		// create overlapped socket for the new connection
		// run acceptex
		// WSAWaitForMultipleEvents for acceptex
		// use setsockopt to let normal winsock functions work on this socket
		// load the connection into the queue
		// run the event that was specified
		// end of while loop
		std::unique_lock<std::mutex> listen_lock(listen_mutex); // protect listen_status until its updated
		std::lock_guard<std::mutex> thread_lock(thread_mutex); // lock until this thread is done
		while (!quit) {
			std::unique_lock<std::mutex> resumed_lock(resumed_mutex);
			while (!resumed && !quit) { // lock into while loop until this resumes
				resumed_cv.wait(resumed_lock);
			}
			// if error occurs above, set quit to true, notify all events/condition_variables and close the socket connection
			lsock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED); // create socket to listen on
			SOCKADDR_IN addrIn; // will store  arguments for listening operation
			addrIn.sin_family = AF_INET;
			addrIn.sin_port = htons(port);
			addrIn.sin_addr.s_addr = INADDR_ANY;
			// bind the arguments to the socket for listening
			bind(lsock, (SOCKADDR*)&addrIn, sizeof(SOCKADDR_IN));
			// attempt to listen - if failure then quit and notify all
			if ((listen_status = listen(lsock, max_listen)) != 0) {
				quit = true;
				resumed = false;
				// notify all that are waiting on this to complete
				accepted_cv.notify_all();
			}
			listen_lock.unlock(); // unlock listen_status so other thread can read it
			while (resumed) {
				SOCKET connection = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);// this is where the new connection will be
				if (connection == INVALID_SOCKET) { // make sure there was successful start
					// quit since failure to properly make a socket
					quit = true;
					break;
				}
				// start overlapped accept operation - wait for event to trigger
				char accBuf[2 * (sizeof(SOCKADDR_IN) + 16)]; // this will store recv data although it wont be used
				DWORD bytesRecv;// store number of bytes recved (always zero since no bytes received is what was specified)
				if (FALSE ==  AcceptEx(lsock, connection, accBuf, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytesRecv, &overlapped)) {
					// if false occurred, accept did not occur, so check if its because no connections are waiting (which means actual error)
					if (WSAGetLastError() != ERROR_IO_PENDING) {
					quit = true;
					break;
				}
			}
			// wait for the accept operation to complete
			WSAWaitForMultipleEvents(1, &overlapped.hEvent, TRUE, WSA_INFINITE, TRUE);
			if (quit) {
				break;
			}
			setsockopt(connection, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&lsock, sizeof(lsock)); // allow normal winsock funtions to work
			// load the socket onto the queue
			{
				std::lock_guard<std::mutex> accept_lock(accepted_mutex); // lock mutex to be thread-safe
				accepted.push(connection);
			}
			// reset event for next loop
			WSAResetEvent(overlapped.hEvent);
			// run atached event
			if (accept_event) { // make sure an event is attached
				accept_event();
			}
			// notify or something
			accepted_cv.notify_one();
			// WORK HERE CURRENTLY FINISHING ACCEPTEX IN CODE NOTES ABOVE!!!!
		}
		// notify all that are waiting for this thread to complete
		accepted_cv.notify_all();
		// end by closing the connection
		closesocket(lsock);
		// relock listen_status for next loop when listening is restarted
		listen_lock.lock();
	}
	// notify all that are waiting for this thread to complete
	accepted_cv.notify_all();
	// end by closing the connection
	closesocket(lsock);
}
Acceptor::Acceptor() {
	quit = false;
	resumed = false;
	overlapped.hEvent = WSACreateEvent();// event is needed for event driven accept operations on thread
	// create the thread that will run the worker
	std::thread acceptThread(&Acceptor::worker, this);
	acceptThread.detach();
}
Acceptor::~Acceptor() {
	quit = true; // make the worker quit next time it reads quit
	resumed_cv.notify_one();
	stop_listen();
	WSACloseEvent(overlapped.hEvent);// event is no longer needed and should free up resources
	// wait for thread to quit
	std::lock_guard<std::mutex> thread_lock(thread_mutex);
}
void Acceptor::attach_event(const std::function<void()>& accept_event) { // change event to be run on connection accept
	this->accept_event = accept_event;
}
int Acceptor::start_listen(int port, int max_listen) { // start listening operations on certain port with these maximum listeners and resume operations
	this->port = port; this->max_listen = max_listen;
	return start_listen();
}
int Acceptor::start_listen() { // start listening but with stored port and max listening and resume operations (true if successful listen)
	// make sure port and max_listen are proper numbers
	if (!(port > 0 && max_listen > 0)) {
		return -1;
	}
	resumed = true;
	// notify the thread
	resumed_cv.notify_one();
	// lock mutex so this function won't return until listen_status is updated
	std::lock_guard<std::mutex> listen_lock(listen_mutex);
	return listen_status;
}
void Acceptor::stop_listen() { // stops listening operations: thread on wait
	resumed = false;
	WSASetEvent(overlapped.hEvent);
	// wait until resumed_mutex is unlocked
	std::lock_guard<std::mutex> resumed_lock(resumed_mutex);
}
SOCKET Acceptor::pop_socket() { // returns a client connection as a socket
	// lock into loop until accepted has an item or this thing should quit
	std::unique_lock<std::mutex> accepted_lock(accepted_mutex);
	while (accepted.empty() && !quit && resumed) {
		accepted_cv.wait(accepted_lock);
	}
	if (quit || !resumed) { // return garbage if failure
		return INVALID_SOCKET;
	}
	SOCKET connection = accepted.front(); // copy first item
	accepted.pop();// remove it so no repeats
	return connection;
}
bool Acceptor::client_present() { // true if connections in queue
	// lock the mutex to prevent conflict with other thread
	std::lock_guard<std::mutex> accept_guard(accepted_mutex);
	return !accepted.empty();
}
bool Acceptor::waitForClient() { // true if ended due to client being present, false if due to listen stop
	// lock into while loop until accepted has something (and make sure acceptor is still active)
	std::unique_lock<std::mutex> accepted_lock(accepted_mutex);
	while (accepted.empty() && !quit && resumed) {
		accepted_cv.wait(accepted_lock);
	}
	return (!quit && resumed);
}
bool Acceptor::listening() {
	return resumed;
}
bool Acceptor::active() { // true if still listening
	return !quit;
}
bool Acceptor::start_winsock() {
	WORD wsVersion = MAKEWORD(2,2);
	WSADATA wsData;
	if (WSAStartup(wsVersion, &wsData) != 0) { // start winsock
		return false; // return false to indicate failure to start
	} 
	return true; // true to mean successful start since successful if this function reached this point
}