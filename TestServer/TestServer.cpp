#include <iostream>
//#include <winsock.h>
#include <ws2tcpip.h>
void WSAinit() {
	WSADATA ws;
	WSAStartup(MAKEWORD(2, 2), &ws);
}
#include <thread>
#include <map>

bool shouldEnd = false;

struct Point {
	double x = 0, y = 0;
};

std::map<SOCKET, bool> connectionList;

SOCKET myMainSocket;


SOCKET createMainSocket(sockaddr_in& sockA) {
	SOCKET mainSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (mainSocket < 0) {
		std::cout << "Error creating main socket; errno: " << errno << std::endl;
		exit(-1);
	}

	sockA.sin_family = AF_INET;
	sockA.sin_addr.S_un.S_addr = INADDR_ANY;
	sockA.sin_port = htons(15212);
	if (bind(mainSocket, (sockaddr*)&sockA, sizeof(sockA)) < 0) {
		std::cout << "Error binding socket; errno: " << errno << std::endl;
		exit(-1);
	}

	if (listen(mainSocket, 10) < 0) {
		std::cout << "Error listening on main socket; errno: " << errno << std::endl;
		exit(-1);
	}

	std::cout << "Main socket created succesfully\n";
	return mainSocket;
}

void receiveConnections(sockaddr_in& sockA) {
	int msLen = sizeof(sockA);
	while (!shouldEnd) {
		auto newCon = accept(myMainSocket, (sockaddr*)&sockA, &msLen);
		if (newCon < 0) {
			std::cout << "Failed to connect; errno: " << errno << std::endl;
		}
		else {
			std::cout << "Connection established with " << newCon << std::endl;
			connectionList.insert(std::pair<SOCKET, bool>(newCon, true));
		}
	}
}



/////////////////////////////////////////


Point mainPoint{ 0,0 };

void sendPacketToClient() {
	if (!connectionList.empty())
		for (auto& cl : connectionList) {
			if (cl.second) {
				if (send(cl.first, (char*)&mainPoint, sizeof(mainPoint), 0) < 0) {
					std::cout << "Failed to send packet to " << cl.first << "; errno: " << errno << " Closing connection..." << std::endl;
					closesocket(cl.first);
					cl.second = false;
				}
			}
		}
}

Point velocity{ 1, 1 };

void doCalculations() { // doServerBasedCalculationsOnThisThread
	if (mainPoint.x > 1000 || mainPoint.x < 0) velocity.x = -velocity.x;
	if (mainPoint.y > 700 || mainPoint.y < 0) velocity.y = -velocity.y;
	mainPoint.x += velocity.x;
	mainPoint.y += velocity.y;
}

void senderTask() {
	while (!shouldEnd) {
		doCalculations();
		sendPacketToClient();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
}

int main() {
#ifdef _WIN32
	WSAinit();
#endif
	sockaddr_in sockAd;
	myMainSocket = createMainSocket(sockAd);
	std::thread conReceiver(receiveConnections, std::ref(sockAd));
	std::thread sender(senderTask);

	conReceiver.join();
	sender.join();
	closesocket(myMainSocket);
}
