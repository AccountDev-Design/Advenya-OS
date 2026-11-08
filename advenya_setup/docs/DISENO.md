# Diseño de la primera experiencia

Referencias (copiadas en `assets/reference/`):
`logo_boot_reference.png`, `ref_01_bienvenida.png`, `ref_02_idioma.png`,
`ref_03_privacidad.png`, `ref_04_personalizacion.png`,
`ref_05_storyboard_boot.png`. Las maquetas tienen la proporción exacta de la
pantalla (480×800); posiciones y tamaños se han medido sobre ellas y los
colores se han muestreado de sus píxeles.

## Animación de arranque (≈3,0 s)

| Tramo | Elemento | Movimiento | Curva |
|---|---|---|---|
| 0,0–0,6 s | A | se revela desde su centro con fundido; brillo azul que se asienta en blanco | ease out |
| 0,6–1,2 s | 3 ondas | se dibujan de izquierda a derecha (2.ª y 3.ª con ligero retraso), trazo luminoso | ease in-out |
| 1,2–1,8 s | órbita | se traza sobre su elipse ajustada y termina en la estrella | ease in-out |
| 1,8–2,4 s | estrella | aparece desde su centro con un destello breve (halo que crece y se apaga) | ease out |
| 2,4–3,0 s | logotipo | completo y estable | — |
| final | transición | fundido de 300 ms a la pantalla del asistente | ease in-out |

**Identidad intacta.** Las capas no son un redibujo: `tools/gen_assets.py`
separa los píxeles del logotipo original por componentes conexos y geometría
(la 1.ª onda forma la barra de la A; la órbita y la estrella se separan con un
ajuste de elipse). Desde 2,4 s se muestra la imagen completa original. Si el
sistema no está listo a los 3 s (fondo aún horneándose) el logotipo queda
fijo hasta que lo esté. Sin sonido.

## Asistente

| Pantalla | Elementos (según referencia) |
|---|---|
| Bienvenida | lockup ADVENYA, «¡Bienvenido!», botón «Comenzar →» |
| Idioma | lockup, título, 5 filas (bandera, nombre nativo, selección), «Continuar →» |
| Privacidad | lockup, título, subtítulo, 3 tarjetas (icono, título, descripción, ›), casilla con enlaces, «Aceptar y continuar →», «Más tarde» |
| Personalización | lockup, título, subtítulo, Apariencia (3 tarjetas), Color del sistema (8), Fondo de pantalla (5), «Continuar →», «Más tarde» |

No hay indicadores de progreso (barras, puntos, porcentajes ni números).

### Movimiento ("Fluid Motion")

| Parámetro | Valor | Pedido |
|---|---|---|
| desplazamiento | 20 px | 16–24 px |
| salida | 260 ms, ease in | ease in |
| entrada | 300 ms tras 110 ms, ease out | ease out |
| total | 410 ms | 350–450 ms |
| selección | 180 ms (color, borde, check) | 150–220 ms |
| fondo | crossfade 320 ms | crossfade |
| pulsación | 120 ms (opacidad 87 %, 1 px), sin rebote | sutil |

Durante una transición se ignora cualquier navegación (doble toque incluido).

## Decisiones de interpretación (documentadas)

1. **Retroceso.** Las referencias no tienen botón «atrás». Se usa el gesto de
   deslizar hacia la derecha (y «Más tarde» en privacidad). Un gesto que
   empieza sobre un botón no lo pulsa.
2. **Fondo del asistente.** Es el fondo de pantalla elegido visto a través de
   un «velo» del color del tema: centro limpio y trazos suaves en las esquinas,
   como en las referencias (con el fondo azul reproduce su aspecto). Así el
   cambio de fondo se ve en el acto, con crossfade, sin añadir pantallas.
3. **Aceptación.** El botón «Aceptar y continuar» conserva el aspecto de la
   referencia; sin la casilla marcada no avanza y resalta la casilla 0,6 s.
4. **Documentos.** Cada tarjeta abre un panel inferior sobre la propia
   pantalla de privacidad con el documento. No hay textos legales: se muestra
   «documento pendiente» traducido (ver `legal/README.md`).
5. **«Más tarde» en personalización.** Termina el asistente conservando lo que
   se ve en pantalla y marca `personalization_deferred = 1` para el núcleo.
6. **Automático.** Sigue la hora (oscuro de 20:00 a 7:00). Durante el
   asistente no hay red ni RTC ajustado, así que la hora es desconocida y se
   resuelve a claro; se reevalúa cada minuto. El ajuste de hora corresponde
   al núcleo.
7. **Fondos.** Los cinco fondos (azul, violeta, melocotón, noche con montañas,
   plata) se reconstruyen por procedimiento a 480×800 a partir de las
   miniaturas de la referencia (que son demasiado pequeñas para usarse
   directamente). Fuente: `tools/gen_assets.py`; vistas en `assets/generated/`.
8. **Colores.** Acentos muestreados de la referencia: `#0066FC #855FEE
   #D158BB #E5363D #FD9440 #FDCD43 #72C456 #31B8B7`. Con acentos claros
   (amarillo) el texto sobre el acento pasa a oscuro para mantener contraste.
9. **Tipografía.** Inter 4.1 (OFL 1.1), similar a la de las referencias, con
   Latin-1 completo para los cinco idiomas. Los botones reducen el tamaño de
   la etiqueta si una traducción no cabe junto a la flecha.

## Capturas del simulador

`docs/capturas/` (generadas por `sim/run_tests.py`): matriz de los cinco
idiomas en claro y oscuro, secuencia de arranque, temas y acentos, fondos y
estados de transición.
