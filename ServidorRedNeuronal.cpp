#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fstream>

constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 1024;


struct FileConfig {
    std::vector<std::string> nombres;
    std::vector<int> ips;
    std::vector<bool> elegido;
};
FileConfig client_config;

void recibir_mensajes(int socket) {
    char buffer[BUFFER_SIZE];
    int bytes_recibidos;

    while ((bytes_recibidos = recv(socket, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytes_recibidos] = '\0';

        if (buffer[0] == 'M') {
            // Procesar vector de nombres
            client_config.nombres.clear();
            std::string data(buffer + 1);
            size_t pos = 0;
            std::string token;
            while ((pos = data.find(';')) != std::string::npos) {
                token = data.substr(0, pos);
                client_config.nombres.push_back(token);
                data.erase(0, pos + 1);
            }

            std::cout << "Contenido de la estructura recibida desde el servidor:" << std::endl;
            std::cout << "Nombres:" << std::endl;
            for (const auto& nombre : client_config.nombres) {
                std::cout << nombre << std::endl;
            }
        } else if (buffer[0] == 'I') {
            // Procesar vector de ips
            client_config.ips.clear();
            std::string data(buffer + 1);
            size_t pos = 0;
            std::string token;
            while ((pos = data.find(';')) != std::string::npos) {
                token = data.substr(0, pos);
                client_config.ips.push_back(std::stoi(token));
                data.erase(0, pos + 1);
            }

            std::cout << "IPs:" << std::endl;
            for (const auto& ip : client_config.ips) {
                std::cout << ip << std::endl;
            }
        } else if (buffer[0] == 'E') {
            // Procesar vector de elegido
            client_config.elegido.clear();
            std::string data(buffer + 1);
            size_t pos = 0;
            std::string token;
            while ((pos = data.find(';')) != std::string::npos) {
                token = data.substr(0, pos);
                client_config.elegido.push_back(token == "1");
                data.erase(0, pos + 1);
            }

            std::cout << "Elegidos:" << std::endl;
            for (const auto& elegido : client_config.elegido) {
                std::cout << (elegido ? "true" : "false") << std::endl;
            }
        } else {
            std::cout << "El servidor dice: " << buffer << std::endl;
        }
    }
}

int main() {
    int socket_cliente;
    struct sockaddr_in direccion_servidor;
    char buffer[BUFFER_SIZE];

    // Crear socket
    if ((socket_cliente = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Error al crear el socket" << std::endl;
        return -1;
    }

    // Configurar dirección del servidor
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &direccion_servidor.sin_addr) <= 0) {
        std::cerr << "Dirección IP inválida o no soportada" << std::endl;
        close(socket_cliente);
        return -1;
    }

    // Conectar al servidor
    if (connect(socket_cliente, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0) {
        std::cerr << "Conexión fallida" << std::endl;
        close(socket_cliente);
        return -1;
    }

    // Recibir el nombre asignado del servidor
    int bytes_recibidos = recv(socket_cliente, buffer, BUFFER_SIZE, 0);
    if (bytes_recibidos > 0) {
        buffer[bytes_recibidos] = '\0';
        std::string nombre_asignado = buffer;
        std::cout << "Conectado al servidor con el nombre: " << nombre_asignado << std::endl;
    } else {
        std::cerr << "Error al recibir el nombre asignado del servidor" << std::endl;
        close(socket_cliente);
        return -1;
    }

    // Iniciar el hilo para recibir mensajes del servidor
    std::thread hilo_receptor(recibir_mensajes, socket_cliente);

    while (true) {
        char comando;
        char mensaje[150];
        std::cout << "Seleccione una opción:\n";
        std::cout << "L para Lista\n";
        std::cout << "B para Broadcast\n";
        std::cout << "M: Enviar mensaje privado\n";
        std::cout << "X para Salir\n";
        std::cout << "K para Imprimir Estructura:\n"; 
        std::cout << "Ingrese su elección: ";
        std::cin >> comando; // Leer el comando
        std::cin.ignore(); // Ignorar el salto de línea

        // Enviar comando al servidor
        write(socket_cliente, &comando, sizeof(comando));

        switch (comando) {
            case 'X':
                // Si el comando es "X", terminar el programa
                close(socket_cliente);
                return 0;
            case 'B':
                // Si el comando es "B", pedir mensaje al usuario
                std::cout << ">> Ingrese mensaje: ";
                std::cin.getline(mensaje, BUFFER_SIZE); // Leer el mensaje

                // Enviar mensaje al servidor
                write(socket_cliente, mensaje, strlen(mensaje));
                break;
            case 'L':
                // Si el comando es "L", no es necesario enviar un mensaje adicional
                break;
            case 'M': {
                    // Leer el nombre del destinatario
                    std::string destinatario;
                    std::cout << ">> Ingrese el nombre del destinatario: ";
                    std::getline(std::cin, destinatario);
                    write(socket_cliente, destinatario.c_str(), destinatario.length());

                    // Leer el mensaje
                    std::string mensaje;
                    std::cout << ">> Ingrese el mensaje: ";
                    std::getline(std::cin, mensaje);
                    write(socket_cliente, mensaje.c_str(), mensaje.length());
                    break;
                }
            case 'K':{
                // Imprimir la estructura guardada localmente
                std::cout << "Contenido de la estructura almacenada en el cliente:" << std::endl;
                std::cout << "Nombres:" << std::endl;
                for (const auto& nombre : client_config.nombres) {
                    std::cout << nombre << std::endl;
                }
                
                std::cout << "IPs:" << std::endl;
                for (const auto& ip : client_config.ips) {
                    std::cout << ip << std::endl;
                }
                std::cout << "Elegidos:" << std::endl;
                for (const auto& elegido : client_config.elegido) {
                    std::cout << (elegido ? "true" : "false") << std::endl;
                }
                break;
            }
            case 'D': {
                // Revisar la estructura local para encontrar un cliente elegido
                bool cliente_encontrado = false;
                for (size_t i = 0; i < client_config.elegido.size(); ++i) {
                    if (client_config.elegido[i]) {
                        // Enviar mensaje al cliente seleccionado
                        std::string mensaje = "Hola master de media ojo";
                        int socket_destinatario = client_config.ips[i]; // Obtener el socket del cliente seleccionado
                        write(socket_cliente, mensaje.c_str(), mensaje.length());
                        cliente_encontrado = true;
                        break;
                    }
                }
                if (!cliente_encontrado) {
                    std::cout << "No se encontró ningún cliente elegido" << std::endl;
                }
                break;
            }

            default:
                std::cerr << "Comando no reconocido" << std::endl;
                break;
        }
    }

    return 0;
}
