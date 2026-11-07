# Windows-kehitys

Varmista, että koneessa on `winget`. Avaa PowerShell projektikansiossa. Jos
PowerShell estää paikallisten skriptien suorittamisen, salli ne vain nykyisessä
PowerShell-istunnossa:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
```

Asenna PSn00bSDK ja tarvittaessa CMake, käännä peli ja käynnistä levykuva
DuckStationissa:

```powershell
.\setup.ps1
.\build.ps1
.\run.ps1
```

Ajettavan 3D-automallin demo käännetään samalla ja käynnistetään komennolla:

```powershell
.\run.ps1 -CarDemo
```

Demo yhdistää `car`-kansion korin ja neljän renkaan OBJ-geometriat ja käyttää
niiden normaaleja valaistukseen. Renkaiden OBJ-tekstuurikoordinaatit sovitetaan
automaattisesti `wheel_texture.c`-tiedostossa luotuun rengas- ja
vannetekstuuriin. Korin ikkunat, valot ja yksityiskohdat muodostetaan
`body_texture.c`-tiedostossa; MTL-materiaalit eivät ole käytössä. Demo käyttää
PS1:n 640 × 240 -kuvatilaa, liukuväristä taivasta ja tienvarren merkkejä.
Levykuvan vanha nimi
säilyy yhteensopivuuden vuoksi: Linuxissa se on `build/cube_demo.cue`, ja
Windowsissa myös `.\run.ps1 -CubeDemo` toimii.
Sulje demo DuckStationissa ennen uutta käännöstä, jotta Windows vapauttaa
levykuvatiedoston kirjoittamista varten. Jos DuckStation palauttaa vanhan
tallennustilan, käynnistä uusi käännös valitsemalla **System > Restart Game**.

Nuoli ylös kiihdyttää eteenpäin ja nuoli alas jarruttaa ja peruuttaa.
Vapautettaessa kaasun auto hidastuu vähitellen pysähdyksiin. Nuolilla vasemmalle
ja oikealle käännetään ajon aikana; peruuttaessa ohjaus kääntyy päinvastaiseksi.
Kamera seuraa auton takaa. Tie muodostetaan ajon aikana, mutkittelee
satunnaisesti ja jatkuu eteenpäin; myös peruuttaminen on mahdollista.
Tie pysyy samana, kun palaat aiempaan kohtaan. DuckStationissa näppäimistön nuolet on
asetettava ohjaimen 1 ristiohjaimen näppäimiksi (Settings > Controllers).
Myös PlayStation-ohjaimen ristiohjain toimii.

VS Coden **Run and Debug** -näkymässä valitse
**PS1: Run and Debug in DuckStation (Windows)** ja paina F5. `setup.ps1`
asentaa kääntäjän mutta ei debuggeria. Asenna Windowsille MIPS-arkkitehtuuria
ja GDB:n remote target-description XML:ää tukeva `gdb-multiarch`, aseta
`PSN00BSDK_GDB` osoittamaan sen suoritettavaan tiedostoon ja käynnistä VS Code
uudelleen. Tavallinen x64-GDB ei riitä. Varmista DuckStationin asetuksista,
että **Settings > Debugging > Enable GDB Server** on päällä ja portti on
`2345`. DuckStationin voi tarvittaessa asettaa `DUCKSTATION`-ympäristömuuttujalla.

Käännöksen tulokset ovat `build\windows\`-hakemistossa. Käynnistys käyttää
oletuksena DuckStationia polusta
`E:\duckstation\duckstation-qt-x64-ReleaseLTCG.exe`. Jos DuckStation on
asennettu muualle, anna suoritettavan tiedoston polku:

```powershell
.\run.ps1 -DuckStationExe 'C:\duckstation\DuckStation.exe'
```
