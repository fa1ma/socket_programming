#include <iostream>
#include <thread>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <numeric>

unsigned int calculate_checksum(const std::string& message) {
    return std::accumulate(message.begin(), message.end(), 0);
}

void log_message(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::tm *tm_ptr = std::localtime(&in_time_t);
    std::ostringstream ss;
    ss << (tm_ptr->tm_year + 1900) << '-'
       << (tm_ptr->tm_mon + 1) << '-'
       << tm_ptr->tm_mday << '_'
       << tm_ptr->tm_hour << '-'
       << tm_ptr->tm_min << '-'
       << tm_ptr->tm_sec;
    std::string timestamp = ss.str();

    std::ofstream log_file("logs/client_log_" + timestamp + ".txt", std::ios::app);
    log_file << message << std::endl;
}

void handle_server(int server_socket) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, 1024);
        int bytes_received = recv(server_socket, buffer, 1024, 0);
        if (bytes_received <= 0) {
            std::cout << "Disconnected from server" << std::endl;
            close(server_socket);
            return;
        }
        std::string message(buffer);
        size_t separator_pos = message.find('|');
        if (separator_pos == std::string::npos) continue;

        std::string actual_message = message.substr(0, separator_pos);
        unsigned int received_checksum = std::stoi(message.substr(separator_pos + 1));

        unsigned int calculated_checksum = calculate_checksum(actual_message);
        

        if (calculated_checksum != received_checksum) {
            std::cerr << "Checksum mismatch! Message discarded." << std::endl;
            continue;
        }

        std::cout << actual_message << std::endl;
        log_message(actual_message);
    }
}

int main() {
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(54000);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(client_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }

    std::string username;
    std::cout << "Enter your username: ";
    std::getline(std::cin, username);
    send(client_socket, username.c_str(), username.size(), 0);

    std::thread server_thread(handle_server, client_socket);
    server_thread.detach();

    std::string input;
    while (true) {
        std::getline(std::cin, input);
        if (input == "exit") {
            unsigned int checksum = calculate_checksum("GONE");
            std::string message_with_checksum = "GONE|" + std::to_string(checksum);
            send(client_socket, message_with_checksum.c_str(), message_with_checksum.size(), 0);
            break;
        }
        unsigned int checksum = calculate_checksum(input);
        std::string message_with_checksum = input + "|" + std::to_string(checksum);
        //std::cout << "Sending message: " << input << ", Checksum: " << checksum << std::endl;
        send(client_socket, message_with_checksum.c_str(), message_with_checksum.size(), 0);
        log_message(username + ": " + input);
    }

    close(client_socket);
    return 0;
}

