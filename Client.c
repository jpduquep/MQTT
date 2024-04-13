#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <fcntl.h> 
#include <pthread.h>
#include <errno.h> 
#include <sys/select.h> // Para select()
//Estos dos ultimas lib son para lo no bloqueante del socket
#define TAMANO_BUFFER 2048

char bufferRespuesta[TAMANO_BUFFER];
ssize_t lenRespuesta;

void lengthToBinary(int length, char* binary) {
    for (int i = 7; i >= 0; --i) {
        binary[7 - i] = (length & (1 << i)) ? '1' : '0';
    }
    binary[8] = '\0';  // Termina la cadena de caracteres
}

void *manejarMensajesEntrantes(void *data ){
    int sockfd = *((int*)data);
   

    while(!(lenRespuesta = recv(sockfd, bufferRespuesta, TAMANO_BUFFER, 0) > 0)) {}
    if (lenRespuesta > 0){
    // Asumiendo que el primer byte de bufferRespuesta contiene el byte de control MQTT
    unsigned char byteControl = bufferRespuesta[0];

    // Descomponer el byte de control
    unsigned char messageType = (byteControl >> 4) & 0x0F; // Primeros 4 bits para el tipo de mensaje
    unsigned char dupFlag = (byteControl >> 3) & 0x01;     // Quinto bit para la bandera DUP
    unsigned char qosLevel = (byteControl >> 1) & 0x03;    // Sexto y séptimo bits para el nivel QoS
    unsigned char retain = byteControl & 0x01;             // Octavo bit para la bandera RETAIN

    

    
    printf("MENSAJE DEL SERVER! \n");
    printf("Contenido del mensaje: %s \n", bufferRespuesta);
    //close(sockfd);
    //exit(EXIT_FAILURE);
    
    }
    else if (lenRespuesta == 0) {
    printf("Cliente desconectado.\n");
    } else if(lenRespuesta == -1) {
    // Manejar error en recv
    
    }
    else{
        perror("Errorete rarete jeje");
    }
    
    return NULL;
}


//Funcion del main.
int main(int argc, char *argv[]) {

    
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

    // Esto es para configurar el socket no bloqueante para recv no estanque el programa :D.
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl(F_GETFL) falló");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    flags = fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    if (flags < 0) {
        perror("fcntl(F_SETFL) falló");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    //Hasta aqui config de no bloqueante y wtf

    //por fin funciona jajajaj, no se como solucione el error creo que era un stdout.
    direccionServidor.sin_family = AF_INET;
    direccionServidor.sin_port = htons(port); // Use user-specified port
    direccionServidor.sin_addr.s_addr = inet_addr(ip); // Use user-specified IP

    // Attempt to connect to the MQTT broker

    if (connect(sockfd, (struct sockaddr *)&direccionServidor, sizeof(direccionServidor)) < 0) {
    if (errno == EINPROGRESS) {

        // La operación de conexión está en progreso
        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(sockfd, &writefds);

        struct timeval tv;
        tv.tv_sec = 5; // Espera hasta 5 segundos
        tv.tv_usec = 0;

        int res = select(sockfd + 1, NULL, &writefds, NULL, &tv);
        if (res > 0) {
            // El socket está listo para escribir, lo que indica que la conexión puede haberse establecido
            int so_error;
            socklen_t len = sizeof(so_error);

            getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);

            if (so_error == 0) {
                printf("Conectado al servidor MQTT.\n");
            } else {
                // Hubo un error al establecer la conexión
                fprintf(stderr, "Conexión fallida: %s\n", strerror(so_error));
                close(sockfd);
                exit(EXIT_FAILURE);
            }
        } else if (res == 0) {
            // Timeout: La conexión no se estableció en el tiempo esperado
            fprintf(stderr, "Conexión fallida: Timeout\n");
            close(sockfd);
            exit(EXIT_FAILURE);
        } else {
            // Error en select()
            perror("select() falló");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
    } else {
        // Error inmediato en connect()
        perror("Conexión fallida");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
} else {
    // Conexión establecida inmediatamente (poco probable en modo no bloqueante)
    printf("Conectado al servidor MQTT.\n");
}

    //working...
        unsigned char mensajeConnect = 0b00010000;
        if (send(sockfd, &mensajeConnect, sizeof(mensajeConnect), 0) < 0) {
        perror("Fallo al enviar mensaje CONNECT");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    else{

        printf("Mensaje CONNECT enviado, esperando CONNACK...\n");
        // Esperar respuesta CONNACK del servidor
        while(!(lenRespuesta = recv(sockfd, bufferRespuesta, TAMANO_BUFFER, 0)> 0)){}
        unsigned char byteControl = bufferRespuesta[0];
        unsigned char messageType = (byteControl >> 4) & 0x0F;
        switch (messageType) {
        case 2: // CONNACK
            printf("CONNACK recibido, puede comenzar a enviar mensajes.\n");

            pthread_t threadId;

           if (pthread_create(&threadId, NULL, manejarMensajesEntrantes, (void *)&sockfd) != 0) {
            perror("Error al crear el hilo para la escucha de mensajes");
            close(sockfd);
            exit(EXIT_FAILURE); //chupapi
        }
            else{
                //printf("Paso por thread de mensajes \n");
            }
            break;
        }
    }

    int opcion;
    char mensaje[1024];
    char topico[256];
    unsigned char paquete[2048];
    // Receive response from the server
    // Interactive loop for sending messages to the server
    while (1) {

        printf("Seleccione una opción:\n");
        printf("1. Publicar\n");
        printf("2. Suscribirse\n");
        printf("3. Desuscribirse\n");
        printf("4. Desconectarse\n");
        scanf("%d", &opcion);

        switch (opcion) {
            case 1:
            
                printf("Ingrese el mensaje:\n");
                scanf(" %[^\n]", mensaje);
                printf("Ingrese el tópico:\n");
                scanf(" %[^\n]", topico);

                int topico_len = strlen(topico);
                int mensaje_len = strlen(mensaje);

                paquete[0] = 0x30;  // Primer byte fijo 00110000 en hexadecimal
                lengthToBinary(topico_len, (char*)&paquete[1]); // Segundo byte: longitud del tópico en binario

                memcpy(&paquete[2], topico, topico_len); // Copia el tópico al paquete

                paquete[2 + topico_len] = 0xDB; // N + 1 byte: identificador del paquete 11011011 en hexadecimal

                memcpy(&paquete[3 + topico_len], mensaje, mensaje_len); // Copia el mensaje después del identificador del paquete

                // Imprimir el paquete para verificación
                printf("Paquete creado:\n");
                for (int i = 0; i < 3 + topico_len + mensaje_len; i++) {
                    printf("%02X ", paquete[i]);
                }
                printf("\n");

                ssize_t bytes_sent = send(sockfd, paquete, 3 + topico_len + mensaje_len, 0);
                if (bytes_sent < 0) {
                    perror("Fallo al enviar el paquete");
                    close(sockfd);
                    exit(EXIT_FAILURE);
                } else {
                    printf("Paquete enviado con éxito. Bytes enviados: %zd\n", bytes_sent);
                }
                break;
            case 2:
                //Suscribirse
            case 3:
                //Desuscribirse
            case 4:
                printf("Desconectándose...\n");
                return 0;
            default:
                printf("Opción no válida. Intente de nuevo.\n");
                break;
        }
    //Bloqueador de mensaje

    /*
    printf("Ingrese su mensaje (escriba 'salir' para terminar): ");
    if (fgets(bufferRespuesta, TAMANO_BUFFER, stdin) == NULL) {
        printf("EOF!!");
        break; // Manejar EOF
    }
    */
    if (strcmp(bufferRespuesta, "salir\n") == 0) {
        printf("Hasta luego!");
        break;
    }

    // Enviar el mensaje ingresado al servidor
    if (send(sockfd, bufferRespuesta, strlen(bufferRespuesta), 0) < 0) {
        perror("Fallo al enviar el mensaje");
        break;
    }
    else{
        printf("Send Exitoso! \n");
    }

    // Esperar una respuesta del servidor después de enviar el mensaje
    
    //Termina whiklw(1)
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