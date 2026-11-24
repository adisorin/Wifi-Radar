# Wifi-Radar
<img width="1668" height="2048" alt="WhatsApp Image 2026-11-24 at 20 40 20" src="https://github.com/user-attachments/assets/6fc811fa-d4c8-4c06-a9c7-4967b01ccd1f" />

📡 WIFI RADAR
Un radar WiFi portabil care scanează rețelele din jur și le arată ca puncte pe un ecran, ca în filme.

✨ Ce face
Scanează rețelele WiFi din jur
Le arată pe un radar, cu distanță estimată pentru fiecare
Afișează TOP 5 cele mai apropiate rețele, cu nume, distanță, semnal și canal
Colorează punctele după cât de aproape sunt:
🔴 aproape
🟡 la distanță medie
🟢 departe
Are radar și în browser, pe telefon sau calculator
Când te conectezi la el, pagina radarului se deschide automat
🛠️ Ce placă este
Placă LilyGO cu ESP32
Ecran TFT color, 170x320
🎮 Butoanele
Buton	Ce face
Stânga	Schimbă raza radarului: 50m → 100m → 150m
Dreapta	Scanează din nou, pe loc
Pe ecran scrie jos ce face fiecare buton, ca să nu le uiți.

📐 Distanțele
Distanța se estimează după puterea semnalului:

Semnal	Distanță
Foarte puternic	Sub 1 m
Puternic	1 – 3 m
Mediu	12 m
Slab	50 m
Foarte slab	100 – 150 m
Distanțele sunt orientative. Pereții, mobila și orientarea antenei le pot schimba.

Punctele de pe radar nu arată direcția reală a rețelei, ci doar o ajută să nu se suprapună unele peste altele.

Creat de: Sorinescu Adrian




📡 WIFI RADAR
A portable WiFi radar that scans nearby networks and shows them as dots on a screen, like in the movies.

✨ What it does
Scans nearby WiFi networks
Shows them on a radar, with an estimated distance for each one
Displays the TOP 5 closest networks, with name, distance, signal and channel
Colors the dots by how close they are:
🔴 close
🟡 medium distance
🟢 far
It also has a radar in the browser, on phone or computer
When you connect to it, the radar page opens automatically
🛠️ What board it is
LilyGO board with ESP32
Color TFT screen, 170x320
🎮 Buttons
Button	What it does
Left	Changes the radar range: 50m → 100m → 150m
Right	Scans again, right now
The screen shows at the bottom what each button does, so you don't forget.

📐 Distances
The distance is estimated from the signal strength:

Signal	Distance
Very strong	Under 1 m
Strong	1 – 3 m
Medium	12 m
Weak	50 m
Very weak	100 – 150 m
Distances are approximate. Walls, furniture and antenna orientation can change them.

The dots on the radar do not show the real direction of the network, they are just placed so they don't overlap each other.


Created by: Sorinescu Adrian

