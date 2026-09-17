# PokeStroller 0.3 sur macOS

Interface native Cocoa de [PokeStroller](https://github.com/jpcerrone/pokestroller),
avec le cœur H8 et les périphériques de
[PocketWalker](https://github.com/h4lfheart/pocketwalker). La révision importée,
la licence GPL-3.0 et les modifications sont décrites dans
[UPSTREAM.md](../third_party/pocketwalker/UPSTREAM.md).

## Démarrage

Ouvrez `PokeStroller.app`, puis **Ouvrir…** et sélectionnez votre ROM et votre
EEPROM à leur emplacement d'origine. Aucun renommage ni déplacement nécessaire.

- ROM : 49 152 octets, ou dump mémoire de 65 536 octets dont seuls les premiers
  48 Kio servent de ROM.
- EEPROM : exactement 65 536 octets.
- Les originaux sont ouverts en lecture uniquement. Aucun dump n'est inclus dans
  le dépôt, l'application ou l'archive. La ROM reste seulement en mémoire vive.
- Les sessions modifiées sont enregistrées séparément dans Application Support.

## Commandes

| Action | Commande |
| --- | --- |
| Gauche / droite | Z / X, ou flèches gauche / droite |
| Réveiller / valider | Espace ou Entrée ; maintenir pour réveiller l'écran |
| Ouvrir les fichiers | ⌘O |
| Pause / reprendre | ⌘P |
| Activer / arrêter la marche | bouton Marcher, ou ⌘M |
| Exporter l'EEPROM | ⌘S |
| Redémarrer avec la sauvegarde automatique | ⌘R |
| Quitter | ⌘Q |

Les boutons sont également cliquables. Le bouton central simule une pression
assez longue pour réveiller l'écran. Le clavier gère l'appui et le relâchement,
y compris après un changement de fenêtre. Le LCD 96 × 64 s'agrandit par multiples
entiers. L'extinction de l'écran après inactivité est normale : la marche continue.

## Marche, son et horloge

**Marcher** injecte des échantillons d'accéléromètre. Le firmware compte les pas
et attribue les watts. La cadence obtenue est proche de deux pas par seconde,
après quelques secondes de détection. Arrêter la marche peut laisser quelques
pas déjà engagés dans le filtre du firmware. La pause suspend les pas simulés.

Le menu **Son** active la sortie audio et règle son volume. Il faut aussi activer
le son dans les **Réglages du PokéWalker**, à l'intérieur de son écran : un dump
provenant d'un appareil muet conserve ce réglage. Le buzzer est synthétisé à
32 kHz ; son timbre reste une approximation numérique du composant physique.

L'horloge suit l'heure locale du Mac. Les registres RTC et les interruptions de
quart de seconde, demi-seconde, seconde, minute et changement de jour sont
émulés. Les changements de jour sont traités par le firmware. Les pas ne sont
pas rattrapés après une pause, une boîte de dialogue ou la veille du Mac.

## Sauvegardes automatiques

L'EEPROM de session est sauvegardée toutes les cinq secondes d'exécution, ainsi
qu'à la pause, au rechargement et à la fermeture. Ouvrir de nouveau la même paire
ROM/EEPROM reprend automatiquement cette sauvegarde.

Emplacement :

```text
~/Library/Application Support/PokeStroller/Sessions/<identifiant>/session.pws
```

**Fichier → Dossier des sauvegardes** ouvre le dossier correspondant. L'identifiant
dépend du contenu initial des fichiers et du profil ; déplacer les originaux ne
change donc pas la session. Le format `.pws` contient l'EEPROM et une empreinte de
contrôle, jamais la ROM. Une copie `previous.pws` permet de récupérer l'écriture
précédente si la dernière est corrompue. Les écritures sont atomiques et un verrou
empêche deux instances d'écraser simultanément le même profil.

Le firmware connu conserve ses compteurs et réglages récents dans un cache RAM.
L'export synchronise ce cache dans les deux blocs HealthData de l'EEPROM, avec
leur contrôle d'intégrité. Cela préserve notamment les watts gagnés depuis la
dernière écriture effectuée par le jeu. Cette adaptation est limitée à la ROM
reconnue ; une autre version conserve l'export brut de son EEPROM.

Ce format n'est pas un instantané du CPU. Le firmware redémarre ; une animation,
une rencontre ou le compteur volatile de la session en cours ne reprennent pas
à l'instruction exacte. Les données persistantes, dont les watts et les pas
cumulés, sont conservées.

En cas d'échec d'écriture, l'application met la session en pause et permet un
export manuel. Si la sauvegarde et sa copie sont toutes deux illisibles, elles
restent intactes : utilisez un nouveau profil ou une exportation antérieure.

**Fichier → Exporter l'EEPROM…** produit un fichier brut de 64 Kio, compatible
avec le chargement de l'émulateur. Les fichiers d'entrée de la session sont
protégés contre l'écrasement, même via un lien symbolique ou physique.

## Infrarouge virtuel

Le menu **Infrarouge** configure une liaison TCP entre émulateurs. Aucun matériel
infrarouge physique ou Flipper Zero n'est utilisé. Le transport est un flux brut
d'octets, comme dans PocketWalker ; le firmware réalise le protocole du jeu.

1. Sur la première instance, choisir **Attendre une connexion locale…**, port
   `31337` par exemple. L'écoute est limitée à `127.0.0.1`.
2. Sur la seconde, choisir **Se connecter…**, avec `127.0.0.1:31337`.
3. Choisir **Connexion** dans les menus des deux PokéWalker. Les deux jeux doivent
   être actifs ; une pause ou une boîte de dialogue peut faire expirer l'échange.
4. **Déconnecter** coupe le transport et vide les données en attente.

Pour ouvrir une seconde instance indépendante :

```sh
open -n build/macos/PokeStroller.app --args --profile second
```

Un profil séparé évite les conflits de sauvegarde ; il ne transforme pas l'identité
Nintendo d'une EEPROM. Pour une rencontre normale, utiliser deux identités de
PokéWalker distinctes. Le client accepte une adresse IPv4 explicite ; le serveur
inclus accepte uniquement les connexions locales.

Une version de melonDS intégrant un serveur TCP IR compatible peut utiliser ce
transport. Le melonDS standard et une vraie Nintendo DS ne sont pas pris en
charge par cette connexion. Aucune validation avec un jeu DS n'a été réalisée.

## Compiler et tester

Prérequis : macOS 11 ou ultérieur, outils Apple récents avec C++23
(Xcode 15 / Command Line Tools 15 ou ultérieurs). Aucun paquet Homebrew ni Qt.

```sh
./tests/run-tests.sh
./tests/run-native-tests.sh
./build-macos.sh --package
open build/macos/PokeStroller.app
```

La compilation produit un binaire universel Apple Silicon + Intel. Pour seulement
Apple Silicon : `ARCHS=arm64 ./build-macos.sh`. L'archive et son empreinte SHA-256
se trouvent dans `dist/`, exclu de Git. La signature est ad hoc, sans signature
Developer ID ni notarisation Apple ; la compilation locale est le parcours pris
en charge.

Lancement avec chemins explicites, application fermée :

```sh
open build/macos/PokeStroller.app --args \
  --rom "/chemin/vers/ma-rom.bin" --eeprom "/chemin/vers/mon-eeprom.bin"
```

`--profile nom` sélectionne une session indépendante. `--save-dir /chemin`
change le dossier des sauvegardes, notamment pour les tests temporaires.

Les tests automatiques utilisent des données synthétiques et les sanitizers.
Un test facultatif lit des dumps personnels extérieurs au dépôt ; il active le
son uniquement dans sa copie en RAM pour vérifier la synthèse :

```sh
./tests/run-native-tests.sh "/chemin/ROM.bin" "/chemin/EEPROM.bin"
```

Ce dernier scénario suppose une EEPROM appairée avec un Pokémon et au moins
10 watts disponibles. Il ne modifie pas les fichiers fournis. Les parcours testés
et les limites de validation figurent dans [VALIDATION.md](VALIDATION.md).

Le frontend Windows conserve le cœur C historique et ses limitations. Les six
fonctions de cette version concernent le port macOS.
