Especificación Técnica de Desarrollo: Pipeline 3D, Paquetes DMA, Distribución de VRAM y Microcódigo VU1 para PlayStation 2

1. Introducción y Arquitectura del Pipeline Geométrico

La arquitectura de la PlayStation 2 representa un hito en la computación paralela distribuida, donde el rendimiento gráfico no depende de un único procesador central, sino de la orquestación de múltiples núcleos autónomos. El corazón de este sistema, el Emotion Engine (EE), delega la carga geométrica a sus Unidades Vectoriales (VPU), permitiendo que el EE Core gestione la lógica de alto nivel mientras las VPUs procesan transformaciones matemáticas masivas en paralelo. Esta autonomía es fundamental: la capacidad de la unidad vectorial para generar y enviar datos de forma independiente al motor de renderizado es lo que define el techo de rendimiento de cualquier motor 3D en esta plataforma.

1.1. Autonomía de la VU1 y la Interfaz GIF (PATH1)

La VU1 (unidad de operación del VPU1) funciona como el motor geométrico principal e independiente del sistema. Posee una conexión directa con el Graphics Synthesizer (GS) a través del Graphic Interface Unit (GIF) mediante el bus dedicado conocido como PATH1. Esta ruta privilegiada permite que la VU1 procese y transfiera de forma autónoma tanto GS primitives (puntos, triángulos, líneas) como Display Lists (grupos de primitivas indicando lotes de imágenes) directamente al motor de dibujo, liberando al bus principal de la carga de transferencia geométrica.

1.2. Diferenciación Arquitectónica: VU0 vs. VU1

Es imperativo distinguir el rol de las unidades vectoriales para evitar la contención del bus y maximizar el rendimiento:

- VU0 (Coprocesador COP2): Íntimamente ligada al EE Core (identificada como COP2 en el manual de instrucciones). Se utiliza para cálculos no estructurados, como física, cinemática inversa o lógica de colisiones. Su flujo de datos viaja por PATH3, compartiendo ancho de banda con el procesador central.
- VU1 (Motor Geométrico): Diseñada para el procesamiento masivo de vértices. Su independencia le permite ejecutar microcódigo de transformación e iluminación simultáneamente con la lógica del juego.

Evaluación de la Separación de Rutas:

- Prevención de Wait States: Al separar el flujo de dibujo (PATH1) de la lógica de sistema (PATH3), se eliminan los estados de espera del procesador central durante ráfagas intensas de geometría.
- Reducción de la Contención de Bus (Bus Contention): La VU1 puede alimentar al GS incluso si el bus principal está saturado por transferencias de la IPU o el IOP, manteniendo la tasa de refresco constante.
- Paralelismo Real: Permite que el sistema trabaje en tres frentes simultáneos: Lógica (EE Core), Geometría (VU1) y Rasterización (GS).

2. Estructura Detallada de Paquetes DMA (DMA Chain & VIF/GIF Headers)

En una arquitectura de memoria distribuida, la eficiencia reside en el uso de Transfer Lists y Packets (datos manejados como unidades lógicas). Estas estructuras permiten que el controlador de DMA (DMAC) orqueste el movimiento masivo de datos entre la memoria principal, la SPR (Scratchpad Memory) y los subsistemas de video con mínima intervención del EE Core.

2.1. Anatomía de la DMAtag (128 bits)

La DMAtag es el encabezado que define el tamaño y los atributos del paquete. El campo ADDR está directamente vinculado a la lógica de Address Translation (Tratamiento de Direcciones Virtuales a Físicas, vAddr a pAddr), donde PSIZE se define como 32 bits en el manual de instrucciones del EE Core.

Campo Bits Función Técnica
QWC 15:0 Quadword Count: Número de unidades de 128 bits a transferir.
PCE 27:26 Priority Control: Gestión de prioridad en el arbitraje del bus.
ID 30:28 Identificador de comando de cadena (control de flujo del DMAC).
IRQ 31 Interrupt Request: Dispara una excepción al finalizar la transferencia.
ADDR 62:32 Dirección física (pAddr) del siguiente elemento o de los datos.
SPR 63 Indica si ADDR apunta a la Scratchpad Memory (SPR).

Nota: Aunque estos campos son estándar del sistema, el manual del EE Core se enfoca en la ejecución de instrucciones, mientras que el DMAC gestiona estos tags a nivel de hardware.

2.2. Análisis de Identificadores de Comando (IDs)

El campo ID orquestará la continuidad del flujo de dibujo:

- cnt: Continúa al siguiente paquete secuencialmente.
- call: Salta a una sub-cadena de transferencia (gestión de pila del DMAC).
- ret: Regresa de una sub-cadena tras finalizar el paquete.
- end: Marca el final absoluto de la Transfer List.

  2.3. Jerarquía de Control: VIFcode y GIFtag

1. VIFcode (32 bits): Instrucciones para el descompresor VIF. El comando UNPACK mueve datos a la memoria de la VU. El bit FLG/MSB gestiona el registro TOPS, esencial para el doble buffer (la VU1 procesa un bloque mientras el VIF carga el siguiente).
2. GIFtag (128 bits): Define atributos de las primitivas en el GS.

- NLOOP: Conteo de elementos.
- EOP: End of Packet.
- PRE/PRIM: Tipo de primitiva y contexto de dibujo.
- REGS/NREG: Destino en los registros del GS (XYZF2, RGBA, ST).

3. Mapa de Asignación y Direccionamiento de Memoria de Video (VRAM)

La VRAM del GS es un recurso crítico de 4 MB (32 Mbits) que requiere direccionamiento en palabras de 32 bits (word) y una alineación manual estricta para garantizar la integridad de los buffers.

3.1. Direccionamiento y Restricciones de Alineación

Los punteros base deben cumplir con factores de alineación específicos para evitar degradación de rendimiento:

- FBP (Frame Buffer Pointer): Base / 2048.
- ZBP (Z-Buffer Pointer): Base / 2048.
- TBP (Texture Buffer Pointer): Base / 64.
- CBP (CLUT Buffer Pointer): Base / 64.

  3.2. Configuración Práctica de Resolución y Buffers

El cálculo de consumo se rige por la fórmula: Ancho _ Alto _ (BPP / 8).

- NTSC (512x448, 32-bit): 512 \times 448 \times 4 = 917,504 bytes (896 KB).
- PAL (512x512, 32-bit): 512 \times 512 \times 4 = 1,048,576 bytes (1024 KB).

Layout de Memoria Sugerido (Modo PSMCT32):

Buffer Resolución Math (Bytes) Dirección Inicio (Hex)
Frame Buffer 512x448 (NTSC) 917,504 0x000000
Z-Buffer 512x448 917,504 0x08C000
Texture Area Restante ~2.2 MB 0x118000

En PAL, el incremento de 128 KB por buffer reduce significativamente el área de texturas, obligando a una gestión más agresiva del cache.

4. Programación de Microcódigo VU1 y Pipeline de Transformación 3D

Las VPUs utilizan el paradigma VLIW (Very Long Instruction Word). Es vital no confundir las instrucciones del EE Core (MIPS I/II) con el microcódigo VU1. Por ejemplo, la instrucción MADD del EE Core (Source Image 8) es una extensión de 64 bits para enteros, mientras que en VU1, MADD es una operación de coma flotante de 4 componentes.

4.1. Recursos y Arquitectura

La VU1 opera con 32 registros VF (128 bits). VF00 es una constante fija (0,0,0,1).

- Upper Instruction: Operaciones FMAC (Floating-point Multiply-Accumulate).
- Lower Instruction: LSU (Load/Store), enteros, saltos y la unidad de división.

  4.2. Implementación del Pipeline de Transformación

Se transforman vértices multiplicando la matriz de proyección (registros VF10-VF13) por el vértice (VF16) mediante instrucciones MULA y MADD. Este proceso ocurre en paralelo a nivel de instrucción, permitiendo procesar componentes X, Y, Z, W simultáneamente.

4.3. Optimización: Software Pipelining y XGKICK

La instrucción de división en VU1 tiene una latencia de 7 ciclos. Mientras que el EE Core posee sus propias unidades DIV1 y DIVU1 (Divide Pipeline 1) para enteros, la VU1 requiere Software Pipelining para ocultar su latencia: mientras se calcula el recíproco de W para un vértice, se inicia la transformación del siguiente.

El pipeline culmina con el comando XGKICK, que transfiere los datos procesados desde la memoria local de la VU1 directamente al GIF para su rasterización.

5. Reglas y Técnicas Clave de Optimización de Hardware

5.1. Alineación DMA y el concepto de "Slice"

Para maximizar el bus a 2.4 GB/s, es obligatorio alinear las transferencias a 128 bytes. Según el glosario oficial, la unidad física de transferencia DMA se denomina "Slice", definida como 8 qwords o menos. Mantener los datos en múltiplos de un "Slice" incrementa el rendimiento entre un 30% y 40% al permitir ráfagas (bursts) ininterrumpidas.

5.2. Eficiencia del Graphics Synthesizer

El GS debe operarse en tiras verticales de 32 píxeles. Esta técnica es crucial debido a que la memoria interna del GS está organizada en páginas; exceder este ancho provoca "DRAM page breaks" y "page misses", generando latencias severas en el motor de rasterización.

5.3. Sincronización de Procesos

- Polling (DMA.STR): Útil para tareas de baja latencia donde el EE Core espera activamente la finalización.
- Interrupciones: Preferible para motores de alto rendimiento, permitiendo que el EE Core procese IA o física (usando instrucciones MIPS I/II) mientras el DMA transfiere los "Slices" de geometría.

El cumplimiento de estas especificaciones garantiza la estabilidad y el máximo aprovechamiento del hardware de PlayStation 2.
