#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>

#define GPIO_BUTTON 586

/** @brief Número de interrupción asociado al GPIO del botón. */
static int button_irq;

/** @brief Cola donde esperan los lectores hasta el próximo flanco de bajada. */
static DECLARE_WAIT_QUEUE_HEAD(button_wait);

/** @brief Número de secuencia que el handler incrementa en cada flanco de bajada. */
static atomic64_t button_seq = ATOMIC64_INIT(0);

/**
 * @brief Atiende la interrupción del botón.
 * @param irq Número de la interrupción que activó esta función.
 * @param dev_id Dato asociado al registrar la interrupción.
 * @return IRQ_HANDLED para indicar que el driver atendió la interrupción.
 */
static irqreturn_t button_irq_handler(int irq, void *dev_id)
{
    atomic64_inc(&button_seq);
    wake_up_interruptible(&button_wait);
    return IRQ_HANDLED;
}

/**
 * @brief Lee GPIO17 y entrega "1\n" si el botón está suelto o "0\n" si está oprimido.
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

/**
 * @brief Espera el próximo flanco de bajada y entrega el carácter 'P'.
 * @param file Apertura del dispositivo de eventos.
 * @param buf  Destino donde el programa recibirá el carácter 'P'.
 * @param len  Cantidad máxima de bytes que el programa solicita.
 * @param off  Posición de lectura. Esta función espera un evento por llamada.
 * @return 1 si entregó el evento, 0 si se pidieron cero bytes o un error negativo.
 */
static ssize_t button_events_read(struct file *file, char __user *buf,
                                  size_t len, loff_t *off)
{
    s64 secuencia_inicial;
    int ret;

    if (len == 0)
        return 0;

    secuencia_inicial = atomic64_read(&button_seq);

    if (file->f_flags & O_NONBLOCK)
        return -EAGAIN;

    ret = wait_event_interruptible(
        button_wait,
        atomic64_read(&button_seq) != secuencia_inicial);
    if (ret)
        return ret;

    if (put_user('P', buf))
        return -EFAULT;

    return 1;
}

/** @brief Operaciones que admite el dispositivo del botón. */
static const struct file_operations button_fops = {
    .owner = THIS_MODULE,
    .read = button_read,
};

/** @brief Define el nombre y las operaciones del dispositivo /dev/button_driver. */
static struct miscdevice button_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "button_driver",
    .fops = &button_fops,
};

/** @brief Conecta la lectura de eventos con la función que espera una pulsación. */
static const struct file_operations button_events_fops = {
    .owner = THIS_MODULE,
    .read = button_events_read,
};

/** @brief Define el dispositivo /dev/button_events. */
static struct miscdevice button_events_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "button_events",
    .fops = &button_events_fops,
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
    if (ret)
    {
        pr_err("button_driver: fallo al solicitar el GPIO (%d)\n", ret);
        return ret;
    }

    // Activa el pull-up interno del GPIO que acabamos de reservar.
    ret = gpiod_set_config(gpio_to_desc(GPIO_BUTTON),
                           pinconf_to_config_packed(PIN_CONFIG_BIAS_PULL_UP, 1));
    if (ret < 0)
    {
        pr_err("button_driver: fallo al activar el pull-up (%d)\n", ret);
        gpio_free(GPIO_BUTTON);
        return ret;
    }

    button_irq = gpio_to_irq(GPIO_BUTTON);
    if (button_irq < 0)
    {
        pr_err("button_driver: fallo al obtener la IRQ de GPIO%d (%d)\n", GPIO_BUTTON, button_irq);
        gpio_free(GPIO_BUTTON);
        return button_irq;
    }

    ret = request_irq(button_irq, button_irq_handler, IRQF_TRIGGER_FALLING,
                      "button_driver_irq", NULL);
    if (ret != 0)
    {
        pr_err("button_driver: fallo al registrar la IRQ (%d)\n", ret);
        gpio_free(GPIO_BUTTON);
        return ret;
    }

    pr_info("button_driver: GPIO %d usa la IRQ %d\n", GPIO_BUTTON, button_irq);

    // Register the misc device
    ret = misc_register(&button_dev);

    if (ret != 0)
    {
        free_irq(button_irq, NULL);
        gpio_free(GPIO_BUTTON);
        pr_err("button_driver: fallo al registrar el dispositivo (%d)\n", ret);
        return ret;
    }

    ret = misc_register(&button_events_dev);
    if (ret != 0)
    {
        pr_err("button_driver: fallo al registrar /dev/button_events (%d)\n", ret);
        misc_deregister(&button_dev);
        free_irq(button_irq, NULL);
        gpio_free(GPIO_BUTTON);
        return ret;
    }

    pr_info("button_driver: /dev/button_driver listo\n");
    return 0;
}

/** @brief Retira /dev/button_driver y libera GPIO17 al descargar el módulo. */
static void __exit button_exit(void)
{
    misc_deregister(&button_events_dev);
    misc_deregister(&button_dev);
    free_irq(button_irq, NULL);
    gpio_free(GPIO_BUTTON);
    pr_info("button_driver: modulo descargado\n");
}

module_init(button_init);
module_exit(button_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ivan");
MODULE_DESCRIPTION("Driver para leer el estado de un boton conectado a GPIO17");