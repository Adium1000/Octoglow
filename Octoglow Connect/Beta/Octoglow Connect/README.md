# Octoglow Connect 2.0

Octoglow Connect este acum hub-ul comun pentru **Octoglow Desk Clock** și
**Octoglow Macro**.

## Ce este implementat

- listă dinamică de dispozitive în meniul din stânga;
- detectare automată Desk Clock în rețeaua locală prin endpoint-ul firmware
  `/authstate`, plus adresa salvată și adresa AP `192.168.4.1`;
- detectare hot-plug pentru Octoglow Macro prin USB HID;
- o pagină Desk Clock cu taburi separate pentru overview, integrări și activity
  log; setările Video/Music Now Playing și Euro Truck Simulator 2 sunt legate de
  acest dispozitiv;
- setările globale controlează doar aplicația: limba, tema, backdrop-ul, pornirea
  cu Windows și comportamentul din system tray;
- o pagină Macro cu VID/PID, revizie USB, layout fizic și highlight live pentru
  cele 7 taste și encoder;
- sender-ul Desk Clock rulează la nivel de aplicație și nu mai depinde de pagina
  deschisă;
- un singur router Win32 pentru tray și Raw Input, pentru a evita conflictele
  dintre hook-urile ferestrei.

## Limita actuală pentru Octoglow Macro

Mapările sunt afișate read-only. Firmware-ul trimite direct acțiunile HID și nu
există în proiect un protocol documentat de configurare (CDC sau HID output).
Aplicația nu deschide și nu hardcodează portul COM expus de placa TinyUSB.

Pentru remapping real, firmware-ul trebuie să trimită ID-ul fizic al tastei și
să ofere comenzi de tip `GET_CONFIG`, `SET_CONFIG` și `SAVE_CONFIG`.

## Build

```powershell
dotnet build .\OctoglowSender.csproj -c Release
```

Executabilul rezultat se numește `OctoglowConnect.exe`.
