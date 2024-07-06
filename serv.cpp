#include <iostream>
#include <thread>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <mutex>
#include <random>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <algorithm>
#include <chrono>

struct ServerConfig {
    std::vector<int> nombres;  // client_id
    std::vector<int> ips;  // client_socket
    std::vector<bool> elegido;
};

std::map<std::string, int> terminal_sockets;
std::map<std::string, sockaddr_in> client_udp_sockets;
std::multimap<int, std::string> DataAprendisaje;
ServerConfig server_config;
std::mutex config_mutex;
std::mutex message_count_mutex;
bool keep_alive_active = false;
bool first_message_received = false;
int message_count = 0;
int cantidad_max_Usuarios = 4;

void handle_tcp_client(int client_socket, std::string client_ip, int udp_socket);
void handle_udp_connection(int udp_socket);
void udp_send_function(int udp_socket, const std::string &message, const sockaddr_in *client_addr = nullptr);
void tcp_broadcast_function(const std::string &message);
void update_selected_client();
void load_data_aprendisaje(const std::string &filename);
void print_data_aprendisaje();
void distribute_data_to_clients(int udp_socket);
void keep_alive_monitor();

int main() {
    load_data_aprendisaje("data.csv");
    
    // Setup TCP server
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in tcp_server_addr;
    tcp_server_addr.sin_family = AF_INET;
    tcp_server_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_server_addr.sin_port = htons(8080);

    bind(tcp_socket, (struct sockaddr *)&tcp_server_addr, sizeof(tcp_server_addr));
    listen(tcp_socket, 5);

    // Setup UDP server
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in udp_server_addr;
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_addr.s_addr = INADDR_ANY;
    udp_server_addr.sin_port = htons(8081);

    bind(udp_socket, (struct sockaddr *)&udp_server_addr, sizeof(udp_server_addr));

    std::thread udp_thread(handle_udp_connection, udp_socket);
    udp_thread.detach();

    // Crear y manejar el hilo de keep-alive monitor en el main
    std::thread keep_alive_thread(keep_alive_monitor);
    keep_alive_thread.detach();

    while (true) {
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int new_socket = accept(tcp_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        std::string client_ip = inet_ntoa(client_addr.sin_addr);

        std::thread tcp_thread(handle_tcp_client, new_socket, client_ip, udp_socket);
        tcp_thread.detach();
    }

    close(tcp_socket);
    close(udp_socket);

    return 0;
}

void handle_tcp_client(int client_socket, std::string client_ip, int udp_socket) {
    char buffer[1024];
    while (true) {
        int bytes_received = read(client_socket, buffer, sizeof(buffer));
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::string message(buffer);

            std::lock_guard<std::mutex> lock(config_mutex);
            switch (message[0]) {
                case '1': {
                    int client_id = server_config.nombres.size() + 1;
                    server_config.nombres.push_back(client_id);
                    server_config.ips.push_back(client_socket);
                    server_config.elegido.push_back(false);
                    terminal_sockets[client_ip] = client_socket;
                    std::cout << "TCP client connected: " << client_ip << " with ID " << client_id << std::endl;
                    if (server_config.nombres.size() == cantidad_max_Usuarios) {
                        update_selected_client();
                        keep_alive_active = true;
                        // Esperar 1 segundo antes de enviar el comando 'H'
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        // Enviar comando 'H' a todos los clientes TCP para activar el keep-alive
                        for (int socket : server_config.ips) {
                            std::string keep_alive_command = "H";
                            send(socket, keep_alive_command.c_str(), keep_alive_command.size(), 0);
                        }
                    }
                    break;
                }
                case 'H': {
                    std::lock_guard<std::mutex> count_lock(message_count_mutex);
                    message_count++;
                    first_message_received = true;
                    break;
                }
                case 'A': {
                    // Broadcast message to all UDP clients
                    std::cout << "Broadcasting message: " << message << std::endl;
                    udp_send_function(udp_socket, message);
                    break;
                }
                case 'B': {
                    for (size_t i = 0; i < server_config.nombres.size(); ++i) {
                        std::cout << "Client ID " << server_config.nombres[i] << " - Socket: " << server_config.ips[i] << " - Elegido: " << server_config.elegido[i] << std::endl;
                    }
                    break;
                }
                case 'G': {
                    distribute_data_to_clients(udp_socket);
                    break;
                }
                case 'M': {
                    print_data_aprendisaje();
                    break;
                }
                default: {
                    std::cout << "Received TCP message: " << message << std::endl;
                    break;
                }
            }
        } else {
            close(client_socket);
            break;
        }
    }
}

void handle_udp_connection(int udp_socket) {
    while (true) {
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        char buffer[1024];
        int bytes_received = recvfrom(udp_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            std::lock_guard<std::mutex> lock(config_mutex);
            client_udp_sockets[client_ip + ":" + std::to_string(ntohs(client_addr.sin_port))] = client_addr;
            std::cout << "UDP client connected: " << client_ip << " on port " << ntohs(client_addr.sin_port) << std::endl;
        }
    }
}

// Function to send UDP messages
void udp_send_function(int udp_socket, const std::string &message, const sockaddr_in *client_addr) {
    if (client_addr) {
        // Send to a specific client
        sendto(udp_socket, message.c_str(), message.size(), 0, (struct sockaddr *)client_addr, sizeof(*client_addr));
    } else {
        // Broadcast to all clients
        std::cout << "Broadcasting para todos los UDP clients" << std::endl;
        for (const auto& [client_key, client_addr] : client_udp_sockets) {
            std::cout << "Sending to UDP client: " << client_key << std::endl;
            sendto(udp_socket, message.c_str(), message.size(), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
        }
    }
}

// Function to send TCP broadcast messages
void tcp_broadcast_function(const std::string &message) {
    for (const auto& socket : server_config.ips) {
        send(socket, message.c_str(), message.size(), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Agregar un pequeño retraso entre envíos
    }
}

void update_selected_client() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, cantidad_max_Usuarios - 1);

    int selected_index = dis(gen);
    for (size_t i = 0; i < server_config.elegido.size(); ++i) {
        server_config.elegido[i] = (i == selected_index);
    }

    // Send the names
    std::string names_message = "1";
    for (const auto& id : server_config.nombres) {
        names_message += std::to_string(id) + ",";
    }
    tcp_broadcast_function(names_message);

    // Send the IPs (which are actually the sockets)
    std::string ips_message = "2";
    for (const auto& socket : server_config.ips) {
        ips_message += std::to_string(socket) + ",";
    }
    tcp_broadcast_function(ips_message);

    // Send the selected
    std::string selected_message = "3";
    for (const auto& selected : server_config.elegido) {
        selected_message += (selected ? "1" : "0") + std::string(",");
    }
    tcp_broadcast_function(selected_message);
}

// Function to load data from CSV
void load_data_aprendisaje(const std::string &filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        int key;
        std::string value;
        if (iss >> key) {
            std::getline(iss, value);
            value.erase(std::remove(value.begin(), value.end(), ' '), value.end());
            DataAprendisaje.insert({key, value});
        }
    }

    file.close();
}

// Function to print the data structure
void print_data_aprendisaje() {
    std::cout << "DataAprendisaje:" << std::endl;
    for (const auto& [key, value] : DataAprendisaje) {
        std::cout << "  " << key << " => " << value << std::endl;
    }
    std::cout << "Total elements: " << DataAprendisaje.size() << std::endl;
}

// Function to distribute data to clients
void distribute_data_to_clients(int udp_socket) {
    int num_clients = server_config.nombres.size();
    if (num_clients == 0) {
        std::cerr << "No clients connected to distribute data" << std::endl;
        return;
    }

    int part_size = DataAprendisaje.size() / num_clients;
    auto it = DataAprendisaje.begin();

    for (const auto& [client_ip, client_addr] : client_udp_sockets) {
        std::string key_message = "K";
        std::string value_message = "L";
        for (int i = 0; i < part_size && it != DataAprendisaje.end(); ++i, ++it) {
            key_message += std::to_string(it->first) + ",";
            value_message += it->second + ",";
        }
        udp_send_function(udp_socket, key_message, &client_addr);
        udp_send_function(udp_socket, value_message, &client_addr);
    }

    // If there are remaining elements, send them to the last client
    if (it != DataAprendisaje.end()) {
        std::string key_message = "K";
        std::string value_message = "L";
        for (; it != DataAprendisaje.end(); ++it) {
            key_message += std::to_string(it->first) + ",";
            value_message += it->second + ",";
        }
        auto last_client_addr = client_udp_sockets.rbegin()->second;
        udp_send_function(udp_socket, key_message, &last_client_addr);
        udp_send_function(udp_socket, value_message, &last_client_addr);
    }
}

// Keep-alive monitor function
void keep_alive_monitor() {
    while (true) {
        if (keep_alive_active) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (first_message_received) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                std::lock_guard<std::mutex> lock(message_count_mutex);
                std::cout << "Mensajes recibidos en el lapso del timeout: " << message_count << std::endl;
                message_count = 0;
                first_message_received = false;
            }
        }
    }
}
