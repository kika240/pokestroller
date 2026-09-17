# PokeStroller sur macOS

Port natif Cocoa du projet [jpcerrone/pokestroller](https://github.com/jpcerrone/pokestroller),
basé sur la révision `8a7b85df005649f61bc3cc2e8aac821fd26a787a`.

## Démarrage

Ouvrez `PokeStroller.app`, puis **Ouvrir…**. Sélectionnez votre ROM puis votre EEPROM,
à leur emplacement d'origine. Il n'est pas nécessaire de les renommer ou de les
copier dans le projet ou dans l'application.

- ROM : 49 152 octets (48 Kio), ou dump de l'espace mémoire de 65 536 octets.
  Dans ce second cas, seuls les premiers 48 Kio servent de ROM.
- EEPROM : exactement 65 536 octets (64 Kio).
- Les images sont lues en binaire et leur taille est contrôlée avant le démarrage.
  Une erreur de chargement conserve la session déjà ouverte.

L'application ne contient aucun fichier Nintendo. Elle conserve les fichiers
sélectionnés à leur emplacement, les ouvre uniquement en lecture et ne les
copie pas sur disque. L'émulation utilise une copie **en mémoire vive**.

## Commandes

| Action | Clavier |
| --- | --- |
| Bouton gauche | Z ou flèche gauche |
| Bouton central, réveiller, valider | Espace ou Entrée |
| Bouton droit | X ou flèche droite |
| Ouvrir une paire de fichiers | ⌘O |
| Pause / reprendre | ⌘P |
| Exporter l'EEPROM | ⌘S |
| Recharger les fichiers sélectionnés | ⌘R |
| Quitter | ⌘Q |

Les trois boutons sous l'écran sont également cliquables. L'affichage conserve
les pixels du LCD 96 × 64 et s'agrandit par multiples entiers. Agrandissez la
fenêtre pour augmenter l'échelle.

## Sauvegarder une session

**Fichier → Exporter l'EEPROM…** écrit l'état courant dans un fichier de votre
choix, de façon atomique. Sélectionnez un nouveau nom, par exemple
`eeprom-session.bin`, hors du dépôt. L'application refuse d'écraser un fichier
ROM ou EEPROM chargé pendant la session (y compris via un lien symbolique ou
un lien physique).

Il n'y a pas d'écriture automatique dans l'EEPROM d'origine. À la fermeture ou
au rechargement, l'application propose d'exporter les modifications. Pour
reprendre, ouvrez votre ROM avec l'EEPROM exportée. Ce n'est pas une sauvegarde
instantanée du CPU : le firmware redémarre avec les données persistantes.

## Compiler

Prérequis : macOS 11 ou plus récent et les outils de compilation Apple
(`xcode-select --install` si absents). Aucun paquet Homebrew n'est nécessaire.

```sh
./build-macos.sh
open build/macos/PokeStroller.app
```

Le script produit une application universelle **Apple Silicon + Intel**. Pour
compiler seulement pour Apple Silicon : `ARCHS=arm64 ./build-macos.sh`.

Pour créer l'archive locale et son empreinte SHA-256 :

```sh
./build-macos.sh --package
```

Les artefacts sont dans `build/macos/` et `dist/`, exclus de Git. La signature
est **ad hoc**, pour une utilisation locale : ni signature Developer ID, ni
notarisation Apple. Une application téléchargée peut donc être bloquée par
Gatekeeper. La compiler localement est le chemin pris en charge.

Les chemins peuvent aussi être fournis explicitement, y compris avec des espaces :

```sh
open build/macos/PokeStroller.app --args \
  --rom "/chemin/vers/ma-rom.bin" \
  --eeprom "/chemin/vers/mon-eeprom.bin"
```

L'application doit être fermée avant un nouveau lancement avec des arguments.

## Limites du cœur d'émulation

Ce port conserve le cœur expérimental de PokeStroller. Il ne simule pas encore
l'infrarouge, le son ou la marche / l'accéléromètre. Les interruptions périodiques
sont exécutées, mais cela ne constitue pas une horloge RTC complète synchronisée
avec macOS. Le défaut Poké Radar signalé en amont n'est pas considéré résolu.

Le cœur contient des adresses et adaptations spécifiques au firmware connu.
La validation de taille ne garantit pas la compatibilité d'une autre révision
de ROM. Une opération non implémentée suspend l'émulation avec son adresse,
au lieu de fermer silencieusement l'application.

## Tests sans ROM redistribuée

```sh
./tests/run-tests.sh
```

Les tests créent uniquement des images synthétiques dans un dossier temporaire.
Ils vérifient les entrées, les tailles, les rechargements, la copie de l'EEPROM
et les accès mémoire aux limites, avec AddressSanitizer et UndefinedBehaviorSanitizer.
Les vrais dumps restent hors du dépôt et des artefacts de distribution.

La licence du projet et de ce port est GPL-3.0 ; voir `LICENSE`.
