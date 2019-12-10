/**y free of unneces
 * ToreroServe: A Lean Web Server
 * COMP 375 - Project 02
 *
 * This program should take two arguments:
 * 	1. The port number on which to bind and listen for connections
 * 	2. The directory out of which to serve files.
 *
 * Author 1: Robert de Brum
 * Author 2: Scott Kolnes
 */

// standard C libraries
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

// operating system specific libraries
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

// C++ standard libraries
#include <vector>
#include <thread>
#include <string>
#include <iostream>
#include <system_error>
#include <fstream>
#include <regex>

// Buffer Library
#include "BoundedBuffer.hpp"

// Import Boost's Filesystem and shorten its namespace to "fs"
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

using std::cout;
using std::string;
using std::vector;
using std::thread;
using std::regex;
using std::smatch;

#define DEBUG
#define BUFF_SIZE 		2048
#define NUM_CLIENTS 	10
#define NUM_THREADS 	4

// This will limit how many clients can be waiting for a connection.
static const int BACKLOG = 10;

// forward declarations
int  createSocketAndListen(const int port_num);

void acceptConnections(const int server_sock, string root_dir);

void handleClient(BoundedBuffer &buff, string root_dir);

void sendData(int socked_fd, const char *data, size_t data_length);

int  receiveData(int socked_fd, char *dest, size_t buff_size);

void sendBadResponse(int client_sock);

void sendOkResponse(string version,  const int client_sock);	

void sendNotFoundResponse(string version, const int client_sock);	

string createDirectoryHtml(string root_dir);

void sendFileData(string filename, const int client_sock);

void sendHeader(string filename, const int client_sock);

bool doesFileExist(string fileName);

bool isDirectory(string path);

string callRegex(string check, string format); 

string checkValidRequest(string request_string);

string getVersionNo(string request);

string getFile(string request);

string determineFileType(string filename);
				
int main(int argc, char** argv) {

	/* Make sure the user called our program correctly. */
	if (argc != 3) {
		cout << "INCORRECT USAGE!\n";
		cout << "Format: ./(executable) (port #) (root directory)\n";
		cout << "Example: ./torero-serve 7107 WWW\n";
		exit(1);
	}

    /* Read the port number from the first command line argument. */
    int port = std::stoi(argv[1]);
	
	// Gets the root directory
	string root_dir(argv[2]);

	/* Create a socket and start listening for new connections on the
	 * specified port. */
	int server_sock(createSocketAndListen(port));

	/* Now let's start accepting connections. */
	acceptConnections(server_sock, root_dir);

    close(server_sock);

	return 0;
}

/**
 * Sends message over given socket, raising an exception if there was a problem
 * sending.
 *
 * @param socket_fd The socket to send data over.
 * @param data The data to send.
 * @param data_length Number of bytes of data to send.
 */
void sendData(int socked_fd, const char *data, size_t data_length) {
	int num_bytes_sent(0);
	
	do { // Make sure that all of the data gets sent.
		num_bytes_sent += send(socked_fd, data, data_length, 0);
	} while ((unsigned int)num_bytes_sent < data_length && num_bytes_sent >= 0); // leave the loop when num_bytes_sent = data_length or is -1

	if (num_bytes_sent == -1) {
		std::error_code ec(errno, std::generic_category());
		throw std::system_error(ec, "send failed");
	}
}

/**
 * Receives message over given socket, raising an exception if there was an
 * error in receiving.
 *
 * @param socket_fd The socket to send data over.
 * @param dest The buffer where we will store the received data.
 * @param buff_size Number of bytes in the buffer.
 * @return The number of bytes received and written to the destination buffer.
 */
int receiveData(int socked_fd, char *dest, size_t buff_size) {
	int num_bytes_received(recv(socked_fd, dest, buff_size, 0));
	if (num_bytes_received == -1) {
		std::error_code ec(errno, std::generic_category());
		throw std::system_error(ec, "recv failed");
	}

	return num_bytes_received;
}

/**
 * Receives a request from a connected HTTP client and sends back the
 * appropriate response.
 *
 * @note After this function returns, client_sock will have been closed (i.e.
 * may not be used again).
 *
 * @param client_sock The client's socket file descriptor.
 * @param root_dir 	  The root directory is used to complete the full path for
 * 					  the request
 */
void handleClient(BoundedBuffer &sock_buff, string root_dir) {
	//Threads will only execute this once, so need it to be while true loop to
	//keep running
	
	while (true) {

		//get socket id from sock_buff and store as client_sock
		int client_sock(sock_buff.getItem());

#ifdef DEBUG
		cout << "Consumed sock\n";
#endif // DEBUG

		// Step 1: Receive the request message from the client
		char received_data[BUFF_SIZE];
		int bytes_received(receiveData(client_sock, received_data, BUFF_SIZE));
    
		// Turn the char array into a C++ string for easier processing.
		//  This string is on the stack .. no need to free
		string request_string(received_data, bytes_received);

	#ifdef DEBUG
		cout << "RECEIVED MESSAGE:\n\n" << request_string << '\n';	
	#endif // DEBUG
	
		// TODO: Step 2: Parse the request string to determine what response to
		// generate. I recommend using regular expressions (specifically C++'s
		// std::regex) to determine if a request is properly formatted.
	
		string request(checkValidRequest(request_string));
		string version(getVersionNo(request));
		string object(getFile(request));

	#ifdef DEBUG
		cout << "Request:\t" << request << '\n';
		cout << "Version:\t" << version << '\n';
		cout << "Object:\t"  << object  << '\n';
	#endif // DEBUG
	
		/***** BAD REQUEST *****/
		if (object == "" || version == "" || request == "") {
		
			// Send Bad Request
			sendBadResponse(client_sock);	
	
		} else { // All components are there

			// Form the complete path
			object = root_dir + object;

	#ifdef DEBUG
			cout << "OBJECT PATH: " << object << '\n';
	#endif // DEBUG
	
			/***** IS A DIRECTORY *****/
			if (isDirectory(object) && object[object.length() - 1] == '/') {
				// Complete file path created in the if statement above

	#ifdef DEBUG
				cout << "DIRECTORY EXISTS\n";
	#endif // DEBUG
		
				// Is a directory: need to check if the index.html file exists
				if (!doesFileExist((object + "index.html"))) {

	#ifdef DEBUG
				cout << "index.html DOES NOT EXIST\n";
	#endif // DEBUG
					
					sendOkResponse(version, client_sock);
					
					// index.html does not exist so we need to list the directories	
					string response(createDirectoryHtml(object));
					sendData(client_sock, response.c_str(), response.length());
					
				} else {

	#ifdef DEBUG
					cout << "index.html EXISTS\n";
	#endif // DEBUG

					object += "index.html"; // change directory to the index.html file	
			
					sendOkResponse(version, client_sock);
					sendHeader(object, client_sock);
					sendFileData(object, client_sock);	
				}	 
			} 
		
			/***** IS A FILE: EXISTS *****/
			else if (doesFileExist(object)) {

	#ifdef DEBUG
					cout << "FILE EXISTS\n";
	#endif // DEBUG

				sendOkResponse(version, client_sock);
				sendHeader(object, client_sock);
				sendFileData(object, client_sock);
			}	 
		
			/***** CAN'T FIND THE OBJECT *****/
			else {

	#ifdef DEBUG
				cout << "DOES NOT EXIST\n";
	#endif // DEBUG

				// Respond with the 404 not found
				sendNotFoundResponse(version, client_sock);
				sendHeader("WWW/error_page.html", client_sock);
				sendFileData("WWW/error_page.html", client_sock);
			}
		}
	
		// Close connection with client.
		close(client_sock);
	}
}

/**
 * Sends a bad response message to the client if the request is bad or of the
 * wrong format
 *
 * @param client_sock	The socket ID of the client to send bad response to
 */ 
void sendBadResponse(const int client_sock) {
	string response("HTTP/1.0 400 BAD REQUEST\r\n\r\n");
	sendData(client_sock, response.c_str(), response.length());

#ifdef DEBUG
	cout << "SENDING:\n" << response << '\n';
#endif

}

/**
 * Sends an OK response to the client 
 *
 * @param version		the HTTP version sent to the client
 * @param client_sock	the client sock id to send response to
 */
void sendOkResponse(string version, const int client_sock)	{
	string response(version + " 200 OK\r\n");
	sendData(client_sock, response.c_str(), response.length());

#ifdef DEBUG
	cout << "SENDING:\n" << response << '\n';
#endif

}

/**
 * Sends a not found response if a request produces no result
 *
 * @param version		The HTTP version sent along with the response message
 * @param client_sock	The client sock id to send the not found response to
 */ 
void sendNotFoundResponse(string version, const int client_sock) {
	string response(version + " 404 Not Found\r\n");
	sendData(client_sock, response.c_str(), response.length());

#ifdef DEBUG
	cout << "SENDING:\n" << response << '\n';
#endif

}

/**
 * Creates the directory HTML with the appropriate header and returns the
 * response message
 *
 * @param root_dir	The root directory 
 *
 * @return The response message with the correct format of an HTML page
 */
string createDirectoryHtml(string root_dir) {
	string response("");
	// Generate the HTML File in a string
	string html_list("<html>\n\t<body>\n\t\t<ul>");

	for (fs::directory_iterator entry(root_dir); entry != fs::directory_iterator(); ++entry) {
		std::string filename = entry->path().filename().string();
	
		if (isDirectory(root_dir + "/" + filename)) filename += "/"; // if it is not a file, then make sure it is a directory/
			
		// Each line should look like this: <li><a href="meow/">meow/</a></li>
		html_list += "\n\t\t\t<li><a href=\"" + filename + "\">" + filename + "</a></li>";

#ifdef DEBUGx
	    cout << filename << '\n';
	    cout << html_list << '\n';
#endif // DEBUG

	}

	// Wrap up the html file	
	html_list += "\n\t\t</ul>\n\t</body>\n</html>";

	// Create the Response (with the header lines)	
	response += "Content-Type: text/html\r\n";
	response += "Content-Length: " + std::to_string(html_list.length()) + "\r\n";
	response += "\r\n" + html_list + "\r\n.\r\n";	
	return response;
}

/**
 * Function to call regex on a specifc format given by the the calling function
 *
 * @param check
 * @param format
 *
 * @return The results of the regex search .. either blank for error or the
 * 		   correct regex expression
 */
string callRegex(string check, string format) {
	
	// Create a regex with the given format
	regex reg(format);
	smatch match;
	
	// Search for the expression
	std::regex_search(check, match, reg);

	// return the results ("" for failure)
	if (match.empty()) {
		return "";
	} else {
		return match[0];
	}
}

/**
 * Function that calls the callRegex function on the request message to see if
 * it follows correct formating and therefore is valid
 *
 * @param request_string The request string that is going to be checked
 *
 * @return either blank from callRegex or the correct formated request string
 */
string checkValidRequest(string request_string) {
	// Look for matches with the HTTP request format
	return callRegex(request_string, "(GET\\s[\\w\\-\\./]*\\sHTTP/\\d\\.\\d)");
}

/**
 * Function that checks the format of the request string for the HTTP version
 * number
 *
 * @param request	The request string to check for the HTTP version
 *
 * @return 	either blank for error or the correct HTTP version number
 */
string getVersionNo(string request) {
	// Call regex looking for the HTTP version	
	return callRegex(request, "(HTTP/\\d\\.\\d)");
}

/**
 * function that uses the Boost file system to check if the file exists
 *
 * @param fileName	The file name to check if it exists
 *
 * @return either True or False indicatinf whether or not the file exists or
 * not
 */
bool doesFileExist(string fileName) {
	// Use Boost file system to determine if the file exists
	fs::path name(fileName);
	return fs::is_regular_file(name);
}

/**
 * function that uses the Boost file system to check if the path is a
 * directory or not
 *
 * @param path	The path to check
 *
 * @return Either True or False based on whether the path is a directory or
 * 		   not
 */
bool isDirectory(string path) {
	// Use Boost file system to determine if the file exists
	fs::path name(path);
	return fs::is_directory(name);
}

/**
 * Function to get the filename/ file by calling regex
 *
 * @param request	The request message containing the file name
 *
 * @return the string of the filename
 */
string getFile(string request) {
	// Call regex to find the filename
	return callRegex(request, "(/[\\w\\./\\-]*)");;
}

/**
 * Sends the header of the response message, broken up in segments on purpose
 *
 * @param filename	The filename that will be used to determine file size and file
 * 				    type
 */
void sendHeader(string filename, const int client_sock) {

	string response("");	
	
	// Add the content length and size
	response += "Content-Length: ";
	response += std::to_string(fs::file_size(filename));
	response += "\r\n";

	response += "Content-Type: ";
	response += determineFileType(filename);
	response += "\r\n";

	// Append the Data to the response string next
	response += "\r\n";
	
	sendData(client_sock, response.c_str(), response.length());

#ifdef DEBUG
	cout << "SENDING:\n" << response << '\n';
#endif

}

/**
 * Function that sends the file data from the specified filename 
 *
 * @param filename		The specifc file name in which to send its data
 * @param client_sock	The client socket number in which to send the data to
 *
 **/
void sendFileData(string filename, const int client_sock) {

	string response("");	
		
	// open the file in binary mode (works for both text and binary files)
	std::ifstream file(filename, std::ios::binary);
	const unsigned int buffer_size(4096);
	char file_data[buffer_size];
	
	// keep reading while we haven't reached the end of the file (EOF)
	while (!file.eof()) {
		file.read(file_data, buffer_size); // read up to buffer_size byte into file_data buffer
	    int bytes_read = file.gcount(); // find out how many bytes we actually read

#ifdef DEBUG
		cout << "Read " << bytes_read << " bytes from file\n";
#endif // DEBUG
		
		// send each chunk
		sendData(client_sock, file_data, bytes_read); 

#ifdef DEBUG
	cout << "SENDING:\n" << file_data << '\n';
#endif

	}
	file.close();
	
	// end the transaction
	sendData(client_sock, "\r\n.\r\n", 6);
}

/**
 * Function that determines the file type
 *
 * @param filename	The file in which to check for the file type
 *
 * @return 			the correct path in order to reach the file type
 */
string determineFileType(string filename) {

	string response("");
	
	// Look for the file ending
	string file_type(callRegex(filename, "(\\.\\w*)"));
	
#ifdef DEBUG
	cout << "File suffix: " << file_type << '\n';
#endif // DEBUG

	if (file_type == ".html") {
		response = "text/html";
	} else if (file_type == ".css") {
		response = "text/css";		 
	} else if (file_type == ".jpg") {
		response = "image/jpg";
	} else if (file_type == ".png") {
		response = "image/png";
	} else if (file_type == ".gif") {
		response = "image/gif";
	} else if (file_type == ".pdf") {
		response = "application/pdf";
	} else {
		response = "text/plain";
	}

	return response;
}

/**
 * Creates a new socket and starts listening on that socket for new
 * connections.
 *
 * @param port_num The port number on which to listen for connections.
 * @returns The socket file descriptor
 */
int createSocketAndListen(const int port_num) {
    int sock(socket(AF_INET, SOCK_STREAM, 0));
    if (sock < 0) {
        perror("Creating socket failed");
        exit(1);
    }

    /* 
	 * A server socket is bound to a port, which it will listen on for incoming
     * connections.  By default, when a bound socket is closed, the OS waits a
     * couple of minutes before allowing the port to be re-used.  This is
     * inconvenient when you're developing an application, since it means that
     * you have to wait a minute or two after you run to try things again, so
     * we can disable the wait time by setting a socket option called
     * SO_REUSEADDR, which tells the OS that we want to be able to immediately
     * re-bind to that same port. See the socket(7) man page ("man 7 socket")
     * and setsockopt(2) pages for more details about socket options.
	 */
    int reuse_true(1);

	int retval; // for checking return values

    retval = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true,
                        sizeof(reuse_true));

    if (retval < 0) {
        perror("Setting socket option failed");
        exit(1);
    }

    /*
	 * Create an address structure.  This is very similar to what we saw on the
     * client side, only this time, we're not telling the OS where to connect,
     * we're telling it to bind to a particular address and port to receive
     * incoming connections.  Like the client side, we must use htons() to put
     * the port number in network byte order.  When specifying the IP address,
     * we use a special constant, INADDR_ANY, which tells the OS to bind to all
     * of the system's addresses.  If your machine has multiple network
     * interfaces, and you only wanted to accept connections from one of them,
     * you could supply the address of the interface you wanted to use here.
	 */
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_num);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* 
	 * As its name implies, this system call asks the OS to bind the socket to
     * address and port specified above.
	 */
    retval = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (retval < 0) {
        perror("Error binding to port");
        exit(1);
    }

    /* 
	 * Now that we've bound to an address and port, we tell the OS that we're
     * ready to start listening for client connections. This effectively
	 * activates the server socket. BACKLOG (a global constant defined above)
	 * tells the OS how much space to reserve for incoming connections that have
	 * not yet been accepted.
	 */
    retval = listen(sock, BACKLOG);
    if (retval < 0) {
        perror("Error listening for connections");
        exit(1);
    }

	// Give some output so we know whats gud
#ifdef DEBUG
	cout << "Socket opened and listening\n";
#endif // DEBUG

	return sock;
}

/**
 * Sit around forever accepting new connections from client.
 *
 * @param server_sock The socket used by the server.
 */
void acceptConnections(const int server_sock, string root_dir) {
    
	BoundedBuffer sock_buffer(BUFF_SIZE);

	//Need to create threads outside of while true loop
	//gonna need more than 1 thread, either 4 or 8
	for(int i = 0; i < NUM_THREADS; i++) {
		//Thread creation here
		std::thread handleThread(handleClient, std::ref(sock_buffer), root_dir);
		handleThread.detach();
	}
	//std::thread handleThread(handleClient, std::ref(sock_buffer), root_dir);
	//handleThread.detach();
		
	while (true) {
        // Declare a socket for the client connection.
        int sock;

        /* 
		 * Another address structure.  This time, the system will automatically
         * fill it in, when we accept a connection, to tell us where the
         * connection came from.
		 */
        struct sockaddr_in remote_addr;
        unsigned int socklen(sizeof(remote_addr)); 

        /* 
		 * Accept the first waiting connection from the server socket and
         * populate the address information.  The result (sock) is a socket
         * descriptor for the conversation with the newly connected client.  If
         * there are no pending connections in the back log, this function will
         * block indefinitely while waiting for a client connection to be made.
         */
        sock = accept(server_sock, (struct sockaddr*) &remote_addr, &socklen);
        if (sock < 0) {
            perror("Error accepting connection");
            exit(1);
        }

#ifdef DEBUG
		cout << "Connection accepted\n";
#endif

        /* 
		 * At this point, you have a connected socket (named sock) that you can
         * use to send() and recv(). The handleClient function should handle all
		 * of the sending and receiving to/from the client.
		 *
		 */

		sock_buffer.putItem(sock);
    }
}
