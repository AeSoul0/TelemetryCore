# Protocollo seriale TelemetryCore — v1

Canale: UART 115200 8N1 (sulla NUCLEO è l'USART2, esposta su USB come porta COM virtuale
dall'ST-LINK). Nel simulatore PC gli stessi byte escono su `stdout`.

## Frame

```text
$<payload>*<checksum>\r\n
```

| Campo | Regola |
|---|---|
| `$` | inizio frame |
| `payload` | ASCII, campi separati da `,`, max 160 caratteri |
| `*` | separatore del checksum |
| `checksum` | XOR di tutti i byte del payload (tra `$` e `*`), 2 cifre hex maiuscole |
| `\r\n` | fine frame |

`$`, `*`, CR e LF sono riservati: se compaiono nel payload il firmware li sostituisce con `_`.

Esempio: il payload `A` (0x41) produce `$A*41\r\n`.

Implementazioni: `platform/src/frame.cpp` (firmware) e `tools/ground_station/telemetry_protocol.py`
(ground station). Entrambe vengono testate con gli stessi vettori.

## Tipi di payload

### `BOOT` — primo frame dopo il reset

```text
BOOT,fw=<versione>,board=<nome>
$BOOT,fw=0.4.0,board=nucleo-f446re*..
```

Ricevuto un `BOOT`, la ground station ricomincia a contare i numeri di sequenza.
Dalla milestone M8 conterrà anche la causa del reset (`reset=IWDG`, ...).

### `TLM` — telemetria, ogni 100 ms

```text
TLM,<seq>,<uptime_ms>,<temp_C>,<press_hPa>,<batt_pct>,<status>,<fault_flags>
$TLM,42,4200,23.6,1013.3,87,OK,00*..
```

| Campo | Tipo | Note |
|---|---|---|
| `seq` | intero, parte da 0 | un salto indica pacchetti persi |
| `uptime_ms` | intero | istante dell'acquisizione (tick del sistema) |
| `temp_C` | 1 decimale | ultimo valore valido se il sensore è in errore |
| `press_hPa` | 1 decimale | come sopra |
| `batt_pct` | intero 0–100 | |
| `status` | `OK` / `WARNING` / `CRITICAL` | vedi sotto |
| `fault_flags` | 2 cifre hex | maschera di bit, vedi sotto |

I decimali sono formattati in virgola fissa (`lround(x * 10)`), quindi il firmware non
ha bisogno del supporto `%f` di newlib-nano.

**Stato** (calcolato da `TelemetryProcessor`):

| Condizione | Stato |
|---|---|
| temperatura ≥ 80 °C oppure batteria < 5 % | `CRITICAL` |
| temperatura ≥ 50 °C oppure batteria < 20 % | `WARNING` |
| campione mancante o non plausibile (dopo 3 tentativi) | `WARNING` |
| 10 campioni consecutivi mancanti o non plausibili (1 s) | `CRITICAL` |
| altrimenti | `OK` |

**Fault flags:**

| Bit | Valore | Nome | Significato |
|---|---|---|---|
| 0 | `01` | `SENSOR_ERROR` | il sensore non ha risposto dopo tutti i tentativi |
| 1 | `02` | `INVALID_DATA` | valore ricevuto ma non plausibile (NaN, fuori range) |
| 2 | `04` | `QUEUE_OVERFLOW` | campioni o pacchetti scartati dall'ultimo pacchetto |
| 3 | `08` | `BATTERY_LOW` | batteria sotto la soglia di warning |

### `LOG` — log strutturati

```text
LOG,<DEBUG|INFO|WARN|ERROR>,<componente>,<messaggio>
$LOG,WARN,SensorTask,sensor not responding after 3 attempts*..
```

Il messaggio può contenere virgole: il parser divide solo i primi tre campi.

## Comandi PC → STM32 (milestone M6)

Riservati per la prossima fase, stesso framing:

```text
$CMD,HELLO*..        ->  $ACK,HELLO*..
$CMD,FAULT,<tipo>*.. ->  fault injection (milestone M8)
```
