#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define MAX_CLIENTES 10 //Cambiar esto.
#define BUFFER_SIZE 2048  // Tamaño suficiente para la mayoría de los paquetes MQTT.
//#define DEFAULT_PORT 1883 //Cambiar esto tambien.

// Estructura para almacenar la información del cliente
typedef struct {
    int sockfd;
    struct sockaddr_in addr; // Información de la dirección del cliente que se conecta al servidor
} Cliente;

void *manejarConexionCliente(void *data) {
    int sockfd = *((int*)data); //pointer points to pointer
    free(data);  // Liberar memoria asignada para el descriptor del socket

    char buffer[BUFFER_SIZE];
    ssize_t mensajeLen;

    // Esperar por un mensaje utf 8 ...
    // Supongamos que `buffer` es un array de unsigned char recibido de recv y ya está definido.
ssize_t mensajeLen = recv(sockfd, buffer, BUFFER_SIZE, 0);
if (mensajeLen > 0) {
    // Asumiendo que buffer[0] contiene el byte de control MQTT
    unsigned char byteControl = buffer[0];

    // Descomponer el byte de control
    unsigned char messageType = (byteControl >> 4) & 0x0F; // Primeros 4 bits
    unsigned char dupFlag = (byteControl >> 3) & 0x01;     // Quinto bit
    unsigned char qosLevel = (byteControl >> 1) & 0x03;    // Sexto y séptimo bits
    unsigned char retain = byteControl & 0x01;             // Octavo bit

    // Usar un switch para interpretar el tipo de mensaje
    switch (messageType) {
        case 1: // CONNECT
            printf("CONNECT: Client request to connect to Server\n");
            // Enviar respuesta CONNACK (representación simplificada)
            unsigned char mensajeConnack = 0b00100000; // Tipo de mensaje CONNACK
            if (send(sockfd, &mensajeConnack, sizeof(mensajeConnack), 0) < 0) {
                perror("Fallo al enviar mensaje CONNACK");
            } else {
                printf("Mensaje CONNACK enviado al cliente.\n");
            }
            break;

        case 3: // PUBLISH
            printf("PUBLISH: Publish message\n");
            // Aquí se enviaría una respuesta adecuada para PUBLISH, por ejemplo, PUBACK
            break;

        // Agregar más casos según sea necesario

        default:
            printf("Mensaje no esperado o desconocido. MessageType: %d\n", messageType);
    }
} else if (mensajeLen == 0) {
    printf("Cliente desconectado antes de cualquier acción\n");
} else {
    perror("Error en recv");
}

close(sockfd);  // Cerrar la conexión
return NULL;
}

int main(int argc, char *argv[]) {

    // Manejo de argumentos por consola
    if(argc != 4){
        fprintf(stderr, "Uso: %s <ip> <port> <path/log.log>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    char *ip = argv[1];
    char *port = argv[2];
    char *logPath = argv[3];

    // Configuración inicial del servidor, incluyendo la creación del socket y la vinculación a la dirección IP y puerto especificados
    struct sockaddr_in servidorAddr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr(ip), 
        .sin_port = htons(atoi(port)),
    };
    
    printf("Iniciando el servidor en Puerto: %s\n", port);

    // Creación del socket
    int servidorSockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (servidorSockfd < 0) {
        perror("Error al crear el socket del servidor");
        exit(EXIT_FAILURE);
    }

    // Vincular el servidor al socket
    if (bind(servidorSockfd, (struct sockaddr *)&servidorAddr, sizeof(servidorAddr)) < 0) {
        perror("Error al vincular el socket del servidor");
        close(servidorSockfd);
        exit(EXIT_FAILURE);
    }

    // Escuchar conexiones entrantes
    if (listen(servidorSockfd, MAX_CLIENTES) < 0) {
        perror("Error al escuchar en el socket del servidor");
        close(servidorSockfd);
        exit(EXIT_FAILURE);
    }

    printf("Servidor ejecutándose en el puerto %s...\n", port);

    // Manejo del archivo de log
    FILE *logFile = fopen(logPath, "a"); // Abrir el archivo en modo append
    if (logFile == NULL) {
        perror("Error al abrir el archivo de log");
        exit(EXIT_FAILURE);
    }
    fprintf(logFile, "Servidor iniciado en IP: %s, Puerto: %s\n", ip, port);
    fclose(logFile); // Cerrar el archivo de log

    // Bucle principal del servidor para aceptar conexiones entrantes
    while (1) {
        struct sockaddr_in clienteAddr;
        socklen_t clienteAddrLen = sizeof(clienteAddr);
        int *nuevoSockfd = malloc(sizeof(int)); 
        *nuevoSockfd = accept(servidorSockfd, (struct sockaddr *)&clienteAddr, &clienteAddrLen);

        if (*nuevoSockfd < 0) {
            perror("Error al aceptar conexión del cliente");
            free(nuevoSockfd);
            continue;
        }

        pthread_t threadId;
        if (pthread_create(&threadId, NULL, manejarConexionCliente, nuevoSockfd) != 0) {
            perror("Error al crear el hilo del cliente");
            close(*nuevoSockfd);
            free(nuevoSockfd);
        }
    }

    return 0;
}


//NOTES:
/*
    -Que pasa si al iniciar el servidor no se asigna un archivo log?
    o se da el nombre de un archivo que no existe?... dar respuestas apropiadas...
    simplicidad al correr...
    mejorar el codigo(ponerlo mas b)






*/