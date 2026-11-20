from signal import pause

from gpiozero import Button


def main():
    with Button(17, pull_up=True, bounce_time=0.05) as boton:
        boton.when_pressed = lambda: print("Presionado", flush=True)
        boton.when_released = lambda: print("Suelto", flush=True)

        print("GPIO17. Pulsa y suelta el botón. Ctrl+C para salir.", flush=True)
        estado = "Presionado" if boton.is_pressed else "Suelto"
        print(f"Estado inicial: {estado}", flush=True)

        try:
            pause()
        except KeyboardInterrupt:
            print("\nPrueba terminada.")


if __name__ == "__main__":
    main()
