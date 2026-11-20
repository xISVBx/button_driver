#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/gpio.h>

#define GPIO_BUTTON 586

/**
 * @brief Entrega el texto "1\n" al programa que lee el dispositivo.
 *
 * @param file Dispositivo abierto por el programa. No se usa en esta versión.
 * @param buf  Búfer del programa donde se copian los caracteres.
 * @param len  Cantidad máxima de bytes que el programa pide leer.
 * @param off  Puntero a la posición de lectura. Avanza con cada byte copiado.
 * @return Cantidad de bytes copiados, 0 al llegar al final del mensaje
 *         o un valor negativo si ocurre un error.
 */
static ssize_t button_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{

    char mensaje[] = "0\n";
    int nivel = gpio_get_value_cansleep(GPIO_BUTTON);

    mensaje[0] = nivel ? '1' : '0';

    return simple_read_from_buffer(buf, len, off, mensaje, sizeof(mensaje) - 1);
}

/** @brief Operaciones que admite el dispositivo del botón. */
static const struct file_operations button_fops = {
    .owner = THIS_MODULE,
    .read  = button_read,
};

/** @brief Define el nombre y las operaciones del dispositivo /dev/button_driver. */
static struct miscdevice button_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "button_driver",
    .fops  = &button_fops,
};


/**
 * @brief Reserva GPIO17 como entrada y registra /dev/button_driver.
 * @return 0 si el módulo se inicializa correctamente o un código negativo
 *         si falla la reserva del GPIO o el registro del dispositivo.
 */
static int __init button_init(void)
{

    int ret;

    // Request the GPIO pin for the button
    ret = gpio_request_one(GPIO_BUTTON, GPIOF_IN, "button_driver");
    if (ret) {
        pr_err("button_driver: fallo al solicitar el GPIO (%d)\n", ret);
        return ret;
    }

    // Register the misc device
    ret = misc_register(&button_dev);
    
    if (ret != 0) {
        gpio_free(GPIO_BUTTON);
        pr_err("button_driver: fallo al registrar el dispositivo (%d)\n", ret);
        return ret;
    }

    pr_info("button_driver: /dev/button_driver listo\n");
    return 0;
}

/** @brief Retira /dev/button_driver y libera GPIO17 al descargar el módulo. */
static void __exit button_exit(void)
{
    misc_deregister(&button_dev);
    gpio_free(GPIO_BUTTON);
    pr_info("button_driver: modulo descargado\n");
}

module_init(button_init);
module_exit(button_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ivan");
MODULE_DESCRIPTION("Driver para leer el estado de un boton conectado a GPIO17");