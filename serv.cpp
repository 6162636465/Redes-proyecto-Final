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
void handle_client(int client_socket) {
    char command;
    char message[BUFFER_SIZE];
    int bytes_received;

    // Leer nombre de cliente
    std::string client_name;
    if ((bytes_received = read(client_socket, message, BUFFER_SIZE - 1)) > 0) {
        message[bytes_received] = '\0';
        client_name = message;
        client_names[client_socket] = client_name;
        std::cout << "Cliente conectado: " << client_name << std::endl;
        server_config.nombres.push_back(client_name); // Agregar nombre del cliente a la estructura
        server_config.ips.push_back(client_socket); // Agregar socket del cliente a la estructura
        server_config.elegido.push_back(false); // Inicializar como no elegido
        servidorRedNeuronal++; // Incrementar contador de usuarios conectados
    }

    // Verificar si se alcanzó el límite de usuarios
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

    while ((bytes_received = read(client_socket, &command, sizeof(command))) > 0) {
        switch (command) {
            case 'X':
                // El cliente solicita cerrar la conexión
                std::cout << "Cliente desconectado: " << client_name << std::endl;
                // Eliminar cliente de la lista
                client_list.erase(client_name);
                // Cerrar conexión
                close(client_socket);
                return;
            case 'L':
                // Enviar lista de usuarios conectados
                {
                    std::string lista_usuarios = "Usuarios conectados:\n";
                    for (const auto& pair : client_list) {
                        lista_usuarios += pair.first + "\n";
                    }
                    std::cout << lista_usuarios; // Imprimir en el servidor
                    send(client_socket, lista_usuarios.c_str(), lista_usuarios.length(), 0); // Enviar al cliente
                    break;
                }
            case 'B':
                if ((bytes_received = read(client_socket, message, BUFFER_SIZE - 1)) > 0) {
                    message[bytes_received] = '\0';

                    std::cout << "Mensaje broadcast de " << client_name << ": " << message << std::endl;
                    // Broadcast the message to all clients
                    for (auto& pair : client_list) {
                        send(pair.second, message, strlen(message), 0);
                    }
                }
                break;
            case 'M': {
                // Obtener el nombre del remitente
                std::string remitente = client_name;

                // Leer el nombre del destinatario
                std::string destinatario;
                bytes_received = read(client_socket, message, BUFFER_SIZE - 1);
                if (bytes_received > 0) {
                    message[bytes_received] = '\0';
                    destinatario = message;
                } else {
                    std::cerr << "Error al leer el nombre del destinatario" << std::endl;
                    break;
                }

                // Leer el mensaje
                std::string mensaje;
                bytes_received = read(client_socket, message, BUFFER_SIZE - 1);
                if (bytes_received > 0) {
                    message[bytes_received] = '\0';
                    mensaje = message;
                } else {
                    std::cerr << "Error al leer el mensaje" << std::endl;
                    break;
                }

                // Imprimir el remitente, el destinatario y el mensaje
                std::cout << "Remitente: " << remitente << std::endl;
                std::cout << "Destinatario: " << destinatario << std::endl;
                std::cout << "Mensaje: " << mensaje << std::endl;

                // Buscar el destinatario en la lista de clientes
                auto it = client_list.find(destinatario);
                if (it != client_list.end()) {
                    // Enviar el mensaje al destinatario
                    std::string mensaje_completo = remitente + ": " + mensaje;
                    send(it->second, mensaje_completo.c_str(), mensaje_completo.length(), 0);
                } else {
                    // Destinatario no encontrado
                    std::string error_msg = "¡El destinatario no existe!";
                    send(client_socket, error_msg.c_str(), error_msg.length(), 0);
                }
                break;
            }
            default:
                std::cout << "Comando inválido" << std::endl;
                break;
        }
    }
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        std::cerr << "Falló la creación del socket" << std::endl;
        return -1;
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        std::cerr << "Falló la configuración del socket" << std::endl;
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Falló el enlace del socket" << std::endl;
        return -1;
    }

    if (listen(server_socket, 3) < 0) {
        std::cerr << "Falló al intentar escuchar en el puerto" << std::endl;
        return -1;
    }

    std::cout << "Esperando conexiones..." << std::endl;

    while (true) {
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len)) < 0) {
            std::cerr << "Error en la aceptación de la conexión" << std::endl;
            return -1;
        }

        std::thread client_thread(handle_client, client_socket);
        client_thread.detach();
    }

    return 0;
}
