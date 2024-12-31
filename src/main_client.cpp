#include <arpa/inet.h>  // for inet_pton
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <random>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "json_utils.hpp"
#include "order.hpp"

/********************************************************************
 * Global for client
 ********************************************************************/
static int g_clientSock = -1;
static std::atomic<bool> g_clientRunning{true};

/********************************************************************
 * Confirmation receiver
 ********************************************************************/
static void clientConfirmationReceiverThread() {
    while (g_clientRunning.load()) {
        char buffer[2048];
        sockaddr_in fromAddr;
        socklen_t fromLen = sizeof(fromAddr);
        ssize_t len = recvfrom(g_clientSock, buffer, sizeof(buffer) - 1, 0,
                               (struct sockaddr*)&fromAddr, &fromLen);
        if (len > 0) {
            buffer[len] = '\0';
            std::string msg(buffer);
            std::cout << "[Client] Confirmation: " << msg << std::endl;
        }
    }
}

/********************************************************************
 * Build random order
 ********************************************************************/
static Order buildRandomOrder(uint64_t orderId) {
    static std::mt19937_64 rng(std::random_device{}());
    static std::uniform_int_distribution<int> typeDist(0, 6);
    static std::uniform_int_distribution<int> actionDist(0, 1);
    static std::uniform_real_distribution<double> priceDist(10.0, 100.0);
    static std::uniform_int_distribution<int> qtyDist(1, 500);

    Order o;
    o.orderId = orderId;
    switch (typeDist(rng)) {
        case 0: o.type = "market"; break;
        case 1: o.type = "limit"; break;
        case 2: o.type = "cancel"; break;
        case 3: o.type = "stop-loss"; o.isStopOrder = true; break;
        case 4: o.type = "ioc"; break;
        case 5: o.type = "fok"; break;
        default: o.type = "limit"; break;
    }
    o.action = (actionDist(rng) == 0) ? "buy" : "sell";
    o.price = priceDist(rng);
    o.quantity = qtyDist(rng);
    o.remainingQuantity = o.quantity;
    if (o.isStopOrder) {
        o.stopPrice = o.price;
    }
    return o;
}

/********************************************************************
 * Build JSON message from Order
 ********************************************************************/
static std::string buildOrderMessage(const Order &o) {
    std::map<std::string, std::string> fields;
    fields["order_id"] = std::to_string(o.orderId);
    fields["type"] = o.type;
    fields["action"] = o.action;
    fields["quantity"] = std::to_string(o.quantity);
    fields["price"] = std::to_string(o.price);
    if (o.isStopOrder) {
        fields["stop_price"] = std::to_string(o.stopPrice);
    }
    return buildJsonString(fields);
}

/********************************************************************
 * runClient
 ********************************************************************/
static void runClient(const std::string &ip, int port) {
    g_clientSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_clientSock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
        perror("inet_pton");
        close(g_clientSock);
        exit(EXIT_FAILURE);
    }

    // Start receiver
    std::thread receiver(clientConfirmationReceiverThread);

    uint64_t orderCounter = 1;
    bool done = false;
    while (!done) {
        std::cout << "\n[Client Menu]\n"
                  << "1) Send a random order\n"
                  << "2) Send multiple random orders (bulk)\n"
                  << "3) Enter a custom order\n"
                  << "4) Quit\n"
                  << "Select: ";
        int choice;
        std::cin >> choice;
        if (!std::cin.good()) {
            std::cin.clear();
            std::cin.ignore(1024, '\n');
            continue;
        }

        switch (choice) {
            case 1: {
                Order randOrder = buildRandomOrder(orderCounter++);
                std::string msg = buildOrderMessage(randOrder);
                sendto(g_clientSock, msg.c_str(), msg.size(), 0,
                       (struct sockaddr*)&serverAddr, sizeof(serverAddr));
                std::cout << "[Client] Sent random order: " << msg << std::endl;
            } break;
            case 2: {
                std::cout << "How many orders? ";
                int n;
                std::cin >> n;
                for (int i = 0; i < n; i++) {
                    Order randOrder = buildRandomOrder(orderCounter++);
                    std::string msg = buildOrderMessage(randOrder);
                    sendto(g_clientSock, msg.c_str(), msg.size(), 0,
                           (struct sockaddr*)&serverAddr, sizeof(serverAddr));
                }
                std::cout << "[Client] Sent " << n << " random orders.\n";
            } break;
            case 3: {
                Order custom;
                custom.orderId = orderCounter++;
                std::cout << "Enter type (market/limit/cancel/stop-loss/ioc/fok): ";
                std::cin >> custom.type;
                std::cout << "Enter action (buy/sell): ";
                std::cin >> custom.action;
                std::cout << "Enter price: ";
                std::cin >> custom.price;
                std::cout << "Enter quantity: ";
                std::cin >> custom.quantity;
                custom.remainingQuantity = custom.quantity;
                if (custom.type == "stop-loss") {
                    custom.isStopOrder = true;
                    std::cout << "Enter stop price: ";
                    std::cin >> custom.stopPrice;
                }
                std::string msg = buildOrderMessage(custom);
                sendto(g_clientSock, msg.c_str(), msg.size(), 0,
                       (struct sockaddr*)&serverAddr, sizeof(serverAddr));
                std::cout << "[Client] Sent custom order: " << msg << std::endl;
            } break;
            case 4: {
                done = true;
            } break;
            default: {
                std::cout << "Invalid choice.\n";
            } break;
        }
    }

    // shutdown
    g_clientRunning.store(false);
    sockaddr_in dummy;
    std::memset(&dummy, 0, sizeof(dummy));
    sendto(g_clientSock, "", 0, 0, (struct sockaddr*)&dummy, sizeof(dummy));

    receiver.join();
    close(g_clientSock);
    std::cout << "[Client] Exiting...\n";
}

/********************************************************************
 * main (client)
 ********************************************************************/
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <IP> <PORT>\n";
        return 1;
    }
    std::string ip = argv[1];
    int port = std::stoi(argv[2]);

    runClient(ip, port);
    return 0;
}
