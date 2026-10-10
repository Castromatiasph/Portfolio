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

Menú inicial (teclas): `1` Práctica · `2` Tanda · `3` Penal (5,40 m) · `4` Libre (7,40 m) · `5/6/7` nivel del arquero
(Infantiles / Primera / Mundial) · `8` estilo del arquero · `Enter` jugar.

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

## Qué falta (para llegar al realismo "de verdad")

1. **Personajes animados.** Hoy el jugador y el arquero son piezas básicas (cilindro, cubos) para que todo sea jugable.
   `PPShooter` y `PPGoalkeeper` ya exponen las variables para un AnimBP (`Speed`, `StridePhase`, `Amp`, `Brake`, `LeanRoll`,
   `TwistAlpha`, `StickPitch`; `Kneel`, `Play`, `PadExt`, `Lean`, `PokeAlpha`). Pasos: importar `SK_Matias` (el jugador del
   juego, esqueleto MakeHuman) o un **MetaHuman**, retargetearlo, y armar un AnimBP con *Control Rig / FullBodyIK* (o Motion
   Matching) que lea esas variables. Asignalo en `BodyMesh` y ocultá `Placeholder`.
2. **Palo, bocha y careta con materiales PBR.** El palo del juego web (`STICK_DATA`, CC BY-NC-SA, de Yogensia) no se exportó;
   usá un modelo propio. La careta importada es `SM_CarataArquero`.
3. **Sonido.** `APPGameMode::OnPPEvent` es un evento de Blueprint que se dispara con `hit`, `net`, `post`, `save`, `goal`,
   `whistle`, `bounce`, `board`, `groan`, `cheer`: heredá un Blueprint del GameMode y enchufá MetaSounds / Niagara ahí.
4. **Hinchada.** `SM_Butaca` y `SM_Hinchas` están importados; falta poblarlas con instancias (HISM) y animación por vértices.
5. **Modos que no se portaron:** *Atajá vos* (vos sos el arquero), *Desafíos*, *2 jugadores* y la repetición del gol.
   La estructura (`EPPFormat`, `NextTurn`) está lista para sumarlos.
6. **Red del arco** con deformación (hoy se usa la malla estática importada).

## Estructura

```
UnrealPenalPatines/
├─ UnrealPenalPatines.uproject
├─ Config/            (render: Lumen/Nanite/VSM/RT, input)
├─ Source/PenalPatines/   (C++: juego completo)
├─ SourceAssets/
│   ├─ Models/        SM_Arco, SM_Butaca, SM_Hinchas, SM_Patin, SM_CarataArquero (.gltf), SK_Jugador, SK_Matias
│   └─ Textures/      texturas de los modelos + parquet 2K generado (Tools/make_parquet.py)
└─ Tools/             setup_level.py (editor), make_parquet.py
```

## Créditos de los modelos (del juego original)

- Patín: "70's - 80's Roller Skate" por gozdemrl (Sketchfab) — CC BY 4.0
- Careta: "Hockey Goalie Mask" por matýsek (Sketchfab) — CC BY 4.0
- Butacas: "2rows Tribune" por iyaroslavm (Sketchfab) — CC BY 4.0
- Palo (no incluido en este port): "Hockey Stick" por Yogensia (Sketchfab) — CC BY-NC-SA 4.0
