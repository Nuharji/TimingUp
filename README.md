# TimingUp! – Satisfactory-Mod (Novy Labs)

Mod für die Let's-Play-Challenge **Satisfactory: TimingUp!** vom YouTube-Kanal [@NovyLabs](https://www.youtube.com/@NovyLabs).

## Regeln, die die Mod im Spiel durchsetzt

| Regel | Umsetzung |
|---|---|
| Upgrade = HUB-Meilenstein (auch Tier 0), MAM-Forschung, Weltraumaufzug-Phase | wird beim Freischalten gezählt |
| Max. 30 Upgrades | danach kein Upgrade mehr möglich |
| Nur Spielzeit zählt | Timer läuft nur, wenn das Spiel nicht pausiert ist |
| Erstes Upgrade nach 1 Minute, jede Wartezeit ×1,2 | `BaseWaitSeconds = 60`, `WaitFactor = 1.2` |
| Timer zum nächsten Upgrade startet erst, wenn das freie Upgrade ausgegeben wurde | Timer pausiert im Zustand „Upgrade frei“ |
| Ende, wenn die theoretische Zeit von Upgrade 31 erreicht ist | danach ist alles gesperrt, Anzeige „CHALLENGE VORBEI!“ |
| Keine Alternativrezepte | Festplatten-Forschung und Alternativrezepte sind blockiert |

Oben in der Bildschirmmitte zeigt ein Overlay Upgrades (x/30), Spielzeit und den Countdown an – praktisch auch für die Aufnahme.
Der Fortschritt wird im Spielstand gespeichert.

**Gesperrt, solange kein Upgrade frei ist:** Meilenstein im HUB auswählen und bezahlen, MAM-Forschung starten, in den Weltraumaufzug einzahlen.

## Bauen

Die Mod ist C++-Code für den **Satisfactory Mod Loader (SML) 3.12+**. Zum Kompilieren brauchst du die offizielle Modding-Umgebung (nur Windows):

1. Anleitung „Getting Started“ auf <https://docs.ficsit.app> befolgen (Visual Studio 2022, Unreal Engine für Satisfactory-Modding, Starter-Projekt).
2. Diesen Ordner nach `SatisfactoryModLoader/Mods/TimingUp` kopieren.
3. Projektdateien neu generieren, im Editor mit **Alpakit** „TimingUp“ für *Windows* packen.
4. Die fertige Mod landet im Satisfactory-Ordner unter `FactoryGame/Mods/TimingUp` (Alpakit kann sie direkt dorthin kopieren).

> ⚠️ Der Code wurde gegen die aktuellen SML-/FactoryGame-Header geschrieben, aber noch **nicht kompiliert oder im Spiel getestet**. Beim ersten Bauen bitte Compiler-Fehler und das Log (`FactoryGame.log`, Suche nach `[TimingUp]`) prüfen.

## Online bauen (GitHub Actions)

Der Workflow `.github/workflows/build.yml` baut die Mod automatisch auf GitHub, ohne Visual Studio oder Unreal auf dem eigenen PC.
Einmalig einrichten:

1. **Engine-Zugang:** GitHub-Account mit dem Epic-Games-Account verknüpfen und unter <https://linker.ficsit.app/link> freischalten (wie in der Modding-Anleitung).
2. **Token:** GitHub → Settings → Developer settings → Personal access tokens → *Tokens (classic)* → Scope **repo** → kopieren.
3. **Audiokinetic-Konto** (kostenlos) auf audiokinetic.com anlegen (für Wwise).
4. Im Repo unter **Settings → Secrets and variables → Actions** drei Secrets anlegen:
   `UE_GH_TOKEN` (Token aus Schritt 2), `WWISE_EMAIL`, `WWISE_PASSWORD`.
5. **Actions → Mod bauen → Run workflow**. Der erste Lauf dauert lange (1–3 h), danach geht's schneller.
6. Die fertige Mod liegt unten im Lauf unter **Artifacts → TimingUp-Windows**. Den Inhalt nach `Satisfactory/FactoryGame/Mods/TimingUp` entpacken (SML muss installiert sein, z. B. über den Satisfactory Mod Manager).

## Testmodus

Startoption **`-TimingUpTest`** hinzufügen (Steam → Satisfactory → Eigenschaften → Startoptionen).
Dann läuft der Timer **60-mal schneller** und das Overlay zeigt `[TESTMODUS x60]`.
Die komplette Challenge (~23,6 h Timer) ist so in rund 24 Minuten durchgetestet.
**Für die echte Aufnahme die Startoption wieder entfernen!** Und immer einen eigenen Test-Spielstand nehmen.

Test-Checkliste:
- [ ] Overlay erscheint oben mit Countdown
- [ ] HUB ist gesperrt, bis „UPGRADE 1 FREI“ (grün) erscheint
- [ ] Timer stoppt im Pausemenü
- [ ] Nächster Countdown startet erst, wenn das freie Upgrade ausgegeben wurde
- [ ] MAM-Forschung und Weltraumaufzug sind ohne freies Upgrade gesperrt
- [ ] Festplatten / Alternativrezepte sind gesperrt
- [ ] Nach Speichern + Neuladen stimmt der Stand
- [ ] Nach Upgrade 30 läuft der End-Countdown, danach „CHALLENGE VORBEI!“

## Werte ändern

In `Source/TimingUp/Public/TimingUpSubsystem.h`:

```cpp
static constexpr int32 MaxUpgrades = 30;
static constexpr float BaseWaitSeconds = 60.0f;
static constexpr float WaitFactor = 1.2f;
```

## Aufbau

- `TimingUpSubsystem` – Timer, Zustand, Speichern, Overlay
- `TimingUpModule` – startet das Subsystem in jeder Spielwelt und hängt sich per SML-Hooks in HUB, MAM und Weltraumaufzug
