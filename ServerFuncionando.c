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

void *manejarConexionCliente(void *data) {
    int sockfd = ((int)data);
    free(data);  // Liberar memoria asignada para el descriptor del socket

    char buffer[TAMANO_BUFFER];
    ssize_t mensajeLen;

    // Esperar por un mensaje CONNECT
    mensajeLen = recv(sockfd, buffer, TAMANO_BUFFER - 1, 0);
    if (mensajeLen > 0) {
        buffer[mensajeLen] = '\0';  // Asegurar que el buffer es una cadena de caracteres válida

        // Verificar si el mensaje es un CONNECT
        if (strcmp(buffer, "Mensaje CONNECT") == 0) {
            printf("Mensaje CONNECT recibido de un cliente.\n"); // Imprimir cuando se recibe CONNECT

            // Enviar respuesta CONNACK (representación simple)
            char mensajeConnack[] = "Mensaje CONNACK";
            if (send(sockfd, mensajeConnack, strlen(mensajeConnack), 0) < 0) {
                perror("Fallo al enviar mensaje CONNACK");
            } else {
                printf("Mensaje CONNACK enviado al cliente.\n"); // Imprimir cuando se envía CONNACK
            }

            // Bucle de escucha para mensajes adicionales después de CONNECT
            while ((mensajeLen = recv(sockfd, buffer, TAMANO_BUFFER - 1, 0)) > 0) {
                buffer[mensajeLen] = '\0'; // Asegurar que el buffer es una cadena de caracteres válida
                printf("Mensaje recibido (bytes): %zd\n", mensajeLen);
                printf("Contenido del mensaje: %s \n", buffer);

                // Aquí se manejarían otros mensajes MQTT recibidos
            }

            if (mensajeLen == 0) {
                printf("Cliente desconectado\n");
            } else if (mensajeLen < 0) {
                perror("Error en recv");
            }
        } else {
            printf("Esperado mensaje CONNECT, recibido: %s\n", buffer);
        }
    } else if (mensajeLen == 0) {
        printf("Cliente desconectado antes de enviar CONNECT\n");
    } else {
        perror("Error en recv al esperar CONNECT");
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