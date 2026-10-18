# Roadmap STM32 — da simulatore a STM32 Telemetry Platform

> Documento di pianificazione per il passaggio di TelemetryCore dalla simulazione su PC
> a un firmware reale su STM32, fino al prodotto finale descritto nella Fase 35 della
> roadmap originale.

---

## 1. L'obiettivo finale (Definition of Done del progetto)

Il progetto è "finito" quando esiste questo sistema, funzionante su una board vera:

```text
 ┌────────────────────────── STM32 (Cortex-M4F) ──────────────────────────┐
 │                                                                         │
 │  BME280 ──I²C──► SensorTask ──Q1──► ProcessingTask ──Q2──► CommTask ──┐ │
 │  Potenziometro ─ADC─┘   (100 ms)        (eventi)              (eventi) │ │
 │                                                                       │ │
 │  Pulsante/Timer ─IRQ─► Semaphore ─► EventTask         UART + DMA ◄────┘ │
 │                                                            │            │
 │  SupervisorTask ──► IWDG (watchdog)   FaultManager   Logger│            │
 └────────────────────────────────────────────────────────────┼────────────┘
                                                              │ USB (ST-LINK VCP)
                                                              ▼
                                                 Ground Station (Python)
                                       Temperatura · Pressione · Batteria · Stato · Uptime
```

Checklist dell'obiettivo (dalla Fase 35 + 36):

| Componente | Significato concreto |
|---|---|
| sensor drivers | Driver BME280 su I²C reale + ADC per la batteria, dietro le stesse interfacce del mock |
| FreeRTOS | Kernel FreeRTOS vero, task statici con priorità e stack dimensionati |
| acquisition / processing / telemetry | La logica applicativa attuale, **invariata o quasi** |
| fault management | Retry sensore, stati degradati, gestione queue piena, HardFault handler |
| UART | Frame di telemetria con checksum, comandi dal PC (HELLO/ACK, fault injection) |
| watchdog | IWDG alimentato da un supervisore che controlla l'heartbeat di tutti i task |
| logging | Livelli compile-time, thread-safe, senza `iostream` |
| tests | Unit test su host + test d'integrazione (opzionale: Renode in CI) |
| CI | Build host + test + static analysis + build firmware ARM con artefatti `.elf/.hex/.map` |
| ground station | Tool Python che legge la seriale e mostra i dati in tempo reale |
| documentazione | README "da portfolio": hardware, flashing, fault handling, design decisions |

**Prova che l'architettura era buona (Fase 28):** `app/` deve compilare identica per PC e per
STM32. Tutto ciò che cambia tra le due piattaforme vive in `platform/` e `drivers/`.

---

## 2. Analisi dello stato attuale

### 2.1 Cosa funziona già bene

- **Separazione a livelli** `app/ drivers/ platform/` già impostata: è la base giusta.
- **Interfaccia sensore astratta** (`TemperatureSensor`) con mock che inietta anomalie.
- **Pipeline Producer → Queue → Consumer** con due queue, mutex UART, semaforo "da ISR".
- **Shutdown pulito** dei task in simulazione (`abort()` sulle primitive bloccanti).
- **Build host verificata**: compila senza warning con `-Wall -Wextra -Wpedantic` e l'output è corretto.
- Toolchain ARM GNU 14.3 già installata sulla macchina.

### 2.2 Gap reali (verificati sul codice)

Il README dice "Fase 22 completata", ma alcune fasi sono completate solo in parte. Conviene
chiuderle **prima** dell'hardware, perché sull'STM32 costano molto di più.

| # | Gap | Dove | Fase originale |
|---|---|---|---|
| G1 | **La cross-compilazione ARM non funziona**: `rtos_sim.h` usa `std::thread/mutex/condition_variable`, che non esistono su `arm-none-eabi` → errori di compilazione. Mancano anche startup code, linker script, syscalls, vector table. | `platform/include/rtos_sim.h`, `toolchain-arm-none-eabi.cmake` | 22 |
| G2 | **Nessun unit test**: `tests/` è vuota; in CI "Run Tests" esegue solo il programma per 3 secondi. | `tests/`, `.github/workflows/build.yml` | 6, 20 |
| G3 | **Error handling solo dichiarato**: `SENSOR_ERROR`, `INVALID_DATA`, ecc. non sono mai usati; il sensore restituisce `float` senza possibilità di errore; nessuna policy retry → WARNING → CRITICAL. | `drivers/include/sensor.h:19`, `platform/include/error_codes.h` | 16 |
| G4 | **Logger non adatto all'MCU**: usa `std::cout` e `std::string`, non è thread-safe di suo (in `tasks.cpp:47` e `:99` si logga senza mutex), non si possono disabilitare i livelli verbosi. | `platform/include/logger.h` | 17 |
| G5 | **Serializzazione con heap**: `std::ostringstream` + `std::string` → allocazioni dinamiche e decine di KB di flash su ARM. Manca il timestamp nel frame e non c'è checksum. | `app/src/telemetry.cpp:26` | 4, 11 |
| G6 | **Allocazione dinamica ovunque**: `std::unique_ptr<Task>`, `std::function`, `std::queue`. Su MCU vogliamo allocazione statica (`xTaskCreateStatic`, `xQueueCreateStatic`). | `app/src/tasks.cpp:26-30`, `rtos_sim.h` | 32 |
| G7 | **Timing non reale**: il timestamp è un contatore `+= 100`, non il tick di sistema; `delay(100)` è relativo (deriva nel tempo) invece di `vTaskDelayUntil`; nessuna priorità, nessun timeout sulle queue; il timing non è mai stato misurato. | `app/src/tasks.cpp:36-52` | 7, 9, 12 |
| G8 | **CMake non pronto per firmware**: `project(... CXX)` senza C/ASM, `include_directories` globali, nessun target separato per test/firmware, `-Map=${CMAKE_PROJECT_NAME}.map` valutato prima di `project()` (nome vuoto), `CMAKE_ASM_COMPILER` dovrebbe essere `arm-none-eabi-gcc`, nessuna `--specs=nano.specs`. | `CMakeLists.txt`, `toolchain-arm-none-eabi.cmake` | 18, 22 |
| G9 | **CI incompleta**: niente static analysis, niente test veri, niente build ARM. `.clang-tidy` con `Checks: '*'` è troppo rumoroso per essere usato davvero. | `.github/workflows/build.yml`, `.clang-tidy` | 19, 20 |
| G10 | `main()` termina dopo 3 s: un firmware non ritorna mai. Serve separare "durata simulazione" da "entry point firmware". | `app/src/main.cpp` | 24 |
| G11 | Minori: `BitUtils` solo su `uint8_t` (i registri STM32 sono a 32 bit; `1 << 31` su `int` è UB); `Mutex` senza RAII (lock/unlock manuali); badge CI del README punta a `samue/TelemetryCore` invece di `AeSoul0/TelemetryCore`; la roadmap originale è in `.gitignore` e quindi non fa parte del portfolio. | vari | — |

---

## 3. Decisioni architetturali

### 3.1 Hardware consigliato

| Componente | Scelta | Perché |
|---|---|---|
| Board | **NUCLEO-F446RE** (in alternativa NUCLEO-F401RE) | Cortex-M4F come già previsto dal toolchain file; ST-LINK integrato (flash + debug + seriale virtuale su USB); economica; documentazione enorme |
| Sensore | **Modulo BME280** (I²C) | Fornisce **temperatura e pressione**: il campo `pressure`, oggi finto (`1013.25f`), diventa reale |
| Batteria | Potenziometro 10 kΩ su un pin ADC (es. PA0) | Simula una batteria che si scarica: permette di testare soglie e allarmi a mano |
| Extra | Breadboard, cavetti dupont, analizzatore logico economico (8 ch, 24 MHz) | L'analizzatore logico è il modo più veloce per debuggare I²C e UART |

Pin di riferimento sulla NUCLEO-F446RE: USART2 (PA2/PA3) collegata al Virtual COM Port dell'ST-LINK,
LED utente LD2 su PA5, pulsante B1 su PC13, I²C1 su PB8 (SCL) / PB9 (SDA) — pin Arduino D15/D14.

> ⚠️ Molti moduli venduti come "BME280" montano in realtà un BMP280 (niente umidità). Il driver
> deve leggere il registro chip-ID (`0xD0`: `0x60` = BME280, `0x58` = BMP280) e comportarsi di conseguenza.

### 3.2 Architettura software target

```text
TelemetryCore/
├── app/                      # IDENTICO su PC e STM32 — non include mai HAL o <thread>
│   ├── include/  src/        # tasks, telemetry processor, fault manager, protocollo
├── platform/
│   ├── include/osal/         # API OS astratta: Task, Queue<T,N>, Mutex, LockGuard, BinarySemaphore, clock
│   ├── osal_host/            # backend std::thread (l'attuale rtos_sim.h, rifattorizzato)
│   ├── osal_freertos/        # backend FreeRTOS nativo (xTaskCreateStatic, xQueueCreateStatic, ...)
│   └── include/              # logger, error_codes, bit_utils
├── drivers/
│   ├── include/              # interfacce: EnvSensor, SerialPort, I2cBus, AdcChannel, Watchdog
│   ├── mock/                 # MockEnvSensor, StdoutSerial, MockWatchdog (per PC e test)
│   └── stm32/                # Bme280Sensor, Stm32Uart(DMA), Stm32I2c, Stm32Adc, Stm32Iwdg
├── bsp/nucleo_f446re/        # codice generato da CubeMX: startup, linker script, clock, MSP init, FreeRTOSConfig.h
├── third_party/              # FreeRTOS-Kernel, CMSIS, STM32 HAL (submodule o generati da CubeMX)
├── tests/                    # unit test host (GoogleTest o doctest via FetchContent)
├── tools/ground_station/     # Python: lettura seriale, parsing frame, dashboard
├── docs/                     # questa roadmap, protocollo, analisi real-time, design decisions
└── CMakePresets.json         # preset: host-debug, host-tests, stm32-debug, stm32-release
```

Principi:

1. **Dependency rule**: `app → interfacce (drivers/include, platform/include/osal)`. Mai il contrario.
   Il target `app` in CMake non deve vedere né `stm32f4xx_hal.h` né `<thread>`.
2. **Zero heap dopo l'avvio**: tutte le queue, i task e i buffer sono statici. Su FreeRTOS si usa
   `configSUPPORT_STATIC_ALLOCATION=1` e, idealmente, `configSUPPORT_DYNAMIC_ALLOCATION=0`.
3. **C++17 "embedded subset"**: `-fno-exceptions -fno-rtti`, niente `std::string`, `iostream`,
   `std::function` nel percorso firmware. Le funzioni virtuali vanno bene.
4. **FreeRTOS API nativa** nel backend OSAL (non CMSIS-RTOS v2): è quella che la roadmap vuole
   insegnare, e l'OSAL è già il tuo livello di astrazione.
5. **CubeMX solo come generatore** di init/clock/pinout (export CMake), non come IDE obbligatorio:
   il progetto resta buildabile da CLion/CMake e in CI.

### 3.3 Interfacce chiave (bozza)

```cpp
// platform/include/osal/osal.h
namespace osal {
    template <typename T, size_t N> class Queue {          // storage statico interno
    public:
        ErrorCode send(const T& item, uint32_t timeoutMs);
        ErrorCode sendFromIsr(const T& item);
        ErrorCode receive(T& item, uint32_t timeoutMs);
    };
    class BinarySemaphore { public: void give(); void giveFromIsr(); ErrorCode take(uint32_t timeoutMs); };
    class Mutex { public: void lock(); void unlock(); };
    class LockGuard { /* RAII su Mutex */ };
    uint32_t nowMs();
    void delayUntil(uint32_t& lastWakeMs, uint32_t periodMs);
}

// drivers/include/env_sensor.h
struct SensorReading { uint32_t timestampMs; float temperatureC; float pressureHpa; };
class EnvSensor {
public:
    virtual ~EnvSensor() = default;
    virtual ErrorCode init() = 0;
    virtual ErrorCode read(SensorReading& out) = 0;          // può fallire → SENSOR_ERROR
};

// drivers/include/serial_port.h
class SerialPort {
public:
    virtual ~SerialPort() = default;
    virtual ErrorCode write(const uint8_t* data, size_t len, uint32_t timeoutMs) = 0;
    virtual size_t read(uint8_t* data, size_t maxLen) = 0;   // non bloccante, da ring buffer RX
};
```

### 3.4 Protocollo di telemetria v1

Testo leggibile (facile da debuggare con un terminale seriale) + checksum per rilevare frame corrotti:

```text
$TLM,<seq>,<uptime_ms>,<temp_C>,<press_hPa>,<batt_pct>,<status>,<fault_flags_hex>*<XOR8_hex>\r\n

$TLM,42,4200,23.6,1013.2,87,OK,00*5A
$BOOT,reset=IWDG,fw=1.0.0*3C           # inviato all'avvio, riporta la causa dell'ultimo reset
$LOG,WARN,SensorTask,retry 2/3*1F      # log strutturati sullo stesso canale
```

Comandi dal PC: `$CMD,HELLO` → `$ACK,HELLO`; `$CMD,FAULT,<tipo>` per la fault injection (Fase 30).

> Formattare i float con `snprintf("%f")` su newlib-nano richiede `-u _printf_float` (+ circa 10 KB di flash).
> Alternativa più "embedded": rappresentare temperatura e pressione in virgola fissa (centesimi di °C,
> decimi di hPa — il BME280 restituisce già interi in questo formato) e formattarli con aritmetica intera.

### 3.5 Task, priorità e periodi iniziali (da validare nella Fase 33)

| Task | Periodo / trigger | Priorità | Stack iniziale | Note |
|---|---|---|---|---|
| SupervisorTask | 250 ms | massima | 256 word | Ricarica l'IWDG solo se tutti i task hanno dato heartbeat |
| EventTask | semaforo da ISR | alta | 256 word | Reazione al pulsante B1 / evento timer |
| SensorTask | 100 ms (`vTaskDelayUntil`) | media-alta | 384 word | Lettura I²C + ADC, retry su errore |
| ProcessingTask | queue Q1 | media | 384 word | Validazione, stato, fault flags |
| CommTask | queue Q2 + RX comandi | media-bassa | 512 word | Serializzazione + UART DMA |
| Idle | — | 0 | minimo | Hook per misurare il carico CPU |

---

## 4. Roadmap per milestone

Ogni milestone ha: obiettivo, attività, criterio di completamento ("Fatto quando…") e cosa devi saper
spiegare prima di passare oltre (la regola della tua roadmap originale resta valida).

Dimensione indicativa: **S** = una sessione, **M** = qualche sessione, **L** = una settimana o più di lavoro serale.

### 🟢 Fase A — Senza board ✅ completata

> Stato: M0–M3 implementate. Verificato in locale: build PC con `-Werror`, 53 test C++ e 14 test Python
> verdi, clang-format e clang-tidy puliti, firmware Debug e Release linkati (Release: 19.6 KB flash,
> 11.8 KB RAM). **Non ancora verificato:** la CI su GitHub (non è stato fatto push) e il firmware su
> una board reale (arriva con M4/M5).

#### M0 · Chiudere i debiti del simulatore — ✅
Fasi originali: 6, 9, 12, 16, 17 · Gap: G2, G3, G4, G5, G7, G11

- [x] Unit test con GoogleTest via `FetchContent` + `ctest` (`tests/`): soglie esatte, NaN, ±inf,
      −20 °C, serializzazione, mock, acquisizione, OSAL, logger, frame, più un test d'integrazione
      dell'intera pipeline (`tests/test_pipeline.cpp`).
- [x] Interfaccia sensore `ErrorCode read(SensorReading&)` (`drivers/include/env_sensor.h`); policy della
      Fase 16: retry ×3 (`app/src/acquisition.cpp`) → WARNING → CRITICAL dopo 10 cicli
      (`app/src/telemetry.cpp`). Il mock simula guasti, scollegamento e dati corrotti.
- [x] Logger con livelli compile-time (`TELEMETRY_LOG_LEVEL`) e runtime, buffer fisso, niente
      `std::string`. *Differenza dal piano:* il mutex non è nel logger ma in `app::Uplink`, l'unico
      proprietario della seriale, così log e telemetria non si mescolano mai sul filo.
- [x] Serializzazione senza heap e senza `%f`, formato v1 (`docs/protocol.md`) con seq, timestamp, checksum.
- [x] Timestamp da `osal::nowMs()`, periodo con `delayUntil`, timeout su queue e semafori.
- [x] `Mutex` + `LockGuard` RAII; `BitUtils` template su qualunque intero senza segno.

**Fatto quando:** `ctest` passa, nessun `new`/`std::string`/`iostream` sotto `app/`, l'output del
simulatore è nel formato v1.
**Devi saper spiegare:** perché l'heap è pericoloso in un firmware; differenza tra delay relativo e assoluto.

#### M1 · OSAL e build multi-target — ✅
Fasi originali: 18, 22 · Gap: G1, G6, G8, G10

- [x] `platform/include/osal/osal.h` (API unica) + `platform/osal_host/` (std::thread) +
      `platform/osal_freertos/` (FreeRTOS nativo, API `*Static`). `Queue<T, N>` con storage interno.
- [x] CMake a target: `platform_core`, `osal`, `drivers_*`, `app_core`, `telemetry_sim`, `unit_tests`,
      `telemetry_fw`; niente `include_directories` globali.
- [x] `CMakePresets.json`: `host-debug` (con test), `host-release`, `stm32-debug`, `stm32-release`.
- [x] Toolchain in `cmake/toolchain-arm-none-eabi.cmake`: C/C++/ASM, nano + nosys specs, map file,
      post-build `.hex/.bin` + report dimensioni.
- [x] Entry point separati: `bsp/host/main_host.cpp` e `bsp/nucleo_f446re/src/main.cpp`.

**Fatto quando:** `app_core` compila **senza modifiche** sia con g++ host sia con `arm-none-eabi-g++`. ✔
**Devi saper spiegare:** host vs target, cosa fa il linker script, cosa contiene la vector table.

#### M2 · Firmware "a vuoto" + CI completa — ✅ (CI da verificare al primo push)
Fasi originali: 19, 20, 22, 23

- [x] BSP NUCLEO-F446RE in `bsp/nucleo_f446re/`: clock 180 MHz da HSI, USART2, LED, pulsante B1 su
      EXTI, timebase HAL su TIM6, FreeRTOS. *Differenza dal piano:* invece di CubeMX il codice di init è
      scritto a mano e HAL/CMSIS/FreeRTOS sono scaricati da CMake a versioni bloccate
      (`cmake/stm32_dependencies.cmake`): si builda da riga di comando e in CI senza GUI.
      I²C, ADC e IWDG si aggiungono con M7 e M8, quando servono davvero.
- [x] Backend `osal_freertos` linkato, `.elf` completo con la pipeline intera (sensore mock).
- [x] CI (`.github/workflows/build.yml`): build PC + test + smoke test del simulatore; regola
      d'architettura su `app/`, clang-format, clang-tidy, cppcheck; firmware Debug/Release con artefatti
      e report dimensioni; test della ground station.
- [ ] *(Opzionale)* Renode in CI — non fatto.

**Fatto quando:** ogni push produce un firmware scaricabile dalla CI e la CI è verde.
**Devi saper spiegare:** Application → FreeRTOS → HAL → CMSIS → Cortex-M → hardware (Fase 23).

#### M3 · Ground Station v1 — ✅
Fase originale: 29

- [x] `tools/ground_station/`: legge seriale (`--port`), pipe (`--stdin`) o file (`--replay`); valida il
      checksum, conta i frame persi tramite `seq`, mostra stato/temperatura/pressione/batteria/uptime.
- [x] Senza board: `telemetry_sim --forever | python tools/ground_station/ground_station.py --stdin`.
- [x] Dashboard da terminale senza dipendenze (sparkline temperatura, barra batteria, ultimi log),
      log CSV (`--csv`), modalità `--check` usata dalla CI.

**Fatto quando:** la ground station visualizza i dati del simulatore e segnala frame corrotti. ✔

### 🔵 Fase B — Con la board

#### M4 · Primo firmware STM32, passo per passo — *M*
Fase originale: 24 · **non saltare passaggi**

1. Blink LD2 con `HAL_Delay` → poi con un timer hardware.
2. UART: `printf` sulla seriale virtuale (retarget di `_write`), poi echo dei caratteri.
3. Interrupt: pulsante B1 su EXTI → toggle LED nella ISR.
4. FreeRTOS: due task che lampeggiano a frequenze diverse.
5. GPIO + timer + RTOS insieme: ISR del pulsante → `xSemaphoreGiveFromISR` → task.

**Fatto quando:** il punto 5 funziona e sai debuggare con breakpoint (ST-LINK + OpenOCD/CLion o STM32CubeIDE).
**Devi saper spiegare:** priorità NVIC e `configMAX_SYSCALL_INTERRUPT_PRIORITY`; perché nelle ISR
si usano solo le API `...FromISR`; `portYIELD_FROM_ISR`.

#### M5 · La pipeline gira sulla board con il sensore MOCK — *S*
Fase originale: 28 (anticipata)

- [ ] Flashare il firmware di M2: stessi task, stesso `app_core`, `MockEnvSensor`, output su UART reale.
- [ ] `EventTask` collegato al pulsante B1 vero (sostituisce `isrSimulationFn`).

**Fatto quando:** la ground station mostra la stessa telemetria del simulatore, ma arrivando dalla STM32.
👉 Questo è il momento in cui dimostri che l'architettura regge: **nessuna riga di `app/` è cambiata.**

#### M6 · UART reale e protocollo bidirezionale — *M*
Fase originale: 25

- [ ] Driver `Stm32Uart`: TX con mutex, RX in interrupt verso un ring buffer (lock-free, singolo produttore/consumatore).
- [ ] Parser comandi in `CommTask`: `$CMD,HELLO` → `$ACK,HELLO`.
- [ ] Gestione errori UART (overrun, framing) → `COMMUNICATION_ERROR` + contatore nei fault flags.

**Fatto quando:** PC e STM32 dialogano in entrambe le direzioni senza perdere caratteri.

#### M7 · I²C, driver BME280 e ADC batteria — *L*
Fasi originali: 26, 27

- [ ] Studiare I²C con l'analizzatore logico: START, indirizzo (`0x76`/`0x77`), ACK/NACK, lettura registri.
- [ ] Driver `Stm32I2c` (interfaccia `I2cBus`, con timeout) e `Bme280Sensor` (implementa `EnvSensor`):
      lettura chip-ID, lettura dei coefficienti di calibrazione, formule di compensazione del datasheet.
- [ ] **Unit test del driver su PC** con un `MockI2cBus` che restituisce i byte di registro: verifichi la
      matematica di compensazione senza hardware.
- [ ] `Stm32Adc` + conversione tensione → percentuale batteria (potenziometro su PA0).
- [ ] Sostituire il mock nel `main_stm32.cpp`. Solo lì.

**Fatto quando:** temperatura, pressione e batteria sono reali; scaldando il sensore con le dita la
temperatura sale; girando il potenziometro la batteria cambia e scatta l'allarme.

### 🟣 Fase C — Robustezza (qui diventa un sistema "serio")

#### M8 · Fault management + Watchdog — *M*
Fasi originali: 30, 31

- [ ] `FaultManager` in `app/`: raccoglie errori, mantiene i `fault_flags`, decide lo stato
      (OK / DEGRADED / WARNING / CRITICAL) e lo "stato sicuro".
- [ ] Watchdog: `SupervisorTask` ricarica l'IWDG (timeout ~1 s) solo se **tutti** i task hanno
      segnalato un heartbeat nella finestra; altrimenti lascia scadere → reset.
- [ ] All'avvio leggere la causa di reset (`RCC->CSR`) e inviarla nel frame `$BOOT`.
- [ ] HardFault handler che salva i registri principali in una sezione RAM `.noinit` e li riporta al boot successivo.
- [ ] Scenari di fault injection (comando `$CMD,FAULT,...` o fisici), ognuno con reazione attesa e documentata:

| Scenario | Come lo provochi | Reazione attesa |
|---|---|---|
| Sensore scollegato | stacchi SDA/SCL | retry ×3 → flag `SENSOR_ERROR` → stato DEGRADED; recupero automatico al ricollegamento |
| Temperatura fuori range | aria calda / comando | WARNING / CRITICAL + log |
| Queue piena | comando che rallenta CommTask | drop contato in `fault_flags`, nessun blocco |
| UART timeout | comando | errore loggato, TX ripresa |
| Dato corrotto | ground station invia frame con checksum errato | frame scartato, contatore errori |
| Task bloccato | comando che fa `while(1)` in un task | watchdog → reset → `$BOOT,reset=IWDG` |

**Fatto quando:** ogni riga della tabella è verificata a mano (e quelle possibili, in test automatici).

#### M9 · Analisi di memoria e real-time — *M*
Fasi originali: 32, 33

- [ ] Memoria: report RAM/Flash da `.map` + `arm-none-eabi-size` in CI; `uxTaskGetStackHighWaterMark`
      per ogni task; `configCHECK_FOR_STACK_OVERFLOW=2` + hook; `configUSE_MALLOC_FAILED_HOOK`.
      Ridimensionare gli stack con un margine documentato.
- [ ] Real-time: misurare il WCET di ogni task con il contatore di cicli `DWT->CYCCNT`; misurare il
      jitter di SensorTask con un GPIO e l'analizzatore logico; calcolare l'utilizzo U = Σ Cᵢ/Tᵢ e
      verificarlo con la Rate Monotonic Analysis.
- [ ] Scrivere `docs/realtime_analysis.md` con la tabella periodo / deadline / WCET / priorità.

**Fatto quando:** puoi dimostrare con numeri misurati che tutte le deadline sono rispettate.

#### M10 · DMA — *S/M*
Fase originale: 34

- [ ] UART TX via DMA: `CommTask` prepara il frame e attende la callback di fine trasferimento
      (semaforo dalla ISR del DMA) invece di aspettare byte per byte.
- [ ] *(Opzionale)* RX con DMA circolare + IDLE line interrupt; I²C via DMA.
- [ ] Rimisurare il carico CPU (M9) e documentare il guadagno prima/dopo.

**Fatto quando:** il carico CPU di CommTask scende in modo misurabile.

### 🏁 Fase D — Prodotto finale

#### M11 · Release v1.0 e documentazione — *M*
Fasi originali: 35, 36

- [ ] README riscritto: Overview, Hardware (schema dei collegamenti + foto), Architecture, Data flow,
      Build, **Flashing** (ST-LINK / `STM32_Programmer_CLI` / OpenOCD), Testing, Fault handling,
      **Design decisions** (queue vs memoria condivisa, perché l'astrazione, perché zero-heap, perché il watchdog supervisionato).
- [ ] `docs/protocol.md` (specifica dei frame), `docs/realtime_analysis.md`, eventuali ADR.
- [ ] Versionare la roadmap di studio (oggi è in `.gitignore`), correggere il badge CI.
- [ ] GIF/video: board + ground station + fault injection + reset da watchdog.
- [ ] Tag `v1.0.0` con il firmware `.hex` allegato alla GitHub Release.

**Fatto quando:** uno sconosciuto può clonare il repo, comprare i componenti della §3.1, flashare e
vedere la telemetria sulla ground station seguendo solo il README.

---

## 5. Vista d'insieme

```text
Fase A (senza board)          Fase B (con board)            Fase C (robustezza)        Fase D
─────────────────────         ──────────────────            ───────────────────        ──────
M0 debiti simulatore ─┐       M4 primo firmware ─┐          M8 fault + watchdog ─┐     M11 v1.0
M1 OSAL + build ──────┼─────► M5 pipeline+mock ──┼────────► M9 memoria + RT ─────┼───► docs
M2 firmware + CI ─────┘       M6 UART ───────────┤          M10 DMA ─────────────┘     release
M3 ground station ───────────────────────────────┘ (usata da M5 in poi)
```

Corrispondenza con la roadmap originale:

| Fase originale | Milestone |
|---|---|
| 6, 9, 12, 16, 17 (completamento) | M0 |
| 18, 22 (completamento) | M1 |
| 19, 20, 23 | M2 |
| 24 | M4 |
| 25 | M6 |
| 26, 27 | M7 |
| 28 | M5 (con mock) + M7 (con sensore reale) |
| 29 | M3 |
| 30, 31 | M8 |
| 32, 33 | M9 |
| 34 | M10 |
| 35, 36 | M11 |

## 6. Trappole tipiche da tenere a mente

- **HAL timebase su SysTick + FreeRTOS** → conflitto. In CubeMX scegli un TIM (es. TIM6) come timebase HAL.
- **Chiamare API FreeRTOS non-ISR da un interrupt**, o da un interrupt con priorità numericamente
  più bassa di `configMAX_SYSCALL_INTERRUPT_PRIORITY` → crash casuali difficilissimi da trovare.
- **`printf` con float** non stampa nulla su newlib-nano senza `-u _printf_float`.
- **Oggetti globali con costruttori** che usano FreeRTOS prima di `vTaskStartScheduler()`: crea le
  primitive in `startTasks()` o usa storage statico con init esplicito.
- **Stack troppo piccoli** → corruzione silenziosa. Attiva subito il controllo overflow (M9 non deve essere la prima volta).
- **I²C bloccato** (slave che tiene SDA bassa dopo un reset): prevedi il bus recovery (9 clock su SCL) nel driver.
- **Rigenerare con CubeMX** sovrascrive il codice fuori dalle sezioni `USER CODE BEGIN/END`: tieni la tua logica in `bsp/`, `drivers/stm32/`, non nei file generati.
