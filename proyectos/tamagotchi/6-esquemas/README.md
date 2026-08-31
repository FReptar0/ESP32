# Esquemas — Tamagotchi

El proyecto mapeado de punta a punta, en dos planos.

| Documento | Qué contiene | Estado |
|---|---|---|
| [arquitectura-end-to-end.md](arquitectura-end-to-end.md) | Plano lógico (flujo de datos), ciclo de vida del firmware, dependencias externas | 🟢 Listo |
| [mapa-de-memoria.md](mapa-de-memoria.md) | Dónde vive cada cosa: flash contra RAM, pila contra montón, y cómo se manejan las letras | 🟢 Listo |
| [conexiones-fisicas.md](conexiones-fisicas.md) | Plano físico: diagrama de conexiones, tabla de pines, consumo y orden de armado | 🟢 Listo — pendiente de armar |
| [alimentacion-por-bateria.md](alimentacion-por-bateria.md) | Batería LiPo, cargador, elevador e interruptor: por qué no se conecta directo, dónde va el interruptor y en qué orden montarlo | 🟢 Diseño cerrado — falta comprar el elevador |

**Por qué Mermaid y no imágenes:** los diagramas son texto plano que se dibuja
solo. Al ser texto viven junto al código, se versionan con Git, y puedes ver
exactamente qué cambió entre dos versiones de un diagrama. Un `.png` exportado de
una herramienta de dibujo no te da nada de eso.
