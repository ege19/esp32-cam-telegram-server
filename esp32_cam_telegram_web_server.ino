void handleInfo() {
  unsigned long now = millis();
  unsigned long uptime = now / 1000;
  int hours = uptime / 3600;
  int mins = (uptime % 3600) / 60;
  
  float fps = frameCounter > 0 ? (frameCounter * 1000.0f) / now : 0;
  uint32_t freeMem = esp_get_free_heap_size();
  uint32_t totalMem = ESP.getHeapSize();
  int memPercent = ((totalMem - freeMem) * 100) / totalMem;
  int rssi = WiFi.RSSI();

  String json = "{\"uptime\":\"" + String(hours) + "h " + String(mins) + "m\",";
  json += "\"fps\":" + String((int)fps) + ",";
  json += "\"ram\":\"" + String(memPercent) + "%\",";
  json += "\"wifi\":" + String(rssi) + "}";

  server.send(200, "application/json", json);
}
