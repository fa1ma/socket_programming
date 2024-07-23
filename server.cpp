#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <mutex>
#include <algorithm>
#include <map>
#include <numeric>

std::vector<int> clients;
std::map<int, std::string> client_names;
std::mutex clients_mutex;

unsigned int calculate_checksum(const std::string& message) {
    return std::accumulate(message.begin(), message.end(), 0);
}

void handle_client(int client_socket) {
    char buffer[1024];

    // Kullanýcý adýný al
    memset(buffer, 0, 1024);
    int bytes_received = recv(client_socket, buffer, 1024, 0);
    if (bytes_received <= 0) {
        close(client_socket);
        return;
    }
    std::string client_name(buffer);

    {
        std::lock_guard<std::mutex> guard(clients_mutex);
        clients.push_back(client_socket);
        client_names[client_socket] = client_name;
    }

    std::cout << client_name << " connected." << std::endl;

    while (true) {
        memset(buffer, 0, 1024);
        int bytes_received = recv(client_socket, buffer, 1024, 0);
        if (bytes_received <= 0) {
            {
                std::lock_guard<std::mutex> guard(clients_mutex);
                clients.erase(std::remove(clients.begin(), clients.end(), client_socket), clients.end());
                client_names.erase(client_socket);
            }
            close(client_socket);
            std::cout << client_name << " disconnected." << std::endl;
            return;
        }

        std::string message(buffer);
        size_t separator_pos = message.find('|');
        if (separator_pos == std::string::npos) continue;

        std::string actual_message = message.substr(0, separator_pos);
        unsigned int received_checksum = std::stoi(message.substr(separator_pos + 1));

        unsigned int calculated_checksum = calculate_checksum(actual_message);
        std::cout << " Received checksum: " << received_checksum << ", Calculated checksum: " << calculated_checksum << "  Message is save. "<< std::endl;

        if (calculated_checksum != received_checksum) {
            std::cerr << "Checksum mismatch! Message discarded." << std::endl;
            continue;
        }

        if (actual_message == "GONE") {
            {
                std::lock_guard<std::mutex> guard(clients_mutex);
                clients.erase(std::remove(clients.begin(), clients.end(), client_socket), clients.end());
                client_names.erase(client_socket);
            }
            close(client_socket);
            std::cout << client_name << " disconnected." << std::endl;
            return;
        }

        std::string formatted_message = client_name + ": " + actual_message;
        std::cout << "Received from " << formatted_message << std::endl;
        for (int client : clients) {
            if (client != client_socket) {
                unsigned int checksum = calculate_checksum(formatted_message);
                std::string message_with_checksum = formatted_message + "|" + std::to_string(checksum);
                send(client, message_with_checksum.c_str(), message_with_checksum.size(), 0);
            }
        }
    }
}

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(54000);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Failed to bind to IP/port" << std::endl;
        return 1;
    }

    if (listen(server_socket, SOMAXCONN) == -1) {
        std::cerr << "Failed to listen" << std::endl;
        return 1;
    }

    std::cout << "Server is listening on port 54000" << std::endl;

    while (true) {
        sockaddr_in client_addr;
        socklen_t client_size = sizeof(client_addr);
        int client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_size);
        if (client_socket == -1) {
            std::cerr << "Failed to accept client" << std::endl;
            continue;
        }

        std::thread client_thread(handle_client, client_socket);
        client_thread.detach();
    }

    close(server_socket);
    return 0;
}

