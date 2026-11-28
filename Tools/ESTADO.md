# Viper — banco de pruebas del acompañante

Estado al **27-11-2026**. Escrito para poder retomarlo sin la conversación en que se construyó.

---

## 1. Qué hay

### El laboratorio en el navegador
Una página que simula al detective y a Marci con **las mismas fórmulas que el C++**, para ajustar
y medir sin abrir el motor.

- Publicada: <https://claude.ai/artifact/Qs18bZmmhHkf1fYhr8Cs6p> (versión 18)
- Copia local, **la que hay que usar para el botón Push**: `Tools/FollowLab.html`
- Arriba: **Play / Build the map**. Derecha: los controles del modo activo. Abajo: todas las
  variables, con su explicación al pasar el ratón.

Qué sabe hacer: editor de mapas (cubos, mover, borrar, puntos de ruta), **generador de laberintos**
con parámetros, colisiones, **A\* con las esquinas tiradas** para los dos personajes,
click-to-move, y *Play as the AI*.

**Decide como `ST_Companion` (27-11).** `stepCompanion` replica `Root/Follow` estado por estado:
misma prioridad (ComeOver, EyeContact, RoamArea, WanderOff, IdleBreak, WalkWithYou), condiciones
de entrada solo al re-seleccionar, y un estado en marcha solo cede por sus transiciones o al
terminar su task. Añadidos *IdleBreak* (`idle`) y *WanderOff* (`wander`, alrededor de sus pies);
`roaming` pasa a ser `bLeaderSettled` (no se cancela al mirarla). Los umbrales del árbol están en
el grupo *Idle moments (game states)* y en *Attention* con `tree:true`: **no** salen en
ViperTune/Push porque viven en el asset, no en la task. Si cambias el árbol, cambia aquí también.
Lo único que no se replica: *TalkingToYou* (la web no habla) y la navegación (A\* vs navmesh).

**Hallazgo al igualarla:** si está lejos mientras camina contigo (*WalkWithYou*) y la miras, no
llega a *ComeOver*: al notarte (0,8 s) nada más vale aún (ComeOver pide 3 s, EyeContact < 5 m), y
*WalkWithYou* la trae **corriendo** por la distancia. ComeOver solo aparece si está paseando o
parada cuando empiezas a mirarla. Pasa igual en el juego.

**Arreglado (28-11), en el juego y en la web: "que venga andando".** Es la regla original: si la
miras fijamente se para y se gira; si está lejos y sigues mirándola, se acerca.
- *EyeContact* ya **no tiene límite de distancia**: entra con `NoticesMe & !LeaderIsMoving`.
  Lejos, se para donde está y te mira (con `bComeToRest` no echa a andar).
- Transición nueva *EyeContact* → Follow con `NoticesMe & TimeLookedAt>3 & Dist>500`, que
  re-selecciona *ComeOver* (andando a 300) en cuanto se cumple.
- *WalkWithYou* reconstruida: la transición hacia EyeContact ya no mira la distancia.
- Todas las `OnTick` de `Root/Follow` llevan el retardo de 0,15 s que tenían las hechas a mano
  (las creadas por la herramienta no lo traían).
- Copia previa: `Backups/ST_Companion_2026-11-27_2356_eyecontact500.uasset`.
Simulado (`sim-check scenario`, 3 veces): "lejos y mirándola" = rest 2,2 s → approach 10,8 s,
8,5 m andando; sin la carrera de *follow*. La web no replica el retardo de 0,15 s.
En juego (`run_st_look14..15`): sin errores, EyeContact a ~4,5 m y vuelta a RoamArea al apartar
la vista. **El caso lejano (>5 m) no salió en juego** (la distancia al empezar a mirarla es azar).

**`InitMap` (28-11):** borrada `BP_Companion_C_1` (la puesta a mano, sin etiqueta) y guardado.
Queda solo la del toolset (`C_4` en el editor). Ojo: la del mapa **no estaba guardada** en disco
hasta entonces, y los nombres de instancia del editor no son los del juego; por eso
`scenario_lookat.txt` busca `BP_Companion` y no un nombre exacto. Copia previa del nivel:
`Backups/InitMap_2026-11-28_0014.umap`.

### En el motor
| Pieza | Dónde |
|---|---|
| Nodos del StateTree del acompañante | `Source/Multiplayer_GAS/*/AI/ViperCompanionStateTreeNodes.*` |
| Memoria que sobrevive a la re-selección de estados | `AI/ViperCompanionAIController.h` |
| Piloto IA que juega al jugador | `Player/ViperTestPilotController.*` |
| Comandos de consola | `Player/ViperPlayerController.*` |
| Toolset MCP que construye el mapa | `Content/Python/viper_map_toolset/` |
| Puente para el botón Push | `Source/Multiplayer_GASEditor/*/ViperMapPushLibrary.*` |
| Editar estructura/bindings de StateTrees | `Source/Multiplayer_GASEditor/*/ViperStateTreeEditLibrary.*` + `Content/Python/viper_map_toolset/toolsets/viper_statetree.py` |
| Permiso de esa única llamada remota | `Config/DefaultRemoteControl.ini` |

---

## 2. Cómo se usa

### Montar un mapa en el editor
1. Genera el laberinto en `Tools/FollowLab.html` (abierta **desde disco**, doble clic).
2. **Push to Unreal**. Si responde que https no puede llamar a http, estás en la publicada.
3. En el editor: **Build → Build Paths** (sin esto el navmesh no ve los cubos) y **Ctrl+S**.
4. Borra el `PlayerStart` viejo del nivel: con dos, el spawn es aleatorio y los tests mienten.

El editor tiene que arrancar con los dos servidores:

    UnrealEditor.exe <ruta>.uproject -ExecCmds="ModelContextProtocol.StartServer, WebControl.StartServer"

Ninguno arranca solo: el del MCP se desactivó en agosto porque rompía el empaquetado
(salía `Cook failed` siendo mentira), y el web de Remote Control nunca arranca por defecto.

### Jugar
En la consola del editor, dentro de Play:

    ViperPlayAs ai            // el piloto recluta a Marci y recorre las pistas
    ViperPlayAs player        // retira el piloto, el pawn es tuyo
    Viper.Companion.Debug 1   // qué está decidiendo Marci

### Pruebas sin nadie al teclado
    Tools/TestBench/run.sh <nombre> <segundos> <scenarios/escenario.txt>
    py -I Tools/TestBench/analyze3.py --brief D:/Multiplayer_GAS/Diffs/run_<nombre>.log

Cada escenario es una lista de comandos de consola. **Las comas separan comandos** en
`-ExecCmds`, así que las listas internas (los recorridos del piloto) usan `+`.

### Comprobaciones automáticas
Desde `Tools/TestBench/`. Leen el script directamente de `Tools/FollowLab.html`, así que siempre
prueban la web tal como está (no hay copia aparte del .js que pueda desfasarse).

    node editor-check.js     // 71: editor de mapas, colisiones, A*, mirada, laberintos
    node sim-check.js scenario
    py -I toolset-check.py   // 37: el contrato entre la web y el editor
    node export-map.js       // un laberinto aleatorio en el JSON de "Copy for Unreal"

---

## 3. Lo que costó encontrar (para no volver a tropezar)

- **Una sola ejecución no es evidencia.** Hay aleatoriedad en todo; se comparan 3 repeticiones.
  Ya hubo que retirar una conclusión por esto.
- Un `PlayerController` **sin `ULocalPlayer` no es local**, y `CharacterMovementComponent` no
  mueve su pawn. Por eso el piloto se crea como segundo jugador local.
- La interacción exige `APlayerController` en `IViperInteractableInterface::Interact`, y los
  candidatos salen de solapes, no del trazo. Interactuar en el mismo frame en que giras falla:
  el personaje elige a qué mira en su propio Tick.
- `AController` ya tiene miembros `Pawn` y `Character`; el módulo compila el sombreado como error.
- `EPathFollowingRequestResult` vive en `Navigation/PathFollowingComponent.h`, no en `AIController.h`.
- Con el **editor abierto no se compila** (Live Coding), y un proceso de juego olvidado hace que
  el log que leas después sea del binario viejo.
- El Python del editor **solo se lee al arrancar**: tocarlo obliga a reiniciar.
- En `ctypes` hay que declarar `argtypes` en **todas** las llamadas, o un handle de 64 bits da
  `OverflowError`.
- `cornerAround` adivinaba una esquina y una vez mandó al acompañante 50 m a la equivocada: de ahí
  que ahora haya A\* de verdad.

---

## 3b. Atención compartida (28-11): mira lo que tú miras

Primera de las mejoras "más humano" (investigación en la conversación del 28-11: Ellie, Elizabeth,
Uncharted 4; seguir la mirada sube la presencia social en estudios con robots).
- **Evaluador** (datos, sin decisiones): `LeaderFocusPoint`, `LeaderFocusActor`,
  `TimeLeaderFocused`, `bLeaderStudying`. Un interactuable dentro de `FocusSnapAngle` (8°) de tu
  vista gana al rayo; si no, el rayo hasta el primer obstáculo (`FocusMaxDistance` 1500). Mismo
  foco = mismo actor, o punto a menos de `FocusRadius` (150). Gracia `FocusBreakGrace` 0,4 s.
  Studying = foco válido, no la miras a ella, y `FocusHoldTime` (1,5 s) cumplido. La vista se
  calcula ya fuera del límite de 9 m del contacto visual. Log: `focus= studying= on=`.
- **Árbol**: *ShareAttention* (copia de EyeContact) entre EyeContact y RoamArea; su `Look At`
  ligado a `LeaderFocusActor`/`LeaderFocusPoint`, `bTurnBody` on, `bBreakEyeContact` off.
  WalkWithYou y RoamArea le ceden el paso. Copia previa: `Backups/ST_Companion_2026-11-28_0055_antes-shareattention.uasset`.
- **Web**: lo mismo (`share`), casilla *Look at the nearest clue*, líneas punteadas hacia el foco,
  parámetros en *Attention* con `tree:true`. `sim-check`: fase *you study a clue* → share ~7,9 s de
  10 y vuelta a roam en 0,2-0,4 s al apartar la vista (3 de 3).
- **Juego**: `scenario_share.txt` (piloto mira una pista a los 60 s, 25 s; a los 90 s la mira a ella).
  Primera versión: se activaba mirando **cualquier pared** (la congelaba 40 s) y no reconocía una
  pista a 40 cm. Corregido: solo cuentan interactuables (`LeaderFocusActor`), el margen crece de
  cerca (`FocusTargetRadius` 60) y un objeto sustituye a un punto en el acto. `run_st_share5..6`:
  `on=ViperClueActor_27 studying=1`, y al mirarla `notices=1 studying=0`. Sin errores.

Fase 2, sin hacer: colocarse a tu lado mirando la pista. (Señalar con la mirada: hecho, ver 3c.)

## 3c. Más humano: mirada, reacción, espacio y formación (28-11)

Todo en las tasks (el "cómo"), sin decisiones nuevas en C++; mismos nombres y valores en la web.
Diff: `D:/Multiplayer_GAS/Diffs/2026-11-28_1111_companion-human/`. Copia previa del asset:
`Backups/ST_Companion_2026-11-28_1112_antes-humano.uasset` (el asset ya recoge los valores nuevos).

| Cambio | Dónde | Parámetros (por defecto) | Comprobado |
|---|---|---|---|
| Rompe el contacto visual a ritmo humano (normal, mirada corta y algo hacia abajo) | `Look At` | `MeanHoldTime` 2,2 / `HoldTimeSpread` 0,6, `Min/MaxHoldTime` 1,2/3,5, apartar 0,4–1,0 s, 10–25°, `GlanceDownAngle` 8 | web (tramos 1,2–3,5 s) |
| Tiempo de reacción al arrancar (no al parar) | `Follow Leader` | `ReactionTimeMean` 0,35, `ReactionTimeSpread` 0,1 (0,15–0,7 s) | web (0,24–0,47 s) |
| No se planta en tu camino ni en pasos estrechos; si vas hacia ella, se aparta de lado | `Follow Leader` | `LeaderPathConeAngle` 35, `LeaderPathConeLength` 400, `MinStandWidth` 140, `StepAsideDistance` 250 | web: lateral 94–124 cm frente a recular 175–298 |
| A tu lado en abierto y despacio; en fila en pasillos | `Follow Leader` | `SideBySideMaxLeaderSpeed` 220, `SideBySideMinWidth` 400, `SideBySideAngle` 75, `SideBySideDistance` 170, `CorridorWidth` 250, `FormationBlendTime` 1 | web: al lado 73% a 150 cm/s; fila 100% en pasillo de 220 |
| Señala con la mirada lo que encuentra (cosa → tú → cosa) | `Wander` | `bGazeLeadOnFinds`, `GazeLeadMaxDistance` 1200, `GazeLeadFindLookTime` 1,0, `GazeLeadLeaderLookTime` 0,75 | **web y juego** (`GAZE LEAD: glancing at you for 0.86s`) |

Juego (`run_st_human_look`, `run_st_human_share`): sin errores; ahí solo se ve la mirada que señala.
Escenario nuevo `scenario_walk.txt` (recluta, pasea a 150, luego a 300, y va hacia ella), con el
comando nuevo `ViperPilotSpeed <cm/s>` (cambia `WalkSpeed` del pawn del piloto por reflexión):
- Al lado paseando a 150: `slot=75deg/170` casi todo el paseo; a 300 vuelve a `35deg/280`.
- Reacción al arrancar: 0,26–0,45 s. Apartarse: `STEP ASIDE 228 to the right` (el piloto la
  persigue porque su orden es ir hasta ella, así que acaba a 68 cm: es la prueba, no el gesto).
- **Corregido aquí:** paseando tú a 150, al cambiar de dirección ella se quedaba 5-8 m atrás y
  **corría a 400-680 cm/s**. Nuevo tope `StrollCatchUpFactor` 1,6 × su paso (≈320) mientras paseas,
  salvo a más de `StrollCatchUpMaxGap` 1100. Medido: máx. 320 (una muestra de 515, a más de 11 m),
  y sigue al lado 70-73% en la web.
- En fila en pasillo: no se ve, los pasillos del laberinto miden más de 360 cm (umbral 250).
Límite conocido: si vas hacia ella rápido (300 cm/s desde 2,2 m) aún os rozáis: reacción + rampa
no le dan tiempo.

**No implementado (necesita assets):** pequeños movimientos (cambiar el peso, cruzar los brazos),
apoyarse o sentarse (animaciones y puntos en el nivel), comentar según cómo te mueves (voces),
sincronizar el paso (Animation Blueprint), guiar hacia un sitio (no hay momento en el juego en
que ella vaya delante).

## 4. Pendiente

1. ~~Cablear `ST_Companion`~~ **Hecho el 27-11** por MCP (sección 5). Probado una vez en juego
   (`run_st_wired_playas.log`, piloto IA): con el jugador quieto sin mirarla, `settled=1` a los
   ~10 s y desde ahí 9 paseos seguidos `wander | ENTER ... (anchored)` a 500-680 cm del ancla,
   sin errores del StateTree.
   **ComeOver / EyeContact** (`scenario_lookat.txt`, `run_st_look1..4.log`): el piloto la mira a
   los 60 s durante 30 s y a los 95 s mira a una pista.
   - Desde 575 cm: `notices` a ~1 s; *WanderOff* hasta `looked>3`; *ComeOver* (form 230, run
     300) la trae de 867 a 299 cm; *EyeContact* la para y la gira hacia ti, y se queda.
   - Desde 424 cm: *EyeContact* directo.
   - Desde 493 cm: hueco 450-500 con `looked<3` → la recoge *WalkWithYou* a **449 cm/s**
     (corriendo) hasta entrar en *EyeContact*. Es el silbato que queríamos evitar.
     **Arreglado el 27-11**: *EyeContact* entra hasta `Dist < 500` (y la transición espejo de
     *WalkWithYou* también), así que encaja con `ComeOver > 500`. Copia previa:
     `Backups/ST_Companion_2026-11-27_2334_cableado.uasset`. Repetido 3 veces
     (`run_st_look5..7`): todo bien, pero las tres empezaron a 201/422/429 cm, **ninguna cayó en
     la franja 450-500**, así que ese caso concreto sigue sin verse en juego.
   - Al apartar la vista: `notices=0` en el acto y vuelve a *RoamArea* (anclada).
   `ViperAfter <s> <cmd con | por espacios>` y `ViperPilotLook <nombre> [segundos]` son los
   añadidos que lo hacen posible (-ExecCmds dispara todo a la vez).
   **Ojo**: `InitMap` tiene **dos** compañeras (`BP_Companion_C_1` puesta a mano y `_C_2` del
   toolset). La que no se recluta llena el log de `idlebreak | ENTER` que no son de Marci.
   Copia del asset anterior: `D:/Multiplayer_GAS/Backups/ST_Companion_2026-11-27_2254.uasset`.
2. ~~Nada está commiteado.~~ Commiteado y mergeado en `main` el 28-11 (PR #1). Lo de después
   (atención compartida) aún no.
3. La disposición nueva de la web **no se ha visto renderizada** (el panel del navegador pide
   iniciar sesión). Si algo está descolocado, es ahí.
4. Dos patologías viejas del acompañante, sin diagnosticar: una excursión intermitente de unos
   50 m y atascos, una de cada tres ejecuciones, también en la versión de referencia.

---

## 5. Abrir el editor y editar `ST_Companion` desde fuera

Recuperado de la conversación anterior (26-11), donde se usó y funcionó.

### Abrir el editor y esperar a que responda
```bash
powershell -NoProfile -Command "Start-Process -FilePath 'D:\UnrealEngine_5.8_EpicGames\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList '\"D:\Multiplayer_GAS\Multiplayer_GAS\Multiplayer_GAS.uproject\"', '-ExecCmds=\"ModelContextProtocol.StartServer, WebControl.StartServer\"'"
# esperar a que escuche el puerto 8000 (MCP) o 30010 (Remote Control), ~15-60 s
for i in $(seq 1 70); do powershell -NoProfile -Command "if (Get-NetTCPConnection -LocalPort 8000 -State Listen -ErrorAction SilentlyContinue) { exit 0 } else { exit 1 }" && break; sleep 5; done
```
Para compilar hay que **cerrarlo** antes (Live Coding). Cerrar solo el de este proyecto:
`Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'"` filtrando `CommandLine -like '*Multiplayer_GAS.uproject*'`,
nunca `taskkill /IM UnrealEditor.exe` a ciegas (cierra también otros proyectos abiertos).

### Editar el StateTree por MCP
No con el `StateTreeToolset` (solo lectura), sino con dos herramientas genéricas del motor:

- `editor_toolset.toolsets.object.ObjectTools` → `get_properties` / `set_properties` sobre los
  **subobjetos** del asset.
- `editor_toolset.toolsets.programmatic.ProgrammaticToolset` → `execute_tool_script`: un script
  Python con `def run()` que llama a `execute_tool(NOMBRE, json)` muchas veces en una sola petición.
- Al final, `editor_toolset.toolsets.asset.AssetTools` → `save_assets(["/Game/AI/ST_Companion"])`.

Rutas: `/Game/AI/ST_Companion.ST_Companion:StateTreeEditorData_0` es la raíz (ahí viven los
`Evaluators`); los estados cuelgan como `...StateTreeEditorData_0.StateTreeState_0` (Root)
`.StateTreeState_4` (...) `.StateTreeState_8` (Eye Contact). Cada estado tiene `Name`, `Children`,
`EnterConditions`, `Tasks`, `Transitions`. Lo primero es siempre volcar el árbol y sacar el mapa
nombre → ruta, porque los números no dicen qué estado es.

Patrón que funcionó (leer → modificar el JSON → escribir → releer para verificar):
```python
import json
GET = "editor_toolset.toolsets.object.ObjectTools.get_properties"
SET = "editor_toolset.toolsets.object.ObjectTools.set_properties"
EYE = "/Game/AI/ST_Companion.ST_Companion:StateTreeEditorData_0.StateTreeState_0.StateTreeState_4.StateTreeState_8"

def props(path, names):
    return json.loads(execute_tool(GET, json.dumps({"instance": {"refPath": path}, "properties": names}))["returnValue"])

def setprops(path, values):
    return execute_tool(SET, json.dumps({"instance": {"refPath": path}, "values": json.dumps(values)}))

def run():
    data = props(EYE, ["Tasks"])
    for t in data["Tasks"]:
        if "IdleBreak" in t["node"]["_structType"]:
            t["instance"]["stopTime"] = 1.5
    setprops(EYE, data)
    return props(EYE, ["Tasks"])   # releer: lo que cuenta es lo que quedó
```

**Hecho así y verificado:** parámetros de tasks (`stopTime`, `formationDistance`, `bComeToRest`...),
umbrales de las `EnterConditions` existentes (p. ej. `Greater 12 → 4`, `Less 0.35 → 0.6`) y
parámetros del evaluador (`eyeContactAngle` 25, `eyeContactTolerance` 3).

**Lo que `ObjectTools` NO puede:** bindings (qué variable lee una condición o una task:
`EditorBindings` no se lee por esa vía), crear estados, tocar transiciones con sentido.

### Estructura y bindings: `ViperStateTreeTools` (27-11)

C++ `UViperStateTreeEditLibrary` (módulo `Multiplayer_GASEditor`) + toolset MCP
`viper_map_toolset.toolsets.viper_statetree.ViperStateTreeTools`. Estados por ruta de nombres
(`Root/Follow/EyeContact`); cada llamada devuelve un informe, y un fallo empieza por `ERROR:`.
Todo es transacción de editor (Ctrl+Z). No guarda: `compile_state_tree` y luego `save_assets`.

| Tool | Qué hace |
|---|---|
| `describe_state_tree` | JSON con estados, condiciones (valores **y a qué están ligadas**), tasks, transiciones |
| `duplicate_state` | copia un estado con tasks, transiciones y **sus bindings**, IDs nuevos, en la posición que digas |
| `clear_enter_conditions` / `clear_transitions` | vacía, quitando también sus bindings |
| `add_bool_enter_condition` / `add_float_enter_condition` | condición ya ligada a una salida del evaluador |
| `add_transition` + `add_bool/float_transition_condition` | "ir a estado" con condiciones ligadas |
| `bind_to_evaluator` | re-liga una entrada (`Condition`, `Task` o `Transition[N]`) a otra salida |
| `compile_state_tree` | compila y devuelve los mensajes del compilador |

**Por qué hacen falta transiciones**: un estado en marcha (p. ej. *WalkWithYou*, cuya task no
termina) solo cede el control por sus transiciones `OnTick → Follow`, que repiten las condiciones
de entrada de los hermanos de más prioridad. Un estado nuevo sin su transición en *WalkWithYou*
no se elegiría nunca.

### Cómo quedó `Root/Follow` (por prioridad)

| Estado | Entra si | Sale (OnTick → Follow) si |
|---|---|---|
| TalkingToYou | IsSpeaking | !IsSpeaking |
| **ComeOver** (nuevo, copia de WalkWithYou) | NoticesMe & TimeLookedAt>3 & Dist>500 | !NoticesMe, o Dist<300 |
| EyeContact | **NoticesMe** (antes LooksAtMe) & !Moving, **a cualquier distancia** | **!NoticesMe**; o NoticesMe & TimeLookedAt>3 & Dist>500 (→ ComeOver) |
| **ShareAttention** (nuevo, copia de EyeContact) | Studying & !NoticesMe & !Moving | !Studying, NoticesMe o Moving; y al terminar |
| **RoamArea** (nuevo, copia de WanderOff) | Settled & !NoticesMe | !Settled, NoticesMe o **Studying**; y al terminar |
| WanderOff | sin cambios | sin cambios |
| IdleBreak | sin cambios | sin cambios |
| WalkWithYou | — | + las de ComeOver y RoamArea; la de EyeContact es NoticesMe & !Moving (sin distancia) |

ComeOver: FollowLeader con `FormationDistance` 230, `SettleStart/Stop` 140/110, `RunSpeed` 300,
`bComeToRest` off. RoamArea: Wander con `LeashCentre ← LeaderAnchor`, `WanderRadius` 750,
`MaxLeashFromLeader` 750, `WanderSpeed` 170, `DwellTime` 2.5.

Trampas: `run()` de `execute_tool_script` tiene que devolver un dict, no una lista. En C++, los
nombres `Error` y `MakeError` chocan con símbolos del motor (`TValueOrError`).

---

## 6. Diffs

Cada cambio tiene su `.htm` en `D:/Multiplayer_GAS/Diffs/<fecha>_<tema>/changes.htm`.
Los de hoy, por orden: `investigar-por-su-cuenta`, `editor-de-mapa`, `colisiones-y-mover`,
`pathfinding-en-la-web`, `mirada-al-pararse`, `laberintos-y-export`, `toolset-para-el-editor`,
`suelo-foso-y-piloto`, `piloto-ia`, `piloto-jugador`, `piloto-tour`,
`modos-de-juego-y-portapapeles`, `push-a-unreal`.
