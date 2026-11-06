# Hello World – PlayStation 1

Pieni PS1-homebrew-peli PSn00bSDK:lla. Peli näyttää PS1:n ruudulla tervehdyksen
ja väripalkin käyttäen kaksinkertaista kuvapuskurointia. Käännös tuottaa PS-X
EXE -tiedoston ja käynnistettävän CD-kuvan.

## Ympäristön asennus Linux Mintissä

Linux Mint perustuu Ubuntuun, joten riippuvuudet asennetaan APT-pakettienhallinnalla.
Avaa pääte projektin hakemistossa ja asenna tarvittavat työkalut:

```sh
sudo apt update
sudo apt install python3 python3-venv make unzip curl
```

`setup.sh` asentaa virallisen PSn00bSDK 0.24:n käyttäjäkohtaiseen hakemistoon.
Jos koneesta puuttuu CMake 3.21 tai uudempi, skripti asentaa sen myös Pythonin
virtuaaliympäristöön eikä järjestelmän CMakea tarvitse päivittää:

```sh
./setup.sh
```

Oletuspolku on `~/.local/opt/psn00bsdk-0.24`. Polun voi vaihtaa
`PSN00BSDK_HOME`-ympäristömuuttujalla. Paikallisen CMake-asennuksen oletuspolku
on `~/.local/opt/psn00bsdk-cmake`; sen voi vaihtaa
`PSN00BSDK_CMAKE_HOME`-muuttujalla.

Jos `python3 -m venv` ilmoittaa, että `venv` puuttuu, varmista että
`python3-venv`-paketti on asennettu yllä olevalla komennolla.

## Käännös

```sh
./build.sh
```

Tulokset löytyvät `build/`-hakemistosta:

- `hello.exe` – PS1:n PS-X EXE -suoritettava tiedosto
- `hello.bin` ja `hello.cue` – PS1-emulaattorissa käynnistettävä CD-kuva

Avaa `hello.cue` DuckStationissa tai muussa PS1-emulaattorissa. Emulaattori ei
sisälly projektiin. DuckStation vaatii PlayStation BIOS -tiedoston. Käytä
omasta konsolistasi dumpattua BIOS-tiedostoa ja valitse se DuckStationin
ensimmäisen käynnistyksen ohjatussa asennuksessa. BIOS ei sisälly tähän
projektiin.
