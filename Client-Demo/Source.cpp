#include "../Network-Module/NetworkModule.hpp"
#include <string>
#include <chrono>
#include <iostream>

bool running = true;
bool hostFound = false;
char hostIp[16] = {0};
USHORT hostPort = 0;

Network network;

//MessageRecieved: Do whatever
void DecodeMessage(NetworkEvent nEvent) {
	std::string s = "Client with id: " + std::to_string(nEvent.clientID) + " says: ";
	s += nEvent.data->rawMsg;
	s += "\n";

	printf(s.c_str());
}

void UpdateHostList(NetworkEvent nEvent) {

	Network::ip_int_to_ip_string(nEvent.data->HostFoundOnLanData.ip_full, hostIp, sizeof(hostIp));
	hostPort = nEvent.data->HostFoundOnLanData.hostPort;

	std::string s = "\nserver found at: ";
	s += hostIp;
	s += ":";
	s += std::to_string(hostPort);
	s += " - "; 
	s += nEvent.data->HostFoundOnLanData.description;
	s += "\n";

	printf(s.c_str());
	hostFound = true;
}

void CALLBACK ProcessPackage(NetworkEvent nEvent) {
	switch (nEvent.eventType)
	{
	case NETWORK_EVENT_TYPE::NETWORK_ERROR:
		break;
	case NETWORK_EVENT_TYPE::CONNECTION_ESTABLISHED:
		//not used by clients
		break;
	case NETWORK_EVENT_TYPE::CONNECTION_CLOSED:
		running = false;
		break;
	case NETWORK_EVENT_TYPE::CONNECTION_RE_ESTABLISHED:
		//not used by clients
		break;
	case NETWORK_EVENT_TYPE::MSG_RECEIVED:
		DecodeMessage(nEvent);
		break;
	case NETWORK_EVENT_TYPE::HOST_ON_LAN_FOUND:
		UpdateHostList(nEvent);
		break;
	default:
		break;
	}
}

int main() {

	SetConsoleTitle(("This is The Client"));
	printf(std::string("Press Enter to start client program... ").c_str());
	getchar();

	/* 1) Client setup */
	network.initialize();

	/* 2) Search hosts on LAN*/
	int maxTries = 3;
	int tries = 0;
	do {
		printf("Looking for hosts ok lan, try %d / %d ", tries+1, maxTries);
		/* 2b) Broadcast on LAN that you want hosts to identify themselfs*/
		network.searchHostsOnLan();

		/* 2c) Wait for response. The HOST_ON_LAN_FOUND event will trigger for each host responding to the request.*/
		for (size_t i = 0; i < 100 && !hostFound; i++)
		{
			if(i % 10 == 0)
				printf(".");
			network.checkForPackages(&ProcessPackage);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		printf("\n");
	} while (!hostFound && ++tries < maxTries);

	if (!hostFound) {
		printf(std::string("No host found on the LAN, Closing client\n").c_str());
		return 0;
	}

	/* 3) Join one host that responded (if any). Host will get the CONNECTION_ESTABLISHED event*/
	if (network.join(hostIp, hostPort)) {
		printf(std::string("Connected to host!\n").c_str());
	}
	else {
		printf(std::string("Could not join the host\n").c_str());
		return 0;
	}

	/* 4) Send the host with some messages*/
	for (size_t i = 0; i < 100; i++)
	{
		std::string s = "Hello Server for the #" + std::to_string(i+1) + " time!";
		if (!network.send(s.c_str(), s.length() + 1)) {
			printf(std::string("Failed to send message: " + s + "\n").c_str());
		}

		/* 4b) dont forget to check if the host responded with anything*/
		network.checkForPackages(&ProcessPackage);
	}

	/* 5) Nothing more to say? just listen for host messages*/
	while (running)
	{
		network.checkForPackages(&ProcessPackage);
	}

	/* 6) When you are done, clean up by calling shutdown. Host will get the CONNECTION_CLOSED Event with the client ID*/
	network.shutdown();

	return 0;
}