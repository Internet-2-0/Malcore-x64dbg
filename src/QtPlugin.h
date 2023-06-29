#pragma once

#include <QVariant>

#include <cstdint>

namespace QtPlugin
{
enum EventType
{
    LoadModule,
    UnloadModule,
    StopDebug,
};

void Init();
void Setup();
void WaitForSetup();
void Stop();
void WaitForStop();
void ShowTab();
void Event(EventType event, const QVariant& data);
} //QtPlugin
