/*
  GROCERY TERMINAL V1
  LCDWiki 2.8" ESP32-S3 capacitive touchscreen
  Display: ILI9341V
  Touch: FT6336G

  Libraries:
    - GFX Library for Arduino

  Board:
    ESP32S3 Dev Module

  Select:
    USB CDC On Boot: Enabled
    Flash Size: 16MB
    PSRAM: OPI PSRAM (if exposed by your board package)

  IMPORTANT:
    Set your WiFi credentials and a private ntfy topic below.
*/

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Arduino_GFX_Library.h>

// ---------------- HARDWARE ----------------

#define LCD_CS    10
#define LCD_DC    46
#define LCD_SCLK  12
#define LCD_MOSI  11
#define LCD_MISO  13
#define LCD_BL    45

#define TOUCH_SDA 16
#define TOUCH_SCL 15
#define TOUCH_RST 18

#define FT6336_ADDR 0x38

Arduino_DataBus *bus = new Arduino_ESP32SPI(
  LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO
);

Arduino_GFX *gfx = new Arduino_ILI9341(
  bus, GFX_NOT_DEFINED, 0, true
);

Preferences prefs;

// ---------------- NETWORK SETTINGS ----------------

const char* WIFI_SSID = "MyNetwork";
const char* WIFI_PASSWORD = "MyPassword";

// Use a long, randomly generated topic name.
// Subscribe to the same topic in the ntfy phone app.
const char* NTFY_TOPIC = "MyTopic";

// Prototype only: TLS certificate validation is disabled below.
// Install a trusted CA certificate before deploying this device.
const char* NTFY_URL = "https://ntfy.sh/";

// ---------------- COLORS ----------------

#define C_BG       0xFFFF
#define C_TEXT     0x0000
#define C_BLUE     0x1D7F
#define C_GREEN    0x2685
#define C_RED      0xE986
#define C_GRAY     0xCE79
#define C_LIGHT    0xEF7D
#define C_WHITE    0xFFFF
#define C_YELLOW   0xFFE0

// ---------------- CATALOG ----------------

struct GroceryItem {
  const char* id;
  const char* name;
  uint8_t category;
};

const char* categories[] = {
  "Dairy & Eggs",
  "Bakery",
  "Fruit",
  "Vegetables",
  "Meat",
  "Pantry",
  "Frozen",
  "Household"
};

const uint8_t CATEGORY_COUNT = 8;

GroceryItem catalog[] = {
  {"milk", "Milk", 0},
  {"butter", "Butter", 0},
  {"cheese", "Cheese", 0},
  {"eggs", "Eggs", 0},
  {"yogurt", "Yogurt", 0},

  {"bread", "Bread", 1},
  {"rolls", "Rolls", 1},
  {"bagels", "Bagels", 1},
  {"tortillas", "Tortillas", 1},
  {"muffins", "Muffins", 1},

  {"apples", "Apples", 2},
  {"bananas", "Bananas", 2},
  {"oranges", "Oranges", 2},
  {"grapes", "Grapes", 2},
  {"strawberries", "Berries", 2},

  {"potatoes", "Potatoes", 3},
  {"carrots", "Carrots", 3},
  {"lettuce", "Lettuce", 3},
  {"onions", "Onions", 3},
  {"tomatoes", "Tomatoes", 3},

  {"chicken", "Chicken", 4},
  {"ground_beef", "Ground Beef", 4},
  {"bacon", "Bacon", 4},
  {"lunch_meat", "Lunch Meat", 4},
  {"fish", "Fish", 4},

  {"rice", "Rice", 5},
  {"pasta", "Pasta", 5},
  {"cereal", "Cereal", 5},
  {"hot chocolate", "Hot Chocolate", 5},
  {"sugar", "Sugar", 5},

  {"frozen_veg", "Frozen Veg", 6},
  {"ice_cream", "Ice Cream", 6},
  {"pizza", "Pizza", 6},
  {"frozen_fruit", "Frozen Fruit", 6},
  {"frozen_meals", "Frozen Meals", 6},

  {"paper_towels", "Paper Towels", 7},
  {"toilet_paper", "Toilet Paper", 7},
  {"dish_soap", "Dish Soap", 7},
  {"hand_soap", "Hand Soap", 7},
  {"detergent", "Detergent", 7}
};

const uint8_t ITEM_COUNT = sizeof(catalog) / sizeof(catalog[0]);

uint8_t quantities[ITEM_COUNT] = {0};

// ---------------- UI STATE ----------------

enum Screen {
  HOME,
  CATEGORY,
  CART
};

Screen currentScreen = HOME;
uint8_t selectedCategory = 0;

String statusMessage = "Ready";

const int SCREEN_W = 240;
const int SCREEN_H = 320;

// ---------------- DRAWING HELPERS ----------------

void drawButton(
  int x, int y, int w, int h,
  const char* label,
  uint16_t color,
  uint8_t textSize = 2
) {
  gfx->fillRoundRect(x, y, w, h, 7, color);
  gfx->drawRoundRect(x, y, w, h, 7, C_GRAY);

  gfx->setTextSize(textSize);
  gfx->setTextColor(C_TEXT);

  int textWidth = strlen(label) * 6 * textSize;
  int tx = x + (w - textWidth) / 2;
  int ty = y + (h - (8 * textSize)) / 2;

  gfx->setCursor(tx, ty);
  gfx->print(label);
}

void drawHeader(const char* title) {
  gfx->fillRect(0, 0, SCREEN_W, 42, C_BLUE);

  gfx->setTextColor(C_WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(8, 13);
  gfx->print(title);

  uint16_t total = 0;
  for (int i = 0; i < ITEM_COUNT; i++)
    total += quantities[i];

  gfx->setTextSize(1);
  gfx->setCursor(184, 16);
  gfx->print("List: ");
  gfx->print(total);
}

void drawHome() {
  currentScreen = HOME;

  gfx->fillScreen(C_BG);
  drawHeader("Grocery List");

  for (int i = 0; i < CATEGORY_COUNT; i++) {
    int col = i % 2;
    int row = i / 2;

    int x = 8 + col * 116;
    int y = 49 + row * 53;

    drawButton(
      x, y, 108, 47,
      categories[i],
      (i % 2) ? C_LIGHT : 0xDFFB,
      1
    );
  }

  drawButton(8, 266, 224, 43, "VIEW MY LIST", C_GREEN, 2);
}

void drawCategory() {
  currentScreen = CATEGORY;

  gfx->fillScreen(C_BG);
  drawHeader(categories[selectedCategory]);

  int visibleCount = 0;

  for (int i = 0; i < ITEM_COUNT; i++) {
    if (catalog[i].category != selectedCategory)
      continue;

    int col = visibleCount % 2;
    int row = visibleCount / 2;

    int x = 8 + col * 116;
    int y = 55 + row * 66;

    drawButton(
      x, y, 108, 56,
      catalog[i].name,
      quantities[i] ? 0xBFEF : C_LIGHT,
      2
    );

    if (quantities[i] > 0) {
      gfx->setTextColor(C_GREEN);
      gfx->setTextSize(1);
      gfx->setCursor(x + 84, y + 4);
      gfx->print(quantities[i]);
    }

    visibleCount++;
  }

  drawButton(8, 263, 105, 43, "BACK", C_GRAY, 2);
  drawButton(127, 263, 105, 43, "MY LIST", C_GREEN, 2);
}

int cartItems[ITEM_COUNT];
int cartCount = 0;

void buildCart() {
  cartCount = 0;

  for (int i = 0; i < ITEM_COUNT; i++) {
    if (quantities[i] > 0)
      cartItems[cartCount++] = i;
  }
}

void drawCart() {
  currentScreen = CART;

  gfx->fillScreen(C_BG);
  drawHeader("Shopping List");

  buildCart();

  if (cartCount == 0) {
    gfx->setTextColor(C_TEXT);
    gfx->setTextSize(2);
    gfx->setCursor(30, 125);
    gfx->print("Your list is empty!");
  }

  int displayed = min(cartCount, 8);

  for (int j = 0; j < displayed; j++) {
    int item = cartItems[j];
    int y = 48 + j * 27;

    gfx->fillRoundRect(5, y, 230, 25, 4, C_LIGHT);

    gfx->setTextColor(C_TEXT);
    gfx->setTextSize(1);
    gfx->setCursor(10, y + 8);
    gfx->print(catalog[item].name);

    gfx->setCursor(151, y + 8);
    gfx->print(quantities[item]);

    drawButton(177, y + 1, 25, 23, "-", C_GRAY, 2);
    drawButton(206, y + 1, 25, 23, "+", C_GREEN, 2);
  }

  if (cartCount > 8) {
    gfx->setTextSize(1);
    gfx->setTextColor(C_TEXT);
    gfx->setCursor(10, 267);
    gfx->print("More items not shown.");
  }

  drawButton(5, 278, 70, 36, "BACK", C_GRAY, 1);
  drawButton(84, 278, 70, 36, "CLEAR", C_RED, 1);
  drawButton(163, 278, 72, 36, "SEND", C_GREEN, 2);
}

// ---------------- TOUCH CONTROLLER ----------------

bool readTouch(int &x, int &y) {
  Wire.beginTransmission(FT6336_ADDR);
  Wire.write(0x02);

  if (Wire.endTransmission(false) != 0)
    return false;

  if (Wire.requestFrom(FT6336_ADDR, 5) != 5)
    return false;

  uint8_t count = Wire.read() & 0x0F;

  uint8_t xh = Wire.read();
  uint8_t xl = Wire.read();
  uint8_t yh = Wire.read();
  uint8_t yl = Wire.read();

  if (count == 0)
    return false;

  x = ((xh & 0x0F) << 8) | xl;
  y = ((yh & 0x0F) << 8) | yl;

  // Clamp to portrait screen dimensions.
  x = constrain(x, 0, 239);
  y = constrain(y, 0, 319);

  return true;
}

// ---------------- STORAGE ----------------

void saveList() {
  prefs.putBytes("qty", quantities, sizeof(quantities));
}

void loadList() {
  size_t len = prefs.getBytesLength("qty");

  if (len == sizeof(quantities)) {
    prefs.getBytes("qty", quantities, sizeof(quantities));
  }
}

// ---------------- WIFI ----------------

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  gfx->fillScreen(C_BG);
  gfx->setTextColor(C_TEXT);
  gfx->setTextSize(2);
  gfx->setCursor(15, 110);
  gfx->print("Connecting WiFi");

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    gfx->print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    statusMessage = "WiFi connected";
  } else {
    Serial.println("WiFi unavailable");
    statusMessage = "Offline";
  }
}

// ---------------- NOTIFICATIONS ----------------

bool sendShoppingList() {
  buildCart();

  if (cartCount == 0) {
    statusMessage = "List is empty";
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    statusMessage = "No WiFi - list saved";
    return false;
  }

  String message = "NEW GROCERY REQUEST\n\n";

  for (int j = 0; j < cartCount; j++) {
    int item = cartItems[j];

    message += String(quantities[item]);
    message += " x ";
    message += catalog[item].name;
    message += "\n";
  }

  message += "\nSent from Grocery Terminal";

  WiFiClientSecure client;

  // DEVELOPMENT ONLY:
  // Replace this with setCACert() using a trusted CA
  // certificate before deploying.
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(15000);

  String url = String(NTFY_URL) + NTFY_TOPIC;

  if (!http.begin(client, url)) {
    statusMessage = "Connection failed";
    return false;
  }

  http.addHeader("Content-Type", "text/plain; charset=utf-8");
  http.addHeader("Title", "New Grocery Request");
  http.addHeader("Priority", "high");
  http.addHeader("Tags", "shopping_cart");

  int responseCode = http.POST(message);

  http.end();

  if (responseCode >= 200 && responseCode < 300) {
    // Only clear the list after successful delivery
    // acknowledgement from the ntfy server.
    memset(quantities, 0, sizeof(quantities));
    saveList();

    statusMessage = "Request sent!";
    return true;
  }

  Serial.printf("Notification error: %d\n", responseCode);
  statusMessage = "Send failed - list saved";
  return false;
}

// ---------------- TOUCH ACTIONS ----------------

void handleTouch(int x, int y) {

  if (currentScreen == HOME) {

    if (y >= 49 && y <= 260) {
      int row = (y - 49) / 53;
      int col = (x >= 120) ? 1 : 0;
      int category = row * 2 + col;

      if (category < CATEGORY_COUNT) {
        selectedCategory = category;
        drawCategory();
      }
    }

    if (y >= 266) {
      drawCart();
    }

  } else if (currentScreen == CATEGORY) {

    if (y >= 263) {
      if (x < 120)
        drawHome();
      else
        drawCart();

      return;
    }

    if (y >= 55 && y < 253) {
      int row = (y - 55) / 66;
      int col = (x >= 120) ? 1 : 0;
      int target = row * 2 + col;

      int found = 0;

      for (int i = 0; i < ITEM_COUNT; i++) {
        if (catalog[i].category != selectedCategory)
          continue;

        if (found == target) {
          if (quantities[i] < 99)
            quantities[i]++;

          saveList();
          drawCategory();
          return;
        }

        found++;
      }
    }

  } else if (currentScreen == CART) {

    if (y >= 278) {

      if (x < 80) {
        drawHome();
      } else if (x < 160) {
        memset(quantities, 0, sizeof(quantities));
        saveList();
        drawCart();
      } else {
        bool sent = sendShoppingList();

        gfx->fillScreen(C_BG);
        gfx->setTextColor(C_TEXT);
        gfx->setTextSize(2);

        gfx->setCursor(20, 110);
        gfx->print(sent ? "REQUEST SENT!" : "NOT SENT");

        gfx->setTextSize(1);
        gfx->setCursor(20, 150);
        gfx->print(statusMessage);

        drawButton(25, 220, 190, 55, "CONTINUE", C_GREEN, 2);
      }

      return;
    }

    if (y >= 48 && y < 264) {
      int row = (y - 48) / 27;

      buildCart();

      if (row < cartCount && row < 8) {
        int item = cartItems[row];

        if (x >= 177 && x < 205) {
          if (quantities[item] > 0)
            quantities[item]--;
        } else if (x >= 205) {
          if (quantities[item] < 99)
            quantities[item]++;
        }

        saveList();
        drawCart();
      }
    }
  }
}

// ---------------- SETUP ----------------

void setup() {
  Serial.begin(115200);

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  Wire.begin(TOUCH_SDA, TOUCH_SCL, 400000);

  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);
  delay(20);
  digitalWrite(TOUCH_RST, HIGH);
  delay(100);

  if (!gfx->begin()) {
    Serial.println("Display initialization failed!");
    while (true) delay(1000);
  }

  gfx->setRotation(0);
  gfx->fillScreen(C_BG);

  prefs.begin("grocery", false);
  loadList();

  drawHome();

  connectWiFi();
  drawHome();
}

// ---------------- MAIN LOOP ----------------

void loop() {
  static bool wasTouched = false;
  static unsigned long lastTouch = 0;

  int x, y;
  bool touched = readTouch(x, y);

  if (touched && !wasTouched &&
      millis() - lastTouch > 250) {

    lastTouch = millis();

    // Debug touch coordinates
    Serial.printf("Touch: %d, %d\n", x, y);

    // Continue button on send-result screen
    if (currentScreen == CART &&
        y >= 220 && y <= 275) {
      drawHome();
    } else {
      handleTouch(x, y);
    }
  }

  wasTouched = touched;

  // Reconnect WiFi periodically if disconnected.
  static unsigned long lastWiFiCheck = 0;

  if (millis() - lastWiFiCheck > 30000) {
    lastWiFiCheck = millis();

    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
    }
  }

  delay(20);
}