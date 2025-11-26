# 📡 Projet BIP BIP ECE

## 🎯 Objectifs du projet
- Concevoir, assembler et tester un **bipeur** capable de recevoir et d’envoyer des messages textuels.  
- Appliquer les acquis en **Électronique Numérique** et en **méthodologie de projet (cycle en V)**.  
- Travailler en équipe de 3 et présenter les résultats lors des soutenances.  

---

## 🧱 Architecture fonctionnelle
Fonction principale : échanger des messages textuels de façon autonome.  

Fonctions secondaires :
- FS1 : Composer un message  
- FS2 : Envoyer un message  
- FS3 : Alerter à la réception d’un message  
- FS4 : Afficher un message reçu  
- FS5 : Régler les paramètres  

---

## ✅ Exigences fonctionnelles
- EF1 : Message ≥ 100 caractères  
- EF2 : Tous les caractères de la langue française utilisables  
- EF3 : Priorité associée au message  
- EF4 : Transmission complète du message  
- EF5 : Ajout du pseudo de l’expéditeur  
- EF6 : Signal sonore à réception  
- EF7 : Signal lumineux selon priorité  
- EF8 : Arrêt des alertes sur commande  
- EF9 : Réception autonome à tout moment  
- EF10 : Affichage du pseudo de l’expéditeur  
- EF11 : Lecture complète du message  
- EF12 : Sélection du canal radio  
- EF13 : Réglage du pseudo  
- EF14 : Choix de la sonorité parmi plusieurs  
- EF15 : Mémorisation des paramètres au redémarrage (EEPROM)  

---

## 🔌 Connexions matérielles
| **Périphérique**     | **Signal**              | **Pin Arduino** |
|----------------------|-------------------------|------------------|
| Encodeur SW2         | A (pull-up)             | D3               |
|                      | B (pull-up)             | D4               |
|                      | Bouton (pull-up)        | D2               |
| LED RGB D2           | Rouge                   | D5               |
|                      | Vert                    | D6               |
|                      | Bleu                    | D9               |
| NRF24 U1             | SPI                     | Pins SPI         |
|                      | CE                      | D7               |
|                      | CSN                     | D8               |
| Buzzer BZ1           | Buzzer                  | D10 ❗            |
| Écran OLED J1        | I²C (pull-up)           | Pins I²C         |
| Bouton SW3           | Bouton (pull-up)        | A6 ⚠️            |

❗ D10 : doit être configurée en sortie si la librairie SPI.h est utilisée  
⚠️ A6 : uniquement utilisable comme **entrée analogique**  

---

## 🧪 Protocoles de test

### 🔊 Buzzer
- **But** : vérifier que le buzzer émet un son.  
- **Protocole** :  
  1. Configurer la pin D10 en sortie.  
  2. Envoyer un signal HIGH pendant 500 ms puis LOW pendant 500 ms.  
  3. Observer l’émission sonore répétée.  
- **Preuve attendue** : son audible en alternance.

---

### 🖥️ Écran OLED
- **But** : vérifier l’affichage de texte.  
- **Protocole** :  
  1. Initialiser la librairie Adafruit SSD1306.  
  2. Envoyer le texte “Test OLED OK”.  
  3. Observer l’affichage sur l’écran.  
- **Preuve attendue** : texte lisible sur l’OLED.

---

### 🌈 LED RGB
- **But** : vérifier l’allumage séquentiel des trois couleurs.  
- **Protocole** :  
  1. Configurer les pins D5, D6, D9 en sortie.  
  2. Allumer successivement Rouge, Vert, Bleu avec délai de 1s.  
  3. Observer la variation de couleur.  
- **Preuve attendue** : LED qui change de couleur toutes les secondes.

---

### 📡 nRF24
- **But** : vérifier l’envoi d’un message radio.  
- **Protocole** :  
  1. Initialiser le module RF24 avec CE=D7 et CSN=D8.  
  2. Envoyer le message “Hello” via SPI.  
  3. Vérifier la réception sur un second module ou via Serial Monitor.  
- **Preuve attendue** : message reçu ou confirmation “Message envoyé”.

---

### 🔄 Encodeur rotatif
- **But** : vérifier la détection de rotation gauche/droite.  
- **Protocole** :  
  1. Configurer les pins D3 et D4 en entrée pull-up.  
  2. Lire l’état de la pin A et comparer avec l’état précédent.  
  3. Déterminer la direction selon la pin B.  
- **Preuve attendue** : affichage “Rotation gauche/droite” dans le Serial Monitor.

---

### 🔘 Bouton utilisateur
- **But** : vérifier la détection d’un appui.  
- **Protocole** :  
  1. Configurer la pin D2 en entrée pull-up.  
  2. Lire l’état du bouton.  
  3. Si appuyé, afficher “Bouton appuyé” dans le Serial Monitor.  
- **Preuve attendue** : message affiché lors de l’appui.

---

## 🧑‍🏫 Soutenances

### Soutenance 1 (5 pts)
- Présentation orale (10 min) : schémas, algos, protocoles, preuves.  
- Discussion (5 min) : compréhension du projet, vérification des soudures.  

### Soutenance 2 (15 pts)
- Présentation orale (10 min) : synthèse du projet, résultats, tests.  
- Démonstration (5 min) : validation du système.  
- Discussion (5 min) : compréhension, anticipation des questions.  

---

## 📚 Ressources documentaires
- TP d’Électronique Numérique  
- Toolbox ECE  
- Bibliothèque Proteus Arduino Nano  
- Tutoriels NRF24L01, LED RGB, encodeur rotatif  
- EEPROM interne ATmega328  
- ⚠️ Toutes les ressources utilisées doivent être **citées selon le standard IEEE**  

---

## 👥 Équipe projet
- **Mathieu** – Organisation du projet, répartition des tâches
- **Tristan** – Soudure
- **Jules** – Conception matérielle, tests unitaires 

---

## 🚀 Prochaines étapes
- Finaliser les tests unitaires  
- Implémenter le protocole de communication  
- Intégrer les modules dans un système complet  
- Préparer la soutenance finale avec démonstration  
