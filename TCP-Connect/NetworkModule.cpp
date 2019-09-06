#include "NetworkModule.hpp"


Network::Network() {}

Network::~Network() {}

void Network::CheckForPackages(void(*m_callbackfunction)(Package))
{

	bool morePackages = true;
	while (morePackages)
	{
		std::lock_guard<std::mutex> lock(m_mutex_packages);

		if (m_pstart == m_pend) {
			morePackages = false;
			break;
		}

		m_callbackfunction(m_awaitingPackages[m_pstart]);

		m_pstart = (m_pstart + 1) % MAX_AWAITING_PACKAGES;
	}

}

bool Network::SetupHost(unsigned short port)
{
	if (m_isInitialized)
		return false;

	m_awaitingPackages = new Package [MAX_AWAITING_PACKAGES];

	WSADATA data;
	WORD version = MAKEWORD(2, 2); // use version winsock 2.2
	int status = WSAStartup(version, &data);
	if (status != 0) {
		printf("Error starting WSA\n");
		return false;
	}
	m_soc = socket(AF_INET, SOCK_STREAM, 0); //IPPROTO_TCP
	if (m_soc == INVALID_SOCKET) {
		printf("Error creating socket\n");
	}
	m_myAddr.sin_addr.S_un.S_addr = ADDR_ANY;
	m_myAddr.sin_family = AF_INET;
	m_myAddr.sin_port = htons(port);

	if (bind(m_soc, (sockaddr*)& m_myAddr, sizeof(m_myAddr)) == SOCKET_ERROR) {
		printf("Error binding socket\n");
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

	m_awaitingPackages = new Package[MAX_AWAITING_PACKAGES];

	WSADATA data;
	WORD version = MAKEWORD(2, 2); // use version winsock 2.2
	int status = WSAStartup(version, &data);
	if (status != 0) {
		printf("Error starting WSA\n");
		return false;
	}

	m_soc = socket(AF_INET, SOCK_STREAM, 0); //IPPROTO_TCP
	if (m_soc == INVALID_SOCKET) {
		printf("Error creating socket\n");
		return false;
	}

	//m_myAddr.sin_addr.S_un.S_addr = ADDR_ANY;
	m_myAddr.sin_family = AF_INET;
	m_myAddr.sin_port = htons(hostport);
	inet_pton(AF_INET, IP_adress, &m_myAddr.sin_addr);

	//connect needs error checking!
	int conres = connect(m_soc, (sockaddr*)&m_myAddr, sizeof(m_myAddr));
	if (conres == SOCKET_ERROR) {
		printf("Error connecting to host\n");
		return false;
	}

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
	printf(std::string(std::to_string(receiverID) + "\n").c_str());

	if (receiverID == -1 && m_isServer) {
		int n = 0;
		{
			std::lock_guard<std::mutex> mu(m_mutex_connections);
			n = m_connections.size();
		}
		for (int i = 0; i < n; i++)
		{
			Send(message, size, i);
		}
		return true;
	}	

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
		std::string s = host;
		s += " connected on port " + std::to_string(ntohs(client.sin_port)) + "\n";
		printf(s.c_str());

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
	char msg[MAX_PACKAGE_SIZE];

	while (!connectionIsClosed) {
		ZeroMemory(msg, sizeof(msg));
		int bytesReceived = recv(conn.socket, msg, MAX_PACKAGE_SIZE, 0);

		switch (bytesReceived)
		{
		case 0:
			printf("Client Disconnected\n");
			connectionIsClosed = true;
			break;
		case SOCKET_ERROR:
			printf("Error Receiving: Disconnecting client.\n");
			connectionIsClosed = true;
			break;
		default:
		{
			std::lock_guard<std::mutex> lock(m_mutex_packages);
			m_awaitingPackages[m_pend].senderId = conn.id;
			memcpy(m_awaitingPackages[m_pend].msg, msg, bytesReceived);
			m_pend = (m_pend + 1) % MAX_AWAITING_PACKAGES;
			if (m_pend == m_pstart)
				m_pstart++;
		}
			break;
		}
	}
}
