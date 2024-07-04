#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <map>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fstream>
#include <random>
#include <sys/types.h>
#include <netinet/in.h>
using namespace std;
#define UDP_PORT 9090

std::map<std::string, sockaddr_in> udp_clients;
std::map<std::string, sockaddr_in> neural_clients;

constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 150;

struct ServerConfig {
    std::vector<std::string> nombres;
    std::vector<int> ips;
    std::vector<bool> elegido;
};

std::map<int, std::string> client_names; // Mapeo de socket a nombre de cliente
ServerConfig server_config;
int servidorRedNeuronal = 0; // Variable para contar los usuarios conectados
int client_counter = 1; // Contador de clientes para asignar nombres numéricos

std::map<std::string, int> client_list;

void marcarElegido(ServerConfig& config) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, config.nombres.size() - 1);
    int index = dis(gen); // Índice aleatorio
    for (size_t i = 0; i < config.elegido.size(); ++i) {
        if (i == index) {
            config.elegido[i] = true;
        } else {
            config.elegido[i] = false;
        }
    }
}
void send_to_all_clients(const std::string& message) {
    for (const auto& pair : client_list) {
        send(pair.second, message.c_str(), message.size(), 0);
    }
}

void send_names(int client_socket, const std::vector<std::string>& nombres) {
    std::string message = "M";
    for (const auto& nombre : nombres) {
        message += nombre + ";";
    }
    send(client_socket, message.c_str(), message.size(), 0);
}
void send_vectorA(int client_socket, const std::string& identifier, const std::vector<int>& vector_data) {
    std::string message = identifier;
    for (const auto& item : vector_data) {
        message += std::to_string(item) + ";";
    }
    send(client_socket, message.c_str(), message.size(), 0);
}

void send_vectorB(int client_socket, const std::string& identifier, const std::vector<bool>& vector_data) {
    std::string message = identifier;
    for (const auto& item : vector_data) {
        message += (item ? "1" : "0") + std::string(";");
    }
    send(client_socket, message.c_str(), message.size(), 0);
}
void manejar_cliente(int socket_cliente, sockaddr_in direccion_cliente) {
    char buffer[BUFFER_SIZE];
    int bytes_recibidos;

    // Asignar nombre automático al cliente
    std::string nombre_cliente = std::to_string(client_counter++);
    client_names[socket_cliente] = nombre_cliente;
    server_config.nombres.push_back(nombre_cliente);
    server_config.ips.push_back(ntohs(direccion_cliente.sin_port)); // Guardar el puerto del cliente en orden de llegada
    server_config.elegido.push_back(false); // Por defecto, nadie está elegido
    client_list[nombre_cliente] = socket_cliente;

    // Enviar el nombre asignado al cliente
    send(socket_cliente, nombre_cliente.c_str(), nombre_cliente.length(), 0);

    std::cout << "Cliente " << nombre_cliente << " se ha conectado desde el puerto " << ntohs(direccion_cliente.sin_port) << std::endl;

    servidorRedNeuronal++;
    if (servidorRedNeuronal == 4) {
        // Seleccionar aleatoriamente un cliente y marcarlo como elegido
        marcarElegido(server_config);

        // Enviar el vector de nombres a todos los clientes
        for (const auto& pair : client_names) {
            send_names(pair.first, server_config.nombres);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            send_vectorA(pair.first, "I", server_config.ips);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            send_vectorB(pair.first, "E", server_config.elegido);
        }
    }
    while (true) {
        if ((bytes_recibidos = recv(socket_cliente, buffer, BUFFER_SIZE, 0)) <= 0) {
            std::cerr << "Error al recibir datos del cliente o conexión cerrada" << std::endl;
            close(socket_cliente);
            break;
        }

        buffer[bytes_recibidos] = '\0';
        char comando = buffer[0];
        
        switch (comando) {
            case 'L': {
                // Enviar la lista de clientes conectados
                std::string lista = "Clientes conectados:\n";
                for (const auto& nombre : server_config.nombres) {
                    lista += nombre + "\n";
                }
                send(socket_cliente, lista.c_str(), lista.size(), 0);
                break;
            }
            case 'B': {
                // Enviar mensaje broadcast a todos los clientes
                std::string mensaje = std::string(buffer + 1);
                std::cout << "Mensaje de broadcast recibido: " << mensaje << std::endl;
                for (const auto& [socket, nombre] : client_names) {
                    send(socket, mensaje.c_str(), mensaje.size(), 0);
                }
                break;
            }
            case 'M': {
                // Enviar mensaje privado
                std::string destinatario(buffer + 1);
                std::string mensaje;
                if ((bytes_recibidos = recv(socket_cliente, buffer, BUFFER_SIZE, 0)) > 0) {
                    buffer[bytes_recibidos] = '\0';
                    mensaje = std::string(buffer);
                }

                if (client_list.find(destinatario) != client_list.end()) {
                    int socket_destinatario = client_list[destinatario];
                    send(socket_destinatario, mensaje.c_str(), mensaje.size(), 0);
                } else {
                    std::string error_msg = "El cliente no está conectado.\n";
                    send(socket_cliente, error_msg.c_str(), error_msg.size(), 0);
                }
                break;
            }
            case 'X': {
                std::string nombre_cliente = client_names[socket_cliente];
                std::cout << nombre_cliente << " se ha desconectado." << std::endl;
                client_names.erase(socket_cliente);
                close(socket_cliente);
                return;
            }
            default:
                std::cerr << "Comando no reconocido: " << comando << std::endl;
                break;
        }
    }
}

void udp_send_function(int udp_socket, const char *message, const std::map<std::string, sockaddr_in> &clients) {
    for (const auto &client : clients) {
        std::cout << "Attempting to send message to " << inet_ntoa(client.second.sin_addr) << ":" << ntohs(client.second.sin_port) << std::endl;
        if (sendto(udp_socket, message, strlen(message), 0, (struct sockaddr *)&client.second, sizeof(client.second)) < 0) {
            std::cerr << "Error sending message to " << inet_ntoa(client.second.sin_addr) << std::endl;
        } else {
            std::cout << "Message sent to " << inet_ntoa(client.second.sin_addr) << ":" << ntohs(client.second.sin_port) << std::endl;
        }
    }
}

void udp_receive_function(int udp_socket) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[1024];

    while (true) {
        int bytes_received = recvfrom(udp_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::string client_ip = inet_ntoa(client_addr.sin_addr);

            std::cout << "Received message from " << client_ip << ":" << ntohs(client_addr.sin_port) << " - " << buffer << std::endl;

            if (buffer[0] == '1') {
                neural_clients[client_ip] = client_addr;
                std::cout << "Stored neural client " << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;
            } else if (buffer[0] == '2') {
                udp_clients[client_ip] = client_addr;
                std::cout << "Stored UDP client " << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;
            }

            // Enviar el mensaje a todos los clientes en neural_clients si el mensaje comienza con 'A'
            if (buffer[1] == 'A') {
                std::cout << "Message enviado a redN: " << buffer << std::endl;
                udp_send_function(udp_socket, buffer , neural_clients);
            } else {
                // Imprimir el mensaje si no cumple ninguna condición
                std::cout << "Message no entro Condicion: " << buffer << std::endl;
            }
        }
    }
}

int main() {
    int socket_servidor, socket_cliente;
    struct sockaddr_in direccion_servidor, direccion_cliente;
    socklen_t len_direccion = sizeof(direccion_cliente);

    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);

    // Crear socket TCP
    if ((socket_servidor = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        std::cerr << "Error al crear el socket" << std::endl;
        return -1;
    }

    // Configurar dirección del servidor TCP
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_addr.s_addr = INADDR_ANY;
    direccion_servidor.sin_port = htons(PORT);

    // Enlazar el socket TCP a la dirección y puerto
    if (bind(socket_servidor, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0) {
        std::cerr << "Error al enlazar el socket" << std::endl;
        close(socket_servidor);
        return -1;
    }

    // Escuchar conexiones entrantes TCP
    if (listen(socket_servidor, 3) < 0) {
        std::cerr << "Error al escuchar en el socket" << std::endl;
        close(socket_servidor);
        return -1;
    }

    // Configurar y enlazar socket UDP
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_addr.s_addr = INADDR_ANY;
    direccion_servidor.sin_port = htons(PORT);
    if (bind(udp_socket, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0) {
        std::cerr << "Error al enlazar el socket UDP" << std::endl;
        close(udp_socket);
        return -1;
    }

    // Iniciar el hilo para recibir mensajes UDP
    std::thread udp_receive_thread(udp_receive_function, udp_socket);

    std::cout << "Servidor escuchando en el puerto " << PORT << std::endl;

    while (true) {
        if ((socket_cliente = accept(socket_servidor, (struct sockaddr *)&direccion_cliente, &len_direccion)) < 0) {
            std::cerr << "Error al aceptar la conexión" << std::endl;
            close(socket_servidor);
            return -1;
        }

        std::cout << "Nueva conexión aceptada" << std::endl;

        // Crear un hilo para manejar al cliente TCP
        std::thread(manejar_cliente, socket_cliente, direccion_cliente).detach();
    }

    udp_receive_thread.join();
    close(socket_servidor);
    close(udp_socket);
    return 0;
}