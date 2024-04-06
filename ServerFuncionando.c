#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define MAX_CLIENTES 10
#define TAMANO_BUFFER 2048  // Tamaño suficiente para la mayoría de los paquetes MQTT
#define PUERTO_MQTT 1883

typedef struct {
    int sockfd;
    struct sockaddr_in addr;
} Cliente;

Cliente clientes[MAX_CLIENTES];
pthread_mutex_t mutexClientes = PTHREAD_MUTEX_INITIALIZER;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h> // Para close()

#define TAMANO_BUFFER 1024

void *manejarConexionCliente(void *data) {
    int sockfd = *((int*)data);
    free(data);  // Liberar memoria asignada para el descriptor del socket

    char buffer[TAMANO_BUFFER];
    ssize_t mensajeLen;

    // Esperar por un mensaje
    mensajeLen = recv(sockfd, buffer, TAMANO_BUFFER - 1, 0);
    if (mensajeLen > 0) {
        buffer[mensajeLen] = '\0';  // Asegurar que el buffer es una cadena de caracteres válida

        // Deserializar el mensaje MQTT
        char MessageType[5]; // Primeros 4 caracteres + '\0'
        char DUPFlag[2];     // Quinto carácter + '\0'
        char QoSFlag[3];     // Sexto y séptimo caracteres + '\0'
        char RETAIN[2];      // Octavo carácter + '\0'

        strncpy(MessageType, buffer, 4);
        MessageType[4] = '\0';

        DUPFlag[0] = buffer[4];
        DUPFlag[1] = '\0';

        strncpy(QoSFlag, buffer + 5, 2);
        QoSFlag[2] = '\0';

        RETAIN[0] = buffer[7];
        RETAIN[1] = '\0';

        // Convertir MessageType de binario a decimal
        int messageTypeInt = strtol(MessageType, NULL, 2);

        // Usar un switch para interpretar MessageType
        switch (messageTypeInt) {
            case 1: // CONNECT
                printf("CONNECT: Client request to connect to Server\n");
                // Enviar respuesta CONNACK (representación simple)
                char mensajeConnack[] = "0010xxxx";
                if (send(sockfd, mensajeConnack, strlen(mensajeConnack), 0) < 0) {
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
                printf("Mensaje no esperado o desconocido. MessageType: %d\n", messageTypeInt);
        }
    } else if (mensajeLen == 0) {
        printf("Cliente desconectado antes de cualquier acción\n");
    } else {
        perror("Error en recv");
    }

    close(sockfd);  // Cerrar la conexión
    return NULL;
}



int main() {
    int servidorSockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (servidorSockfd < 0) {
        perror("Error al crear el socket del servidor");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in servidorAddr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(PUERTO_MQTT),
    };

    if (bind(servidorSockfd, (struct sockaddr *)&servidorAddr, sizeof(servidorAddr)) < 0) {
        perror("Error al vincular el socket del servidor");
        close(servidorSockfd);
        exit(EXIT_FAILURE);
    }

    if (listen(servidorSockfd, MAX_CLIENTES) < 0) {
        perror("Error al escuchar en el socket del servidor");
        close(servidorSockfd);
        exit(EXIT_FAILURE);
    }

    printf("Servidor MQTT ejecutándose en el puerto %d...\n", PUERTO_MQTT);

    while (1) {
        struct sockaddr_in clienteAddr;
        socklen_t clienteAddrLen = sizeof(clienteAddr);
        int* nuevoSockfd = malloc(sizeof(int));
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

    // Nota: Este código no incluye la limpieza y terminación adecuadas para simplificar
    return 0;
}