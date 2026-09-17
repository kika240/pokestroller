# Validation macOS 0.3.0 — 17 septembre 2026

Base historique : `jpcerrone/pokestroller` à la révision
`8a7b85df005649f61bc3cc2e8aac821fd26a787a`.
Cœur natif : `h4lfheart/pocketwalker` à la révision
`2f3b4512a668e3b7c321f213c1c8d5344627e96e`, avec les corrections documentées
[dans la notice](../third_party/pocketwalker/UPSTREAM.md).

## Résultats

| Vérification | Résultat observé |
| --- | --- |
| Compilation Apple Clang / SDK macOS 27, minimum macOS 11 | Réussie, arm64 et x86_64 |
| Signature ad hoc et archive SHA-256 | Vérifiées |
| Régressions du cœur C historique | Réussies sous ASan / UBSan |
| Régressions du nouveau cœur | Arithmétique 8/16/32 bits, flags, multiplication signée, accès désalignés et erreur CPU récupérable |
| RTC | Passage 23:59:59 → 00:00:00, BCD, PM et interruptions minute/heure/jour ; aucune fausse transition au premier tick |
| Buzzer | Exactement 32 000 échantillons par seconde simulée, fréquence de test 1 kHz |
| TCP / SCI3 | 120 000 octets bidirectionnels, octet transmis entre deux SCI3, fermeture distante |
| Sauvegardes | Écriture / reprise, isolation des profils, verrou exclusif et restauration après corruption |
| ROM personnelle / Poké Radar | Choix du mauvais buisson, message « Il est parti… », puis retour sans erreur |
| ROM personnelle / marche | 0 → 68 pas dans le scénario ; attribution des watts par le firmware |
| ROM personnelle / audio | 1 744 000 échantillons, dont 46 317 non nuls dans le scénario instrumenté |
| ROM personnelle / reprise EEPROM | 1 957 watts avant fermeture et après redémarrage |
| Rencontre entre deux cœurs via TCP | 1 656 et 1 621 octets envoyés ; compteurs de réception correspondants, animation de rencontre, cadeau enregistré sur les deux appareils |
| Interface Cocoa sur Apple Silicon | Affichage, commandes et marche observés ; compteur à 226 pas, pause et sauvegarde effective dans Application Support |
| Fichiers d'entrée | SHA-256 inchangés après les tests |

Les commandes finales ont réussi :

```sh
./tests/run-tests.sh
./tests/run-native-tests.sh "/chemin/ROM.bin" "/chemin/EEPROM.bin" "/dossier/temporaire"
./build-macos.sh --package
```

Les régressions natives et les deux scénarios utilisant la ROM ont été exécutés
avec AddressSanitizer et UndefinedBehaviorSanitizer, sans erreur. Le test de
rencontre vérifie les deux inventaires de cadeaux, pas seulement l'ouverture du
socket. Le mauvais buisson et le cadeau « Élixir en cadeau ! » ont également
été contrôlés visuellement sur les trames produites.

## Conditions des tests avec les dumps

Les fichiers ont été lus directement dans leur dossier extérieur au dépôt.
Le test sonore active le volume uniquement dans la copie en RAM de l'EEPROM,
car le fichier d'origine désactive le son. Le test IR construit une seconde
identité de test en RAM, avec des contrôles d'intégrité valides. Il utilise deux
origines RTC déterministes et un décalage entre les pressions de connexion.
Ces adaptations ne sont pas appliquées par l'application aux fichiers utilisateur.

Aucune ROM, EEPROM, trace réseau ou capture issue des dumps n'est ajoutée à Git
ou au bundle. Les captures de contrôle sont dans le dossier temporaire du système.
L'application stocke ses sessions dans Application Support, hors du projet.

## Portée et limites

- Le port macOS utilise le nouveau cœur. Le frontend Windows reste sur le cœur C
  historique ; il n'a pas été compilé ou exécuté sur Windows pendant cette session.
- Exécution vérifiée sur Apple Silicon avec macOS 27. Intel et macOS 11 sont des
  cibles de compilation, sans exécution sur ces configurations dans cette session.
- L'IR a été validé entre deux cœurs avec le même transport TCP que l'application.
  Pas de rencontre avec une Nintendo DS physique ni de test avec un jeu melonDS.
  Les pauses, des latences importantes ou deux identités identiques peuvent faire
  échouer une rencontre comme le signale le firmware.
- L'EEPROM sauvegardée est un état persistant, pas un instantané CPU. Le compteur
  quotidien volatile du firmware repart à zéro au redémarrage dans le scénario
  testé ; les watts et le cumul de pas du cache HealthData sont conservés.
- L'accéléromètre et le timbre du buzzer sont des simulations. Les tests ne
  démontrent pas l'exactitude de toutes les instructions H8, de tous les timings
  matériels ou de toutes les révisions du firmware.
- L'export manuel utilise le même instantané EEPROM que l'autosauvegarde. Le cycle
  de persistance a été vérifié automatiquement ; le dialogue d'export était déjà
  accessible dans la version précédente.
- Signature ad hoc uniquement, sans notarisation Apple ni signature Developer ID.

La CI exécute les régressions synthétiques des deux cœurs, teste la persistance et
le transport, puis construit et vérifie le bundle universel. Elle n'a jamais accès
aux dumps personnels ; les scénarios avec la ROM sont des validations locales.
