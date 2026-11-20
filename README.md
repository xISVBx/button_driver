# Driver de un pulsador para Raspberry Pi 5

Partimos de un módulo mínimo de Linux en la Raspberry Pi 5. Después leímos GPIO17 desde un dispositivo de caracteres y añadimos una segunda interfaz para esperar una pulsación mediante interrupciones. Al inicio comprobamos el cableado desde espacio de usuario con GPIO Zero.

## Estructura

| Ruta | Contenido |
| --- | --- |
| [`app/`](app/) | Diagnóstico con GPIO Zero y aplicaciones de prueba para las fases 2 y 3. |
| [`kernel/fase1/Makefile`](kernel/fase1/Makefile) | Compila y limpia la fase 1. |
| [`kernel/fase2/Makefile`](kernel/fase2/Makefile) | Compila y limpia la fase 2. |
| [`kernel/fase1/src/`](kernel/fase1/src/) | Módulo mínimo y reglas de Kbuild de la fase 1. |
| [`kernel/fase2/`](kernel/fase2/) | Driver que entrega el nivel actual de GPIO17. |
| [`kernel/fase1/build/`](kernel/fase1/build/) | Artefactos de compilación de la fase 1. |
| [`kernel/fase2/build/`](kernel/fase2/build/) | Artefactos de compilación de la fase 2. |
| [`kernel/fase3/`](kernel/fase3/) | Driver con lectura de estado y eventos por interrupción. |

## Cableado y prueba del pulsador

### Cableado

Con la Pi apagada, conecta el pulsador entre GPIO17 y GND.

| Contacto del pulsador | Conexión en la Pi | Pin físico |
| --- | --- | --- |
| Un contacto | GPIO17 | 11 |
| El otro contacto | GND | 6 o 9 |

### Prueba del pulsador

Con el pulsador conectado entre GPIO17 y GND, abre una terminal en la Raspberry Pi y sitúate en la raíz del proyecto. Si el proyecto está en otro equipo, copia la carpeta `button_driver` a tu directorio personal en la Pi.

```bash
sudo apt update
sudo apt install python3-gpiozero python3-lgpio
GPIOZERO_PIN_FACTORY=lgpio /usr/bin/python3 app/probar_boton.py
```

El programa muestra el estado inicial y luego imprime `Presionado` al pulsar y `Suelto` al soltar. Repite la prueba varias veces. Para salir, pulsa Ctrl+C. Puedes hacer esta prueba antes de instalar los headers del kernel.

[Documentación de GPIO Zero](https://gpiozero.readthedocs.io/en/stable/api_input.html#button)

## Fase 1 · Primer módulo del kernel

Un LKM (*Loadable Kernel Module*) es código que Linux puede incorporar a un kernel en ejecución y retirar después. El [módulo de este proyecto](kernel/fase1/src/button_driver.c) sirve para practicar ese ciclo con dos mensajes en el registro del kernel.

### Espacio de usuario y espacio de kernel

| Tema | Programa C en espacio de usuario | Módulo del kernel |
| --- | --- | --- |
| Entrada | El programa empieza en `main()`. | El kernel invoca las funciones registradas para la carga, la descarga y otros eventos que implemente el módulo. |
| Cabeceras | Biblioteca estándar de C, como `<stdio.h>`. | Cabeceras y API del kernel, como `<linux/module.h>`. |
| Mensajes | `printf()` escribe en la salida estándar. | `pr_info()` escribe en el registro circular del kernel. |
| Memoria dinámica | `malloc()` y `free()`. | Por ejemplo, `kmalloc()` y `kfree()`. |
| Fallos graves | Un acceso inválido puede terminar el proceso. | Un acceso inválido puede provocar un *oops* o un *panic* y afectar a todo el sistema. |
| Compilación | Por ejemplo, `gcc programa.c -o programa`. | Kbuild genera un archivo `.ko` con la configuración del kernel de destino. |

Evita la aritmética de punto flotante en el código ordinario del kernel y las variables locales grandes, porque el tamaño de la pila depende de la arquitectura y la configuración. Si la inicialización falla después de adquirir recursos, libera lo que ya se adquirió antes de devolver el error. Tras una carga exitosa, la función de salida libera los recursos que sigan activos. [Guía de programación del kernel](https://kernel.org/doc/html/latest/kernel-hacking/hacking.html)

### Anatomía de `button_driver.c`

```c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

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
```

#### Cabeceras

| Cabecera | Para qué se usa aquí |
| --- | --- |
| `<linux/init.h>` | Proporciona las anotaciones `__init` y `__exit`. |
| `<linux/module.h>` | Proporciona `module_init()`, `module_exit()` y las macros de metadatos `MODULE_*`. |
| `<linux/kernel.h>` | Cabecera general del kernel que incluye el soporte de logging usado aquí. `pr_info()` se define en `<linux/printk.h>`. |

#### Puntos de entrada y salida

1. **Carga.** `module_init(button_init)` registra el punto de entrada. Cuando `insmod` inserta el módulo, el kernel llama a `button_init()`. La función escribe un mensaje con `pr_info()` y devuelve `0` para indicar éxito. Un código negativo, como `-EINVAL`, haría fallar la carga. `__init` permite liberar el código de inicialización después de una carga exitosa.
2. **Descarga.** `module_exit(button_exit)` registra el punto de salida. Cuando `rmmod` retira el módulo, el kernel llama a `button_exit()`. `__exit` identifica el código que se usa al retirar el módulo. La función devuelve `void` y en esta fase solo escribe el mensaje de descarga. Cuando el módulo adquiera recursos, como un GPIO, también tendrá que liberarlos.
3. **Visibilidad y registro.** `static` limita los nombres de ambas funciones al archivo C. `pr_info()` escribe con prioridad informativa en el búfer circular del kernel. Ahí quedan los mensajes que después consulta `dmesg`.

#### Metadatos

- `MODULE_LICENSE("GPL")` declara una licencia compatible con GPL a efectos del kernel. Esto permite usar símbolos reservados a módulos compatibles con GPL si el módulo los necesita.
- `MODULE_AUTHOR("Ivan")` registra el autor.
- `MODULE_DESCRIPTION(...)` describe para qué sirve el módulo.

Puedes consultar estos tres campos en el archivo compilado con `modinfo build/button_driver.ko`.

### Preparar las herramientas y las cabeceras

En la Raspberry Pi, parte de la raíz del proyecto. Puedes hacer esta prueba con el pulsador desconectado.

```bash
uname -r
sudo apt update
sudo apt install build-essential "linux-headers-$(uname -r)"
ls -ldL "/lib/modules/$(uname -r)/build"
```

Si faltan los headers, comprueba que el paquete corresponda a la versión que devuelve `uname -r`. Puedes seleccionar otro árbol de compilación con `make KDIR=/ruta/al/build` desde `kernel/fase1/`. [Documentación del kernel de Raspberry Pi](https://www.raspberrypi.com/documentation/computers/linux_kernel.html)

### Cómo se compila

El [Makefile del módulo](kernel/fase1/Makefile) copia los archivos de la fase 1 a `build/` antes de llamar a Kbuild. `-C` selecciona el árbol de compilación del kernel en ejecución por defecto. `M=` apunta a la ruta absoluta de `build/`. Allí, la copia de [Kbuild](kernel/fase1/src/Kbuild) usa `obj-m += button_driver.o` para compilar el módulo. Las recetas de Make requieren un tabulador al inicio de cada línea. [Compilación de módulos externos con Kbuild](https://docs.kernel.org/kbuild/modules.html)

| Comando desde `kernel/fase1/` | Resultado |
| --- | --- |
| `make` | Compila el módulo y genera `build/button_driver.ko`. |
| `sudo insmod build/button_driver.ko` | Inserta el módulo y ejecuta `button_init()`. |
| `lsmod` | Muestra los módulos cargados. |
| `sudo dmesg` | Consulta los mensajes del kernel. |
| `sudo rmmod button_driver` | Retira el módulo por nombre y ejecuta `button_exit()`. |
| `make clean` | Elimina los archivos generados por Kbuild. |

### Comprobar la carga y la descarga

Desde la raíz del proyecto en la Pi:

```bash
cd kernel/fase1
make
modinfo build/button_driver.ko
sudo insmod build/button_driver.ko
lsmod | grep '^button_driver '
sudo dmesg | grep 'button_driver:' | tail -n 5
```

`lsmod` muestra que `button_driver` quedó cargado. En `dmesg`, busca `button_driver: modulo cargado en el kernel`. Ese mensaje confirma que se ejecutó el cuerpo de `button_init()`.

El aviso `loading out-of-tree module taints kernel` indica que se cargó un módulo compilado fuera del árbol del kernel. Es esperable para este proyecto. La marca se usa para diagnóstico y persiste después de retirar el módulo. [Documentación del kernel sobre taint](https://docs.kernel.org/admin-guide/tainted-kernels.html)

```bash
sudo rmmod button_driver
lsmod | grep '^button_driver '
sudo dmesg | grep 'button_driver:' | tail -n 5
make clean
```

`rmmod` recibe el nombre del módulo sin `.ko`. Después de retirarlo, el filtro de `lsmod` queda sin salida y en `dmesg` aparece `button_driver: modulo descargado del kernel`. Los mensajes anteriores pueden seguir en el búfer, así que fíjate en el orden y en las marcas de tiempo. `make clean` elimina los archivos generados en disco.

### VS Code e IntelliSense

Abre la raíz del proyecto desde la sesión Remote SSH conectada a la Pi. La configuración existente de `.vscode/c_cpp_properties.json` apunta al compilador ARM64 y a las cabeceras de `6.12.47+rpt-rpi-2712`. Si cambia el entorno, actualiza las rutas de cabeceras comunes y generadas y revisa `compilerPath`. Si el editor subraya `<linux/init.h>`, comprueba primero el resultado de `make`. Si Kbuild compila el módulo, revisa la configuración de IntelliSense. Si también falla Kbuild, usa su mensaje de error para diagnosticar el entorno.

## Registro de avances

- En la Pi con kernel `6.12.47+rpt-rpi-2712` se verificaron las herramientas de compilación y los headers.
- El script de GPIO Zero mostró `Suelto` al inicio y registró cinco ciclos de pulsación y liberación.
- Se confirmó la carga y descarga del módulo de la fase 1 en la Pi. En ese entorno, GPIO17 se identificó con el número global `586`.
- En la fase 2 registramos `/dev/button_driver`, comprobamos la lectura de texto fijo y después conectamos la lectura a GPIO17.

## Fase 2 · Lectura de GPIO17 desde el driver

### Preparar el dispositivo de caracteres

`<linux/miscdevice.h>` proporciona `struct miscdevice`. En este paso la usamos para describir el dispositivo que más adelante permitirá consultar el pulsador.

```c
#include <linux/miscdevice.h>

static struct miscdevice button_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "button_driver",
};
```

- `button_dev` es la variable en C que se pasará a `misc_register(&button_dev)`.
- `.name` define el nombre del dispositivo que se usará para `/dev/button_driver`.
- `MISC_DYNAMIC_MINOR` pide a Linux que elija un número menor libre al registrar el dispositivo. Los dispositivos `miscdevice` comparten un número mayor y el menor los distingue entre sí. Tras un registro exitoso, Linux guarda el número asignado en `button_dev.minor`. Ese identificador es independiente del número del pin GPIO.

En este paso solo queda definida la estructura. El registro con `misc_register()` y la operación de lectura se incorporarán en pasos posteriores. Una vez creado el nodo, `ls -l /dev/button_driver` permitirá ver sus números mayor y menor. [Documentación del kernel sobre `miscdevice`](https://docs.kernel.org/driver-api/misc_devices.html)

### Preparar una lectura con un valor fijo

El paso anterior dio nombre al dispositivo. Después preparamos en `kernel/fase2/src/button_driver.c` la función que entregará datos cuando un programa lea ese dispositivo. En esa versión la respuesta era el texto fijo `1\n`. La lectura del GPIO se incorporará después.

Añadimos `#include <linux/fs.h>` junto a las demás cabeceras. Durante `make`, el compilador usa esa cabecera para conocer la declaración de `simple_read_from_buffer()` y comprobar los tipos de sus argumentos, igual que usa `<stdio.h>` para comprobar una llamada a `printf()` en un programa de usuario.

```c
#include <linux/fs.h>

static ssize_t button_read(struct file *file, char __user *buf,
                           size_t len, loff_t *off)
{
    static const char mensaje[] = "1\n";

    return simple_read_from_buffer(buf, len, off,
                                   mensaje, sizeof(mensaje) - 1);
}
```

#### Qué recibe `button_read()`

| Parámetro | Qué representa |
| --- | --- |
| `file` | El archivo de dispositivo abierto por el programa. Forma parte de la firma de lectura y este ejemplo puede dejarlo sin usar. |
| `buf` | El destino de los bytes. `__user` indica que el puntero apunta a memoria del espacio de usuario. |
| `len` | La cantidad máxima de bytes que pidió el programa en esta llamada. |
| `off` | Un puntero a la posición de lectura. `*off` empieza en cero para una apertura nueva en este ejemplo y avanza con los bytes entregados. |

El primer `static` limita el nombre de la función a este archivo C. El tipo de retorno `ssize_t` permite devolver una cantidad de bytes, cero al llegar al final o un valor negativo si hubo un error.

#### Qué se copia y qué se devuelve

`mensaje` contiene tres bytes en memoria: `'1'`, `'\n'` y el terminador de cadena `'\0'`. `static` conserva el arreglo entre llamadas y `const` indica que su contenido es de solo lectura. `sizeof(mensaje) - 1` pone a disposición de la lectura los primeros dos bytes y deja fuera el terminador.

`simple_read_from_buffer()` copia hacia `buf` desde la posición `*off`, respetando tanto el límite `len` como el tamaño de los datos disponibles. Al copiar bytes, avanza la posición y devuelve la cantidad copiada. [API de sistemas de archivos del kernel](https://docs.kernel.org/filesystems/api-summary.html)

Para probarlo con `cat /dev/button_driver`, el siguiente paso será asociar `button_read()` con la operación `.read` del dispositivo y registrar el dispositivo.

### Paso 7. Conectar la lectura del dispositivo con `button_read()`

Con `button_read()` definida, conectamos la función a la operación de lectura del dispositivo.

Colocamos la tabla de operaciones después de `button_read()` y antes de `button_dev`.

```c
/** @brief Operaciones que admite el dispositivo del botón. */
static const struct file_operations button_fops = {
    .owner = THIS_MODULE,
    .read  = button_read,
};
```

- `.read = button_read` guarda un puntero a la función. Usamos el nombre sin paréntesis porque el kernel la llamará más adelante, cuando un programa solicite leer.
- `.owner = THIS_MODULE` identifica a este módulo como propietario de la tabla. El kernel usa esa referencia para mantener el módulo cargado mientras el dispositivo esté abierto.

Luego asociamos la tabla con el dispositivo mediante `.fops = &button_fops`.

```c
static struct miscdevice button_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "button_driver",
    .fops  = &button_fops,
};
```

`fops` significa *file operations* y `&button_fops` es la dirección de la tabla. Cuando el dispositivo esté registrado, una llamada a `read()` seguirá esta cadena:

```text
button_dev.fops → button_fops.read → button_read()
```

Con la tabla conectada, ya puedes registrar el dispositivo al cargar el módulo.

### Paso 8. Registrar y retirar `/dev/button_driver`

Registramos `button_dev` en `button_init()` y lo retiramos en `button_exit()`. Conservamos `module_init(button_init)` y `module_exit(button_exit)` para que el kernel llame a cada función en el momento correspondiente.

```c
static int __init button_init(void)
{
    int ret = misc_register(&button_dev);

    if (ret != 0) {
        pr_err("button_driver: fallo al registrar el dispositivo (%d)\n", ret);
        return ret;
    }

    pr_info("button_driver: /dev/button_driver listo\n");
    return 0;
}

static void __exit button_exit(void)
{
    misc_deregister(&button_dev);
    pr_info("button_driver: modulo descargado\n");
}
```

`misc_register(&button_dev)` registra el dispositivo con el nombre y las operaciones que preparamos. Devuelve cero si tiene éxito. Si devuelve un error negativo, `button_init()` lo reporta y lo devuelve al kernel para detener la carga. Cuando `rmmod` llama a `button_exit()`, `misc_deregister(&button_dev)` deshace el registro. [API de dispositivos misc del kernel](https://docs.kernel.org/driver-api/misc_devices.html)

#### Compilar, cargar y leer en la Raspberry Pi

El primer `make` falló porque el Makefile esperaba `src/Kbuild`. Ese archivo ya contiene `obj-m += button_driver.o`. El Makefile copia el archivo y el fuente C a `build/`, donde Kbuild genera `button_driver.ko`.

Desde la raíz del proyecto, con el módulo de la fase 1 descargado porque ambas fases usan el nombre `button_driver`:

```bash
cd kernel/fase2
make && sudo insmod ./build/button_driver.ko
ls -l /dev/button_driver
sudo cat /dev/button_driver
```

En la prueba, `ls -l` mostró `crw-------`, propietario `root root` y números `10, 123`. La `c` identifica un dispositivo de caracteres. El mayor `10` corresponde a los dispositivos misc y el menor `123` fue el que Linux asignó en esa carga. Con esos permisos, se usó `sudo` para leerlo.

La terminal mostró `1`. Así comprobamos que la lectura de `/dev/button_driver` llegaba hasta `button_read()` y entregaba el texto fijo de `mensaje[]`. Con ese recorrido funcionando, pasamos a conectar el GPIO.

#### Comprobar la retirada

```bash
sudo rmmod button_driver
ls -l /dev/button_driver
```

El resultado esperado es que `ls` indique que la ruta ya no existe. La sesión compartida confirma la carga, la aparición del nodo y la lectura. La retirada queda por comprobar.

### Paso 9. Reservar GPIO17 como entrada y liberarlo al terminar

Partimos de un dispositivo que ya entregaba un `1` fijo. Antes de cambiar la lectura, añadimos `<linux/gpio.h>` y definimos `GPIO_BUTTON` como `586`, el número global que el kernel de nuestra Pi asignó a GPIO17 en ese entorno. El preprocesador sustituye `GPIO_BUTTON` por ese valor. Así queda claro qué pin usamos en cada llamada.

Primero usamos `gpio_request(GPIO_BUTTON, "button_driver")` para reservar el pin. Después pasamos a `gpio_request_one(GPIO_BUTTON, GPIOF_IN, "button_driver")` para garantizar su dirección en cada carga. La reserva decide quién usa el pin y `GPIOF_IN` lo configura como entrada. El nivel que leemos depende del circuito y de su polarización. [API GPIO del kernel](https://www.kernel.org/doc/html/v6.8/driver-api/gpio/legacy.html)

En `button_init()` reservamos el pin antes de registrar el dispositivo:

```c
ret = gpio_request_one(GPIO_BUTTON, GPIOF_IN, "button_driver");
if (ret) {
    pr_err("button_driver: fallo al solicitar el GPIO (%d)\n", ret);
    return ret;
}

ret = misc_register(&button_dev);
if (ret != 0) {
    gpio_free(GPIO_BUTTON);
    pr_err("button_driver: fallo al registrar el dispositivo (%d)\n", ret);
    return ret;
}
```

Si falla la reserva, salimos sin liberar el pin. Si la reserva tiene éxito y falla el registro, liberamos el GPIO antes de devolver el error, porque `button_exit()` no se ejecuta cuando falla la inicialización. Tras una carga exitosa, la descarga normal sigue el orden inverso: `misc_deregister(&button_dev)` retira el dispositivo y `gpio_free(GPIO_BUTTON)` devuelve el pin.

#### Qué vimos en el diagnóstico del kernel

Revisamos `/sys/kernel/debug/gpio` para separar la prueba del hardware de la lectura por `/dev/`. El primer filtro buscaba `button_driver_gpio` y quedó vacío. `grep` solo filtra el texto que recibe por la tubería `|` o lee del archivo indicado. Al revisar el archivo completo encontramos esta línea:

```text
gpio-586 (GPIO17              |button_driver       ) in  lo
```

`button_driver` era la etiqueta de la reserva, `in` confirmaba la dirección de entrada y `lo` indicaba el nivel bajo en ese instante. A partir de ahí filtramos por el número del pin con `sudo grep 'gpio-586 ' /sys/kernel/debug/gpio`. Con el botón suelto vimos `hi` y al mantenerlo oprimido vimos `lo`. Así comprobamos que nuestra conexión es activa en bajo.

### Paso 10. Entregar el nivel del GPIO desde `button_read()`

Con el pin reservado, reemplazamos el mensaje fijo por un arreglo local modificable. Leemos el nivel al atender la llamada y cambiamos el primer carácter. Conservamos el salto de línea y la copia al espacio de usuario que ya funcionaba.

```c
char mensaje[] = "0\n";
int nivel = gpio_get_value_cansleep(GPIO_BUTTON);

mensaje[0] = nivel ? '1' : '0';

return simple_read_from_buffer(buf, len, off,
                               mensaje, sizeof(mensaje) - 1);
```

El sufijo `_cansleep` permite que la llamada espere si el controlador GPIO lo necesita. La reserva sigue en `button_init()`, la consulta queda en `button_read()` y la liberación en `button_exit()`. [API GPIO del kernel](https://www.kernel.org/doc/html/v6.8/driver-api/gpio/legacy.html)

Para volver a comparar las lecturas, compilamos desde `kernel/fase2/`. Con la versión anterior cargada, la retiramos y cargamos el nuevo `.ko` desde el directorio `build/`:

```bash
make && sudo rmmod button_driver && sudo insmod build/button_driver.ko
```

Comparamos estas dos salidas con el botón suelto y después mientras lo manteníamos oprimido:

```bash
sudo grep 'gpio-586 ' /sys/kernel/debug/gpio
sudo cat /dev/button_driver
```

| Estado del botón | Nivel en el diagnóstico GPIO | Texto esperado en `/dev/button_driver` |
| --- | --- | --- |
| Suelto | `hi` | `1` |
| Oprimido | `lo` | `0` |

Durante la prueba apareció un `0` con el botón suelto. La versión anterior ya mostraba `in` en el diagnóstico, así que el origen de ese `0` quedó sin confirmar. Cambiamos la reserva para solicitar explícitamente la dirección de entrada y luego vimos `1` con el botón suelto. Si ambas lecturas muestran bajo al soltarlo, revisamos el nivel del pin y el *pull-up*. Si el diagnóstico muestra `hi` y el dispositivo sigue entregando `0`, revisamos la versión cargada y el camino de lectura.

### App de la fase 2 · Detectar pulsaciones por sondeo

Con la fase 2 cargada, cada ejecución de `cat /dev/button_driver` nos daba una muestra. En [`app/fase_2.c`](app/fase_2.c) automatizamos esa consulta. Usamos `fopen()`, `fgetc()` y `fclose()` en cada vuelta para empezar con una apertura nueva y volver a la posición cero. El driver entrega `1\n` o `0\n`. Con `fgetc()` tomamos el primer carácter, `'1'` o `'0'`, y con `fclose()` cerramos la apertura. Es el mismo recorrido que repetíamos al volver a ejecutar `cat`.

Guardamos la muestra anterior en `anterior`, fuera del bucle. La inicializamos en `'1'` suponiendo que el botón arranca suelto. Si arranca oprimido, la primera muestra se interpreta como una pulsación. Usamos `int` para guardar el resultado de `fgetc()`, que también puede devolver `EOF` cuando no obtiene un carácter. Si falla la apertura, mostramos el motivo con `perror()`. Si obtenemos `EOF`, mostramos un error de lectura. En ambos casos salimos del programa.

```c
if (anterior == '1' && actual == '0') {
    puts("¡Botón oprimido!");
}
anterior = actual;
usleep(50000);
```

| Botón | Lectura anterior | Lectura actual | Resultado |
| --- | --- | --- | --- |
| Sigue suelto | `'1'` | `'1'` | Silencio |
| Lo oprimimos | `'1'` | `'0'` | Aviso |
| Lo mantenemos oprimido | `'0'` | `'0'` | Silencio |
| Lo soltamos | `'0'` | `'1'` | Silencio |

Actualizamos `anterior` después de comparar para conservar la transición. Con `usleep(50000)`, de `<unistd.h>`, esperamos 50 ms entre vueltas. Las operaciones de archivo y salida vienen de `<stdio.h>`. Si oprimimos y soltamos el botón entre dos muestras, podemos pasar por alto toda la pulsación. Esa limitación nos llevó a usar una interrupción.

Desde la raíz del proyecto, con el módulo de la fase 2 cargado, podemos compilar y ejecutar la app así:

```bash
cc -Wall -Wextra app/fase_2.c -o /tmp/button-fase2
sudo /tmp/button-fase2
```

Esperamos un aviso al oprimir el botón y silencio mientras lo mantenemos abajo. Terminamos con Ctrl+C.

## Fase 3 · Captura de pulsaciones mediante interrupciones

Partimos de una copia del módulo de la fase 2 en [`kernel/fase3/`](kernel/fase3/). Reservamos GPIO17 una vez y exponemos dos lecturas sobre el mismo pin.

| Dispositivo | Qué obtenemos al leer |
| --- | --- |
| `/dev/button_driver` | Una muestra inmediata del nivel eléctrico. `1` indica suelto y `0` indica oprimido. |
| `/dev/button_events` | Una `P` cuando llega un flanco de bajada posterior al inicio de la lectura. |

Al plantear esta fase consideramos guardar pulsaciones pendientes. Durante la construcción elegimos esperar el **próximo flanco** desde el inicio de cada lectura. Si el programa está esperando cuando oprimimos el botón, el driver puede avisarle aunque lo soltemos antes de que el programa procese el aviso. Las pulsaciones anteriores al inicio de la lectura quedan fuera.

### Identificamos la IRQ y conectamos el handler

Después de reservar GPIO17 como entrada, usamos `gpio_to_irq(GPIO_BUTTON)` para obtener la IRQ que Linux asocia al pin. Guardamos el resultado en `button_irq`, que también usamos al retirar la interrupción. Si el resultado es negativo, liberamos el GPIO y devolvemos el error. Al cargar la primera versión vimos en `dmesg`:

```text
button_driver: GPIO 586 usa la IRQ 186
```

El `586` era el número global del GPIO en nuestra Pi y el `186` fue la IRQ asignada en esa prueba. Obtuvimos la IRQ en cada carga con `gpio_to_irq()`.

Con `<linux/interrupt.h>` definimos `button_irq_handler()`. Al principio solo devolvía `IRQ_HANDLED` para indicar al kernel que atendimos la interrupción. Después asociamos la función a la IRQ:

```c
ret = request_irq(button_irq, button_irq_handler, IRQF_TRIGGER_FALLING,
                  "button_driver_irq", NULL);
```

- `button_irq` identifica la interrupción y `button_irq_handler` es la función que el kernel llama cuando llega.
- `IRQF_TRIGGER_FALLING` selecciona el cambio de alto a bajo que ocurre al oprimir el botón. Un flanco de subida correspondería a soltarlo.
- `"button_driver_irq"` es la etiqueta que buscamos en `/proc/interrupts`. Al comparar los contadores antes y después de oprimir, comprobamos el recorrido desde el pin hasta la IRQ.
- El último argumento se recibe como `dev_id`. En nuestro código usamos `NULL`, así que retiramos el mismo registro con `free_irq(button_irq, NULL)`.

### Estabilizamos el nivel de reposo

Al oprimir, el pulsador une GPIO17 a tierra. Al soltarlo, ese camino queda abierto. Con el *pull-up* interno, una resistencia conecta débilmente el pin a 3,3 V para que quede en alto en reposo. Sin polarización, una entrada flotante puede captar ruido y producir flancos aunque el botón esté quieto.

Primero usamos `sudo pinctrl set 17 pu`. Con el botón suelto vimos que el contador de interrupciones se mantuvo en `84344` durante cinco segundos. Luego llevamos esa configuración al propio módulo, después de reservar el GPIO como entrada y antes de activar la IRQ:

```c
ret = gpiod_set_config(gpio_to_desc(GPIO_BUTTON),
                       pinconf_to_config_packed(PIN_CONFIG_BIAS_PULL_UP, 1));
```

`gpio_to_desc()` obtiene la referencia al pin reservado. `pinconf_to_config_packed()` codifica la solicitud de activar el *pull-up* y `gpiod_set_config()` la aplica. Si falla, liberamos el GPIO y devolvemos el error. En la prueba vimos que la configuración pasó de `pn` a `pu` al cargar el módulo. El *pull-up* estabiliza el reposo. El rebote mecánico sigue pudiendo producir varios flancos durante una sola pulsación física.

### Conectamos la interrupción con la lectura bloqueante

Añadimos una cola de espera con `DECLARE_WAIT_QUEUE_HEAD(button_wait)` y una secuencia de 64 bits con `atomic64_t button_seq = ATOMIC64_INIT(0)`. La cola permite dormir a los lectores. La secuencia nos permite detectar un cambio entre el lector y el handler.

```c
static irqreturn_t button_irq_handler(int irq, void *dev_id)
{
    atomic64_inc(&button_seq);
    wake_up_interruptible(&button_wait);
    return IRQ_HANDLED;
}
```

En el handler incrementamos la secuencia antes de despertar a los lectores. En `button_events_read()` tomamos una muestra inicial con `atomic64_read()` y usamos `wait_event_interruptible()` para esperar hasta que la secuencia sea distinta. Si empieza en 4, la siguiente bajada la lleva a 5 y permite continuar. Esa comparación con `!=` también funciona cuando el contador pasa del máximo positivo al mínimo negativo. Cada llamada devuelve como máximo un evento, y un mismo cambio puede despertar a varios lectores. [Colas de espera del kernel](https://docs.kernel.org/driver-api/basics.html), [API de operaciones atómicas](https://docs.kernel.org/core-api/wrappers/atomic_t.html)

Al salir de la espera copiamos un byte con `put_user('P', buf)` y devolvemos `1`. Una petición de cero bytes devuelve `0` de inmediato. Con `O_NONBLOCK` devolvemos `-EAGAIN`, y si la espera se interrumpe por una señal propagamos su retorno. Si falla la copia a espacio de usuario devolvemos `-EFAULT`.

### Registramos el segundo dispositivo y ordenamos la limpieza

Enlazamos `button_events_read()` a `.read` en `button_events_fops`. Después asociamos esa tabla a `button_events_dev`, con nombre `button_events` y menor dinámico. Registramos ambos dispositivos con `misc_register()`, después de tener lista la IRQ.

Si falla `request_irq()`, liberamos el GPIO. Si falla el registro del primer dispositivo, liberamos la IRQ y el GPIO. Si falla el segundo, además retiramos el primero. En la descarga normal retiramos los dos dispositivos, liberamos la IRQ y finalmente devolvemos el GPIO.

### Qué esperamos en la prueba conjunta

Desde `kernel/fase3/` compilamos con `make` y cargamos `build/button_driver.ko` tras descargar la versión anterior si estaba cargada. Para observar los dos recorridos podemos usar dos terminales:

| Terminal | Comando | Resultado esperado |
| --- | --- | --- |
| A | `sudo head -c 1 /dev/button_events` | Queda esperando con el botón suelto. Al oprimir, muestra una `P` y termina. |
| B | `sudo cat /dev/button_driver` | Devuelve el nivel en ese instante, aunque A siga esperando. |

Usamos `head -c 1` porque queremos una sola lectura de evento. La `P` no lleva salto de línea y puede quedar pegada al prompt. En la Pi quedó comprobada la asignación de la IRQ y la estabilización con el *pull-up*. La salida de esta prueba conjunta queda por registrar.

### App de la fase 3 · Esperar pulsaciones

En [`app/fase_3.c`](app/fase_3.c) abrimos `/dev/button_events` una sola vez con `open(..., O_RDONLY)` y guardamos el descriptor en `fd`. Cada vuelta del bucle pide un byte usando ese mismo descriptor:

```c
char aviso;
ssize_t cantidad = read(fd, &aviso, 1);
```

Como abrimos el dispositivo en modo bloqueante, la llamada espera dentro del kernel hasta el próximo flanco de bajada. Cuando oprimimos el botón, el handler despierta al lector y el driver escribe `'P'` en `aviso`. El retorno queda en `cantidad`:

| Retorno de `read()` | Qué hacemos |
| --- | --- |
| `1` | Si `aviso == 'P'`, imprimimos `¡Botón oprimido!` y volvemos a esperar. |
| `0` | Informamos de un fin de archivo inesperado, salimos del bucle y cerramos el descriptor. |
| `-1` | Mostramos el motivo con `perror()`, cerramos el descriptor y terminamos con error. |

Al volver a llamar a `read()`, el driver toma una nueva referencia y espera otro flanco. Las pulsaciones anteriores a esa llamada quedan fuera. Así eliminamos del programa la pausa de 50 ms y la comparación de dos muestras. El reconocimiento del flanco queda en el driver.

Desde la raíz del proyecto, con el módulo de la fase 3 cargado:

```bash
cc -Wall -Wextra app/fase_3.c -o /tmp/button-fase3
sudo /tmp/button-fase3
```

Esperamos ver `Esperando el botón. Usa Ctrl+C para salir.` al arrancar y `¡Botón oprimido!` cada vez que la app recibe una `P`. Detenemos la app con Ctrl+C antes de descargar el módulo, porque el descriptor abierto mantiene una referencia a él.
