# Penal sobre Patines — port a Unreal Engine 5

Port del juego web "Penal sobre Patines" (hockey sobre patines: penales y tiros libres directos) a **Unreal Engine 5.5**
(C++), con la iluminación y el render configurados para el máximo realismo: **Lumen** (GI y reflejos, con hardware
ray tracing si la placa lo soporta), **Nanite**, **Virtual Shadow Maps**, TSR, luces rectangulares con sombras trazadas,
parquet con *clear coat* y los modelos 3D originales (arco, patín, careta, tribuna, jugadores).

> **Estado honesto:** el código está escrito y revisado a mano, pero **no pude compilarlo ni abrirlo en Unreal desde el
> entorno donde lo generé** (no hay Unreal instalado ahí). Es muy probable que el primer *Build* te marque algún error
> menor de API o de nombres; los corregís en minutos. Mirá "Qué falta" abajo.

## Cómo abrirlo

1. Instalá **Unreal Engine 5.5** (5.4 también debería andar: cambiá `EngineAssociation` en el `.uproject`) y
   **Visual Studio 2022** con "Game development with C++".
2. Clic derecho en `UnrealPenalPatines.uproject` → *Generate Visual Studio project files* → abrí el `.sln` y compilá
   (`Development Editor`, Win64). Abrí el proyecto.
3. En el editor: `Tools > Execute Python Script…` y elegí `Tools/setup_level.py`. Importa los modelos y texturas,
   crea `M_Parquet` (clear coat) y arma el mapa `/Game/Maps/Pista`. Dale **Play**.
   - Si no corrés el script, igual funciona: `APPGameMode` crea la pista con formas básicas al arrancar (sin parquet ni red).
4. Para el render máximo: *Project Settings > Platforms > Windows* → DX12 / SM6, y activá **Support Hardware Ray Tracing**
   (ya está en `Config/DefaultEngine.ini`). Pide una RTX 3070 o superior para 4K.

## Rendimiento y calidad gráfica

El proyecto arranca en calidad **Baja** (60 % de resolución, sin iluminación global, luz de relleno) para que ande fluido en cualquier PC.
Para cambiarla, en el menú inicial tocá **9**: **Baja** (60 %, sin GI, luz de relleno — para placas modestas), **Media** o **Alta**
(100 %, reflejos Lumen, sombras suaves en todas las luces). La pista usa pocas luces (6), componentes estáticos e instancias
(hinchada, red, papelitos), así que es liviana.

## Controles (iguales al juego web)

| Acción | Teclado y mouse | Joystick |
|---|---|---|
| Patinar | WASD | stick izq. |
| Apuntar | mouse (o stick der.) | stick der. |
| Golpe | clic y tirar el mouse hacia atrás; soltar | RT (carga por tiempo) |
| Picada / efecto | Shift / Ctrl al soltar | LB / RB |
| Arrastre / fuetazo / revés | clic derecho o F / V / B | A / X / B |
| Amagues | Q / E | cruceta |
| Cámara / volver al punto / pausa | C / R / Esc | Y / — / Start |

Menú inicial (teclas): `1` Práctica · `2` Tanda · `3` Atajá vos · `4` Desafíos · `5` 2 jugadores · `6` Penal/Libre ·
`7` nivel del arquero · `8` estilo del arquero · `9` calidad gráfica · `Enter` jugar.

**Atajá vos:** mouse para ubicar las manos, clic o Espacio para tirarte, Shift para salir por el área, clic derecho o E para el palo.
**2 jugadores:** patea con WASD + mouse; ataja con flechas, Enter (tirarse), Shift derecho (salir) y Ctrl derecho (palo).

## Qué se portó (y dónde está)

| Juego web | Unreal (C++) |
|---|---|
| Constantes y enums (`BALL_R`, `DIFF`, `GK_STYLES`…) | `Source/PenalPatines/PPTypes.h` |
| Bocha | `PPBall.*` (estado en metros; el actor se mueve en cm) |
| `skateStep`, poses del palo, mira, carga, cámaras | `PPShooter.*` |
| `launch`, `stepBall`, `settle`, `finalize`, tanda, rebotes | `PPGameMode.*` |
| `updateGoalie`, `goalieBlocks`, estocada, atajadas | `PPGoalkeeper.*` |
| Marcador, mira, medidor, menú | `PPHUD.*` (Canvas, sin UMG) |
| Pista, vallas, arco, gradas, luces | `PPArena.*` + `Tools/setup_level.py` |

Las reglas, tiempos de reacción, errores de puntería, velocidades (hasta ~19 km/h patinando, tiros de 29–95 km/h),
probabilidades de la tanda y el comportamiento del arquero son **los mismos números** del juego web.

Convención de ejes: la simulación corre en el espacio del juego web (metros, y arriba, arco en z=0) y recién al
mover los actores se convierte a Unreal (`PP::ToUE`): `UE.X = -z·100`, `UE.Y = x·100`, `UE.Z = y·100`.
El arco queda en el origen y el que patea mira hacia +X.

## Qué incluye ahora

Modos: práctica, tanda contra un rival, **Atajá vos**, **Desafíos** (7 objetivos con estrellas) y **2 jugadores**; repetición del gol
en cámara lenta (clic o Espacio para saltearla); hinchada que salta, papelitos, red del arco, publicidad en la valla; sonidos
sintetizados (`Tools/make_sounds.py`) y ambiente de estadio; jugador armado con formas básicas que patina (piernas, brazos, palo).

## Jugador: Matías (modelo propio)

El jugador es el modelo `Matias_175cm.glb` (MakeHuman, esqueleto de 137 huesos, sin animaciones). Se prepara así:
- `Tools/prepare_matias_mesh.py` saca texturas y morph targets (2 MB en vez de 28) → `SK_Matias.glb`.
- `Tools/dress_matias.py` **lo viste** (camiseta azul con bandas celestes, short, medias, patines y guantes) pintando sobre la textura de piel
  con los pesos del esqueleto, y extrae pelo, cejas, ojos y dientes.
- `setup_level.py` importa todo a `/Game/Models/SK_Matias` y `/Game/Textures/Matias`, y crea `M_Cutout` (pelo/cejas con transparencia por recorte).
- `PPRig` lo mueve por código (sin AnimBP): pelvis, inclinación y giro del torso, brazos al palo y piernas con IK de dos huesos; detecta solo
  los ejes y la escala del modelo. Si algo falla (falta un hueso), cae al cuerpo de formas básicas y lo avisa en el Output Log.

## Qué falta

1. **Arquero realista.** El arquero sigue siendo de formas básicas (el jugador ya es Matías). `PPShooter` y `PPGoalkeeper` exponen las
   variables para un AnimBP (`Speed`, `StridePhase`, `Amp`, `Brake`, `LeanRoll`, `TwistAlpha`, `StickPitch`; `Kneel`, `Play`, `PadExt`,
   `Lean`, `PokeAlpha`). Poné `IMPORT_MODELS = True` en `Tools/setup_level.py` para importar `SK_Matias` y `SK_Jugador`, o usá un
   **MetaHuman**, retargetealo y armá un AnimBP con Control Rig / FullBodyIK. Asignalo en `BodyMesh`.
2. **Bocha y careta con materiales PBR.** El palo ya es el modelo real (`SM_Palo`, preparado con `Tools/prepare_stick.py`). Su licencia (CC BY-NC-SA 4.0) no permite uso comercial.
3. **Joystick en Atajá vos y 2 jugadores** (hoy el arquero se maneja con mouse y teclado).
4. **Deformación de la red** al entrar la bocha.

## Estructura

```
UnrealPenalPatines/
├─ UnrealPenalPatines.uproject
├─ Config/            (render: Lumen/Nanite/VSM/RT, input)
├─ Source/PenalPatines/   (C++: juego completo)
├─ SourceAssets/
│   ├─ Models/        SM_Arco, SM_Butaca, SM_Hinchas, SM_Patin, SM_CarataArquero (.gltf), SK_Jugador, SK_Matias
│   ├─ Textures/      texturas de los modelos + parquet 2K y publicidad generados
│   └─ Audio/         efectos y ambiente (WAV sintetizados)
└─ Tools/             setup_level.py (editor), make_parquet.py, make_ads.py, make_sounds.py
```

## Créditos de los modelos (del juego original)

- Patín: "70's - 80's Roller Skate" por gozdemrl (Sketchfab) — CC BY 4.0 (en el juego: `SM_PatinReal`, preparado con `Tools/prepare_skate.mjs`)
- Careta: "Hockey Goalie Mask" por matýsek (Sketchfab) — CC BY 4.0
- Butacas: "2rows Tribune" por iyaroslavm (Sketchfab) — CC BY 4.0
- Palo (no incluido en este port): "Hockey Stick" por Yogensia (Sketchfab) — CC BY-NC-SA 4.0
