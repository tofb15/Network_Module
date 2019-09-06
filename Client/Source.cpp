#include "../TCP-Connect/NetworkModule.hpp"
#include <string>
int main() {

	Network n;
	n.SetupClient("127.0.0.1", 54000);

	for (size_t i = 0; i < 10; i++)
	{
		std::string s = "Hello Server for the #" + std::to_string(i+1) + " time!";
		int res = n.Send(s.c_str(), s.length() + 1);
		std::cout << res << std::endl;
	}


	//Keep main from closing
	while (true) {};

	return 0;
}