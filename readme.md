# PlayStation 1 - 2D-peli

Ohjaa ympyrää PlayStation-ohjaimen ristiohjaimella tai vasemmalla tatilla.
Tatti kalibroidaan automaattisesti ohjaimen kytkemisen jälkeen; pidä se keskellä
kalibrointitekstin näkyessä. Ristiohjain ei tarvitse kalibrointia.

Kerää värillisiä ympyröitä. Jokainen antaa satunnaisen kahden sekunnin efektin:
nopeutus, hidastus, käänteiset ohjaimet, ruudun heilunta tai värien vääristymä.
Kerätyt efektit jonoutuvat ja aktivoituvat peräkkäin.
Jokainen keräys pienentää sinistä ympyrää ja kaikkia kerättäviä ympyröitä,
kunnes ne saavuttavat pienen vähimmäiskoon.
Jokainen keräys lisää harmaan neliöesteen, enintään kahdeksan. Esteiden läpi ei
voi kulkea, ja kerättävät ympyrät sijoitetaan niiden ulkopuolelle.
Sinisen ympyrän ilme muuttuu peliajan mukaan: 60–100 % `:D`, 40–59 % `:\`,
10–39 % `:(` ja alle 10 % `x(`. Pelaajaympyrän minimikoko pitää ilmeen
luettavana.
Ensimmäinen keräys antaa kaksi sekuntia peliaikaa; jokainen lisäkeräys lisää
jäljellä olevaan aikaan kaksi sekuntia. Jos aika loppuu, paina Start
aloittaaksesi pelin uudelleen.
Aktiivisella efektillä on oma kestopalkki, ja QUEUE-rivi näyttää jonossa odottavat.

Käännä ja käynnistä peli projektin kansiosta:

```bash
./build.sh
~/.local/opt/duckstation/DuckStation-x64.AppImage build/hello.cue
```
