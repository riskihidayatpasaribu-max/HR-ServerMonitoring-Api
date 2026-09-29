# RH_ServerRoomMonitor

Firmware ESP32 NodeMCU-32S untuk monitoring ruang server: DHT22, MQ2, buzzer, 3 tombol panel, dan display 3.5" 480x320 (SPI langsung, driver sama dengan `AB_test_fullscreen.ino`). Data dikirim via MQTT ke broker lokal.

---

## 1. Yang WAJIB dikerjakan sebelum upload

### a. Dua resistor pull-up untuk tombol

GPIO34 dan GPIO35 adalah pin **input-only** dan **tidak punya pull-up internal**. Tanpa resistor luar, kakinya mengambang dan ESP32 akan membaca tombol "tertekan sendiri" karena noise.

```
3.3V ──[10k]──┬── GPIO35 ──┐
              │            │
              └────────────┴──[ tombol ]── GND
```

Lakukan hal yang sama untuk GPIO34. GPIO32 tidak perlu — sudah pakai `INPUT_PULLUP`.

### b. Pembagi tegangan untuk MQ2

Heater MQ2 butuh 5V, dan pin AO-nya mengikuti tegangan suplai. ADC ESP32 maksimal 3.3V.

```
MQ2 AO ──[10k]──┬── GPIO33
                │
              [20k]
                │
               GND
```

Ini menurunkan 5V menjadi ~3.33V. Tanpa ini, ADC GPIO33 bisa rusak saat asap tebal.

### c. Install library "Adafruit GFX Library"

Library Manager Arduino IDE -> cari **Adafruit GFX Library** -> Install. Kalau IDE menawarkan **Adafruit BusIO**, pilih *Install all*.

Tidak ada lagi langkah menimpa `User_Setup.h` di folder library. **TFT_eSPI tidak dipakai lagi** — kalau masih terpasang, biarkan saja, tidak mengganggu.

### d. Isi kredensial

Di `RH_Config.h`, ganti: `HR_WIFI_SSID`, `HR_WIFI_PASS`, `HR_MQTT_HOST` (IP broker Mosquitto kantor).

---

## 2. Wiring

### Sensor & aktuator

| Komponen | Pin ESP32 | Catatan |
|---|---|---|
| DHT22 DATA | GPIO17 | pull-up 4.7k–10k ke 3.3V |
| MQ2 AO | GPIO33 | lewat pembagi tegangan |
| Buzzer | GPIO16 | |
| Button 1 | GPIO32 | pull-up internal |
| Button 2 | GPIO35 | **pull-up 10k eksternal** |
| Button 3 | GPIO34 | **pull-up 10k eksternal** |

### Display

Pin sama persis dengan `AB_test_fullscreen.ino` (terbukti jalan). Semuanya bisa diubah di `RH_Config.h`.

| Fungsi | GPIO ESP32 |
|---|---|
| CS | GPIO5 |
| DC | GPIO2 |
| RST | GPIO4 |
| SCLK | GPIO18 |
| MOSI | GPIO23 |
| MISO | tidak dipakai (layar hanya ditulis) |
| 3.3V / 5V / GND | sesuai label pin display (**butuh 5V**, bukan 3.3V) |

Urutan init juga sama dengan tes: reset hardware -> `0x01` -> `0x11` -> COLMOD `0x55` -> MADCTL `0x28` -> `0x29`, SPI 20 MHz mode 0. Touch (XPT2046) tidak dipakai.

**Kalau layar putih / kosong:** bandingkan dulu wiring dengan tabel di atas, lalu upload `AB_test_fullscreen.ino` untuk memastikan panelnya sendiri sehat. Pin dan init di firmware ini identik dengan tes itu.

Backlight panel ini menarik ~150 mA dari 5V. Kalau ESP32 hanya disuplai dari USB laptop, layar bisa redup atau board restart. Pakai adaptor 5V 2A terpisah.

---

## 3. Library yang dibutuhkan

Library Manager Arduino IDE:

- **Adafruit GFX Library** (+ Adafruit BusIO, otomatis)
- **DHT sensor library** (Adafruit) + **Adafruit Unified Sensor**
- **PubSubClient** (Nick O'Leary)

Board: `ESP32 Dev Module`, Partition Scheme default, Upload Speed 921600.

---

## 4. Struktur file

| File | Isi |
|---|---|
| `RH_ServerRoomMonitor_dynamic_threshold.ino` | loop utama, penghubung semua modul |
| `RH_Config.h` | **semua pengaturan** — pin, threshold, WiFi, MQTT |
| `RH_Sensors.h/.cpp` | baca DHT22 + MQ2, tentukan level OK/WARN/ALARM |
| `RH_Buttons.h/.cpp` | debounce 3 tombol, deteksi tekan singkat / tahan |
| `RH_Alarm.h/.cpp` | pola buzzer tanpa `delay()` |
| `RH_Mqtt.h/.cpp` | WiFi + MQTT + penampung data saat offline |
| `RH_Display.h/.cpp` | dashboard TFT, 2 halaman (teks dirender off-screen lalu dikirim per blok, tanpa kedip) |
| `RH_Logos.h` | data logo RSUP & Automation Engineer (RGB565, hasil `tools/RH_png2rgb565.py`) |
| `RH_Tft.h/.cpp` | **driver layar** SPI langsung, init identik dengan tes full screen |

---

## 5. Fungsi tombol — masih sementara

Kamu belum menentukan fungsinya, jadi lapisan pembacaannya sudah jadi tapi aksinya masih placeholder. Semuanya di satu fungsi: `hrHandleButtons()` di file `.ino`.

Sementara ini:

| Tombol | Tekan singkat | Tahan |
|---|---|---|
| GPIO32 | ganti halaman | *kosong* |
| GPIO35 | bisukan buzzer | *kosong* |
| GPIO34 | gambar ulang layar | *kosong* |

Tinggal isi bagian bertanda `// TODO` di fungsi itu.

---

## 6. Format data MQTT

Topic dasar: `serverroom/srv-room-01`

| Topic | Isi |
|---|---|
| `/telemetry` | data sensor (JSON) |
| `/status` | `online` / `offline` (retained, pakai Last Will) |
| `/event` | perubahan status alarm |
| `/cmd` | perintah masuk: `mute`, `unmute`, `reboot` |

Contoh payload `/telemetry`:

```json
{
  "device_id": "srv-room-01",
  "location": "Ruang Server Lt.1",
  "fw": "1.0.0",
  "uptime_s": 3612,
  "age_ms": 0,
  "temp_c": 24.7,
  "hum_pct": 58.2,
  "gas_raw": 842,
  "gas_pct": 20.6,
  "level": { "temp": "ok", "hum": "ok", "gas": "ok" },
  "status": "ok",
  "rssi": -61
}
```

`age_ms` = umur data dalam milidetik. Nilainya `0` untuk kiriman langsung, dan lebih besar dari 0 untuk data susulan dari buffer offline. **Backend harus memakai field ini** untuk menghitung waktu sebenarnya, jangan pakai waktu terima.

Uji cepat dari laptop:

```bash
mosquitto_sub -h 192.168.1.10 -t 'serverroom/#' -v
mosquitto_pub -h 192.168.1.10 -t 'serverroom/srv-room-01/cmd' -m 'mute'
```

---

## 7. Kalibrasi MQ2

Nilai `HR_GAS_WARN` dan `HR_GAS_ALARM` di `RH_Config.h` masih angka tebakan. Harus dikalibrasi di ruanganmu:

1. Upload firmware, buka Serial Monitor (115200)
2. Biarkan alat menyala **minimal 24 jam** (burn-in heater baru; kalau sensor sudah pernah dipakai lama, 30 menit cukup)
3. Catat nilai `gas_raw` saat udara bersih → ini baseline
4. Set `HR_GAS_WARN = baseline + 400`, `HR_GAS_ALARM = baseline + 900`
5. Uji dengan pemantik gas (jangan dinyalakan) dari jarak 20 cm

MQ2 tidak bisa membedakan jenis gas. Untuk ruang server, gunanya mendeteksi **asap terbakar** — bukan sebagai pengganti detektor asap bersertifikat.

---

## 8. Ambang batas awal

| Parameter | Peringatan | Alarm |
|---|---|---|
| Suhu | ≥ 27 °C | ≥ 32 °C |
| Kelembapan tinggi | ≥ 65 %RH | ≥ 75 %RH |
| Kelembapan rendah | ≤ 35 %RH | ≤ 25 %RH |
| Gas (ADC) | ≥ 1200 | ≥ 2000 |

Status naik langsung saat terlampaui, tapi baru turun setelah kondisi normal bertahan 5 detik. Ini mencegah status kedap-kedip saat nilainya pas di garis batas.

---

## 9. Riwayat perubahan

**1.3.0** — Perbaikan UI. Kontras teks dinaikkan (semua pasangan warna >= 4.5:1; sebelumnya putih di bar hijau hanya 1.4:1), semua font jadi bold, sensor gagal tampil abu-abu (bukan hijau). Header putih dengan logo PT RSUP-Industry di kiri-atas (teks biru sesuai logo), watermark tim Automation Engineer di kanan-bawah (teks cyan sesuai logo). Muncul di semua halaman.

**1.2.0** — Layar dipindah dari TFT_eSPI (ILI9486 tipe RPi, DC=GPIO27 hasil tebakan) ke driver SPI langsung `RH_Tft` dengan pin dan urutan init dari `AB_test_fullscreen.ino`. Sensor, tombol, buzzer, MQTT, dan threshold dinamis **tidak diubah**. Font berganti dari font bawaan TFT_eSPI ke FreeFonts Adafruit GFX (tampilan sedikit berbeda, tata letak sama).

---

## 10. Mengganti logo

Logo disimpan sebagai data RGB565 di `RH_Logos.h` (total sekitar 7 KB flash). Untuk menggantinya:

1. Timpa `tools/RH_logo_rsup.png` (logo PT) atau `tools/RH_logo_ae.png` (watermark tim). PNG transparan boleh.
2. Atur ukuran / warna latar di bagian `LOGOS` pada `tools/RH_png2rgb565.py`. Latar logo AE harus sama dengan `C_BG` di `RH_Display.cpp`, latar logo RSUP harus putih.
3. `pip install pillow`, lalu jalankan `python3 RH_png2rgb565.py` dari folder `tools/`.
4. Pindahkan `RH_Logos.h` hasilnya ke folder sketch (menimpa yang lama). Kalau ukuran logo berubah, sesuaikan posisi di `RH_Display.cpp`.

Warna teks merek ada di bagian palet `RH_Display.cpp`: `C_RSUP_BLUE` dan `C_AE_CYAN`.
