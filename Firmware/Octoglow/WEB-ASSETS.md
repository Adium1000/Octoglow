# `build_web.py` — cum ajunge interfața web în firmware

Documentație pentru oricine modifică interfața sau vrea să contribuie.

---

## Regula scurtă

**După orice modificare în `portal.html` sau `auth.html`, rulează:**

```bash
python build_web.py
```

**abia apoi compilezi.** Dacă uiți, nu primești nicio eroare — sketch-ul compilează
liniștit și flash-ezi versiunea veche a interfeței. Ăsta e singurul mod în care
lanțul te poate mușca, și e tăcut.

Obicei recomandat: rulează-l pur și simplu de fiecare dată înainte de compilare,
fără să te întrebi dacă ai schimbat ceva. Dacă nimic nu s-a modificat, îți spune
„era deja la zi, nimic de făcut", nu rescrie fișierul și nu declanșează o
recompilare inutilă. Costă o secundă.

---

## De ce există

Interfața web era înainte un raw string în `Octoglow.ino`:

```cpp
const char PORTAL_HTML[] PROGMEM = R"PORTALHTML( ... 400 KB de HTML ... )PORTALHTML";
```

Cu 404 KB de text brut, pagina ocupa **~23% din tot binarul** și ducea sketch-ul
la 87% din partiția de aplicație. Nu mai era loc de crescut.

Soluția: pagina se comprimă gzip la build și se trimite comprimată, cu antetul
`Content-Encoding: gzip`. Browserul o decomprimă singur, deci utilizatorul
primește exact aceiași octeți ca înainte — dar în flash ocupă 85 KB în loc de 404.

| | înainte | după |
|---|---|---|
| `portal.html` | 404.111 | 84.694 (21,0%) |
| `auth.html` | 13.681 | 4.161 (30,4%) |
| **sketch total** | **1.718.319 (87%)** | **1.389.551 (70%)** |

RAM-ul nu se schimbă: tablourile sunt `PROGMEM`, deci stau în flash, nu în RAM.

---

## Cum funcționează lanțul

```
portal.html  ─┐
              ├─►  build_web.py  ─►  web_assets.h  ─►  #include în Octoglow.ino
auth.html    ─┘      (gzip -9)        (generat)              │
                                                              ▼
                                                      sendGzipHtml()
                                                   Content-Encoding: gzip
                                                              │
                                                              ▼
                                                   browserul decomprimă
```

### Fișierele

| Fișier | Rol | În git? |
|---|---|---|
| `portal.html` | dashboard-ul, **sursa de adevăr** — aici editezi | da |
| `auth.html` | pagina de login | da |
| `build_web.py` | generatorul | da |
| `web_assets.h` | **generat** — nu edita manual | vezi mai jos |

`web_assets.h` este regenerabil integral: șterge-l și `build_web.py` îl reface
identic bit cu bit. Poate sta în `.gitignore` (istoric curat, diff-uri lizibile în
`portal.html`) cu prețul că oricine clonează repo-ul trebuie să ruleze scriptul
înainte de prima compilare — altfel `#include "web_assets.h"` eșuează cu
*No such file or directory*.

---

## Ce conține `web_assets.h`

Nu conține logică, doar date:

```cpp
// portal.html: 404111 -> 84694 bytes
#define PORTAL_HTML_GZ_SRC_SHA "93dac1ca6e9f1a32"
const uint8_t PORTAL_HTML_GZ[] PROGMEM = {
  0x1f, 0x8b, 0x08, 0x00, ...
};
const uint32_t PORTAL_HTML_GZ_LEN = 84694;
```

Pentru fiecare fișier sursă se generează trei simboluri:

- `<SIMBOL>[]` — octeții comprimați. Primii trei (`1f 8b 08`) sunt semnătura gzip.
- `<SIMBOL>_LEN` — lungimea, ca firmware-ul să știe cât să trimită.
- `<SIMBOL>_SRC_SHA` — primii 16 hex din SHA-256 al **sursei necomprimate**.
  Nu e folosit de firmware; e o urmă care spune ce HTML a produs blobul, utilă
  când te uiți la un diff sau la un binar și vrei să știi din ce provine.

---

## Cum îl folosește firmware-ul

Trei locuri în `Octoglow.ino`:

```cpp
#include "web_assets.h"                       // linia ~5531

static void sendGzipHtml(const uint8_t* body, uint32_t len) {
  server.sendHeader("Content-Encoding", "gzip");
  server.setContentLength(len);
  server.send(200, "text/html", "");
  server.sendContent_P((PGM_P)body, len);
}
```

apoi, în `handleDashboard()` și `handleRoot()`:

```cpp
sendGzipHtml(PORTAL_HTML_GZ, PORTAL_HTML_GZ_LEN);
sendGzipHtml(AUTH_SHELL_GZ,  AUTH_SHELL_GZ_LEN);
```

`sendHeader()` trebuie apelat **înainte** de `send()` — `WebServer` adună
antetele și le emite la `send()`.

---

## Detalii de implementare

### `mtime=0` la compresie

```python
blob = gzip.compress(raw, compresslevel=9, mtime=0)
```

Implicit, gzip scrie timestamp-ul curent în header, deci **același HTML ar produce
octeți diferiți la fiecare rulare**. Asta ar rescrie `web_assets.h` de fiecare
dată, ar murdări fiecare diff din git și ar forța Arduino să recompileze un fișier
de 5.574 de linii degeaba. Cu `mtime=0` generarea e deterministă.

### Scrierea e idempotentă

Scriptul construiește conținutul complet în memorie și îl compară cu ce e pe disc.
Dacă e identic, **nu atinge fișierul** — deci mtime-ul rămâne neschimbat și Arduino
nu vede niciun motiv de recompilare.

### Nu minifică

Scriptul doar comprimă. Nu atinge HTML-ul, nu redenumește variabile, nu scoate
spații. De asta se poate demonstra echivalența bit cu bit (vezi mai jos). Gzip
elimină oricum redundanța pe care ar fi vizat-o o minificare; măsurat pe acest
proiect, scoaterea indentării ar fi adus doar 57 KB, față de 319 KB de la gzip.

---

## Utilizare

```bash
python build_web.py           # regenerează web_assets.h
python build_web.py --check   # doar verifică; ies cu cod 1 dacă nu e la zi
```

`--check` nu scrie nimic. E gândit pentru un git hook sau CI, ca să nu se poată
comita un `web_assets.h` rămas în urmă față de `portal.html`. Exemplu de
`.git/hooks/pre-commit`:

```bash
#!/bin/sh
cd Firmware/Octoglow && python build_web.py --check || {
  echo "Ruleaza: python build_web.py"
  exit 1
}
```

Cerințe: Python 3 (testat pe 3.14), doar biblioteca standard — `gzip`, `hashlib`,
`os`, `sys`. Fără dependențe externe.

---

## Cum adaugi o pagină nouă

1. Pui fișierul `.html` lângă sketch.
2. Îl adaugi în lista din `build_web.py`:

```python
ASSETS = [
    ("portal.html", "PORTAL_HTML_GZ"),
    ("auth.html",   "AUTH_SHELL_GZ"),
    ("pagina.html", "PAGINA_GZ"),        # nou
]
```

3. Rulezi scriptul și îl servești cu `sendGzipHtml(PAGINA_GZ, PAGINA_GZ_LEN)`.

Nu trebuie modificat nimic altceva — restul generatorului e generic.

---

## Capcane de reținut

**Paginile nu mai pot conține substituții la runtime.** Înainte ar fi fost posibil
să cauți și să înlocuiești ceva în string înainte de trimitere; într-un blob gzip
nu se poate. Momentan nu se face nicio substituție — ambele pagini se trimit
verbatim, toate datele vin prin `/state` și celelalte endpointuri JSON — deci
restricția nu costă nimic. Dacă vreodată ai nevoie de conținut dinamic în HTML,
îl adaugi ca endpoint JSON, nu ca substituție în pagină.

**Clienții trebuie să accepte gzip.** Nu există fallback necomprimat — versiunea
brută nu mai există în flash. Toate browserele trimit `Accept-Encoding: gzip`, iar
singurul alt consumator din proiect, Octoglow Connect, vorbește doar cu
endpointurile JSON (`/nowplaying`, `/notification`, `/ets2speed`), nu cu paginile.
Dacă testezi cu `curl`, folosește `curl --compressed`, altfel primești octeți gzip
în terminal.

**`portal.html` e UTF-8 fără BOM și are diacritice.** Scriptul citește binar
(`open(..., "rb")`), deci encoding-ul e păstrat exact. Ai grijă ca editorul tău să
nu adauge BOM și să nu convertească terminatorii de linie — ar schimba octeții
trimiși.

**Nu edita `web_assets.h`.** Orice modificare se pierde la următoarea rulare a
scriptului.

---

## Verificare

Că lanțul e corect se poate demonstra, nu doar presupune:

```bash
# 1. gzip-ul se decomprimă înapoi în sursă, bit cu bit
python -c "
import gzip
h = open('web_assets.h', encoding='utf-8').read()
i = h.index('PORTAL_HTML_GZ[] PROGMEM = {'); j = h.index('};', i)
b = bytes(int(t,16) for t in h[h.index('{',i)+1:j].replace('\n',' ').split(',') if t.strip())
print(gzip.decompress(b) == open('portal.html','rb').read())
"

# 2. generarea e deterministă
python build_web.py && python build_web.py --check
```

Ambele au fost rulate la introducerea acestui lanț: round-trip identic pe toți cei
404.111 octeți, iar ștergerea și regenerarea lui `web_assets.h` a produs un fișier
identic bit cu bit.

---

## Depanare

| Simptom | Cauză | Rezolvare |
|---|---|---|
| `web_assets.h: No such file or directory` la compilare | headerul lipsește (proaspăt clonat, sau ignorat de git) | `python build_web.py` |
| Modificările din interfață nu apar după flash | ai uitat să rulezi scriptul | `python build_web.py`, apoi recompilează |
| Browserul descarcă un fișier în loc să afișeze pagina | `Content-Encoding` lipsă sau greșit la trimitere | verifică `sendGzipHtml()` |
| `curl` scoate caractere ilizibile | curl nu cere gzip implicit | `curl --compressed` |
| Diff uriaș în `web_assets.h` la o schimbare minusculă | normal — gzip-ul se schimbă global la orice modificare | citește diff-ul din `portal.html`, nu din header |
