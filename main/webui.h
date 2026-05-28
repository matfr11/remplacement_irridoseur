#pragma once

// Interface web embarquée — HTML + CSS + JS
// Stockée en flash (section RODATA) pour économiser la RAM dynamique.
// Implémentée en PR-08 : 3 onglets (Accueil, Stats, Config), dark theme,
// mobile 6" portrait, touch-friendly, boutons ≥ 48px, zéro CDN externe.
//
// Format final : tableau uint8_t compressé gzip ou chaîne C littérale.

static const char WEBUI_HTML[] =
    "<!DOCTYPE html>"
    "<html lang=\"fr\">"
    "<head>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
    "<title>Irrifrance ESP32</title>"
    "<style>"
    "body{font-family:sans-serif;background:#1a1a2e;color:#eee;margin:0;padding:16px;}"
    "h1{color:#4fc3f7;font-size:1.4em;}"
    "p{color:#aaa;}"
    "</style>"
    "</head>"
    "<body>"
    "<h1>Irrifrance ESP32</h1>"
    "<p>Interface en cours de développement &mdash; disponible en PR-08.</p>"
    "<p>WebSocket : <code>ws://192.168.4.1/ws</code></p>"
    "</body>"
    "</html>";
