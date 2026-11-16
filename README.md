# MPPT de 50 W — ESP32-S3 (repositorio base)

Proyecto de investigación sobre seguimiento del punto de máxima potencia en un sistema fotovoltaico de 50 W bajo sombreado parcial.

**Estado:** estructura inicial preparada a partir del ZIP de firmware entregado por el responsable de implementación. **No es todavía una reproducción íntegra de la campaña experimental ni un repositorio publicado.** Se deben verificar la versión empleada en los ensayos, permisos de redistribución y los archivos faltantes antes de publicarlo.

## Contenido

- `firmware/MPPT_ESP32-S3/`: copia intacta de los archivos proporcionados en `MPPT_ESP32S3_SOLO_FIRMWARE.zip` (ESP-IDF, C/CMake, sdkconfig).
- `docs/diagramas/`: ubicación prevista para los ocho diagramas de flujo terminados por Codex; **aún no están incluidos** en este paquete.
- `hardware/`: ubicación prevista para el esquemático, BOM, netlist y vistas de PCB.
- `simulacion/`: ubicación prevista para modelos y scripts MATLAB/Simulink.
- `datos/`: ubicación prevista para MAT y CSV de campañas, con instrucciones de procedencia.
- `reproducibilidad/`: inventario de controladores y comprobaciones pendientes.

## Estado del firmware entregado

El selector declarado en `firmware/MPPT_ESP32-S3/components/mppt/include/mppt.h` enumera tres modos: `MPPT_PO`, `MPPT_MENG` y `MPPT_COMBINACION_FINAL`. Esto no demuestra soporte seleccionable para los otros cuatro controladores de la campaña experimental de siete estrategias.

`firmware/MPPT_ESP32-S3/main/main.c` usa `driver/ledc.h`, `ledc_timer_config`, `ledc_set_duty` y `ledc_update_duty`, con resolución solicitada de 10 bits y frecuencia configurada por `PWM_FREQ_HZ`. **No corresponde a la descripción previa de MCPWM**, por lo que debe aclararse cuál revisión ejecutó las mediciones de la campaña.

## Compilación (pendiente de verificación)

Con un entorno ESP-IDF compatible, desde `firmware/MPPT_ESP32-S3/`:

```bash
idf.py set-target esp32s3
idf.py build
```

Estos comandos son una guía a comprobar, **no una afirmación de compilación verificada**. Los flasheos, puertos y condiciones de banco deben documentarse por el responsable. Revisar configuración de GPIO, restricciones de seguridad y correspondencia con la placa antes de ejecutarlo.

## Límites de la evidencia

El contenido de este repositorio base **no prueba** por sí mismo que el firmware corresponda exactamente a los 175 ensayos físicos; hace falta vincular la revisión fuente, configuraciones y registros de campaña. No se debe modificar o completar con código inventado y etiquetarlo como firmware utilizado experimentalmente.

## Publicación

No se ha añadido una licencia abierta porque no consta la autorización de los autores para su redistribución. Solicitar permiso a todos los titulares y revisar datos y configuraciones antes de hacer público el repositorio.
