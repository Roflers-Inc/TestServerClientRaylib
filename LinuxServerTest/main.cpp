#include <iostream>
//#include <winsock.h>
#ifdef _WIN32
#include <ws2tcpip.h>
void WSAinit() {
	WSADATA ws;
	WSAStartup(MAKEWORD(2, 2), &ws);
}
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>
#include <stdio.h>
typedef int SOCKET;
#define closesocket(socket) shutdown(socket, 0)
#endif
#include <string.h>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <deque>

std::deque<std::string> messageHistory(20);

bool shouldEnd = false;

struct Packet {
	double x = 0, y = 0;
	Packet(double _x, double _y) {
		x = _x; y = _y;
	}
};

std::mutex mtx;

std::map<SOCKET, bool> connectionList;
std::map<SOCKET, std::pair<SOCKET, bool>>messageNotifiable;

SOCKET myMainSocket;

int serverPort = 15212;

SOCKET createMainSocket(sockaddr_in& sockA) {
	SOCKET mainSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (mainSocket < 0) {
		std::cout << "Error creating main socket; errno: " << errno << std::endl;
		exit(-1);
	}

	sockA.sin_family = AF_INET;
	sockA.sin_addr.s_addr = INADDR_ANY;
	sockA.sin_port = htons(serverPort);
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

Packet mainPoint{ 0.0, 0.0 };

std::mutex senderMtx;

void sendToNotifiables() {
	char temp[30];
	strcpy(temp, messageHistory.back().c_str());
	for (auto notif : messageNotifiable) {
		if (notif.second.second) {
			if (send(notif.second.first, temp, sizeof(temp), 0) < 0) notif.second.second = false;
		}
	}
}

void receiverTask(std::pair<SOCKET, SOCKET> assocSocket) {
	char msg[30];
	char temp[30];
	for (std::string s : messageHistory) {
		strcpy(temp, s.c_str());
		send(assocSocket.second, temp, sizeof(temp), 0);
	}
	while (messageNotifiable.at(assocSocket.first).second) {
		if (recv(assocSocket.second, msg, sizeof(msg), 0) > 0) { // TODO : something wrong with zero-terminating on client; maybe clear msg next time but why?
			messageHistory.pop_front();
			messageHistory.push_back(std::string(msg));
			mtx.lock();
			sendToNotifiables();
			mtx.unlock();
			std::cout << msg << std::endl;
		}
	}
}

void receiveConnections(sockaddr_in& sockA) {
	socklen_t msLen = sizeof(sockA);
	while (!shouldEnd) {
		auto newCon = accept(myMainSocket, (sockaddr*)&sockA, &msLen);
		if (newCon < 0) {
			std::cout << "Failed to connect; errno: " << errno << std::endl;
		}
		else {
			std::cout << "Connection established with " << newCon << std::endl;
			connectionList.insert(std::pair<SOCKET, bool>(newCon, true));

			auto msgCon = accept(myMainSocket, (sockaddr*)&sockA, &msLen);
			if (msgCon < 0) {
				std::cout << "Failed to connect message; errno: " << errno << std::endl;
			}
			messageNotifiable.insert(std::pair<SOCKET, std::pair<SOCKET, bool>>(newCon, std::make_pair(msgCon, true)));
			std::thread newMsgReceiver(receiverTask, std::make_pair(newCon, msgCon));
			newMsgReceiver.detach();
		}
	}
}


/////////////////////////////////////////


void sendPacketToClient() {
	if (!connectionList.empty())
		for (auto& cl : connectionList) {
			if (cl.second) {
				if (send(cl.first, (char*)&mainPoint, sizeof(mainPoint), 0) < 0) {
					std::cout << "Failed to send packet to " << cl.first << "; errno: " << errno << " Closing connection..." << std::endl;
					mtx.lock();
					closesocket(cl.first);
					cl.second = false;
					messageNotifiable.at(cl.first).second = false;
					closesocket(messageNotifiable.at(cl.first).first);
					mtx.unlock();
				}
			}
		}
}

Packet velocity{ 1, 1 };

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
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void clearClosedCons() {
	while (!shouldEnd) {
		std::this_thread::sleep_for(std::chrono::minutes(2));
		mtx.lock();
		auto it = connectionList.begin();
		auto it2 = messageNotifiable.begin();
		for (; it != connectionList.end();) {
			if (it->second == false) {
				connectionList.erase(it);
				messageNotifiable.erase(it2);
			}
			else {
				it++;
				it2++;
			}
		}
		mtx.unlock();
	}
}

int main(int argc, char** argv) {
#ifdef _WIN32
	WSAinit();
#endif
	if (argc > 1) {
		serverPort = std::stoi(argv[1]);
	}
	sockaddr_in sockAd;
	myMainSocket = createMainSocket(sockAd);
	std::thread conReceiver(receiveConnections, std::ref(sockAd));
	std::thread sender(senderTask);
	std::thread cleaner(clearClosedCons);

	cleaner.join();
	conReceiver.join();
	sender.join();
	closesocket(myMainSocket);
}
