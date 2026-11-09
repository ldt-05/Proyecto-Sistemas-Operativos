# Interfaz gráfica del simulador

Módulo de visualización desarrollado con **raylib**. Permite consultar en tiempo real el estado de los procesos, el reloj del simulador y el diagrama de Gantt, manteniendo disponible la interfaz de comandos (CLI).

## Requisitos

Se necesita un compilador de C, `make`, `pkg-config` y la biblioteca de desarrollo de raylib.

## Compilación y ejecución

Desde la raíz del repositorio:

```bash
make -f gui/Makefile
./gui/simulador_gui
```

## Uso

Al ejecutar el programa se abrirá la ventana gráfica y permanecerá disponible la CLI en la terminal.

La ventana muestra los procesos registrados, sus estados, las ráfagas restantes, los indicadores generales y el historial de ejecución de la CPU.

Los procesos pueden crearse desde la CLI mediante `create`. Una vez integrados los módulos correspondientes, también estarán disponibles `load`, `start`, `pause` y los demás comandos del planificador.

Para cerrar únicamente la ventana, utiliza el botón **Cerrar GUI**. La CLI continuará funcionando hasta ejecutar `exit`.
