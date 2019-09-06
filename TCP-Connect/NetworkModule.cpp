#include "NetworkModule.hpp"


Network::Network() {}

Network::~Network() {}

bool Network::SetupHost(unsigned short port)
{
	if (m_isInitialized)
		return false;

	WSADATA data;
	WORD version = MAKEWORD(2, 2); // use version winsock 2.2
	int status = WSAStartup(version, &data);
	if (status != 0) {
		std::cout << "Error starting WSA\n";
		return false;
	}
	m_soc = socket(AF_INET, SOCK_STREAM, 0); //IPPROTO_TCP
	if (m_soc == INVALID_SOCKET) {
		std::cout << "Error creating socket\n";
	}
	m_myAddr.sin_addr.S_un.S_addr = ADDR_ANY;
	m_myAddr.sin_family = AF_INET;
	m_myAddr.sin_port = htons(port);

	if (bind(m_soc, (sockaddr*)& m_myAddr, sizeof(m_myAddr)) == SOCKET_ERROR) {
		std::cout << "Error binding socket\n";
		return false;
	}

	//Prepare the socket to listen
	listen(m_soc, SOMAXCONN);

	m_isInitialized = true;
	m_isServer = true;

	//Start a new thread that will wait for new connections
	m_clientAcceptThread = new std::thread(&Network::WaitForNewConnections, this);

	return true;
}

bool Network::SetupClient(const char* IP_adress, unsigned short hostport)
{
	if (m_isInitialized)
		return false;

	WSADATA data;
	WORD version = MAKEWORD(2, 2); // use version winsock 2.2
	int status = WSAStartup(version, &data);
	if (status != 0) {
		std::cout << "Error starting WSA\n";
		return false;
	}

	m_soc = socket(AF_INET, SOCK_STREAM, 0); //IPPROTO_TCP
	if (m_soc == INVALID_SOCKET) {
		std::cout << "Error creating socket\n";
	}

	//m_myAddr.sin_addr.S_un.S_addr = ADDR_ANY;
	m_myAddr.sin_family = AF_INET;
	m_myAddr.sin_port = htons(hostport);
	inet_pton(AF_INET, IP_adress, &m_myAddr.sin_addr);

	//if (bind(m_soc, (sockaddr*)& m_myAddr, sizeof(m_myAddr)) == SOCKET_ERROR) {
	//	std::cout << "Error binding socket\n";
	//	return false;
	//}

	//char msg[1024] = "Hello Server!";

	//connect needs error checking!
	int conres = connect(m_soc, (sockaddr*)&m_myAddr, sizeof(m_myAddr));

	//int res = send(m_soc, msg, sizeof(msg), 0);
	//if (res == SOCKET_ERROR) {
	//	res = WSAGetLastError();
	//}

	//std::cout << "Result was: " << res << std::endl;

	//ZeroMemory(msg, 1024);
	//int bytesRes = recv(m_soc, msg, 1024, 0);

	//std::cout << "Message from server: " << msg << std::endl;

	Connection conn;
	conn.isConnected = true;
	conn.socket = m_soc;
	conn.ip = "";
	conn.port = ntohs(m_myAddr.sin_port);
	conn.id = 0;
	conn.thread = new std::thread(&Network::Listen, this, conn); //Create new listening thread listening for the host
	m_connections.push_back(conn);

	m_isInitialized = true;
	m_isServer = false;

	return true;
}

bool Network::Send(const char* message, size_t size, int receiverID)
{
	Connection conn;
	{
		std::lock_guard<std::mutex> mu(m_mutex_connections);
		if (receiverID > m_connections.size())
			return false;

		conn = m_connections[receiverID];
	}

	if (!conn.isConnected)
		return false;

	if (send(conn.socket, message, size, 0) == SOCKET_ERROR)
		return false;

	return true;
}

bool Network::Send(const char* message, size_t size, Connection conn)
{
	if (!conn.isConnected)
		return false;

	if (send(conn.socket, message, size, 0) == SOCKET_ERROR)
		return false;

	return true;
}

void Network::WaitForNewConnections()
{
	while (true)
	{
		sockaddr_in client;
		int clientSize = sizeof(client);

		SOCKET clientSocket = accept(m_soc, (sockaddr*)& client, &clientSize);
		if (clientSocket == INVALID_SOCKET) {
			//Do something
			return;
		}

		char host[NI_MAXHOST] = { 0 }; // Client's remote name
		//char service[NI_MAXSERV] = { 0 }; // Service (i.e port) the client is connect on
		//if (getnameinfo((sockaddr*)&client, sizeof(client), host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0) {
		//	std::cout << host << "Host connected on port " << service << std::endl;
		//}
		//else {
		//	inet_ntop(AF_INET, &client.sin_addr, host, NI_MAXHOST);
		//	std::cout << host << "Host connected on port " << ntohs(client.sin_port) << std::endl;
		//}

		inet_ntop(AF_INET, &client.sin_addr, host, NI_MAXHOST);
		std::cout << host << "Host connected on port " << ntohs(client.sin_port) << std::endl;

		Connection conn;
		conn.isConnected = true;
		conn.socket = clientSocket;
		conn.ip = host;
		conn.port = ntohs(client.sin_port);

		{
			//Critical region for m_connections
			std::lock_guard<std::mutex> lock(m_mutex_connections);
			conn.id = m_connections.size();
			conn.thread = new std::thread(&Network::Listen, this, conn); //Create new listening thread for the new connection
			m_connections.push_back(conn);
		}
	}
}

void Network::Listen(const Connection conn)
{
	bool connectionIsClosed = false;
	char msg[4096];

	while (!connectionIsClosed) {
		ZeroMemory(msg, sizeof(msg));
		int bytesReceived = recv(conn.socket, msg, sizeof(msg), 0);

		switch (bytesReceived)
		{
		case 0:
			std::cout << "Client Disconnected" << std::endl;
			connectionIsClosed = true;
			break;
		case SOCKET_ERROR:
			std::cout << "Error Receiving" << std::endl;
			connectionIsClosed = true;
			break;
		default:
			std::cout << "Received: " << msg << std::endl;
			//Send(msg, (size_t)(bytesReceived) + 1, conn);
			break;
		}
	}
}
