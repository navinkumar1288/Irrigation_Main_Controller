// ScheduleManager.cpp
#include "ScheduleManager.h"
#include <ArduinoJson.h>

ScheduleManager::ScheduleManager() {}

void ScheduleManager::setPump(bool on) {
  pinMode(PUMP_PIN, OUTPUT);
  if (PUMP_ACTIVE_HIGH) {
    digitalWrite(PUMP_PIN, on ? HIGH : LOW);
  } else {
    digitalWrite(PUMP_PIN, on ? LOW : HIGH);
  }
  Serial.printf("[Pump] %s\n", on ? "ON" : "OFF");
}

bool ScheduleManager::openNode(int node, int idx, uint32_t duration) {
  Serial.printf("[Schedule] Opening node %d (idx %d, duration %lu ms)\n", node, idx, duration);
  return loraComm.sendWithAck("OPEN", node, currentScheduleId, idx, duration);
}

bool ScheduleManager::closeNode(int node, int idx) {
  Serial.printf("[Schedule] Closing node %d (idx %d)\n", node, idx);
  return loraComm.sendWithAck("CLOSE", node, currentScheduleId, idx, 0);
}

bool ScheduleManager::parseCompact(const String &compact, Schedule &s) {
  s.id = "";
  s.rec = 'O';
  s.start_epoch = 0;
  s.timeStr = "";
  s.weekday_mask = 0;
  s.seq.clear();
  s.pump_on_before_ms = PUMP_ON_LEAD_DEFAULT_MS;
  s.pump_off_after_ms = PUMP_OFF_DELAY_DEFAULT_MS;
  s.enabled = true;
  s.next_run_epoch = 0;
  s.ts = 0;
  
  int p = compact.indexOf("SCH|");
  String body = (p >= 0) ? compact.substring(p + 4) : compact;
  body.trim();
  
  int pos = 0;
  while (pos < (int)body.length()) {
    int comma = body.indexOf(',', pos);
    String token = (comma == -1) ? body.substring(pos) : body.substring(pos, comma);
    token.trim();
    
    int eq = token.indexOf('=');
    if (eq > 0) {
      String k = token.substring(0, eq);
      String v = token.substring(eq + 1);
      k.trim();
      v.trim();
      
      if (k == "ID") s.id = v;
      else if (k == "REC") s.rec = v.length() ? v.charAt(0) : 'O';
      else if (k == "T") s.timeStr = v;
      else if (k == "SEQ") {
        String seqs = v;
        int spos = 0;
        while (spos < (int)seqs.length()) {
          int semi = seqs.indexOf(';', spos);
          String pair = (semi == -1) ? seqs.substring(spos) : seqs.substring(spos, semi);
          int colon = pair.indexOf(':');
          if (colon > 0) {
            SeqStep st;
            st.node_id = pair.substring(0, colon).toInt();
            st.duration_ms = (uint32_t)pair.substring(colon + 1).toInt() * 1000UL;
            s.seq.push_back(st);
          }
          if (semi == -1) break;
          spos = semi + 1;
        }
      } else if (k == "WD") {
        String tmp = v;
        tmp.toUpperCase();
        int sp = 0;
        while (sp < (int)tmp.length()) {
          int cm = tmp.indexOf(',', sp);
          String d = (cm == -1) ? tmp.substring(sp) : tmp.substring(sp, cm);
          d.trim();
          if (d == "MON") s.weekday_mask |= (1 << 1);
          else if (d == "TUE") s.weekday_mask |= (1 << 2);
          else if (d == "WED") s.weekday_mask |= (1 << 3);
          else if (d == "THU") s.weekday_mask |= (1 << 4);
          else if (d == "FRI") s.weekday_mask |= (1 << 5);
          else if (d == "SAT") s.weekday_mask |= (1 << 6);
          else if (d == "SUN") s.weekday_mask |= (1 << 0);
          if (cm == -1) break;
          sp = cm + 1;
        }
      } else if (k == "PB") s.pump_on_before_ms = (uint32_t)v.toInt();
      else if (k == "PA") s.pump_off_after_ms = (uint32_t)v.toInt();
      else if (k == "TS") s.ts = (uint32_t)v.toInt();
    }
    
    if (comma == -1) break;
    pos = comma + 1;
  }
  
  // Parse onetime start_epoch
  if (s.rec == 'O' && s.timeStr.length()) {
    int year = 0, mon = 0, mday = 0, hour = 0, min = 0, sec = 0;
    if (sscanf(s.timeStr.c_str(), "%d-%d-%dT%d:%d:%d", &year, &mon, &mday, &hour, &min, &sec) >= 6) {
      struct tm tm;
      memset(&tm, 0, sizeof(tm));
      tm.tm_year = year - 1900;
      tm.tm_mon = mon - 1;
      tm.tm_mday = mday;
      tm.tm_hour = hour;
      tm.tm_min = min;
      tm.tm_sec = sec;
      s.start_epoch = mktime(&tm);
    }
  }
  
  return (s.id.length() > 0);
}

bool ScheduleManager::parseJSON(const String &json, Schedule &s) {
  s = storage.scheduleFromJson(json);
  return (s.id.length() > 0);
}

bool ScheduleManager::validateAndLoad(const String &payload) {
  String trimmed = payload;
  trimmed.trim();
  
  if (trimmed.length() == 0) return false;
  
  String src = extractSrc(trimmed);
  String fromNumber = extractKeyVal(trimmed, "_FROM");
  
  Serial.println("[Schedule] Processing: " + trimmed);
  
  // Verify token
  if (!verifyTokenForSrc(trimmed, fromNumber)) {
    Serial.println("❌ Auth failed for: " + src);
    return false;
  }
  
  Schedule s;
  bool success = false;
  
  // Try JSON
  if (trimmed.startsWith("{")) {
    success = parseJSON(trimmed, s);
  }
  // Try compact
  else if (trimmed.indexOf("SCH|") >= 0) {
    success = parseCompact(trimmed, s);
  }
  
  if (!success || s.id.length() == 0) {
    Serial.println("❌ Invalid schedule format");
    return false;
  }
  
  // Save schedule
  if (!storage.saveSchedule(s)) {
    Serial.println("⚠ Failed to save schedule file");
  }
  
  // Update schedules list
  bool found = false;
  for (size_t i = 0; i < schedules.size(); ++i) {
    if (schedules[i].id == s.id) {
      schedules[i] = s;
      found = true;
      break;
    }
  }
  if (!found) {
    schedules.push_back(s);
  }
  
  // Load as current if none loaded
  if (!scheduleLoaded) {
    seq.clear();
    for (auto &st : s.seq) seq.push_back(st);
    currentScheduleId = s.id;
    pumpOnBeforeMs = s.pump_on_before_ms;
    pumpOffAfterMs = s.pump_off_after_ms;
    scheduleLoaded = true;
    currentStepIndex = -1;
    scheduleStartEpoch = s.start_epoch;
    Serial.println("✓ Schedule loaded: " + s.id);
  }
  
  return true;
}

time_t ScheduleManager::computeNextRun(const Schedule &s, time_t now) {
  if (!s.enabled) return 0;
  
  if (s.rec == 'O') {
    return s.start_epoch;
  } else if (s.rec == 'D') {
    int hh, mm;
    if (!parseTimeHHMM(s.timeStr, hh, mm)) return 0;
    
    struct tm tmnow;
    localtime_r(&now, &tmnow);
    struct tm tmc = tmnow;
    tmc.tm_hour = hh;
    tmc.tm_min = mm;
    tmc.tm_sec = 0;
    
    time_t cand = mktime(&tmc);
    if (cand > now) return cand;
    
    tmc.tm_mday += 1;
    return mktime(&tmc);
  } else if (s.rec == 'W') {
    int hh, mm;
    if (!parseTimeHHMM(s.timeStr, hh, mm)) return 0;
    return nextWeekdayOccurrence(now, s.weekday_mask, hh, mm);
  }
  
  return 0;
}

void ScheduleManager::startIfDue() {
  if (!scheduleLoaded) return;
  if (scheduleRunning) return;
  if (seq.size() == 0) return;
  
  time_t now = time(nullptr);
  if (now == (time_t)-1) return;
  
  Serial.println("[Schedule] Starting execution...");
  
  // Find first node that opens successfully
  int startIndex = -1;
  for (size_t i = 0; i < seq.size(); ++i) {
    Serial.printf("[Schedule] Trying node %d (idx %zu)...\n", seq[i].node_id, i);
    if (openNode(seq[i].node_id, (int)i, seq[i].duration_ms)) {
      startIndex = (int)i;
      Serial.printf("✓ Node %d opened\n", seq[i].node_id);
      break;
    }
  }
  
  if (startIndex < 0) {
    Serial.println("❌ No node responded, aborting");

    // Notify about schedule failure
    publishStatus("ERR|SCH|START_FAIL|S=" + currentScheduleId + "|NO_NODES");
    sendSMSNotification("ERROR: Schedule '" + currentScheduleId +
                        "' failed to start - no nodes responded",
                        "SCH_START_FAIL");

    // Clear the loaded schedule
    scheduleLoaded = false;
    currentScheduleId = "";
    return;
  }
  
  // Close all other nodes
  for (size_t i = 0; i < seq.size(); ++i) {
    if ((int)i == startIndex) continue;
    closeNode(seq[i].node_id, (int)i);
  }
  
  // Turn on pump
  setPump(true);
  // NOTE: This delay blocks the entire system (MQTT, SMS, LoRa processing)
  // TODO: Refactor to non-blocking state machine for better responsiveness
  delay(pumpOnBeforeMs);
  
  scheduleRunning = true;
  currentStepIndex = startIndex;
  stepStartMillis = millis();
  
  prefs.putInt("active_index", currentStepIndex);
  prefs.putString("active_schedule", currentScheduleId);
  
  Serial.println("✓ Schedule started");
}

void ScheduleManager::runLoop() {
  if (!scheduleRunning) {
    startIfDue();
    return;
  }
  
  if (currentStepIndex < 0 || currentStepIndex >= (int)seq.size()) {
    Serial.println("[Schedule] Invalid step index, stopping");
    stop();
    return;
  }
  
  SeqStep &step = seq[currentStepIndex];
  
  // Check if current step is complete
  if (millis() - stepStartMillis >= step.duration_ms) {
    Serial.printf("[Schedule] Step %d complete\n", currentStepIndex);
    
    // Find next node
    int nextIdx = -1;
    for (int cand = currentStepIndex + 1; cand < (int)seq.size(); ++cand) {
      if (openNode(seq[cand].node_id, cand, seq[cand].duration_ms)) {
        nextIdx = cand;
        Serial.printf("✓ Next node %d opened\n", seq[cand].node_id);
        break;
      }
    }
    
    // Close current node
    closeNode(step.node_id, currentStepIndex);
    
    if (nextIdx >= 0) {
      currentStepIndex = nextIdx;
      stepStartMillis = millis();
      prefs.putInt("active_index", currentStepIndex);
      Serial.printf("✓ Moved to step %d\n", currentStepIndex);
    } else {
      Serial.println("✓ Schedule complete");
      // NOTE: This delay blocks the entire system (MQTT, SMS, LoRa processing)
      // TODO: Refactor to non-blocking state machine for better responsiveness
      delay(pumpOffAfterMs);
      setPump(false);
      scheduleRunning = false;
      currentStepIndex = -1;
      prefs.putInt("active_index", -1);
    }
  }
  
  // Save progress periodically
  static unsigned long lastProgressSave = 0;
  if (millis() - lastProgressSave > SAVE_PROGRESS_INTERVAL_MS) {
    prefs.putString("active_schedule", currentScheduleId);
    prefs.putInt("active_index", currentStepIndex);
    lastProgressSave = millis();
  }
}

void ScheduleManager::stop() {
  if (currentStepIndex >= 0 && currentStepIndex < (int)seq.size()) {
    closeNode(seq[currentStepIndex].node_id, currentStepIndex);
  }
  
  setPump(false);
  scheduleRunning = false;
  currentStepIndex = -1;
  prefs.putInt("active_index", -1);
  
  Serial.println("✓ Schedule stopped");
}

bool ScheduleManager::isRunning() {
  return scheduleRunning;
}