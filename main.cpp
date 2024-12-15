#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <csignal>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

constexpr int BUFFER_SIZE = 4096;

std::map<std::string, std::string> routes;
std::atomic<bool> running(true);

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutting down server gracefully...\n";
        running = false;
    }
}

bool load_config(const std::string& filename) {
    std::ifstream config_file(filename);
    if (!config_file) {
        std::cerr << "Failed to open config file: " << filename << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(config_file, line)) {
        std::istringstream line_stream(line);
        std::string path, port;
        if (std::getline(line_stream, path, ':') && std::getline(line_stream, port)) {
            routes[path] = port;
        } else {
            std::cerr << "Invalid config line: " << line << std::endl;
        }
    }
    return true;
}

void handle(int client_socket) {
    std::vector<char> buffer(BUFFER_SIZE, 0);

    ssize_t bytes_received = read(client_socket, buffer.data(), buffer.size() - 1);
    if (bytes_received < 0) {
        std::cerr << "Failed to read from client." << std::endl;
        close(client_socket);
        return;
    }

    std::string request(buffer.begin(), buffer.end());
    std::cout << "Received request:\n" << request << std::endl;

    std::istringstream request_stream(request);
    std::string method, path, version;
    request_stream >> method >> path >> version;

    std::string response;
    if (routes.find(path) != routes.end()) {
        std::string port = routes[path];
        response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: " + std::to_string(path.size() + port.size() + 14) + "\r\n"
                                                                                      "\r\n" +
                "Path: " + path + ", Port: " + port;
    } else {
        response =
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 9\r\n"
                "\r\n"
                "Not Found";
    }

    write(client_socket, response.c_str(), response.size());
    close(client_socket);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <config_file>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    std::string config_file = argv[2];

    if (!load_config(config_file)) {
        return 1;
    }

    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        return 1;
    }

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (sockaddr*)&server_address, sizeof(server_address)) == -1) {
        std::cerr << "Failed to bind socket." << std::endl;
        close(server_socket);
        return 1;
    }

    if (listen(server_socket, SOMAXCONN) == -1) {
        std::cerr << "Failed to listen on socket." << std::endl;
        close(server_socket);
        return 1;
    }

    std::cout << "Server is running on port " << port << "..." << std::endl;

    while (running) {
        sockaddr_in client_address{};
        socklen_t client_len = sizeof(client_address);
        int client_socket = accept(server_socket, (sockaddr*)&client_address, &client_len);

        if (client_socket == -1) {
            if (running) {
                std::cerr << "Failed to accept client connection." << std::endl;
            }
            continue;
        }

        handle(client_socket);
    }

    std::cout << "Closing server socket...\n";
    close(server_socket);
    return 0;
}
