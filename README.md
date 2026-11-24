# Wifi-Radar
<img width="1668" height="2048" alt="WhatsApp Image 2026-11-24 at 20 40 20" src="https://github.com/user-attachments/assets/6fc811fa-d4c8-4c06-a9c7-4967b01ccd1f" />

📡 WIFI RADAR
Un radar WiFi portabil care scanează rețelele din jur și le arată ca puncte pe un ecran, ca în filme.

WIFI RADAR

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
