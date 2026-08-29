# Trampa — Arduino define macros con nombres muy comunes

- **Fecha:** 2026-08-29
- **Cuántas veces mordió:** 2, el mismo día

---

## Qué pasó

Dos errores de compilación seguidos, ambos con mensajes que no señalaban al
problema real.

### 1. `HEX`

```cpp
static const char* HEX = "0123456789ABCDEF";
out += HEX[c >> 4];
```

```
error: expected unqualified-id before numeric constant
error: invalid types 'int[int]' for array subscript
```

Arduino define `HEX` como macro **con valor 16** — es la que se usa en
`Serial.print(x, HEX)`. El preprocesador la sustituye antes de compilar, así que
`HEX[c >> 4]` se convierte en `16[c >> 4]`.

De ahí el mensaje absurdo *"invalid types int[int]"*: el compilador está viendo
un número usado como si fuera un array.

### 2. `cli`

```cpp
class LectorChunked : public Stream {
  WiFiClient* cli;
```

```
error: macro "cli" passed 1 arguments, but takes just 0
```

`cli()` es una macro heredada de los AVR: *clear interrupts*, deshabilitar
interrupciones. Su pareja es `sei()`.

## Nombres que conviene evitar

| Categoría | Nombres |
|---|---|
| Bases numéricas | `HEX` `DEC` `OCT` `BIN` |
| Interrupciones | `cli` `sei` |
| Niveles y estados | `HIGH` `LOW` `INPUT` `OUTPUT` `LED_BUILTIN` |
| Utilidades | `min` `max` `abs` `round` `constrain` `map` |
| Booleanos | `true` `false` `boolean` `byte` `word` |

## Cómo reconocerlo la próxima vez

🔑 **Si un nombre corto y común provoca un error de compilación que no tiene
ningún sentido, sospecha de una macro.**

La pista es que el mensaje habla de algo que tú no escribiste — un número donde
pusiste un texto, o argumentos donde no hay ninguna llamada. Eso pasa porque el
preprocesador ya cambió tu código antes de que el compilador lo viera.

**Cómo salir de dudas:**

```bash
pio run -t idedata     # o compilar con -E para ver el código ya preprocesado
```

**Cómo evitarlo:** prefijar los miembros privados (`_c` en vez de `cli`) o usar
nombres específicos (`DIGITOS` en vez de `HEX`). Cuesta nada y ahorra la búsqueda.
