Welcome to my todo list


~~1. UAC fix, sa nu fie necesar sa ne logam de fiecare data cand moare ceasul~~ 

~~2 Fix icoana de luminozitate~~(2.8.26)

~~3. fix language piker on touch in sender~~ FUCIN NEW C# VERSION CUZE OF THAT

~~4. Fix number font~~ (3.8.26)

~~5. fix animation tranzion on single circuit tile on~~

~~6. fix non circuit tiles on~~ (3.8.26)

~~7. Ui cons fix~~ (2.8.26)

~~8. Page icon~~

~~9. Add ASCII art~~

~~10. Add to touch actions on touch show ip adress~~

~~11. Smart House integration~~ (Bullshit, after reading some docs it is not worth it)

~~12. Help Manager~~

 13. Limbă intrefata : Engleză Si Romana

 14. Reset & Restore & backup functions in about

 ~~15. Tile manager Custom Fonts~~

~~16. Custom accent color~~

~~17. Scrolling freeze bugs~~

~~18. AP scrolling Ip adress~~ Why did I added this?

19. AP settings one time show

20. AP set time witout  and date witout internet conexion

~~21. UAC redesign~~

~~22. Fix Now Playing number font~~

~~23. Looking for something else!~~

~~24. Login page redesign~~

~~25. Notification tile font issue~~

~~26. IP adress Scroling font issue~~

~~27. Data formatting DD/MM/YYY~~

~~28.Design Flow in tile manager settings~~

/State bugs

Nu, e important să fie clar: schimbarea asta e **pur cosmetică/UX**. Nu reduce numărul de erori reale și nu îmbunătățește stabilitatea backend-ului — doar ascunde de la utilizator faptul că datele nu sunt încă gata, în loc să-l lase să vadă un UI pe jumătate populat.

Concret, ce **nu** s-a schimbat:
- `/state` poate în continuare să eșueze (timeout, JSON incomplet, `hasCompleteCircuitState` fals) exact ca înainte — logica de retry (5 încercări, backoff 500ms/încercare) a rămas identică.
- Cauzele reale ale eșecurilor — buffer-ul ESP-ului care nu apucă să se umple, memorie insuficientă, `WiFi.scanNetworks` blocant, `Content-Length` greșit etc. — nu au fost atinse deloc.
- Dacă rețeaua e proastă sau device-ul e supraîncărcat, tot vei ajunge la eșecul final după 5 încercări — doar că acum utilizatorul vede un popup clar cu eroarea, în loc de tile-uri goale.

Ce **s-a îmbunătățit** cu adevărat:
- Percepția de stabilitate (nu mai vezi UI stricat/incomplet).
- Vizibilitate — acum vezi exact mesajul de eroare (`err.message`) în loc să ghicești de ce nu s-a încărcat ceva.

Dacă vrei stabilitate reală (mai puține eșecuri, nu doar ascunderea lor), ar trebui să mă uit la partea de server (`handleState`, cât de mare e payload-ul, dacă se face streaming corect, dacă `WiFi.scanNetworks` sau alte operații blocante rulează în timp ce se servește `/state`). Vrei să sap acolo?

Fragmentare heap din String, pe termen lung — ăsta e cel mai real risc. Ai ~110 utilizări de String (concatenări în handler-ele web, parsare JSON etc.). Pe ESP32, alocarea/dezalocarea repetată de String-uri de dimensiuni variabile fragmentează heap-ul încet, chiar dacă ai destulă RAM liberă în total. Simptom tipic: după zile/săptămâni de uptime continuu, apar crash-uri random sau Guru Meditation Error la un malloc care eșuează, deși teoretic mai ai memorie. Soluție pe termen lung: char[] cu buffere fixe în locurile fierbinți (build de JSON pt dashboard, request handlers).
Stutter la buzzer — melodiile tale folosesc tone() + delay() secvențial (ex: 5-6 note x ~50-100ms fiecare = până la jumătate de secundă blocat). În acel interval, loop-ul e complet blocat: matricea nu se actualizează, server.handleClient() nu se apelează, deci dashboard-ul poate părea "înghețat" scurt de fiecare dată când sună un sunet. Nu e grav (sub 1s), dar dacă cineva dă refresh pe dashboard exact când sună notificarea, poate vedea un request care durează puțin mai mult.
Riscul "un state uitat" — ai 31 de apeluri manuale la server.handleClient() distribuite prin cod, ca să ții dashboard-ul responsive în timpul animațiilor de scroll. Cu 16k linii și atâtea tile-uri/stări, e ușor ca la un tile nou adăugat în viitor să uiți acel apel undeva — nu crapă nimic, dar apare un tile unde dashboard-ul devine brusc lent/nereactiv cât timp rulează acel tile. Genul de bug care se descoperă abia când adaugi ceva nou.
Debugging mai greu, nu performanță mai slabă — cu 229 de variabile globale și tot codul într-un singur fișier, orice bug subtil de state (ex: un tile care nu-și resetează corect un flag) e mai greu de urmărit. Nu afectează viteza de rulare, dar te costă timp când apare ceva ciudat.