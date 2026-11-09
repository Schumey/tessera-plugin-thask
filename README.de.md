# tHAsk

> Braucht die tHAsk-Integration ab 0.18.2 in Home Assistant und einen Bildschirm, der Home-Assistant-Aktionen ausführen darf.

Deine [tHAsk](https://github.com/Schumey/tHAsk)-Aufgaben auf einem Tessera-Bildschirm: eine Liste oder eine
intelligente Ansicht (Heute, Diese Woche, Wichtig …) mit den unscharfen Fälligkeiten aus tHAsk („KW 46“, „November“,
„Q1“), abgehakt mit einem Tipp.

- **Kachel „Aufgaben“**, 1x1 bis 4x4: Name der Liste und wie viele Aufgaben offen sind, darunter eine Zeile pro Aufgabe:
  ein Kreis in der Farbe der Priorität (rot hoch, orange mittel, grün niedrig), der Titel und rechts die Fälligkeit,
  rot wenn überfällig. Eine 1x1-Kachel zeigt die Anzahl groß.
- **Tipp auf den Kreis** hakt die Aufgabe ab; ein zweiter Tipp, solange sie noch zu sehen ist, öffnet sie wieder. Eine
  wiederkehrende Aufgabe bekommt ihren nächsten Termin, wie in tHAsk.
- **Tipp daneben** öffnet die ganze Liste über der Seite, jede Aufgabe mit ihrer Liste.
- **Obere Leiste**: wie viele Aufgaben einer Liste offen sind (standardmäßig Heute), das Symbol rot, solange eine
  überfällig ist.

## Einrichten

1. tHAsk ab 0.18.2 in Home Assistant installieren (github.com/Schumey/tHAsk).
2. In Tessera das Plugin per Link zu einem Bildschirm hinzufügen und die **Liste für die obere Leiste** wählen (z. B.
   tHAsk Heute); der Bildschirm baut einmal.
3. Unter Layout die Kachel **Aufgaben** aus der Gruppe Plugins der Bibliothek platzieren und eine tHAsk-Liste als
   Entity wählen: eine eigene Liste oder eine intelligente wie tHAsk Heute oder tHAsk Diese Woche.
4. Dem Bildschirm Home-Assistant-Aktionen erlauben: Einstellungen → Geräte & Dienste → ESPHome → der Bildschirm →
   Konfigurieren → **Dem Gerät erlauben, Home-Assistant-Aktionen auszuführen**. Ohne das erscheinen die Aufgaben, aber
   ein Tipp hakt nichts ab.
5. Optional: im Inspektor der Kachel eine andere **Ansicht** wählen (Heute, Überfällig, Diese Woche, Diesen Monat,
   Wichtig), um nur diese Aufgaben der Liste zu zeigen, bei einer intelligenten Liste aus allen Listen. **Offene
   Aufgaben** in die obere Leiste einer Seite setzen.

## Gut zu wissen

- Der Bildschirm fragt Home Assistant nach den Aufgaben (`thask.get_tasks`), wenn sich die Liste ändert, und einmal pro
  Minute, solange die Kachel zu sehen ist. Nichts verlässt dein Zuhause.
- Emojis in Listennamen erscheinen auf dem Bildschirm nicht; seine Schrift hat keine.
- Eine Kachel zeigt bis zu zwölf Aufgaben, die Karte bis zu sechzehn, die am frühesten fälligen zuerst.
