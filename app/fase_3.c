#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    int fd = open("/dev/button_events", O_RDONLY);
    if (fd == -1)
    {
        perror("No se pudo abrir /dev/button_events");
        return 1;
    }

    puts("Esperando el botón. Usa Ctrl+C para salir.");

    while (1)
    {
        char aviso;
        ssize_t cantidad = read(fd, &aviso, 1);

        if (cantidad == -1)
        {
            perror("Error al leer el botón");
            close(fd);
            return 1;
        }
        if (cantidad == 0)
        {
            puts("Fin de archivo inesperado. Cerrando.");
            break;
        }

        if (aviso == 'P')
            puts("¡Botón oprimido!");
    }

    close(fd);
    return 0;
}