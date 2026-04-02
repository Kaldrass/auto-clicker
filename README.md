# AutoClicker Windows en C++

Petit autoclicker natif Windows avec deux modes :

- `Point fixe` : spam rapide sur une position unique.
- `Macro` : enchainement d'etapes de clics configurable ligne par ligne.

Trois sorties sont disponibles :

- `Curseur reel` : deplace physiquement la souris puis clique. Compatible presque partout, mais monopolise la souris.
- `Fenetre ciblee` : essaie d'envoyer les clics directement a une fenetre Windows capturee, sans bouger le curseur. Pratique pour garder la main sur un autre ecran, mais compatibilite variable selon l'application cible.
- `Fenetre ciblee +` : variante plus aggressive qui envoie les messages de facon synchrone et peut tenter un focus temporaire. Peut mieux marcher sur certaines applis, mais reste non garantie.

## Fonctions

- Deux groupes de choix distincts : `Mode d'action` et `Mode d'envoi des clics`.
- UI qui active/desactive les champs selon le mode courant pour limiter les confusions.
- Capture rapide de la position actuelle de la souris.
- Choix du bouton : gauche, droit, milieu.
- Intervalle en millisecondes entre les clics.
- Nombre de repetitions par etape.
- Demarrage / arret avec boutons ou raccourci global `F6`.
- Capture de la souris pour le mode point fixe avec `F7`.
- Capture directe d'une etape macro avec `F8`.
- Capture de la fenetre cible sous la souris avec `F9`.
- Bouton `Tester un clic` pour verifier rapidement un mode cible avant la boucle.
- Option `Focus temporaire` pour le mode `Fenetre ciblee +`.
- Journal visuel dans l'interface pour suivre captures, tests, demarrages et erreurs.
- Affichage des bornes de tous les ecrans detectes.
- Liste de points sauvegardes reutilisable dans la macro.
- Sauvegarde / chargement de profils `.ini` contenant l'etat de l'UI, la macro et les points sauvegardes.

## Format macro

Chaque ligne de la zone macro suit ce format :

```text
x,y,button,count,delayMs
```

Exemples :

```text
-1200,500,left,5,20
-900,500,right,1,200
750,650,left,10,10
```

Signification :

- `x`, `y` : position ecran, valeurs negatives autorisees.
- `button` : `left`, `right` ou `middle`.
- `count` : nombre de clics a cette etape.
- `delayMs` : pause apres chaque clic de l'etape.

## Build

Depuis PowerShell :

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Executable genere :

```text
build/Release/AutoClicker.exe
```

## Utilisation

1. Lance l'application.
2. Dans `Mode d'action`, choisis `Point fixe` ou `Macro`.
3. Dans `Mode d'envoi des clics`, choisis `Curseur reel`, `Fenetre ciblee` ou `Fenetre ciblee +`.
4. Si tu utilises un mode `Fenetre ciblee`, place la souris au-dessus de la fenetre voulue puis clique `Capturer fenetre sous la souris` ou appuie sur `F9`.
5. Si tu testes `Fenetre ciblee +`, essaie d'abord sans `Focus temporaire`, puis active cette option si l'application ne reagit pas assez bien.
6. Utilise `Tester un clic` pour verifier rapidement si cette application accepte bien le mode cible.
7. Observe le bloc `Journal` pour voir le mode utilise, les tests, les demarrages et les erreurs de clic.
8. Consulte la zone `Bornes des ecrans detectes` pour reperer les coordonnees exactes de chaque moniteur.
9. Utilise `Capturer position souris` ou `F7` pour remplir `X` et `Y` du mode point fixe.
10. Utilise `Capturer pour la macro` ou `F8` pour ajouter une ligne dans la macro avec les parametres macro actuels.
11. Utilise `Sauver position actuelle` pour garder des points frequents, puis `Inserer dans la macro` pour les reutiliser.
12. Utilise `Sauvegarder un profil` / `Charger un profil` pour retrouver ta configuration complete.
13. Clique sur `Demarrer` ou appuie sur `F6`.
14. Re-appuie sur `F6` ou `Arreter` pour stopper.

## Important sur les modes Fenetre ciblee

- Ces modes ne deplacent pas le curseur systeme.
- Ils peuvent permettre de laisser tourner une macro sur un ecran pendant que tu utilises la souris ailleurs.
- Ils peuvent aussi etre utilises avec `Point fixe` si tu veux simplement spammer un seul point dans une fenetre ciblee.
- `Fenetre ciblee` envoie des messages de base, de facon asynchrone.
- `Fenetre ciblee +` envoie des messages plus stricts, de facon synchrone, et peut tenter un focus temporaire.
- Certaines applications reagissent mieux au mode `+`, d'autres pas du tout.
- Les jeux, applis Electron, rendu DirectX/OpenGL/Vulkan, applis protegees ou interfaces tres personnalisees peuvent toujours ignorer ces clics.
- Si aucun mode cible ne reagit correctement, repasse en mode `Curseur reel`.

## Notes

- Une valeur `0` dans `Repetitions` du mode point fixe signifie boucle infinie.
- Les coordonnees restent des coordonnees ecran absolues.
- Les profils rechargent le titre et la classe de la fenetre cible comme aide visuelle, mais le handle Windows n'est pas restaurable : il faut recapturer la fenetre avec `F9` apres chargement.
