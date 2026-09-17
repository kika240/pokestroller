# Validation du port macOS — 17 septembre 2026

Base amont : `jpcerrone/pokestroller`, révision
`8a7b85df005649f61bc3cc2e8aac821fd26a787a`.

## Résultats locaux

| Vérification | Résultat |
| --- | --- |
| Compilation Apple Clang, SDK macOS 27, cible minimale macOS 11 | Réussie |
| Binaire universel `arm64` + `x86_64` | Deux architectures présentes |
| Vérification du bundle et de sa signature ad hoc | Réussie |
| Tests synthétiques AddressSanitizer + UndefinedBehaviorSanitizer | Réussis |
| Dump personnel : démarrage, réveil, 30 secondes simulées | Réussi, 110 592 000 cycles, 56 changements d'image |
| Dump personnel : séquence de navigation, 30 secondes simulées | Réussi, 110 592 000 cycles, 110 changements d'image |
| Dump personnel : séquence de boutons prolongée, 60 secondes simulées | Réussi, 221 184 000 cycles, 129 changements d'image |
| Dump personnel : autre séquence de boutons, 30 secondes simulées | Réussi, 110 592 000 cycles, 96 changements d'image |
| Build optimisé `-O2` : même test de démarrage | Même empreinte de trame finale que le build instrumenté |
| Interface Cocoa sur Apple Silicon | Accueil animé, menu et entrée dans Poké Radar observés |
| Pause et dialogue d'export | Accessibles dans l'application |
| Fichiers d'entrée après les tests | Empreintes SHA-256 inchangées |

Les tests synthétiques couvrent notamment le rechargement après erreur, les
fichiers absents / tronqués / trop grands, la réutilisation de la file de boutons,
l'initialisation déterministe du LCD, la copie de l'EEPROM et les accès CPU en
limite de l'espace d'adressage de 16 bits.

Les dumps ont été lus directement dans leur dossier extérieur au dépôt. Aucune
ROM, EEPROM ou capture provenant de ces fichiers n'est incluse dans Git ou dans
l'application. Les captures de contrôle du cœur ont été écrites uniquement
dans le dossier temporaire du système.

## Portée et limites

- Exécution native vérifiée sur Apple Silicon avec macOS 27. Le code Intel est
  compilé mais n'a pas été exécuté sur un Mac Intel. macOS 11 est la cible de
  compilation, pas une version sur laquelle cette session a fait un test réel.
- Le frontend Windows est conservé, mais n'a pas été compilé ou exécuté sur
  Windows pendant cette validation.
- Les séquences automatiques montrent que les parcours testés ne déclenchent
  pas les sanitizers. Elles ne prouvent pas la justesse de toutes les
  instructions H8 ni de tous les états des mini-jeux.
- L'export EEPROM est une fonction du port : il sérialise la mémoire EEPROM
  dans un fichier séparé. Le dialogue a été ouvert, mais le cycle complet
  export / réouverture n'a pas été validé dans cette session de contrôle UI.
- Le cœur amont reste expérimental : infrarouge, audio, accéléromètre et RTC
  complète restent à réaliser. Le bug Poké Radar documenté en amont n'est pas
  annoncé corrigé. Des adaptations du firmware connu, dont l'attribution de
  watts à une adresse spécifique, restent héritées de l'émulateur original.
- Signature ad hoc seulement ; pas de notarisation Apple ou de signature
  Developer ID. Le build est destiné à l'utilisation et aux essais locaux.

La CI du fork construit les deux architectures et exécute les tests synthétiques
sans avoir accès aux dumps personnels. Consulter le résultat du workflow sur
GitHub pour connaître son état effectif.
