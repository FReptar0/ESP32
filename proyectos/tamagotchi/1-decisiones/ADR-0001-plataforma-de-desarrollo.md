# ADR-0001 — Usar PlatformIO con el framework Arduino, no el IDE de Arduino ni ESP-IDF

- **Estado:** `Aceptado`
- **Fecha:** 2026-08-29
- **Reemplaza a:** —
- **Relacionado con:** Fase 2 (entorno), RNF-04 (reproducibilidad)

---

## Contexto

Hay que elegir con qué herramienta se escribe, compila y carga el código al
ESP32. Es la primera decisión del proyecto y condiciona todas las demás: cambiar
de herramienta a mitad del camino significa rehacer la configuración y, a veces,
partes del código.

Restricciones del momento:

- El usuario está **aprendiendo** electrónica e IoT. La curva de aprendizaje
  pesa mucho.
- Se trabaja en **macOS 26.5.2, Apple Silicon (arm64)**.
- La metodología del taller exige **entornos aislados por proyecto**: cada
  proyecto congela sus versiones de librerías.
- Claude Code participa en el desarrollo, así que la herramienta debe poder
  manejarse **desde la línea de comandos**.

## Qué sabíamos y qué no

**Lo que sabíamos con certeza:**

- El ESP32 tiene dos ecosistemas de software: el framework **Arduino** (capa
  amigable, muchísimas librerías) y **ESP-IDF** (el SDK oficial de Espressif,
  potente y de bajo nivel).
- En la Mac ya están instalados Homebrew, Python 3.13.2, Git y Node 24.
- No hay instalado ningún IDE ni toolchain de embebidos.

**Lo que asumimos sin verificar:**

- Que el proyecto va a necesitar librerías de terceros (pantalla, JSON, cliente
  HTTP), no solo lo que trae de fábrica el framework.
- Que el proyecto va a crecer más allá de un solo archivo `.ino`.

**Lo que quedó pendiente de averiguar:**

- Si el ESP32 tiene RAM suficiente para hacer HTTPS + parseo de JSON al mismo
  tiempo. Se resuelve en la fase 5.

## Opciones consideradas

### Opción A — IDE de Arduino

- **A favor:** es lo más común en tutoriales para principiantes. Se instala y
  funciona. Interfaz gráfica sencilla.
- **En contra:** las librerías se instalan **globales**, compartidas entre todos
  los proyectos. Eso rompe frontalmente el requisito de entornos aislados:
  actualizar una librería para un proyecto nuevo puede romper uno viejo sin que
  te enteres. Además es una app gráfica, así que Claude Code no puede operarla —
  cada compilación y cada carga tendría que hacerla el usuario a mano.
- **Costo:** bajo al inicio, alto a los tres meses.

### Opción B — ESP-IDF (SDK oficial de Espressif)

- **A favor:** es la herramienta "de verdad" para ESP32. Control total del
  hardware, mejor gestión de memoria, es lo que se usa en producción.
- **En contra:** curva de aprendizaje muy pronunciada. Configuración por menús
  (`menuconfig`) con cientos de opciones, sistema de build propio, todo mucho más
  verboso. Para alguien que está empezando, el 90% del esfuerzo se iría en pelear
  con la herramienta en vez de en entender la electrónica.
- **Costo:** alto en tiempo de aprendizaje.

### Opción C — PlatformIO con framework Arduino ← *elegida*

- **A favor:**
  - El archivo `platformio.ini` fija **placa, framework y versión exacta de cada
    librería**. Es exactamente el aislamiento que pide la metodología.
  - Las librerías se instalan **dentro del proyecto**, no en el sistema.
  - Se maneja 100% por CLI (`pio run`, `pio run -t upload`, `pio device monitor`),
    así que Claude Code puede compilar, cargar y leer el monitor serie.
  - Sigue usando el framework Arduino, así que **todos los tutoriales de Arduino
    para ESP32 siguen siendo válidos** — no se pierde el ecosistema de aprendizaje.
  - Descarga sola el toolchain de compilación. No hay que configurar nada a mano.
- **En contra:** una capa más de conceptos al principio (qué es un "environment",
  qué hace `platformio.ini`). Y la primera compilación descarga ~1 GB de
  herramientas.
- **Costo:** medio al inicio, bajo después.

### Opción D — No decidir todavía y empezar con cualquier cosa

- **En contra:** el código escrito para el IDE de Arduino no se organiza igual
  que el de PlatformIO. Migrar después implica reestructurar carpetas y reinstalar
  librerías. Es más barato decidir ahora.

## Decisión

**Elegimos PlatformIO con el framework Arduino (opción C).**

El factor que pesó más: es la única opción que cumple el requisito de **entorno
aislado por proyecto** sin sacrificar el ecosistema Arduino, que es donde está
todo el material de aprendizaje. ESP-IDF cumpliría el aislamiento pero cobra un
precio de complejidad que no tiene sentido pagar cuando el objetivo es aprender
electrónica, no aprender sistemas de build.

Encima, que se maneje por CLI significa que Claude Code puede compilar, cargar y
diagnosticar directamente — lo que en la práctica multiplica cuánto se avanza por
sesión.

## Consecuencias

**Lo que esto nos habilita:**

- Cada proyecto congela sus versiones. Un proyecto de hace un año se puede
  recompilar y funciona igual.
- Claude Code puede ejecutar el ciclo completo (compilar → cargar → leer el
  monitor serie) sin intervención manual.
- El `platformio.ini` sirve de documentación: se lee y se sabe exactamente qué
  necesita el proyecto.

**Lo que esto nos cierra o nos cuesta:**

- Hay que aprender un concepto nuevo (`platformio.ini`) antes de escribir la
  primera línea de código.
- La primera compilación es lenta (descarga el toolchain, ~1 GB). Las siguientes
  son rápidas.
- Los tutoriales que dicen "abre el IDE de Arduino y dale a Verificar" hay que
  traducirlos mentalmente. El código en sí es idéntico.

**Lo que hay que vigilar:**

- Si en algún momento se necesita algo que **solo** existe en ESP-IDF (control
  fino de consumo para batería, por ejemplo), habrá que reevaluar. PlatformIO
  también soporta ESP-IDF como framework, así que la migración sería parcial, no
  desde cero.
- Si la compilación se vuelve tan lenta que estorbe al ciclo de aprendizaje.

## Cómo revertir

**Barato.** El código en framework Arduino es prácticamente el mismo en las tres
opciones: se cambia `src/main.cpp` a `tamagotchi.ino`, se instalan las librerías
en el IDE de Arduino, y listo. La diferencia es de estructura de carpetas y
gestión de dependencias, no de lógica del programa.

Por eso mismo esta decisión se pudo tomar rápido y sin investigar más: si sale
mal, deshacerla cuesta menos de una hora.
