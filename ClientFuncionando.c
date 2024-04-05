#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PUERTO_MQTT 1883
#define TAMANO_BUFFER 2048

int main() {
    int sockfd;
    struct sockaddr_in direccionServidor;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    direccionServidor.sin_family = AF_INET;
    direccionServidor.sin_port = htons(PUERTO_MQTT);
    direccionServidor.sin_addr.s_addr = inet_addr("127.0.0.1");  // Asumimos servidor en localhost

    // Conectar al servidor MQTT
    if (connect(sockfd, (struct sockaddr *)&direccionServidor, sizeof(direccionServidor)) < 0) {
        perror("Conexión fallida");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Conectado al servidor MQTT.\n");

    // Enviar mensaje CONNECT (representación simple)
    char mensajeConnect[] = "Mensaje CONNECT";
    if (send(sockfd, mensajeConnect, strlen(mensajeConnect), 0) < 0) {
        perror("Fallo al enviar mensaje CONNECT");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Mensaje CONNECT enviado, esperando CONNACK...\n");

    // Esperar respuesta CONNACK del servidor
    char bufferRespuesta[TAMANO_BUFFER];
    ssize_t lenRespuesta = recv(sockfd, bufferRespuesta, TAMANO_BUFFER - 1, 0);
    if (lenRespuesta < 0) {
        perror("Error al recibir respuesta");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Asegurarse de que la respuesta es una cadena de caracteres válida
    bufferRespuesta[lenRespuesta] = '\0';
    if(strcmp(bufferRespuesta, "Mensaje CONNACK") == 0) {
        printf("CONNACK recibido, puede comenzar a enviar mensajes.\n");
    } else {
        printf("Respuesta no reconocida: %s\n", bufferRespuesta);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Bucle para enviar mensajes ingresados por el usuario
    while (1) {
        printf("Ingrese su mensaje (escriba 'salir' para terminar): ");
        char mensajeUsuario[TAMANO_BUFFER];
        if (fgets(mensajeUsuario, TAMANO_BUFFER, stdin) != NULL) {
            // Verificar si el usuario quiere terminar
            if (strcmp(mensajeUsuario, "salir\n") == 0) {
                break;
            }

            // Enviar el mensaje ingresado al servidor
            if (send(sockfd, mensajeUsuario, strlen(mensajeUsuario), 0) < 0) {
                perror("Fallo al enviar el mensaje");
                break;
            }
        }
    }

    close(sockfd);
    return 0;
}