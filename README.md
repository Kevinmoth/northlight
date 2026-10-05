# Northlight renderer

Una extensión de Direct3D 9 para el cliente de World of Warcraft 3.3.5a: un `d3d9.dll` proxy
(compilado como `frd9.dll`) que añade iluminación, sombras, niebla, GI y efectos de cielo. El repositorio
también contiene los generadores de recursos offline (parches MPQ, caché del mundo) y su validación.

El repositorio no contiene archivos del juego: toda herramienta que necesite datos del juego los lee de tu
propio cliente 3.3.5a. Licenciado bajo la [Licencia MIT](LICENSE). (El original en inglés está en
[README.en.md](README.en.md).)

El changelog, las notas de versión y los registros de validación no forman parte del código público.

## Qué añade Northlight

Todos los efectos de esta lista vienen activados por defecto y se dibujan sobre el propio fotograma del juego.

- **Sol y luna.** Discos propios de sol y luna en una órbita que sigue el reloj del juego; los
  billboards de sol y luna del juego quedan ocultos. El sol resplandece con el propio color de sol del juego para
  la zona y la hora, y un velo suaviza árboles, torres y crestas que haya frente a él; la niebla y la bruma del
  horizonte toman ese mismo tono hacia el sol.
- **Sombras.** Sombras de sol y de luna en dos cascadas (unos 48 y 192 yardas alrededor del jugador), con
  una capa estática en caché para el terreno, los edificios, los árboles y el atrezzo, y sombras de terreno lejano
  hasta 928 yardas (4096 en las zonas listadas en `shadow-range-profiles.ini`). Los personajes, criaturas,
  monturas, puertas, barcos y demás objetos en movimiento también proyectan sombra. Las sombras precalculadas del
  terreno del juego y las sombras blob redondas bajo los personajes quedan reemplazadas, y donde el sol está
  bloqueado la luz solar pintada del juego se elimina suavemente, sin escalones entre facetas.
- **Iluminación global.** Luz de cielo y luz rebotada desde sondas, trazada por rayos contra la geometría del
  mundo en un hilo en segundo plano, en unas 76 yardas alrededor de la cámara (52 en los presets Balanced y
  Performance); los personajes añaden su propio rebote y oclusión (preset Quality).
- **Oclusión ambiental.** AO en espacio de pantalla con sombreado de contacto y un bloom ligero.
- **Niebla y aire.** Luz volumétrica de sol y luna con haces de luz entre las sombras; bruma suave sobre el
  paisaje lejano y el cielo más bajo con el propio color de niebla del juego; niebla de suelo regional en bosques,
  humedales y cuencas, más densa de noche, derivada del mapa.
- **Lámparas.** Hasta 32 lámparas, faroles, braseros y fogatas cercanas iluminan el suelo y las paredes a su
  alrededor y brillan en la niebla. Con sol directo, las lámparas se atenúan. Las sombras de lámpara están apagadas
  por defecto (`PointShadows=1` las activa): solo las luces de interior proyectan sombras tenues, al anochecer y
  de noche; las farolas, faroles y antorchas nunca.
- **Capa de arte de iluminación.** Un parche MPQ (`patch-z`) construido a partir de los `Light*.dbc` de tu
  propio cliente: luz exterior y colores de niebla de cielo despejado retocados, un Mulgore más cálido, niebla
  diurna más densa en Ventormenta. Los modelos de cielo que pintan su propio sol o luna en el cielo despejado lo
  pierden.
- **Agua.** El agua del juego se dibuja sin cambios; una máscara de líquido mantiene el reiluminado y la AO
  fuera de la superficie, y la niebla se mide hasta la superficie del agua.
- **Ajustes.** `northlight-quality.ini` tiene tres presets (Quality, el predeterminado, Balanced y
  Performance) y unas 30 claves para sombras, GI, lámparas y bruma; `celestial-profiles.ini` define el aspecto
  del sol y la luna por zona.
- **Atajos** (con Ctrl+Shift): F7 niebla y bruma, F8 GI, F9 sombras, F10 todos los efectos, F12 vistas de
  depuración (sombras, GI, volumen de niebla). En Mac, quita primero el atajo Control+F7 propio de macOS.
- **Costo.** Northlight cuesta tiempo de fotograma, sobre todo en el hilo principal de CPU del juego. Las
  sombras de personajes son la mayor parte entre multitudes (unos 4-5 ms por fotograma): `ActorShadows=0` en
  `northlight-quality.ini` deja solo las sombras estáticas, y los presets Balanced y Performance cambian pequeños
  detalles por velocidad.
- **Plataformas e instalación.** macOS con WoWSilicon (precargado como `mods/d3d9.dll`) y Windows (un
  `d3d9.dll` en la carpeta del juego sobre el DXVK 2.7.1 incluido, el D3D9 del sistema o un `d3d9.dll`
  existente). El instalador nunca escribe `wow.exe`. Construye la caché del mundo (terreno, modelos, lámparas y
  regiones de niebla) y la capa de arte de iluminación a partir de tu propio cliente en tu máquina: unos 10-40
  minutos y al menos 8 GB de RAM; no se distribuye nada del juego. Los paquetes incluyen su propio Python y
  StormLib, y la desinstalación restaura cada cambio.

## Estructura

| Ruta | Contenido |
|---|---|
| `src/<group>/` | Fuentes de la DLL, agrupadas: `proxy` (el proxy D3D9 y el espejo del dispositivo), `core` (ajustes, memoria, logging, profiling), `world`, `replay`, `shadows`, `gi`, `lights`, `sky`, `water`, `gamedata` (tablas derivadas de los MPQ del juego), `generated` (cabeceras escritas por los generadores y los compiladores de shaders; no editar) |
| `shaders/` | `*.hlsl` y sus manifiestos `*-shader-build.json`; `shaders/compiled/` contiene los `<Entry>.bin` compilados y los `.bin.asm` |
| `scripts/` | `build_renderer.py`, `build_environment.py`, `generate_*.py`, `run_tests.py`, `check_layout.py`, `pe_normalized_hash.py`; `scripts/shaders/` contiene los compiladores de shaders (`compile_*_shaders.py`, `compile_shaders.cpp`, `disassemble_*.cpp`) |
| `renderer/` | `windows-package/`, `mac-package/`, el instalador para jugadores `northlight_install.py`, las herramientas de pipeline y empaquetado (`world_*_builder.py`, `extract_*.py`, `migrate_mac_proxy.py`, `build_packages.py`, `package-pins.json`, ...) y el `frd9.dll` compilado |
| `tests/` | runners de tests (`test_*.py`), tests nativos (`test_*.cpp/.h`), validadores de release (`validate_*.py`, `verify_*.py`) |
| `tests/support/`, `tests/fixtures/` | shims de d3d9.h/windows.h para builds nativos; las líneas base archivadas (fuentes antiguas de este proyecto, digests) contra las que comparan los tests. Los datos reales del cliente (shaders, colocaciones de doodads) los lee de tu cliente `tests/client_fixtures.py`; nunca se almacenan |
| `northlight_paths.py` | el único lugar que sabe dónde están el cliente, el toolchain y las salidas |
| `client-config/` | copias versionadas de los archivos de perfil `.ini` de la raíz del cliente |
| `*.py` (nivel superior) | el generador de la capa de arte (parche MPQ) `build_art_layer.py` y sus pasos, `mpq.py`, `client_archives.py`, `renderer_status.py` (instalar/desinstalar en macOS) |
| `out/` | salida de build y tests (ignorada) |

`tools/` y `backups/` son locales y no están versionados.

## Requisitos

- macOS en Apple Silicon es el host de desarrollo soportado. Linux puede funcionar con zig, clang y
  Wine en el `PATH`, pero no está probado. Windows es solo un objetivo: la DLL funciona allí, pero los
  scripts de desarrollo no.
- Python 3.9 o más nuevo; solo se usa la librería estándar.
- [zig 0.15.2](https://ziglang.org/download/) para compilar la DLL (compila de forma cruzada a Windows de 32 bits).
- clang++ y c++ (herramientas de línea de comandos de Xcode) para los tests nativos.
- Opcional:
  - un cliente de WoW 3.3.5a (datos del juego, caché del mundo);
  - Wine, solo para los compiladores de shaders (`scripts/shaders/compile_*_shaders.py`);
  - StormLib, para leer MPQ;
  - el archivo externo y `backups/`, para tests diferenciales contra versiones antiguas.

## Compilación

```sh
python3 scripts/build_renderer.py           # -> renderer/frd9.dll
python3 scripts/pe_normalized_hash.py renderer/frd9.dll
```

La compilación ejecuta primero la comprobación de estructura (nada en una ruta previa a un movimiento,
nombres base únicos). Regenera las cabeceras de reenvío en `src/generated/`. Comprueba cada
`shaders/*.hlsl` contra su `*-shader-build.json` y se niega a compilar si un shader cambió sin
recompilar. Los bytes en bruto de la DLL dependen del directorio de build, porque las rutas PDB alimentan el
build ID. Compara builds con `pe_normalized_hash.py`, que pone a cero solo esos campos.

Tras editar un shader, recompílalo (Wine, solo el compilador; nada toca el juego):

```sh
python3 scripts/shaders/compile_world_shaders.py   # o compile_shaders, compile_water_shaders, ...
```

Un compilador escribe `shaders/compiled/<Entry>.bin` y `.bin.asm`,
`src/generated/*_compiled_shaders.h` y el manifiesto `shaders/*-shader-build.json`.
`build_environment.py` mantiene el prefijo de Wine fuera de la carpeta del cliente.

## Tests

```sh
python3 scripts/run_tests.py                     # todos los tests/test_*.py, 4 a la vez, nice 15
python3 scripts/run_tests.py test_replay_*       # por patrón; validate_*/verify_* solo si se nombran
python3 scripts/run_tests.py --require client    # un cliente ausente FALLA en vez de SKIP
python3 scripts/run_tests.py --record 0.3.158    # escribe informes en renderer/records/validation-0.3.158
```

Los informes van a `out/test-output/<run>/<test>/`. No se escribe nada en el árbol salvo que pases
`--record` (`renderer/records/` está ignorado por git, y ningún test lee de ahí). Un test declara lo que
necesita en una línea bajo el shebang:

```python
# northlight-test: requires=cxx,client timing
```

| Etiqueta | Significado |
|---|---|
| `cxx` | `clang++`/`c++` del host |
| `client` | un cliente de WoW (ver `NORTHLIGHT_CLIENT`) |
| `zig`, `wine`, `stormlib` | piezas del toolchain (ver abajo) |
| `stormlib-src` | el árbol de fuentes de StormLib fijado en `tools/StormLib-master` (`scripts/build_stormlib.py`) |
| `world-cache` | la caché del mundo del cliente, `<client>/world-cache` (`scripts/install_world_cache.py`) |
| `dll` | un `renderer/frd9.dll` compilado (compilar primero) |
| `backups`, `archive` | tarballs antiguos del renderer / el archivo externo, para tests diferenciales |
| `base` | `BASE=<árbol prístino de un renderer antiguo>` para el control de release |
| `timing` | presupuestos de tiempo de CPU: el test corre solo después de los demás |
| `slow` | se inicia primero |
| `manual` | necesita argumentos; corre solo si se nombra |
| `known-fail=<reason>` | última de la línea; falla en la versión actual por un motivo conocido ([tests/KNOWN_FAILURES.md](tests/KNOWN_FAILURES.md)): sigue ejecutándose, se reporta XFAIL (XPASS si pasa), y no falla la corrida |

Un test cuyo requisito falta se reporta como SKIP con el motivo. `tests/discovery.txt` lista los
tests que el runner debe encontrar. Tras añadir un test, corre con `--update-discovery`.

Sin cliente, toolchain ni archivo puedes igualmente compilar la DLL (con zig) y correr la mayoría de los
tests, que son Python puro o C++ nativo. Los tests que corren sobre datos reales del cliente los leen de tu
cliente a través de `tests/client_fixtures.py`: la piel de cuatro huesos y los programas de shader de una
influencia de los MPQ originales (`client,stormlib`), y colocaciones reales de doodads de la caché del mundo
(`world-cache`).

Unos 50 archivos `tests/test_*.cpp` no tienen runner de Python: ningún runner los ejecuta, y se compilan a
mano como describen sus docs (por ejemplo
[tests/support/gpu_profile/README.md](tests/support/gpu_profile/README.md)). La herramienta de release
`tests/verify_packages.py` comprueba los zips que escribió `renderer/build_packages.py` (ejecútala por su
nombre).

## Configuración de la máquina

Nada en el código nombra una ruta de la máquina. Cada ajuste proviene, en este orden: de una variable de
entorno, luego `northlight.local.ini` (copia [northlight.local.ini.example](northlight.local.ini.example);
git la ignora), luego un valor por defecto. `python3 northlight_paths.py` imprime lo que se resuelve.

| Variable | Por defecto | Se usa para |
|---|---|---|
| `NORTHLIGHT_CLIENT` | la carpeta padre del repositorio, si contiene `Wow.exe` y `Data/` | datos del cliente para tests, validadores y scripts del pipeline, y el cliente donde instala `renderer_status.py` |
| `NORTHLIGHT_ZIG` | `tools/zig-*/zig`, luego `zig` en el `PATH` (debe ser 0.15.2) | build de la DLL, generadores, herramientas de shaders |
| `NORTHLIGHT_WINE` | el Wine incluido con WoWSilicon, luego `wine` en el `PATH` | solo los compiladores de shaders |
| `NORTHLIGHT_STORMLIB` | `tools/storm-build/storm.framework/storm` | `mpq.py` |
| `NORTHLIGHT_TOOLS` | `tools/` | descargas de terceros (DXVK, StormLib, zig) |
| `NORTHLIGHT_ARCHIVE` | ninguno | rollbacks antiguos y diagnósticos de runtime (`<archive>/renderer/`) |
| `NORTHLIGHT_BACKUPS` | `backups/` | tarballs antiguos del renderer |
| `NORTHLIGHT_OUT` | `out/` | salida de build y tests |
| `NORTHLIGHT_TEST_OUTPUT_DIR` | lo fija `run_tests.py` | dónde escribe su informe un test |
| `NORTHLIGHT_SHADER_CORPUS` | ninguno | corpus de shaders capturado, opcional (subcasos extra) |
| `NORTHLIGHT_DOWNLOADS` | ninguno (luego `tools/`) | descargas de runtime fijadas para `renderer/build_packages.py` (`renderer/package-pins.json`) |
| `NORTHLIGHT_LIVE_CLIENT`, `NORTHLIGHT_STOCK_CLIENT`, `NORTHLIGHT_LIGHTS_STAGE`, `BASE` | ninguno | unos pocos tests concretos; ver sus docstrings (`NORTHLIGHT_STOCK_CLIENT`: un cliente 3.3.5a original para los tests de identidad y variantes) |

## Convenciones

- **Los nombres base son el espacio de nombres de los includes.** Los includes de C++ usan nombres
  desnudos (`#include "world_gi.h"`), cada `src/<group>/` aporta un `-I`
  (`northlight_paths.include_flags()`), y Python encuentra las fuentes igual, con
  `northlight_paths.src('world_gi.h')` (fuentes y shaders) o `tracked()` (también scripts y tests).
  Los directorios solo agrupan archivos: mover un archivo entre grupos no exige ninguna edición. Cada
  nombre base debe ser único; `scripts/check_layout.py` lo impone.
- Un test empieza con la línea de etiqueta y estas dos líneas, y luego usa `fp.src(...)`,
  `fp.test_include_flags()`, `fp.output_dir()` y demás:
  ```python
  import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # raíz del repo
  import northlight_paths as fp
  ```
- Los tests nunca escriben junto a las fuentes. Escriben en `fp.output_dir()`.
- `.gitattributes` tiene `* -text`: las puertas de hash comparan bytes en bruto, así que conserva los
  finales de línea LF.
- El hook de pre-commit rechaza archivos de más de 5 MB y payloads del juego o binarios, y ejecuta
  `check_layout.py`. Actívalo en cada clon con `git config core.hooksPath githooks`.
- `git clean -x` también borra las carpetas locales ignoradas (`tools/`, `backups/`, `out/`).

## Instalación en macOS (WoWSilicon)

Con el juego cerrado:

```sh
python3 <repositorio>/renderer_status.py status
python3 <repositorio>/renderer_status.py on      # o: off
python3 <repositorio>/renderer_status.py on --client <carpeta del cliente>
```

El cliente es `--client PATH`; si no, `NORTHLIGHT_CLIENT`; si no, `northlight.local.ini`
`[paths] client`; si no, la carpeta padre del repositorio si contiene `Wow.exe` y `Data/`; sin
ninguna de estas, la herramienta se detiene con un error que las nombra.
`on` instala `renderer/frd9.dll` como `mods/d3d9.dll`, y `off` restaura la transacción
registrada. Añade `--dry-run` para previsualizar. La plantilla del paquete de Windows está en
`renderer/windows-package/`.
