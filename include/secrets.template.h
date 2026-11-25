

#pragma once
// Copy this file to ./include/secrets.h and replace the default entries
//   with your SSID, WIFI_Password, and topic
#define WIFI_SSID_D "YOUR_SSID"
#define WIFI_PASSWORD_D "YOUR_WIFI_PASSWORD"

// Change NTFY_URL if you are using a custom server
#define NTFY_URL_D     "https://ntfy.sh/"

#define NTFY_TOPIC_D  "YOUR_NTFY_TOPIC"
#define NTFY_OUT_TOPIC_D  "YOUR_NTFY_OUTBOUND_TOPIC"
#define NTFY_IN_TOPIC_D   "YOUR_NTFY_INBOUND_TOPIC"


// Leave empty for an intentionally public topic.
// For private ntfy topics, configure authentication.
#define NTFY_USER       ""
#define NTFY_PASSWORD   ""

// TLS uses the ESP-IDF certificate bundle.
// Never disable certificate verification.
