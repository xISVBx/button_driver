#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>

static int __init button_init(void)
{
    pr_info("button_driver: modulo cargado en el kernel\n");
    return 0;
}

static void __exit button_exit(void)
{
    pr_info("button_driver: modulo descargado del kernel\n");
}

module_init(button_init);
module_exit(button_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ivan");
MODULE_DESCRIPTION("Modulo minimo para validar carga y descarga");
