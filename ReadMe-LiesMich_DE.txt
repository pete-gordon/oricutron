Oricutron 1.2
-------------

(c)2009-2014 Peter Gordon (pete@petergordon.org.uk)

This is a work in progress.


Aktueller status
==============

  6502:  100% fertig (außer einigen unbekannten Fehlern :)
  VIA:   95% fertig.
  AY:    99% fertig.
  Video: 100% fertig
  Tape:  99% fertig (.TAP, .ORT und .WAV werden unterstützt)
  Disk:  Sektoren Lesen/Schreiben funktioniert. Lesen/Schreiben der Tracks geht 		 nicht.



Credits
=======

  Programmierung
  -----------

  Peter Gordon


  Zusätzliche Programmierung
  --------------------------

  Francois Revol
  Alexandre Devert
  Stefan Haubenthal
  Ibisum
  Kamel Biskri
  Iss
  Christian from defence-force forum


  Amiga & Windows Portierungen
  ----------------------------

  Peter Gordon


  BeOS/Haiku Portierung
  ---------------------

  Francois Revol


  macOS Portierung
  ----------------

  Francois Revol
  Kamel Biskri
  Patrice Torguet


  MorphOS & AROS Portierungen
  ---------------------------

  Stefan Haubenthal


  Linux Portierung
  ----------------

  Francois Revol
  Ibisum
  Alexandre Devert


  Pandora Portierung
  ------------------

  Ibisum


  ACIA & Pravetz Disk-Unterstützung
  ---------------------------------

  Iss


  CH376 Unterstützung
  -------------------

  Offset (cpc) & Jede


Danksagung
==========

Danke an DBug und Twilighte, dass sie mich ihre Demos und Spiele mit Oricutron zusammen verteilen lassen.

Danke an DBug, Twilighte, Chema, kamelito, Yicker, JamesD, Algarbi, ibisum,
jede, thrust26 und alle anderen für Ihre Hilfe und Ihr Feedback!



AVI export notes
================

Der AVI Export nutzt den MRLE Codec. Vielleicht unterstützt dein Spieler der Wahl diesen nicht, aber MPlayer spielt ihn, ffmpeg konvertiert es, sodass du es direkt nach Youtube laden kannst.

Beachte, dass der MRLE Codec bei dem Amiga OS4 Port des MPlayer ein paar Probleme mit Endian hat, sodass der Ton irgendwie schrottig rüberkommt und falsche Farben hat bis das im MPLayer behoben ist. :-(


Kommandozeile
=============

Sie können bestimmte Optionen auf der Befehlszeile angeben. Alle Optionen haben
sowohl kurze als auch lange Versionen. Zum Beispiel:

  -mblah

  oder

  --machine blah

ist dasselbe. Beachte, dass die Kurzversion kein Leerzeichen, die Lange aber schon eins hat.

Hier sind alle Optionen:


  -m / --machine     = Lege Systemtyp fest. Diese sind:

                       "atmos" oder "a" für Oric atmos
                       "oric1" oder "1" für Oric-1
                       "o16k" für Oric-1 16k
                       "telestrat" oder "t" für Telestrat
                       "pravetz", "pravetz8d" oder "p" für Pravetz 8D

  -d / --disk        = Wähle ein Disk Image zur Nutzung in Laufwerk 0
  -t / --tape        = Wähle ein Tape Image
  -k / --drive       = Wähle einen Disk Drive Controller. Diese sind:

                       "microdisc" oder "m" für Microdisc
                       "jasmin" oder "j" für Jasmin
                       "bd500" oder "b" für ByteDrive BD-500
                       "pravetz" oder "p" für Pravetz-8D FDC

  -s / --symbols     = Lade Symbole aus einer Datei
  -f / --fullscreen  = Starte Oricutron im Vollbild-Modus
  -w / --window      = Starte Oricutron in einem Fenster
  -R / --rendermode  = Rendermodus. Diese sind:

                       "soft" Software rendering
                       "opengl" OpenGL

  -b / --debug       = Start Oricutron im Debugger
  -r / --breakpoint  = Setze einen Breakpoint (Siehe Hinweis 2)
  -h / --help        = Aktiviere Kommandozeilen-Hilfe und Programm beenden

  --turbotape on|off = Aktiviere oder deaktiviere Turbotape
  --lightpen on|off  = Aktiviere oder deaktiviere Lichtgriffel
  --vsynchack on|off = Aktiviere oder deaktiviere VSync hack
  --scanlines on|off = Aktiviere oder deaktiviere Scanline Simulation

  --serial_address N = Setze die Adresse der seriellen Karte auf N (default ist 	  					$31C)
                        N liegt dezimal oder hexadezimal innerhalb des Bereiches 							$31c..$3fc
                         (i.e. 796, 0x31c, $31C repräsentiert den gleichen Wert)

  --serial <type>    = Set serial card back-end emulation:
                        'none' - keine serielle Karte
                        'loopback' - zum Testen - alle TX Daten werden an RX zurück 						gegeben
                        'modem[:port]' - emuliert einen Com-Port mit 							angeschlossenem Modem, nur wenige AT Kommandos werden 							unterstützt und Daten werden an TCP umgeleitet. Default 						Port ist 23 (telnet)

                        'com:115200,8,N,1,<device>' - nutzt reale oder virtuelle 							<device> beim Host als emuliertes ACIA.
                        Baudrate, data bits, Parität und Stop bits können nach 							Bedarf verändert werden
                        Bsp.:  Windows: 'com:115200,8,N,1,COM1'
                               Linux:   'com:19200,8,N,1,/dev/ttyS0'
                                                  'com:115200,8,N,1,/dev/ttyUSB0'

BEACHTE: Falls Sie nicht sicher sind, welchen Rechner oder welchen Laufwerkstyp ein Disk- oder Tapeimage benötigt, geben Sie den Dateinamen ohne weitere Optionen ein und Oricutron wird es selbst versuchen.

Beachte 2: Listen mit vielen Breakpoints können von der Kommandozeile geladen werden. Nutze die standardmässigen Schalter -r oder --breakpoint,
aber anstelle einer Adresse gebe Präfixe für die Dateinamen mit ':' an. Die Datei ist eine Textdatei, die notwendige Breakpoint-Adressen enthält - eine pro Zeile, mit der selben Syntax wie am Monitor. Breakpoints können mit absoluten Adressen oder mit Symbolen (-s oder --symbols) geladen werden.

Beispiele:

oricutron tapes/tape_image.tap
oricutron disks/disk_image.dsk
oricutron --machine atmos --tape "tape files/foo.tap" --symbols "my files/symbols"
oricutron -m1 -tBUILD/foo.tap -sBUILD/symbols -b
oricutron --drive microdisc --disk demos/barbitoric.dsk --fullscreen
oricutron -ddemos/barbitoric.dsk -f
oricutron --turbotape off tapes/hobbit.tap
oricutron -s myproject.sym -r :myprojectbp.txt


Schlüssel
=========

  Im Emulator
  -----------

  F1       - Zeigt Menü
  F2       - Gehe zum Debugger/Monitor
  F3       - Reset (NMI)
  F4       - Harter Reset
  Shift+F4 - Jasmin Reset
  F5       - Schalte FPS um
  F6       - Toggle warp speed
  F7       - Speichere alle veränderten Disks
  Shift+F7 - Speichere alle veränderten Disks in neuen Disk Images
  F8       - Toggle Vollbild
  F9       - Speichere Kassettenausgabe
  F10      - Start/Stop AVI Aufnahme
  F11      - Kopiere Bildschirm in Zwischenablage (BeOS, Linux & Windows)
  F12      - Einfügen (BeOS, Linux & Windows)
  Help     - Zeige Hilfe (Amiga, MorphOS and AROS)
  AltGr    - Zusätzlicher Modifizierer


  In Menüs
  --------

  Cursors   - Navigiere
  Enter     - Führe Befehl aus
  Backspace - Gehe zurück
  Escape    - Verlasse Menüs
  (oder benutz die Maus)


  Im Debugger/Monitor
  -------------------

  F1      - Gehe zum Menü
  F2      - Zurück zum Emulator
  F3      - Wechsle zwischen Konsole/Debug Ausgabe/Speicherbeobachtung
  F4      - Wechsle zwischen VIA/AY/Disk-Information
  F9      - Setze Zyklus-Zählung neu
  F10     - Step over code
  F11     - Step over code ohne Rückverfolgung in Unterroutinen.
  F12     - Überspringe Instruktion


  In der Konsole:
  ---------------

  Up/Down - Befehls-Historie


  In memwatch:
  ------------

  Up/Down           - Scrollen (+shift zum seitenweisen Scrollen)
  Page Up/Page Down - Seite hoch/runter
  Hex digits        - Gebe Addresse ein
  S                 - Toggle split mode
  Tab               - Bringe Fenster in split mode



Monitor Instruktionen
=====================

Im Monitor sind numerische Argumente standardmäßig dezimal, oder haben ein vorangestelltes $ für hex oder % für binär. Fast alles wird in hex ausgegeben.

Fast immer, wenn eine Zahl oder Adresse eingegeben werden kann, kann auch ein CPU- oder VIA-Register genommen werden. (VIA-Register sind Vordefiniert mit V, Z.B. VDDRA). Überall wo eine Adresse angegeben werden kann, kann auch ein Syymbol verwendet werden.

Befehle:

  ?                     - Hilfe
  a <addr>              - Assembliere
  bc <bp id>            - Leere Breakpoint
  bcm <bp id>           - Leere mem Breakpoint
  bl                    - Liste Breakpoints auf
  blm                   - List mem Breakpoints
  bs <addr> [zc]        - Setze Breakpoint
  bsm <addr> [rwc]      - Setze mem Breakpoint
  bz                    - Zap Breakpoints
  bzm                   - Zap mem Breakpoints
  d <addr>              - Disassembliere
  fd <addr> <end> <file>- Disassembliere zu Datei
  fw <addr> <len> <file>- Schreibe Speicher zu Datei
  fr <addr> <file>      - Lese Datei zu Speicher
  m <addr>              - Dump memory
  mm <addr> <value>     - Modifiziere Memory
  mw <addr>             - Beobachte Memory an addr
  nl <file>             - Lade Snapshot
  ns <file>             - Speichere Snapshot
  r <reg> <val>         - Setze <reg> zu <val>
  q, x or qm            - Beende Monitor
  qe                    - Beende Emulator
  sa <name> <addr>      - Füge user symbol hinzu oder bewege es
  sk <name>             - Eliminiere user symbol
  sc                    - Symbole nicht case-sensitive
  sC                    - Symbole case-sensitive
  sl <file>             - Lade user symbols
  sx <file>             - Exportiere user symbols
  sz                    - Zap user symbols



Breakpoints
===========

Es gibt zwei Arten von Breakpoints. "Normale" Breakpoints werden ausgelöst, wenn die CPU
im Begriff ist, eine Anweisung an der Breakpoint-Adresse auszuführen. "Speicher" Breakpoints.
auslösen, wenn auf die Breakpoint-Adresse zugegriffen oder diese geändert wird.

Normale Haltepunkte können 'z'- und/oder 'c'-Modifikatoren verwenden.
bs $0c00 	<-- Pause, wenn die CPU im Begriff ist, Code bei $0c00 auszuführen.
bs $0c00 z 	<-- Pause, wenn die CPU im Begriff ist, Code bei $0c00 auszuführen
                und setzen Sie den Zykluszähler auf 0
bs $0c00 zc <-- Setzen Sie den Zykluszähler auf 0 und fahren Sie fort
bs $0c00 c 	<-- Setzt die Ausführung fort (d.h. deaktivierter Haltepunkt)

Der Hauptzweck dieser Modifikatoren ist es, das Zählen der Zyklen zu erleichtern.
Wenn Symbole geladen sind, können sie anstelle von absoluten Adressen verwendet werden.

Es gibt drei Möglichkeiten, wie ein Speicherbreakpoint ausgelöst werden kann; wenn die CPU etwa
um die Adresse (r) zu lesen, und die CPU dabei ist, die Adresse (w) zu schreiben, oder sich der Wert an der Adresse aus irgendeinem Grund ändert (c).

Sie geben an, auf welche Weise der Breakpoint beim Setzen des Speichers ausgelöst werden soll

Haltepunkt:

bsm $0c00 r		 <-- Pause, wenn die CPU im Begriff ist, von $0c00 zu lesen
bsm $0c00 rw 	 <-- Pause, wenn die CPU im Begriff ist, auf $0c00 zuzugreifen
bsm $0c00 c 	 <-- Pause nach dann Inhalt von $0c00 ändern
bsm $0c00 rwc 	 <-- Pause kurz bevor die CPU auf $0c00 zugreift, oder kurz danach
                      	       Änderungen aus irgendeinem Grund.


Internationale Tastaturen unter Linux und macOS
===============================================

Es gibt viele Probleme mit einigen internationalen Tastaturen unter Linux
und MacOS. Der beste Weg, mit ihnen fertig zu werden, ist die Installation eines UK- oder US Tastaturdefinition und zum Umschalten darauf *vor* dem Start von oricutron.

Unter MacOS können Sie das in den "Systemeinstellungen", "Tastatur", "Eingabe Quellen". Klicken Sie auf das + und suchen Sie nach der britischen oder US-amerikanischen Tastatur.

Unter Ubuntu können Sie dies im Menü System tun, indem Sie Präferenzen wählen und dann wählen Sie Tastatur. Wählen Sie im Dialogfeld Tastatureinstellungen die Option Layouts, und klicken Sie auf Hinzufügen.

Eine bessere Lösung finden Sie unter "Visuelle Tastatur" hier unten.



Virtuelle Tastatur
==================

Oricutron kann eine visuelle Tastatur anzeigen, die auch eine Funktion zur Neudefinition der Tastaturbelegung enthält.

Sie ist über ein Untermenü namens "Tastatur-Optionen" zugänglich.

In diesem Untermenü finden Sie:

- einen Umschalter, der die visuelle Tastatur ein-/ausblendet (Sie können auf die Tasten der Tastatur klicken, um Tastendrücke/Freigaben einzugeben) ;
- einen Umschalter, der Sie in den Modus zur Definition der Tastenbelegung bringt (Sie können dann auf eine Taste der visuellen Tastatur klicken; drücken Sie eine echte Taste auf Ihrer Tastatur und die Belegung wird funktionieren) ;
- ein Umschalter, der es erlaubt, Mod-Tasten (Strg, Umschalt, Funktion) zu klemmen (d.h. Sie klicken zuerst auf eine Taste, um sie zu drücken, und klicken dann entweder erneut darauf, um sie zu lösen, oder Sie klicken auf eine andere Taste, und es wird ein modifizierter Tastendruck erzeugt - z.B. ein Strg-T statt T - und dann die Taste automatisch loslassen) ;
- eine Option zum Speichern einer Tastaturbelegung (.kma-Datei) ;
- eine Option, um eine Tastaturbelegung zu laden;
- eine Option, die die Tastaturbelegung auf die Standard-Tastaturbelegung    		 zurücksetzt.

Sie können auch das Folgende in Ihrer oricutron.cfg hinzufügen, um eine Tastatur automatisch zu laden
Mapping (hier Test.kma im Keymap-Verzeichnis, das im Verzeichnis von Oricutron gefunden wurde):

; lädt automatisch eine Tastaturbelegungsdatei
autoload_keyboard_mapping = 'keymap/Test.kma

Mit anderen Optionen können Sie die Tastatur anzeigen lassen und automatisch sticky Mod-Tasten aktivieren:
show_keyboard = ja
sticky_mod_keys = ja


Emulation der seriellen Karte (ACIA)
====================================

Oricutron kann ACIA an der Adresse #31C (Standardadresse für Telestrat) emulieren.
Die Emulation funktioniert für Oric, Atmos, Telestrat und Pravetz und kann zusammen mit einem beliebigen Plattentyp verwendet werden.

Die emulierte ACIA kommuniziert mit der Außenwelt über Back-Ends.
Back-Ends können in der 'oricutron.cfg' oder über die Befehlszeile konfiguriert werden.
(siehe Standard 'oricutron.cfg' für die Verwendung).

Back-Ends sind:
- keine - deaktiviert die ACIA-Unterstützung
- Loopback - jedes gesendete Byte wird in den Empfangspuffer zurückgeschickt (zu Testzwecken)
- com - Oricutron verwendet einen beliebigen realen oder virtuellen COM-Port im Host-Rechner und kommuniziert mit der an diesen seriellen Port angeschlossenen Hardware.
- Modem - vereint ACIA mit angeschlossenem Modem, das mit dem Internet verbunden ist, mit Server- und Client-Steckdosen

Im 'Modem'-Modus sind folgende 'AT'-Befehle verfügbar:
AT - gibt 'OK' zurück
ATZ - das Modem initialisieren
AT&F - das Modem initialisieren
ATS0=0 - automatische Anrufbeantwortung ausschalten (trennende Steckdose schließen)
ATS0=1 - automatische Anrufbeantwortung aktivieren (Öffnen Sie den Socket und beginnen Sie das Abhören auf dem ausgewählten Port (Standard ist Telnet-Port 23))
ATA - gleich wie 'ATS0=1'.
ATS0?       - liefert 'AUTOANSWER OFF' oder 'AUTOANSWER ON' je nach aktuellem Status der einzelnen Sockets
ATH0 - aktuell angeschlossene Steckdosen trennen
+++ - wenn angeschlossen, schaltet in den Befehlsmodus
ATO - kehrt vom Befehlsmodus in den Online-Modus zurück
ATD ip:port - verbindet sich als Client mit ip:port. 'ip' kann ein beliebiger Hostname (z.B.:localhost) oder die echte IP (z.B.:127.0.0.1) im LAN oder im Internet sein. ATDP und ATDT sind aus Kompatibilitätsgründen eine Alternative.

Emulation der CH376-Karte
=========================

Oricutron läuft mit dem Chip ch376. Dieser Chip ist in der Lage, eine sdcard und einen usbkey (und USB-Anschluss) zu lesen. Dieser Chip verarbeitet FAT32. usbdrive/ Ordner ist der CH376 Emulationsordner. Das bedeutet, dass, wenn der ch376 usbkey lesen soll, liest er in diesem Ordner. Bitte beachten Sie, dass Lesen/Schreiben emuliert wird (siehe unten für den emulierten Befehl ch376). Bitte beachten Sie zudem, dass die Emulation nur im Telestrat-Modus funktioniert. Unter Atmos läuft dieser Chip zwar, aber die ROMs wurde noch nicht freigegeben (und auch die Karte mit den ROMs noch nicht).

Orix (http://orix.oric.org) arbeitet hauptsächlich mit diesem Chip. Bitte verändern Sie die ch376-Emulation nicht: Kontaktieren Sie Jede (jede@oric.org), weil diese Emulation auch im ACE-Emulator (cpc-Emulation) verwendet wird. Offset und ich versuchen hier, dieselbe Emulation zu nutzen. Es ist einfacher, zusammen zu arbeiten als allein.

CH376 Befehl emuliert :
- CH376_CMD_GET_IC_VER
- CH376_CMD_CHECK_EXIST
- CH376_CMD_SET_USB_MODE
- CH376_CMD_GET_STATUS
- CH376_CMD_RD_USB_DATA0
- CH376_CMD_WR_REQ_DATA
- CH376_CMD_SET_FILE_NAME
- CH376_CMD_DISK_MOUNT
- CH376_CMD_FILE_OPEN
- CH376_CMD_FILE_ENUM_GO
- CH376_CMD_FILE_CREATE
- CH376_CMD_FILE_ERASE (unter Linux nicht emuliert)
- CH376_CMD_DATEI_CLOSE
- CH376_CMD_BYTE_LOCATE
- CH376_CMD_BYTE_READ
- CH376_CMD_BYTE_RD_GO
- CH376_CMD_BYTE_WRITE
- CH376_CMD_BYTE_WR_GO
- CH376_CMD_DISK_QUERY
- CH376_CMD_DIR_CREATE (unter Linux nicht emuliert)
- CH376_CMD_DISK_RD_GO

Bekannter Fehler :
Unter Windows sendet die API-Datei keine "." und "..."-Einträge, wenn der Inhalt des Ordners gelesen wird. Das ist ein Problem, weil der ch376-Chip diese Einträge sendet, wenn der Chip den Inhalt eines Verzeichnisses lesen soll.

Wenn jemand CH376_CMD_DIR_CREATE und CH376_CMD_FILE_ERASE emulieren möchte ist das einfach zu machen: man muss nur die WIN32-Funktion kopieren, das DeleteFile ersetzen und CreateDir mit rm() und mkdir(). Aber es wird nicht gemacht, weil wir nicht in der Lage sind, es zu testen.

CH376-Emulation hinzugefügt für :
CH376_CMD_FILE_ERASE unterdrückt eine Datei
CH376_CMD_DIR_CREATE Ordner erstellen

ROM-Patch-Dateien
=================

Für die detaillierte Verwendung siehe die enthaltenen '.pch'-Dateien im Unterverzeichnis roms.

Zusätzlich kann eine unbegrenzte Anzahl von binären Patches hinzugefügt werden:

$XXXX:00112233445566778899AABBCCDDEEFF....
$YYYYY:AA55AA55.....
$ZZZZ:FF00FF00.....

wobei XXXX,YYYYY,ZZZZ - hexadezimale Adressen relativ zur ROM-Startadresse sind
(d.h. um Byte bei C000 auf 00 zu setzen, verwenden Sie: $0000:00)
