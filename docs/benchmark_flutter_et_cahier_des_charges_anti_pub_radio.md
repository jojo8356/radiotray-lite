# Benchmark Flutter vs stack de RadioTray Lite et cahier des charges d’un anti-pub radio par IA

**Version :** 0.1 — **Date :** 19 octobre 2026

**Nature :** benchmark d’architecture qualitatif et spécification produit/technique. Ce document ne prétend pas fournir des mesures de performances qui n’ont pas été exécutées.

## 0. Synthèse et décision recommandée

1. **Ne pas migrer toute l’application vers Flutter uniquement pour ajouter un anti-pub.** RadioTray Lite a déjà une intégration native GTK/AppIndicator et un lecteur GStreamer. Le décodeur audio et le moteur de détection doivent rester proches de cette pile.
2. **Ajouter un détecteur local, asynchrone et hybride** à la chaîne GStreamer : classification temporelle de l’audio + empreintes acoustiques pour les publicités déjà connues + reconnaissance vocale facultative pour enrichir les indices.
3. **Faire taire la sortie locale, sans mettre le flux en pause.** Pour éviter d’entendre le début d’une publicité avant la décision du modèle, prévoir un court tampon de lecture réglable. Une radio en direct ne permet pas de « retirer » le segment déjà diffusé ni de retrouver le programme manqué sans décalage/stockage.
4. **Démarrer en mode observation**, puis activer une sourdine automatique prudente uniquement après validation sur des radios et des publicités non vues à l’entraînement. En cas de doute ou de panne du modèle, le son continue : comportement *fail-open*.
5. **Flutter reste une option d’interface**, surtout si l’objectif devient une application multiplateforme avec une interface plus riche. Ce choix n’élimine pas les contraintes Linux du tray, de Wayland ni le besoin d’une intégration native pour l’audio.

Le mot « complet » doit être compris comme une détection continue avec plusieurs méthodes et commandes utilisateur, **pas comme la garantie de supprimer 100 % des publicités sur toutes les radios**. Les spots nouveaux, locaux, musicaux, lus par un animateur ou mêlés au programme rendent une promesse universelle irréaliste sans données et validation propres à chaque contexte.

---

## 1. Périmètre de l’analyse

### 1.1 Stack actuelle du dépôt

Le dépôt est une application de bureau **Linux, tray-first**, en C++11. La configuration CMake lie notamment GTK 3/gtkmm, GLib, GStreamer/gstreamermm, libcurl, libxml2, libmagic, libnotify et un backend AppIndicator/Ayatana (avec repli possible sur `GtkStatusIcon`). Le lecteur actuel crée un `Gst::PlayBin`, lui fournit une URI de flux et reçoit les événements du bus GStreamer. La compilation conserve aussi des chemins de compatibilité GStreamer/gstreamermm 0.10 et 1.x. Ces constats proviennent de `CMakeLists.txt`, `src/CMakeLists.txt`, `src/player.cpp` et `COMPATIBILITY.md` du dépôt.

C’est un bon point de départ pour un filtrage audio local : le son est déjà décodé par GStreamer et les actions de lecture sont gérées nativement. En revanche, le lecteur actuel n’expose pas encore au modèle une branche PCM d’analyse ni une sortie retardée et contrôlable pour une sourdine alignée sur les horodatages.

### 1.2 Ce que signifie ici « anti-pub »

Pour un flux radio **en direct**, le produit détecte un segment probablement publicitaire et agit sur **la lecture locale**. Il ne modifie pas la radio, ne retire pas le segment de son flux source et ne reconstitue pas le contenu éditorial diffusé pendant la sourdine.

Modes fonctionnels proposés :

- **Désactivé** : lecture normale.
- **Observation** : détecter et afficher les segments, sans modifier le son.
- **Sourdine prudente** : rendre silencieux les spots payants suffisamment certains, puis rétablir le son à la fin.
- **Saut dans un différé** *(évolution)* : avancer le curseur de lecture d’un tampon local. Ce mode ajoute stockage, latence et exigences de conformité; il n’est pas nécessaire au premier produit.
- **Bascule vers une station de secours** *(option utilisateur)* : solution de remplacement, distincte de la suppression d’une publicité.

Les promotions de la station, jingles, mentions de parrainage et publicités lues par les animateurs doivent être des catégories séparées. Les assimiler toutes aux spots payants augmente fortement le risque de couper du contenu éditorial.

---

## 2. Benchmark d’architecture : Flutter ou stack native actuelle ?

### 2.1 Comparaison qualitative

| Critère | C++ / GTK 3 / GStreamer actuels | Flutter desktop | Conclusion pour RadioTray Lite |
|---|---|---|---|
| **UI de petite fenêtre, formulaire, liste adaptative** | Possible avec GTK, mais le layout, les contraintes, les tailles de texte et les cas limites demandent du code GTK et des tests manuels. | Très bon outillage déclaratif pour les contraintes, listes défilantes, formulaires, réglages de police et layouts réactifs. | Avantage Flutter pour refaire une interface plus riche; avantage moins déterminant pour une app essentiellement tray. |
| **Lecture audio Internet** | GStreamer est déjà intégré au lecteur via `PlayBin`; codecs, bus et états de lecture sont déjà dans la stack. | Flutter ne fournit pas par lui-même un remplacement équivalent de cette chaîne. Il faudrait conserver GStreamer via plugin/FFI ou ajouter un autre moteur natif. | Garder GStreamer; une migration de l’interface ne devrait pas réécrire le lecteur. |
| **Tray Linux** | AppIndicator/Ayatana est géré directement dans le code GTK, avec plusieurs backends et un menu natif. Il reste des différences entre panneaux et sessions. | Il faut un plugin ou un runner Linux natif. `tray_manager` récent s’appuie sur StatusNotifierItem, mais n’expose sous Linux ni les clics sur l’icône ni sa position; GNOME peut nécessiter l’extension AppIndicator. | La migration ne simplifie pas le problème le plus spécifique à Linux; garder l’intégration existante est plus direct. |
| **Popup et limites d’écran** | Réalisable via GTK/GDK, mais demande de traiter work area, plusieurs écrans et échelle. | `window_manager` et `screen_retriever` apportent des APIs pratiques pour taille/position et informations d’écrans; l’ancrage exact à l’icône tray reste absent sous Linux dans le plugin examiné. | Flutter facilite le code de fenêtre, pas les pouvoirs que le compositeur ou le tray ne fournit pas. Tester X11 et Wayland séparément. |
| **Poids mémoire et taille installée** | En principe, peu de surcoût de framework si les dépendances GTK/GStreamer sont déjà présentes sur le système; dépend du packaging et des bibliothèques embarquées. | Le moteur Flutter et ses ressources ajoutent un coût de base et une autre chaîne de distribution. | Avantage attendu à la stack native pour un petit utilitaire, mais **aucun chiffre ne doit être affirmé sans mesure**. |
| **Inférence IA** | Intégration directe possible en C/C++ avec une file de travail et une bibliothèque d’inférence native. | Il faudrait de toute façon une bibliothèque native ou un service séparé pour une inférence CPU efficace; Flutter n’améliore pas le modèle. | L’IA ne justifie pas de migrer l’UI. |
| **Multiplateforme** | Le dépôt et son tray sont Linux/GTK centrés. | Flutter facilite la réutilisation d’une grande partie de l’interface sur Linux, Windows et macOS. Les intégrations de tray/audio restent spécifiques à chaque OS. | Flutter devient intéressant si le multiplateforme est un objectif produit réel, pas seulement pour corriger une todo-list. |
| **Build et maintenance** | Stack déjà connue par le projet, mais nombreuses dépendances système et compatibilité AppIndicator/GStreamer à maintenir. | Une couche UI Dart et une couche native/plugin à maintenir; le build Linux demande aussi une toolchain native GTK/CMake. | Éviter de maintenir deux stacks sans bénéfice produit justifié. |

Flutter documente la prise en charge des applications desktop Linux et des plugins, ainsi que l’intégration possible de code C/C++ natif. Cela confirme qu’un front Flutter peut cohabiter avec GStreamer, mais ne rend pas cette intégration gratuite. Les outils de build Linux Flutter demandent notamment CMake, Ninja, `pkg-config` et GTK 3. [Flutter desktop](https://docs.flutter.dev/platform-integration/desktop) · [build Linux et code natif](https://docs.flutter.dev/platform-integration/linux/building) · [setup Linux](https://docs.flutter.dev/platform-integration/linux/setup)

Sur le plan UI, `LayoutBuilder`, `ListView` et `TextField.onSubmitted` auraient probablement rendu plus simples les problèmes de largeur, de ligne coupée, de police et de validation. Pour le tray, la conclusion est différente : la documentation actuelle de `tray_manager` décrit une intégration StatusNotifierItem, dépendante d’un panel hôte; sous Linux l’API ne fournit pas les événements de clic ni les bounds de l’icône. [Layout Flutter](https://docs.flutter.dev/ui/widgets/layout) · [tray_manager](https://pub.dev/packages/tray_manager) · [window_manager](https://pub.dev/packages/window_manager) · [screen_retriever](https://pub.dev/packages/screen_retriever)

À la date du benchmark, les pages pub.dev indiquent `tray_manager 0.8.0`, `window_manager 0.5.2` et `screen_retriever 0.3.0`. Les versions récentes de `tray_manager`/`screen_retriever` visent un SDK Flutter/Dart récent (Flutter 3.47 / Dart 3.13 pour les versions vérifiées). Pour une branche Flutter plus ancienne, il faudrait épingler des versions compatibles et vérifier que leurs capacités Linux sont les mêmes; ne pas copier un exemple de plugin sans vérifier sa version.
### 2.2 Les irritants de la todo-list Slint évoquée précédemment

Cette todo-list n’est pas l’interface actuelle de RadioTray Lite : elle sert ici de cas concret pour comparer les capacités UI. Les problèmes de rendu et les problèmes de shell Linux ne se traitent pas au même niveau.

| Irritant | Apport concret de Flutter | Limite à ne pas masquer |
|---|---|---|
| Popup hors écran | Lire la zone de travail, calculer la taille, puis borner x/y à ses limites avec `screen_retriever` et `window_manager`. | Sous Linux, `tray_manager` ne donne pas le rectangle de l’icône; le popup ne peut donc pas être ancré sûrement à celle-ci avec ce seul plugin. Les coordonnées de fenêtres doivent être testées sous X11 et Wayland. |
| Clic droit pour refermer | Cacher la fenêtre sur perte de focus/Échap; utiliser des actions de menu explicites pour ouvrir/cacher. | L’API Linux récente de `tray_manager` ne transmet pas l’événement de clic tray. Pour exiger le geste exact sur l’icône, il faut une couche Linux native ou changer l’interaction. |
| Valider un réglage | Faire appeler la même fonction de sauvegarde par `TextField.onSubmitted` et un bouton « Enregistrer ». | La validation du contenu et les messages d’erreur restent à concevoir. |
| Polices trop grandes | Centraliser les tailles dans le thème et tenir compte de l’échelle de texte; proposer un réglage borné si nécessaire. | Flutter n’empêche pas des tailles codées en dur ou un mauvais choix typographique. |
| Ligne N+1 coupée | Donner une hauteur explicite à la zone de liste et utiliser une liste défilante avec hauteur de ligne connue, au lieu d’étirer toute la fenêtre. | Le calcul doit prendre en compte le texte qui s’enroule et l’agrandissement des polices. |
| Largeur configurable | Persister une largeur utilisateur et la borner à une plage valide dans la zone de travail; adapter le contenu aux contraintes du parent. | Les contraintes widget ne remplacent pas la demande de redimensionnement de la fenêtre système. |
| Icône tray avec « ! » | Corriger l’asset si le point d’exclamation fait partie du fichier icône; vérifier aussi l’icône `.desktop` et le packaging. | Si le « ! » est ajouté par le panneau ou le backend comme indicateur d’état, ce n’est pas un défaut de widget Flutter; il faut identifier la couche qui le dessine. |

**Bilan :** Flutter aurait facilité les problèmes de formulaire, de police, de largeur et d’overflow. Il aurait fourni de meilleurs outils de calcul pour le popup, mais le clic tray, son rectangle et l’icône restent des questions d’intégration desktop/Linux.

### 2.3 Ce qui n’est pas un benchmark mesuré

**Aucun test comparatif CPU/RAM/latence n’a été exécuté.** Dans l’environnement de l’audit, `cmake` et les dépendances de développement GTK/GStreamer ne sont pas installés; le projet ne peut donc pas y être compilé pour comparer des binaires. Le tableau ci-dessus est une comparaison de conception, pas un résultat expérimental.

Protocole recommandé avant une décision de migration :

1. Construire en mode release l’application C++ actuelle, un petit prototype Flutter et, si pertinent, le prototype hybride Flutter + moteur GStreamer natif.
2. Utiliser la même machine, le même bureau Linux, le même flux station et la même durée d’écoute; répéter au moins trois fois et rapporter médiane et p95.
3. Mesurer démarrage jusqu’au premier son, RSS à vide et en lecture, CPU moyen/pic, taille installée, temps de chargement du modèle, facteur temps réel d’inférence, latence de sourdine et pertes/retards audio.
4. Tester séparément GNOME avec extension AppIndicator, un panel StatusNotifier, X11 et Wayland. Une capture d’écran seule ne valide pas la chaîne tray.
5. Publier les versions SDK/plugins, dépendances installées, modèle utilisé et commandes de mesure avec les résultats.

**Recommandation de stack :** conserver l’application C++/GTK/GStreamer pour le premier anti-pub; créer le détecteur en C++ avec entraînement/évaluation Python hors ligne. Réexaminer Flutter seulement si une interface multiplateforme devient un objectif à financer. Si Flutter est retenu plus tard, garder GStreamer et le tray comme intégrations natives, plutôt que réécrire l’audio en Dart.

---

## 3. Cahier des charges produit

### 3.1 Objectif

Détecter en temps réel, sur le poste de l’utilisateur, les **spots publicitaires payants** d’un flux de radio Internet, et rendre silencieux uniquement les intervalles dont la probabilité dépasse un seuil prudent. Donner à l’utilisateur un contrôle immédiat, un mode d’observation et un moyen simple de corriger les erreurs.

### 3.2 Public cible et première version

- Utilisateur principal : auditeur de radios Internet qui écoute depuis RadioTray Lite sous Linux.
- V1 : flux audio Internet pouvant être décodés par la configuration GStreamer prise en charge, avec une validation initiale sur des stations francophones. Le moteur doit accepter plusieurs langues à terme; la couverture réelle dépendra des données et du modèle pour chaque langue.
- La liste des flux reste celle gérée par l’application. Une station sans données de test suffisantes peut fonctionner en **observation seulement**.

### 3.3 Hors périmètre V1

- Promesse de blocage intégral sur toutes les radios, langues, pays, podcasts ou contenus personnalisés.
- Reconnaissance infaillible des placements de produits et des publicités intégrées à la parole d’un animateur.
- Reconstruction du programme supprimé ou accès à une version « propre » non fournie par la station.
- Envoi par défaut de l’audio à un serveur, entraînement automatique sur les enregistrements des utilisateurs, ou conservation permanente de flux complets.
- Pause du pipeline GStreamer comme méthode de sourdine : un flux live continuerait à avancer, se désynchroniserait ou se comporterait différemment selon son buffering.

---

## 4. Exigences fonctionnelles

### 4.1 Lecture et analyse

- **FR-01 — Compatibilité.** Ne pas changer l’URL de lecture ni les décodeurs de playlists existants sans test de non-régression. Le détecteur ne doit pas nécessiter que chaque station fournisse des métadonnées spécifiques.
- **FR-02 — Analyse du signal décodé.** Analyser une copie mono PCM normalisée, par exemple à 16 kHz, après décodage. Les métadonnées du flux, titres ICY, informations HLS ou marqueurs de régie, lorsqu’ils existent, sont des indices supplémentaires et jamais une dépendance obligatoire.
- **FR-03 — Non-blocage.** Les callbacks GStreamer ne font ni transcription ni inférence lourde. Ils déposent des blocs horodatés dans une file bornée et rendent la main immédiatement. Si le détecteur est trop lent, il abandonne des blocs d’analyse selon une politique contrôlée, sans bloquer la lecture.
- **FR-04 — Horodatages.** Toute prédiction est associée aux timestamps du média et à un intervalle de début/fin. Le module de lecture et le détecteur doivent utiliser la même base temporelle malgré reconnexion, buffering ou changement de flux.
- **FR-05 — Reconnexion.** Au changement d’URI ou après une reconnexion, vider le contexte temporel et les prédictions de l’ancien flux; ne pas conserver un état « publicité » issu de la station précédente.

### 4.2 Détection et décision

- **FR-06 — Classes.** Le modèle au minimum distingue : spot payant, parole éditoriale/animateur, musique, promotion de la station, jingle/transition, actualités/météo/chronique et inconnu. Parrainage/host-read peut être une classe distincte.
- **FR-07 — Sortie probabiliste.** Produire une probabilité et une classe, pas seulement un booléen. Les seuils sont calibrables par station et par catégorie.
- **FR-08 — Lissage temporel.** Éviter les bascules son/muet à chaque fenêtre : utiliser une hystérésis, un minimum de durée et un lissage tenant compte du contexte. Les limites de segment doivent rester horodatées et inspectables.
- **FR-09 — Trois modes.** Off, observation et sourdine automatique. L’observation est le mode initial recommandé. L’activation du mode automatique est explicite et réversible.
- **FR-10 — Seuil prudent.** Si les indices sont ambigus, si le modèle est indisponible ou si la station n’est pas validée, **ne pas couper le son**. Un profil « plus agressif » peut être proposé comme choix explicite, jamais comme comportement implicite.
- **FR-11 — Action audio.** En mode de sourdine, le flux continue à être décodé; seule la sortie locale est atténuée ou mise à zéro pour l’intervalle prévu. Le retour au son doit être progressif et éviter les clics audio.
- **FR-12 — Annulation.** Exposer une action immédiate « Ce n’était pas une pub » / « Rétablir le son ». L’utilisateur peut corriger la classe; la correction ne déclenche aucun envoi réseau par défaut.
- **FR-13 — Signal visuel.** Indiquer « analyse », « publicité probable », « son coupé », « incertain » ou « détecteur indisponible » dans le menu/état de l’application. Une icône tray seule ne suffit pas à communiquer une décision importante.
- **FR-14 — Modèle.** Afficher la version du modèle et permettre de revenir à une version précédente. Les mises à jour sont vérifiables par empreinte cryptographique; le modèle n’exécute pas de code fourni par un serveur.

### 4.3 Tampon de décision et expérience live

Une décision basée sur plusieurs secondes de signal ne peut pas toujours reconnaître le début exact d’un spot sans retard de lecture. La V1 doit donc choisir explicitement entre :

- **latence minimale** : détection après le début, avec risque d’entendre les premières secondes;
- **courte avance de lecture** : conserver un tampon RAM horodaté et retarder le rendu, par défaut environ 3–5 secondes, afin que le détecteur puisse décider avant la sortie du segment concerné.

La deuxième option est recommandée pour l’auto-sourdine; la durée doit être réglable et affichée. Un tampon de quelques secondes en PCM mono est faible en mémoire; la complexité réside surtout dans le maintien de timestamps cohérents et la synchronisation, pas dans le stockage brut.

Le comportement V1 est une **sourdine**, pas un saut : pendant le spot, le direct avance. Le saut en différé exige un tampon de lecture distinct, une commande de retour au direct, des contrôles de stockage et un examen juridique avant livraison.

---

## 5. Spécification IA et stratégie de données

### 5.1 Approche recommandée : hybride, pas « un LLM qui écoute »

La détection devrait combiner plusieurs indices qui se complètent :

1. **Classifieur acoustique temporel** : caractéristique du son, type de parole/musique, transitions, niveau, dynamique et motifs de spots. Modèle entraîné ou affiné sur des extraits radio annotés.
2. **Empreintes acoustiques** : reconnaître une création publicitaire déjà vue, même si elle revient avec un faible changement de niveau, de compression ou de bruit de diffusion. Très utile à haute confiance pour une publicité connue, mais ne découvre pas à lui seul un nouveau spot.
3. **ASR facultative** : transformer des fenêtres de parole en texte; des indices tels que marque, prix, URL, numéro de téléphone ou appel à l’action enrichissent la décision. Une transcription imparfaite ne doit jamais suffire seule à couper une émission.
4. **Contexte et séquence** : jingle de séparation, succession de spots, changements de format et répétitions propres à une station. Le contexte augmente ou diminue la confiance; il ne doit pas remplacer la classification.
5. **Règles de sécurité** : seuils élevés et hystérésis pour la sourdine automatique; si les sources d’indices se contredisent, revenir à « incertain ».

Le projet Adblock Radio, archivé en avril 2021 et non intégré à RadioTray Lite, utilisait déjà une combinaison de classification spectrale par réseau de neurones et de recherche d’empreintes d’annonces/jingles; son dépôt décrit des blocs PCM d’environ une seconde et une étape de post-traitement. C’est un précédent architectural utile, pas une preuve de qualité universelle ni une dépendance à reprendre telle quelle. [Dépôt Adblock Radio](https://github.com/adblockradio/adblockradio) · [explication de la méthode](https://www.adblockradio.com/blog/2018/11/15/designing-audio-ad-block-radio-podcast/index.html)

Une étude récente RadIA rapporte un F1 macro de 87,76 sur son propre corpus avec transcription et classification de texte. Ce résultat soutient l’intérêt d’un signal linguistique, mais ne constitue pas une garantie sur d’autres stations, langues ou publicités. [RadIA](https://arxiv.org/html/2403.03538v1)

### 5.2 Modèles de départ

- Utiliser un modèle général comme **YAMNet** ou **PANNs** comme baseline de tagging/embedding, pas comme détecteur final « ad / non-ad ». YAMNet prédit 521 événements AudioSet et produit des embeddings; PANNs est également un modèle général d’audio tagging. Ils aident à démarrer le transfert d’apprentissage, mais un modèle générique ne connaît pas automatiquement la frontière entre une pub, une promo de station et une chronique. [YAMNet/TensorFlow](https://www.tensorflow.org/tutorials/audio/transfer_learning_audio) · [PANNs](https://github.com/qiuqiangkong/audioset_tagging_cnn)
- Entraîner une petite tête de classification ou un modèle temporel adapté à des extraits radio. Mesurer plusieurs tailles de modèle; ne pas embarquer un très grand modèle général par défaut si une représentation plus légère suffit.
- Exporter l’inférence vers un runtime natif tel qu’**ONNX Runtime** si le pipeline retenu et les licences du modèle le permettent. ONNX Runtime fournit des APIs C/C++ et permet de déployer des modèles issus de plusieurs frameworks. [ONNX Runtime](https://onnxruntime.ai/inference)
- Si l’ASR apporte un gain mesuré, l’ajouter comme voie facultative. `whisper.cpp` offre une inférence locale en C/C++; les coûts de calcul, le modèle choisi et la latence doivent être mesurés. L’élément GStreamer `whispertranscriber` documente une stratégie par blocs pour le live et donne un exemple avec environ six secondes de latence : raison de ne pas rendre l’ASR obligatoire dans le chemin de décision V1. [whisper.cpp](https://github.com/ggml-org/whisper.cpp) · [GStreamer whispertranscriber](https://gstreamer.freedesktop.org/documentation/whisper/index.html)

### 5.3 Taxonomie d’annotation

Chaque intervalle annoté doit avoir une classe principale et des bornes temporelles cohérentes :

| Classe | Traitement par défaut |
|---|---|
| `paid_ad` — publicité payante | Candidate à la sourdine automatique si la confiance dépasse le seuil. |
| `sponsor_read` — parrainage/lecture de marque par l’animateur | Observation seulement par défaut; option utilisateur distincte. |
| `station_promo` — autopromotion de station/émission | Ne pas couper par défaut. |
| `jingle_transition` | Indice de frontière, pas une publicité à lui seul. |
| `music` | Ne pas couper. |
| `program_speech` / `host` | Ne pas couper. |
| `news_weather_traffic` | Ne pas couper. |
| `silence_unknown` | Ne pas couper; réinitialiser ou réduire la confiance selon le contexte. |

### 5.4 Constitution du corpus

- **Pilote de faisabilité :** commencer par environ 20–40 heures de flux enregistrés légalement, issus de plusieurs stations, plages horaires et types de programmes. Ce volume sert à découvrir les erreurs, pas à certifier une couverture générale.
- **Évaluation V1 :** viser au moins une centaine d’heures indépendantes, plusieurs stations et journées, avec des créations publicitaires absentes de l’entraînement. Ajuster le volume après analyse des erreurs et du nombre d’événements rares.
- Annoter les débuts/fins au niveau sub-seconde ou à la seconde selon la capacité humaine; définir un guide d’annotation et faire annoter en double un échantillon pour mesurer l’accord.
- Séparer entraînement, validation et test **par station, période et création publicitaire**. Un même spot ne doit pas apparaître dans train et test, sinon les performances de fingerprinting gonflent artificiellement.
- Documenter origine, autorisations, langue, codec, niveau, date et licences des données et des modèles. Les extraits d’écoute ne sont pas conservés par défaut dans l’application finale.
- Utiliser le feedback utilisateur uniquement sur consentement explicite et après décision sur la base légale, la durée de conservation, la minimisation et la protection des données. Le RGPD pose notamment les principes de limitation de finalité, minimisation et conservation limitée. [Article 5 RGPD](https://gdpr-info.eu/art-5-gdpr/)

---

## 6. Architecture technique cible

### 6.1 Chemin audio proposé

```text
URI radio
  → GStreamer / PlayBin et décodeurs actuels
  → audio décodé + timestamps
      ├─ branche sortie : tampon de décision → gate volume → autoaudiosink
      └─ branche analyse : tee/queue bornée → conversion mono PCM
                           → appsink → file courte → worker de détection
                                               ├─ modèle acoustique
                                               ├─ fingerprint facultatif
                                               └─ ASR facultative
                           ← événements horodatés → contrôleur de sourdine
```

Une intégration plausible est un `audio-sink` GStreamer personnalisé contenant un `tee`, une file indépendante vers la sortie audio et une autre vers `appsink`. Chaque branche doit être séparée par une `queue`; les callbacks de `appsink` doivent rester courts et sa file doit avoir des limites, pour qu’une inférence lente ne bloque pas le flux et ne consomme pas une mémoire croissante. Les guides GStreamer décrivent le pattern `tee` + files par branche et les risques de blocage/accumulation de la file `appsink`. [tee](https://gstreamer.freedesktop.org/documentation/coreelements/tee.html) · [appsink/pipeline](https://gstreamer.freedesktop.org/documentation/application-development/advanced/pipeline-manipulation.html)

L’architecture exacte devra être prototypée avec la version de GStreamer/gstreamermm retenue. Le dépôt conserve des chemins 0.10 et 1.x; un nouvel élément `appsink`/`tee`, le runtime IA et leurs dépendances peuvent rendre nécessaire une décision séparée sur l’abandon ou non de GStreamer 0.10.

### 6.2 Répartition du code

- **Thread principal GTK** : UI, menus, état, configuration; jamais d’inférence ou de requête réseau bloquante.
- **Thread streaming GStreamer** : décodage; callbacks d’analyse limités à récupérer/copier ou référencer un bloc PCM et son timestamp.
- **Worker IA** : fenêtrage, calcul de caractéristiques, inférence, fingerprint et lissage temporel.
- **Contrôleur de lecture** : applique une décision au segment de sortie correspondant, maintient l’état de sourdine, expose annulation et métriques.
- **Entraînement hors ligne** : Python pour préparation/annotation/évaluation; runtime C/C++ dans l’application. Si le worker plante, la lecture reste audible et l’application continue.

### 6.3 Contrat logiciel du détecteur

Événement minimal recommandé :

```text
DetectionEvent {
  station_id,
  media_start_ms,
  media_end_ms,
  class_label,
  confidence,
  model_version,
  evidence_flags
}
```

Le détecteur ne commande pas directement la sortie son. Il publie des événements horodatés; un contrôleur applique le seuil du profil utilisateur et la politique de sourdine. Cette séparation facilite les tests offline, les corrections manuelles et le remplacement du modèle.

---

## 7. Interface et réglages

- Entrée dans le menu tray : **Anti-pub : désactivé / observation / sourdine prudente**.
- Indicateur : écoute, modèle prêt, spot probable, sourdine active, incertitude, erreur ou flux non analysable.
- Réglages : catégories à supprimer, seuil prudent/plus agressif, délai de lecture, lancement automatique, politique de feedback et mise à jour des modèles.
- Actions immédiates : rétablir le son, signaler un faux positif, « ne pas couper cette catégorie », ouvrir le journal local des décisions.
- Un historique minimal peut montrer heures et classes détectées sans stocker l’audio.
- Sous Linux, conserver d’abord le menu GTK/AppIndicator existant plutôt que d’exiger un clic de tray universel. Les panneaux, l’extension GNOME et la distinction X11/Wayland doivent être validés en conditions réelles.
- Les widgets Flutter seraient un choix possible pour une future fenêtre de réglages responsive; ils ne remplacent ni la branche audio GStreamer ni la gestion tray Linux.

---

## 8. Exigences non fonctionnelles et critères d’acceptation

Les objectifs ci-dessous sont des **seuils proposés à tester**, pas des performances déjà obtenues.

### Qualité de détection

- Rapport séparé par station, langue, classe et spot connu/nouveau.
- Mesurer précision, rappel, F1, erreur de frontière médiane/p95, secondes de programme injustement mises en sourdine par heure et minutes de publicité manquées par heure.
- Avant d’activer l’auto-sourdine dans un profil, objectif de lancement : précision d’au moins **97 %** sur les spots payants évalués et rappel d’au moins **80 %**, avec un budget inférieur à **60 secondes de faux silence par 10 heures d’écoute** sur le test indépendant. Ces valeurs sont un *go/no-go* à confirmer; si elles ne sont pas atteintes, livrer uniquement observation ou recommandations.
- Évaluer sur des stations non vues à l’entraînement et sur des créations publicitaires inconnues. Rapporter aussi la performance par station; une moyenne globale ne doit pas cacher une station inutilisable.

### Temps réel et robustesse

- Facteur temps réel du modèle acoustique seul : cible moyenne `RTF < 0,25` sur une machine de référence définie avant les tests; documenter CPU, compiler, runtime, modèle et threads. L’ASR est évaluée séparément.
- Aucun blocage de la boucle GTK, aucun underrun ou gel reproductible dû à l’analyse sur un test continu d’au moins 8 heures.
- Délai de décision réglable; mode retardé documenté, cible de sourdine appliquée au début du spot dans la plage du tampon de lecture choisi.
- Panne modèle, téléchargement interrompu, modèle invalide, file pleine ou PCM inconnu : lecture en *fail-open*, message discret et logs explicites.
- Utilisation mémoire/CPU et taille du modèle mesurées sur la machine de référence; publier un budget avant d’inclure une ASR ou un modèle plus lourd.

### Non-régression

- Démarrer, arrêter, mettre en pause et reprendre une station fonctionne même si le détecteur est désactivé ou indisponible.
- La gestion des erreurs, reconnexions, changement de station, playlists et états du lecteur reste testée.
- Mode observation et lecture d’un fichier de test produisent des événements stables reproductibles; mêmes fichiers d’entrée, mêmes timestamps et version du modèle donnent les mêmes résultats à tolérance numérique définie.

---

## 9. Tests, livraison et phases

### Plan de tests

1. **Unitaires** : conversion PCM, fenêtres, timestamps, normalisation, lissage/hystérésis, logique de seuil, détection de fin, fingerprint, configuration.
2. **Corpus offline** : replay de fichiers annotés sans sortie audio; métriques par classe/station et analyse de chaque faux positif/faux négatif.
3. **Intégration GStreamer** : flux simulés et réels, branches `tee`, files bornées, `appsink` lent, EOS, reconnexion, changement URI, formats testés.
4. **End-to-end** : injection d’un spot connu/inconnu dans un programme, vérification du début/fin de sourdine et absence de blocage/underrun.
5. **UX Linux** : GNOME, KDE ou panel StatusNotifier disponible, X11 et Wayland; backend AppIndicator choisi par le build; icône et menu avec l’extension GNOME documentée.
6. **Packaging** : `.deb` et AppImage, dépendances GStreamer, runtime IA, modèle manquant/invalidé, mise à jour et retour arrière.

### Jalons proposés

- **J0 — cadrage** : définir les stations/langues, catégories, mode d’action, machine de référence, droits d’annotation et formats ciblés.
- **J1 — prototype offline** : annoter le pilote, comparer classifieur acoustique, embeddings, fingerprints et ASR facultative; publier les erreurs, pas seulement le score moyen.
- **J2 — mode observation** : intégrer la branche GStreamer, le worker non bloquant et les événements UI; aucune action sur le son.
- **J3 — bêta sourdine prudente** : activer seulement les stations/profils passant le seuil de qualité; court tampon réglable, annulation facile, collecte opt-in.
- **J4 — durcissement** : corpus plus diversifié, packaging, mise à jour signée, tests longs, documentation utilisateur et politique de confidentialité.
- **J5 — différé/skip** : étude et prototype séparés après validation de la V1; stockage, quotas, chiffrement/effacement et reprise du direct à définir.

---

## 10. Risques, limites et décisions ouvertes

1. **Généralisation** : un spot musical ressemble à une chanson; une chronique promotionnelle ressemble à un spot; un animateur peut lire une offre. Le modèle et les seuils devront gérer ces cas au lieu de promettre l’impossible.
2. **Empreintes** : très fiables pour un spot déjà indexé, aveugles aux créations nouvelles; elles ne remplacent pas le classifieur.
3. **ASR** : peut aider sur l’appel à l’action, mais ajoute calcul, latence et erreurs de transcription. Ne pas mettre un LLM cloud dans la boucle temps réel par défaut.
4. **Latence** : retarder suffisamment l’écoute améliore l’alignement mais éloigne l’utilisateur du direct; sans tampon, quelques secondes de pub peuvent être audibles.
5. **Données et droits** : enregistrement, annotation, partage ou réutilisation de créations radiophoniques doit être examiné juridiquement selon les pays et les licences. Le comportement par défaut doit minimiser collecte et rétention.
6. **Compatibilité** : le code actuel garde une compatibilité ancienne GStreamer; l’intégration IA peut justifier une version minimale et un plan de migration distinct.
7. **Tray et sessions Linux** : une interface Flutter n’assure pas l’accès au rectangle d’icône ni à un clic tray sous Linux; GNOME peut demander une extension. Éviter de lier la fiabilité de la détection à un popup précis.
8. **Périmètre « toutes les radios »** : le produit peut traiter tous les flux décodables, mais la qualité de détection ne peut être garantie que sur les domaines testés. Chaque station/langue non validée doit rester en observation ou utiliser un seuil plus prudent.

### Décisions à prendre avant le développement

- Quelle est la priorité : **sourdine immédiate**, court retard invisible, saut en différé, ou bascule de station ?
- Quelles catégories l’utilisateur veut-il réellement supprimer : uniquement spots payants, ou également promos/parrainages ?
- Quelle première zone de couverture : stations françaises uniquement, ou plusieurs langues dès la V1 ?
- Le produit doit-il rester strictement local/offline, y compris pour les mises à jour des modèles ?
- Quelle machine de référence et quel budget de latence, mémoire, taille de paquet sont acceptables ?

---

## 11. Sources consultées

1. [RadioTray Lite — dépôt](https://github.com/thekvs/radiotray-lite), ainsi que les fichiers locaux `CMakeLists.txt`, `src/player.cpp`, `src/tray.cpp` et `COMPATIBILITY.md`.
2. [Flutter — desktop support](https://docs.flutter.dev/platform-integration/desktop), [Linux setup](https://docs.flutter.dev/platform-integration/linux/setup) et [build/intégration native](https://docs.flutter.dev/platform-integration/linux/building).
3. Plugins Flutter : [`tray_manager`](https://pub.dev/packages/tray_manager), [`window_manager`](https://pub.dev/packages/window_manager), [`screen_retriever`](https://pub.dev/packages/screen_retriever). Pages vérifiées le 19/10/2026; les versions récentes et les exemples plus anciens peuvent avoir des APIs différentes.
4. [GStreamer `tee`](https://gstreamer.freedesktop.org/documentation/coreelements/tee.html), [appsink et manipulation de pipeline](https://gstreamer.freedesktop.org/documentation/application-development/advanced/pipeline-manipulation.html).
5. [Adblock Radio — dépôt archivé](https://github.com/adblockradio/adblockradio) et [explication de sa méthode hybride](https://www.adblockradio.com/blog/2018/11/15/designing-audio-ad-block-radio-podcast/index.html).
6. [RadIA — Radio Advertisement Detection with Intelligent Analytics](https://arxiv.org/html/2403.03538v1).
7. [YAMNet — transfert d’apprentissage audio](https://www.tensorflow.org/tutorials/audio/transfer_learning_audio) et [PANNs — code et modèles audio](https://github.com/qiuqiangkong/audioset_tagging_cnn).
8. [ONNX Runtime — inférence](https://onnxruntime.ai/inference), [`whisper.cpp`](https://github.com/ggml-org/whisper.cpp), [GStreamer `whispertranscriber`](https://gstreamer.freedesktop.org/documentation/whisper/index.html).
9. [RGPD, article 5 — principes de traitement](https://gdpr-info.eu/art-5-gdpr/).
