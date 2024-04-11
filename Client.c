#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define TAMANO_BUFFER 2048

int main(int argc, char *argv[]) {

    char bufferRespuesta[TAMANO_BUFFER];
    ssize_t lenRespuesta;
    // Ensure correct usage
    if (argc != 2) {
        printf("Usage: %s <Location/log.txt>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char *logPath = argv[1]; // Log file path from the command line argument

    char ip[INET_ADDRSTRLEN]; // Buffer for IP address input by the user
    int port; // Variable to store the user-input port number

    // Prompt the user for the IP address of the MQTT broker
    printf("Aplicacion Cliente\n");
    printf("Ingrese la IP del broker: ");
    scanf("%15s", ip); // Capture IP address from the user 

    // Prompt the user for the port number
    printf("Ingrese el puerto: ");
    scanf("%d", &port); // Capture port number from the user
    
    //get time in c for current client...
    //Hay muchas formas de sacar el tiempo actual en c mexi no se si esta sea la mas adecuada

    time_t seconds;   //very weird.. time in seconds since 1970
    struct tm * timeinfo; //timeinfo pointer to a tm structure explained in notes*
    time ( &seconds );  //seconds is passed here directly so that time retrieves the current calendar time that coresponds to that ammount in seconds
    timeinfo = localtime ( &seconds ); //another pointer ._. this time the pointer timeinfo, previously declared stores the value of time but taking into account local time...
    //printf ( "Current local time and date: %s", asctime (timeinfo) ); //asctime just converts to readable string.
    //this is an easy way to do this maybe theres more but idk.

    // Logging connection attempt
    FILE *logFile = fopen(logPath, "a");
    if (logFile == NULL) {
        perror("Error al abrir el archivo de log");
        exit(EXIT_FAILURE);
    }
    //hay que formatear el tiempo
    char str[80];
    strftime(str, 80, "%c",timeinfo);  //wow :)
    fprintf(logFile, "[%s], Intentando conectar al servidor MQTT en %s:%d\n",str, ip, port); //log file done...
    fclose(logFile);

    // Creating the socket
    int sockfd;   //https://www.gta.ufrj.br/ensino/eel878/sockets/sockaddr_inman.html
    struct sockaddr_in direccionServidor;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }
    //por fin funciona jajajaj, no se como solucione el error creo que era un stdout.
    direccionServidor.sin_family = AF_INET;
    direccionServidor.sin_port = htons(port); // Use user-specified port
    direccionServidor.sin_addr.s_addr = inet_addr(ip); // Use user-specified IP

    // Attempt to connect to the MQTT broker

    if (connect(sockfd, (struct sockaddr *)&direccionServidor, sizeof(direccionServidor)) < 0) {
        perror("Conexión fallida");
        close(sockfd);
        exit(EXIT_FAILURE);
        
    } else {
        printf("Conectado al servidor MQTT.\n");
        fflush(stdout);
    }

    //working...

    // Placeholder for MQTT CONNECT message. Should be replaced with actual implementation.
    unsigned char mensajeConnect = 0b00010000;
    if (send(sockfd, &mensajeConnect, sizeof(mensajeConnect), 0) < 0) {
    perror("Fallo al enviar mensaje CONNECT");
    close(sockfd);
    exit(EXIT_FAILURE);
}
    else{
        
        printf("Mensaje CONNECT enviado, esperando CONNACK...\n");
        // Esperar respuesta CONNACK del servidor
        lenRespuesta = recv(sockfd, bufferRespuesta, TAMANO_BUFFER - 1, 0);
        if (lenRespuesta < 0) {
            perror("Error al recibir respuesta");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
    }


    // Receive response from the server


    lenRespuesta = recv(sockfd, bufferRespuesta, TAMANO_BUFFER, 0);
    if (lenRespuesta > 0) {
    // Asumiendo que el primer byte de bufferRespuesta contiene el byte de control MQTT
    unsigned char byteControl = bufferRespuesta[0];

    // Descomponer el byte de control
    unsigned char messageType = (byteControl >> 4) & 0x0F; // Primeros 4 bits para el tipo de mensaje
    unsigned char dupFlag = (byteControl >> 3) & 0x01;     // Quinto bit para la bandera DUP
    unsigned char qosLevel = (byteControl >> 1) & 0x03;    // Sexto y séptimo bits para el nivel QoS
    unsigned char retain = byteControl & 0x01;             // Octavo bit para la bandera RETAIN

    // No necesitas convertir messageType a una cadena ni usar strtol ya que messageType ya es un entero

        switch (messageType) {
        case 2: // CONNACK
            printf("CONNACK recibido, puede comenzar a enviar mensajes.\n");
            break;

        // Agregar más casos según sea necesario

        default:
            printf("Longitud de la respuesta: %zd\n", lenRespuesta);
            printf("Respuesta no reconocida. Tipo de mensaje: %d\n", messageType);
            close(sockfd);
            exit(EXIT_FAILURE);
        }
    } else if (lenRespuesta == 0) {
    // Manejar el caso de desconexión
    printf("Cliente desconectado.\n");
    } else {
    // Manejar error en recv
    perror("Error en recv");
    }

    // Interactive loop for sending messages to the server
    // Interactive loop for sending messages to the server
    while (1) {
    printf("Ingrese su mensaje (escriba 'salir' para terminar): ");
    if (fgets(bufferRespuesta, TAMANO_BUFFER, stdin) == NULL) {
        break; // Manejar EOF
    }
    if (strcmp(bufferRespuesta, "salir\n") == 0) {
        break;
    }

    // Enviar el mensaje ingresado al servidor
    if (send(sockfd, bufferRespuesta, strlen(bufferRespuesta), 0) < 0) {
        perror("Fallo al enviar el mensaje");
        break;
    }

    // Esperar una respuesta del servidor después de enviar el mensaje
    lenRespuesta = recv(sockfd, bufferRespuesta, TAMANO_BUFFER - 1, 0);
    if (lenRespuesta > 0) {
        bufferRespuesta[lenRespuesta] = '\0'; // Asegurar que el buffer es una cadena de caracteres válida
        printf("Respuesta del servidor: %s\n", bufferRespuesta);
    } else if (lenRespuesta == 0) {
        printf("El servidor cerró la conexión.\n");
        break;
    } else {
        perror("Error al recibir respuesta");
        break;
    }
}

        // Cerrar el socket antes de terminar el programa
    close(sockfd);

    return 0;
}

//Notes 
/*
    -Fix, implement ip and por from client -> Doing.
    -Complete protocol -> Doing.
    -Integrate -> Working.
    -Comments -> Doing.

    some important explanations 
    when seeing something like:
    struct tm *info;
    ptr info is a ptr to a tm struct.
    
*/