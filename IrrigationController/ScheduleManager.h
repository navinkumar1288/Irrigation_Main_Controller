// ScheduleManager.h
#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

#include "Config.h"
#include "Utils.h"
#include "StorageManager.h"
#include "LoRaComm.h"

class ScheduleManager {
private:
  void setPump(bool on);
  bool openNode(int node, int idx, uint32_t duration);
  bool closeNode(int node, int idx);

public:
  ScheduleManager();
  bool parseCompact(const String &compact, Schedule &s);
  bool parseJSON(const String &json, Schedule &s);
  bool validateAndLoad(const String &payload);
  void startIfDue();
  void runLoop();
  void stop();
  time_t computeNextRun(const Schedule &s, time_t now);
  bool isRunning();
};

extern ScheduleManager scheduleMgr;

#endif