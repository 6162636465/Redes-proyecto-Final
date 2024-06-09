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

int main() {
    int socket_servidor, socket_cliente;
    struct sockaddr_in direccion_servidor, direccion_cliente;
    socklen_t len_direccion = sizeof(direccion_cliente);

    // Crear socket
    if ((socket_servidor = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        std::cerr << "Error al crear el socket" << std::endl;
        return -1;
    }

    // Configurar dirección del servidor
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_addr.s_addr = INADDR_ANY;
    direccion_servidor.sin_port = htons(PORT);

    // Enlazar el socket a la dirección y puerto
    if (bind(socket_servidor, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0) {
        std::cerr << "Error al enlazar el socket" << std::endl;
        close(socket_servidor);
        return -1;
    }

    // Escuchar conexiones entrantes
    if (listen(socket_servidor, 3) < 0) {
        std::cerr << "Error al escuchar en el socket" << std::endl;
        close(socket_servidor);
        return -1;
    }

    std::cout << "Servidor escuchando en el puerto " << PORT << std::endl;

    while (true) {
        if ((socket_cliente = accept(socket_servidor, (struct sockaddr *)&direccion_cliente, &len_direccion)) < 0) {
            std::cerr << "Error al aceptar la conexión" << std::endl;
            close(socket_servidor);
            return -1;
        }

        std::cout << "Nueva conexión aceptada" << std::endl;

        // Crear un hilo para manejar al cliente
        std::thread(manejar_cliente, socket_cliente, direccion_cliente).detach();
    }

    close(socket_servidor);
    return 0;
}
