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
    struct sockaddr_in addr;// Información de la dirección del cliente que se conecta al servidor
    pthread_t threadId;
    int isActive; // Indica si la entrada está activa
} Cliente;

Cliente clientes[MAX_CLIENTES]; // Array para almacenar clientes
pthread_mutex_t clientes_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex para proteger el array de clientes
// Inicializar la lista de clientes
void initClientes() {
    for (int i = 0; i < MAX_CLIENTES; i++) {
        clientes[i].sockfd = -1;
        clientes[i].isActive = 0;
    }
}

// Añadir un cliente a la lista
int agregarCliente(int sockfd, struct sockaddr_in addr) {
    pthread_mutex_lock(&clientes_mutex);
    for (int i = 0; i < MAX_CLIENTES; i++) {
        if (!clientes[i].isActive) {
            clientes[i].sockfd = sockfd;
            clientes[i].addr = addr;
            clientes[i].isActive = 1;
            pthread_mutex_unlock(&clientes_mutex);
            return i;
        }
    }
    pthread_mutex_unlock(&clientes_mutex);
    return -1;
}

// Remover un cliente de la lista
void removerCliente(int index) {
    pthread_mutex_lock(&clientes_mutex);
    if (index >= 0 && index < MAX_CLIENTES && clientes[index].isActive) {
        clientes[index].isActive = 0;
        close(clientes[index].sockfd);
    }
    pthread_mutex_unlock(&clientes_mutex);
}
void enviarATodos(char *mensaje) {
    pthread_mutex_lock(&clientes_mutex);
    for (int i = 0; i < MAX_CLIENTES; i++) {
        if (clientes[i].isActive) {
            if (send(clientes[i].sockfd, mensaje, strlen(mensaje), 0) < 0) {
                perror("Error al enviar mensaje a cliente");
                // Podrías considerar remover el cliente si hay un error
                removerCliente(i);
            }
        }
    }
    pthread_mutex_unlock(&clientes_mutex);
}

void *manejarConexionCliente(void *data) {
    //printf("Entrando en manejarConexionCliente ");
    int sockfd = *((int*)data); //pointer points to pointer

    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    getpeername(sockfd, (struct sockaddr *)&addr, &addr_len); // Obtiene la dirección del cliente

    int index = agregarCliente(sockfd, addr);
    if (index == -1) {
        printf("Máximo número de clientes alcanzado.\n");
        close(sockfd);
        return NULL;
    }
    char buffer[BUFFER_SIZE];
    ssize_t mensajeLen;

    // Esperar por un mensaje utf 8 ...
    // Supongamos que buffer es un array de unsigned char recibido de recv y ya está definido.
    


    mensajeLen = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if (mensajeLen > 0) {
    //printf("Entrando en mensajeLen>0 \n"); Control
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
            } 
            else {
                printf("Mensaje CONNACK enviado al cliente.\n");
                
                while ((mensajeLen = recv(sockfd, buffer, BUFFER_SIZE , 0)) > 0) {
                    //printf("Estoy en while despues de CONNACK \n"); Control

                    
                //Este while es despues de recibir el CONNECT y enviar el CONNACK
                unsigned char byteControl = buffer[0];

                
                unsigned char messageType = (byteControl >> 4) & 0x0F; // Primeros 4 bits
                unsigned char dupFlag = (byteControl >> 3) & 0x01;     // Quinto bit
                unsigned char qosLevel = (byteControl >> 1) & 0x03;    // Sexto y séptimo bits
                unsigned char retain = byteControl & 0x01;
                
                switch(messageType){

                case 3:
                    printf("PUBLISH: Recibido un mensaje de publicación.\n");
        
                    // Asignar y verificar el tipo de mensaje y otros flags si necesario
                    // Ya tenemos messageType, dupFlag, qosLevel, retain desde antes
                    
                    // Leer la longitud del tópico
                    int longitudTopico = buffer[1];
                    char topico[256] = {0}; // Asegúrate que el tamaño del topico sea suficiente o maneja dinámicamente
                    memcpy(topico, &buffer[2], longitudTopico);
                    
                    // Identificador del paquete
                    unsigned char identificadorPaquete = buffer[2 + longitudTopico];
                    
                    // Extraer el mensaje
                    char mensaje[1024] = {0}; // Asegúrate de que el buffer sea suficiente o maneja dinámicamente
                    int inicioMensaje = 3 + longitudTopico;
                    int longitudMensaje = mensajeLen - inicioMensaje;
                    memcpy(mensaje, &buffer[inicioMensaje], longitudMensaje);
                    
                    printf("Topico: %s\n", topico);
                    printf("Mensaje: %s\n", mensaje);
                    
                    // Opcionalmente, puedes usar la función enviarATodos para retransmitir este mensaje
                    char mensajeCompleto[2048]; // Este tamaño debería ser calculado basado en la longitud del mensaje y el tópico
                    sprintf(mensajeCompleto, "Topico: %s, Mensaje: %s", topico, mensaje);
                    enviarATodos(mensajeCompleto);
                    break;
                
                }
                }
            if (mensajeLen == 0) {
                printf("Cliente desconectado\n");
            } 
            else if (mensajeLen < 0) {
                perror("Error en recv");
            }
            }
            
            
            //Aqui arriba cambios
            //break;

            //break;

        // Agregar más casos según sea necesario
        
        default:
            printf("Mensaje no esperado o desconocido. MessageType: %d\n", messageType);
            printf("Contenido del mensaje: %s \n", buffer);
            printf("Tamano recibido (bytes): %zd\n", mensajeLen);
    }
} else if (mensajeLen == 0) {
    printf("Cliente desconectado antes de cualquier acción\n");
} else {
    perror("Error en recv");
}

//printf("Cerrando por Ult de manejarConexionCliente \n"); Control
close(sockfd);  // Cerrar la conexión
removerCliente(index);
return NULL;
}

int main(int argc, char *argv[]) {

    // Manejo de argumentos por consola
    if(argc != 4){
        fprintf(stderr, "Uso: %s <ip> <port> <path/log.log>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    initClientes();
    
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
        else{
            //printf("Paso por NuevoSockfd "); Control
        }

        pthread_t threadId;
        if (pthread_create(&threadId, NULL, manejarConexionCliente, nuevoSockfd) != 0) {
            perror("Error al crear el hilo del cliente");
            close(*nuevoSockfd);
            free(nuevoSockfd);
        }
        else{
            //printf("Paso por thread \n"); Control
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