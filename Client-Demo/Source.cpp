#include "../Network-Module/NetworkModule.hpp"
#include <string>
#include <chrono>
#include <iostream>

//MessageRecieved: Do whatever
void DecodeMessage(NetworkEvent nEvent) {
	std::string s = "Client #" + std::to_string(nEvent.clientID) + " says: ";
	s += nEvent.data->rawMsg;
	s += "\n";

	printf(s.c_str());
}

void UpdateHostList(NetworkEvent nEvent) {
	std::string s = "server found at: ";
	s += nEvent.data->HostFoundOnLanData.hostIP;
	s += ":";
	s += std::to_string(nEvent.data->HostFoundOnLanData.hostPort);

	printf(s.c_str());
}

void CALLBACK ProcessPackage(NetworkEvent nEvent) {
	switch (nEvent.eventType)
	{
	case NETWORK_EVENT_TYPE::NETWORK_ERROR:
		break;
	case NETWORK_EVENT_TYPE::CLIENT_JOINED:
		//not used by clients
		break;
	case NETWORK_EVENT_TYPE::CLIENT_DISCONNECTED:
		//not used by clients
		break;
	case NETWORK_EVENT_TYPE::CLIENT_RECONNECTED:
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
	int a;
	std::cin >> a;

	Network n;
	//bool ok = n.SetupClient("192.168.1.179", 54000);
	n.initialize();
	bool ok = n.join("127.0.0.1", 54000);

	if (!ok) {
		return 0;
	}

	n.searchHostsOnLan();

	for (size_t i = 0; i < 100; i++)
	{
		n.checkForPackages(&ProcessPackage);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	for (size_t i = 0; i < 0; i++)
	{
		std::string s = "Hello Server for the #" + std::to_string(i+1) + " time!";
		//std::this_thread::sleep_for(std::chrono::milliseconds(10));
		if (!n.send(s.c_str(), s.length() + 1)) {
			printf(std::string("Failed to send message: " + s + "\n").c_str());
		}

		n.checkForPackages(&ProcessPackage);
	}

	while (true)
	{
		n.checkForPackages(&ProcessPackage);
	}

	return 0;
}