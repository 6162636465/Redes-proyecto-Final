#include <iostream>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SERVER_PORT 8080
#define UDP_PORT 9090
#define BUFFER_SIZE 1024

void send_udp_message(int udp_socket, struct sockaddr_in &server_addr, const std::string &message) {
    if (sendto(udp_socket, message.c_str(), message.length(), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error sending UDP message" << std::endl;
    } else {
        std::cout << "UDP message sent to " << inet_ntoa(server_addr.sin_addr) << ":" << ntohs(server_addr.sin_port) << std::endl;
    }
}

int main() {
    int tcp_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // Crear socket TCP
    if ((tcp_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Error al crear el socket TCP" << std::endl;
        return -1;
    }

    // Configurar dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        std::cerr << "Dirección IP inválida o no soportada" << std::endl;
        close(tcp_socket);
        return -1;
    }

    // Conectar al servidor
    if (connect(tcp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Conexión fallida" << std::endl;
        close(tcp_socket);
        return -1;
    }

    // Crear socket UDP
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);

    // Configurar dirección del servidor UDP
    struct sockaddr_in udp_server_addr;
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_port = htons(UDP_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &udp_server_addr.sin_addr) <= 0) {
        std::cerr << "Dirección IP inválida o no soportada" << std::endl;
        close(udp_socket);
        return -1;
    }

    // Enviar mensaje de autologin
    send_udp_message(udp_socket, udp_server_addr, "2");

    while (true) {
        std::cout << "Enter a message: ";
        std::string message;
        std::getline(std::cin, message);

        // Agregar "A" al inicio del mensaje si corresponde
        if (message[0] == 'A') {
            send_udp_message(udp_socket, udp_server_addr, message);
        }
    }

    close(tcp_socket);
    close(udp_socket);
    return 0;
}
