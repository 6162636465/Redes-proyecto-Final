#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <map>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fstream>

constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 150;

std::map<std::string, int> client_list;

// Función para jugar al tic tac toe
bool play_tic_tac_toe(char board[3][3], char marca, int fila, int columna) {
    if (fila < 0 || fila >= 3 || columna < 0 || columna >= 3 || board[fila][columna] != ' ') {
        return false; // Posición inválida o ya ocupada
    }

    board[fila][columna] = marca;

    // Verificar si hay un ganador en la fila
    for (int i = 0; i < 3; ++i) {
        if (board[fila][i] != marca) break;
        if (i == 2) return true;
    }

    // Verificar si hay un ganador en la columna
    for (int i = 0; i < 3; ++i) {
        if (board[i][columna] != marca) break;
        if (i == 2) return true;
    }

    // Verificar si hay un ganador en la diagonal principal
    if (fila == columna) {
        for (int i = 0; i < 3; ++i) {
            if (board[i][i] != marca) break;
            if (i == 2) return true;
        }
    }

    // Verificar si hay un ganador en la diagonal secundaria
    if (fila + columna == 2) {
        for (int i = 0; i < 3; ++i) {
            if (board[i][2 - i] != marca) break;
            if (i == 2) return true;
        }
    }

    return false; // No hay ganador todavía
}

void handle_client(int client_socket) {
    char command;
    char message[BUFFER_SIZE];
    int bytes_received;

    std::string client_name;

    if ((bytes_received = read(client_socket, message, BUFFER_SIZE - 1)) > 0) {
        message[bytes_received] = '\0';
        client_name = message;
        client_list[client_name] = client_socket;
        std::cout << "Cliente conectado: " << client_name << std::endl;
        std::string welcome_msg = "¡Hola " + client_name + "! Bienvenido al servidor de chat.";
        send(client_socket, welcome_msg.c_str(), welcome_msg.length(), 0);
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

            case 'G': {
                // Leer la posición y el nombre del jugador
                int position;
                bytes_received = read(client_socket, &position, sizeof(position));
                if (bytes_received != sizeof(position)) {
                    std::cerr << "Error al leer la posición del tablero" << std::endl;
                    break;
                }

                // Verificar si la posición es válida
                if (position < 1 || position > 9) {
                    std::string error_msg = "Posición inválida. Debe estar entre 1 y 9.";
                    send(client_socket, error_msg.c_str(), error_msg.length(), 0);
                    break;
                }

                // Leer el nombre del jugador
                std::string player_name = client_name.substr(0, 1); // Primer caracter del nombre del jugador

                // Actualizar el tablero del tic tac toe
                static char board[3][3] = {{' ', ' ', ' '}, {' ', ' ', ' '}, {' ', ' ', ' '}};
                int row = (position - 1) / 3;
                int col = (position - 1) % 3;

                if (play_tic_tac_toe(board, player_name[0], row, col)) {
                    // Si hay un ganador, enviar mensaje de victoria
                    std::string victory_msg = "¡" + player_name + " ha ganado!";
                    send(client_socket, victory_msg.c_str(), victory_msg.length(), 0);
                } else {
                    // Si no hay ganador, enviar el estado actual del tablero
                    std::string board_state = "Estado actual del tablero:\n";
                    for (int i = 0; i < 3; ++i) {
                        for (int j = 0; j < 3; ++j) {
                            board_state += board[i][j];
                        }
                        board_state += '\n';
                    }
                    send(client_socket, board_state.c_str(), board_state.length(), 0);
                }
                break;
            }

            case 'I':{
                // Leer tamaño del nombre del archivo
                char filename_size_str[3];
                read(client_socket, filename_size_str, 2);
                filename_size_str[2] = '\0';
                int filename_size = atoi(filename_size_str);

                // Leer nombre del archivo
                char filename[filename_size];
                read(client_socket, filename, filename_size);

                // Leer tamaño del archivo
                char filesize_str[16];
                read(client_socket, filesize_str, 15);
                filesize_str[15] = '\0';
                long int filesize = atol(filesize_str);

                // Leer tamaño del nombre del destinatario
                char reciever_size_str[3];
                read(client_socket, reciever_size_str, 2);
                reciever_size_str[2] = '\0';
                int reciever_size = atoi(reciever_size_str);

                // Leer nombre del destinatario
                char reciever[reciever_size];
                read(client_socket, reciever, reciever_size);

                // Archivo de destino en el servidor
                std::ofstream outfile(std::string(filename), std::ios::binary);

                // Recibir el archivo
                char buffer[BUFFER_SIZE];
                int bytes_received;
                while (filesize > 0) {
                    bytes_received = read(client_socket, buffer, std::min(BUFFER_SIZE, (int)filesize));
                    outfile.write(buffer, bytes_received);
                    filesize -= bytes_received;
                }
                outfile.close();

                // Notificar al cliente emisor
                std::string confirmation_msg = "Archivo \"" + std::string(filename) + "\" enviado correctamente";
                send(client_socket, confirmation_msg.c_str(), confirmation_msg.length(), 0);

                // Enviar el archivo al destinatario si está conectado
                auto it = client_list.find(std::string(reciever));
                if (it != client_list.end()) {
                    // Enviar comando 'I' al destinatario
                    char i_command = 'I';
                    send(it->second, &i_command, sizeof(i_command), 0);

                    // Enviar tamaño del nombre del archivo al destinatario
                    send(it->second, filename_size_str, 2, 0);

                    // Enviar nombre del archivo al destinatario
                    send(it->second, filename, filename_size, 0);

                    // Enviar tamaño del archivo al destinatario
                    send(it->second, filesize_str, 15, 0);

                    // Enviar tamaño del nombre del destinatario al destinatario
                    send(it->second, reciever_size_str, 2, 0);

                    // Enviar nombre del destinatario al destinatario
                    send(it->second, reciever, reciever_size, 0);

                    // Enviar el archivo al destinatario
                    std::ifstream infile(std::string(filename), std::ios::binary);
                    while (infile) {
                        infile.read(buffer, BUFFER_SIZE);
                        int bytes_read = infile.gcount();
                        send(it->second, buffer, bytes_read, 0);
                    }
                    infile.close();

                    // Notificar al cliente destinatario
                    std::string notification_msg = "Archivo \"" + std::string(filename) + "\" enviado a " + std::string(reciever);
                    send(client_socket, notification_msg.c_str(), notification_msg.length(), 0);
                } else {
                    // Notificar al cliente emisor si el destinatario no está conectado
                    std::string error_msg = "El destinatario \"" + std::string(reciever) + "\" no está conectado.";
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
