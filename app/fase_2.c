#include <stdio.h>
#include <unistd.h>

int main(void)
{
    int anterior = '1';  // Suponemos que empiezas con el botón suelto.

    while (1) {
        FILE *boton = fopen("/dev/button_driver", "r");
        if (boton == NULL) {
            perror("No se pudo abrir el botón");
            return 1;
        }

        int actual = fgetc(boton);  // Lee un carácter del driver.
        fclose(boton);

        if (actual == EOF) {
            fprintf(stderr, "No se pudo leer el estado del botón\n");
            return 1;
        }

        if (anterior == '1' && actual == '0') {
            puts("¡Botón oprimido!");
        }

        anterior = actual;  // Guarda esta lectura para la próxima vuelta.
        usleep(50000);      // Espera 50 milisegundos.
    }
}