#include "../TCP-Connect/NetworkModule.hpp"
#include <string>

void CALLBACK ProcessPackage(Package p) {

	std::string s = "I Can See A Message That Says: ";
	s += p.msg;
	s += "\n";

	printf(s.c_str());
}

int main() {

	SetConsoleTitle(("This is The Client"));

	Network n;
	bool ok = n.SetupClient("127.0.0.1", 54000);

	if (!ok) {
		return 0;
	}

	for (size_t i = 0; i < 10; i++)
	{
		std::string s = "Hello Server for the #" + std::to_string(i+1) + " time!";
		if (!n.Send(s.c_str(), s.length() + 1)) {
			printf(std::string("Failed to send message: " + s + "\n").c_str());
		}

		n.CheckForPackages(&ProcessPackage);
	}

	while (true)
	{
		n.CheckForPackages(&ProcessPackage);
	}

	return 0;
}