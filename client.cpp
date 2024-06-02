#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fstream>

constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 150;

void recibir_mensajes(int socket) {
    char buffer[BUFFER_SIZE];
    int bytes_recibidos;

    while ((bytes_recibidos = read(socket, buffer, BUFFER_SIZE)) > 0) {
        buffer[bytes_recibidos] = '\0';
        std::cout << "El servidor dice: " << buffer << std::endl;

        // Si el mensaje comienza con "Archivo", se espera recibir un archivo
        if (strncmp(buffer, "Archivo", 7) == 0) {
            std::string archivo = buffer + 9; // Saltar "Archivo " en el mensaje
            // Recibir el archivo y guardarlo en el directorio local
            std::ofstream file(archivo, std::ios::binary);
            while ((bytes_recibidos = read(socket, buffer, BUFFER_SIZE)) > 0) {
                file.write(buffer, bytes_recibidos);
            }
            file.close();
        }
    }
}

void send_file(int client_socket, const std::string& filename, const std::string& reciever) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.good()) {
        std::cerr << "No se pudo abrir el archivo \"" << filename << "\"" << std::endl;
        return;
    }

    // Get file size
    file.seekg(0, std::ios::end);
    long int filesize = file.tellg();
    file.seekg(0, std::ios::beg);

    // Send the filename size
    char filename_size_str[3];
    sprintf(filename_size_str, "%2d", static_cast<int>(filename.size()));
    send(client_socket, filename_size_str, 2, 0);

    // Send the filename
    send(client_socket, filename.c_str(), filename.size(), 0);

    // Send the file size
    char filesize_str[16];
    sprintf(filesize_str, "%15ld", filesize);
    send(client_socket, filesize_str, 15, 0);

    // Send the recipient's name size
    char reciever_size_str[3];
    sprintf(reciever_size_str, "%2d", static_cast<int>(reciever.size()));
    send(client_socket, reciever_size_str, 2, 0);

    // Send the recipient's name
    send(client_socket, reciever.c_str(), reciever.size(), 0);

    // Send the file in BUFFER_SIZE chunks
    char buffer[BUFFER_SIZE];
    while (file.good()) {
        file.read(buffer, BUFFER_SIZE);
        int bytes_read = file.gcount();
        send(client_socket, buffer, bytes_read, 0);
    }
    file.close();
}

int main() {
    int socket_cliente;
    struct sockaddr_in direccion_servidor;

    // Crear socket
    if ((socket_cliente = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        std::cerr << "Error al crear el socket" << std::endl;
        return -1;
    }

    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_port = htons(PORT);

    // Convertir direcciones IPv4 e IPv6 de texto a forma binaria
    if (inet_pton(AF_INET, "127.0.0.1", &direccion_servidor.sin_addr) <= 0) {
        std::cerr << "Dirección inválida o no soportada" << std::endl;
        return -1;
    }

    // Conectar al servidor
    if (connect(socket_cliente, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0) {
        std::cerr << "Error al conectar con el servidor" << std::endl;
        return -1;
    }

    std::cout << "Conectado al servidor" << std::endl;

    // Enviar nombre de cliente al servidor
    std::string nombre_cliente;
    std::cout << "Ingrese su nombre: ";
    std::getline(std::cin, nombre_cliente);
    write(socket_cliente, nombre_cliente.c_str(), nombre_cliente.length()); // Enviar nombre de cliente al servidor

    // Iniciar hilo para recibir mensajes del servidor
    std::thread hilo_recepcion(recibir_mensajes, socket_cliente);
    hilo_recepcion.detach();

    // Enviar comandos o mensajes al servidor
    while (true) {
        char comando;
        char mensaje[150];
        std::cout << "Seleccione una opción:\n";
        std::cout << "L para Lista\n";
        std::cout << "B para Broadcast\n";
        std::cout << "M: Enviar mensaje privado\n";
        std::cout << "G para Jugar\n";
        std::cout << "X para Salir\n";
        std::cout << "P para Enviar Archivo):\n";
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

            case 'G':
                {
                    // Pedir al usuario que ingrese la posición en el tablero
                    int posicion;
                    std::cout << ">> Ingrese la posición (1-9) en el tablero para colocar su marca: ";
                    std::cin >> posicion;
                    std::cin.ignore(); // Ignorar el salto de línea

                    // Enviar posición al servidor
                    write(socket_cliente, &posicion, sizeof(posicion));
                }
                break;
            case 'P': {
                // Enviar archivo
                std::string filename, recipient;
                std::cout << "Ingrese la ruta del archivo: ";
                std::getline(std::cin, filename);

                std::cout << "Ingrese el nombre del destinatario: ";
                std::getline(std::cin, recipient);

                // Envía el comando 'I' al servidor
                char i_command = 'I';
                send(socket_cliente, &i_command, sizeof(i_command), 0);

                // Envía el archivo al servidor
                send_file(socket_cliente, filename, recipient);
                break;
            }
            default:
                std::cerr << "Comando no reconocido" << std::endl;
                break;
        }
    }

    return 0;
}
