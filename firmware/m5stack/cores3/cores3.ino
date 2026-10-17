#include <M5Unified.h>

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    M5.Display.setTextSize(3);
    M5.Display.setCursor(20, 80);
    M5.Display.println("Hello Arduino CLI!");
}

void loop() {
    M5.update();
}
