#include "QtPlugin.h"
#include "LoginDialog.h"
#include "PluginMainWindow.h"
#include "pluginmain.h"

#include <functional>

#include <QFile>
#include <QDebug>

static PluginMainWindow* pluginTabWidget;
static HANDLE hSetupEvent;
static HANDLE hStopEvent;

static QByteArray getResourceBytes(const char* path)
{
    QByteArray b;
    QFile s(path);
    if(s.open(QFile::ReadOnly))
        b = s.readAll();
    return b;
}

static QWidget* getParent()
{
    return QWidget::find((WId)Plugin::hwndDlg);
}

void QtPlugin::Init()
{
    hSetupEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
}

void QtPlugin::Setup()
{
    QWidget* parent = getParent();

    pluginTabWidget = new PluginMainWindow(parent);
    GuiAddQWidgetTab(pluginTabWidget);
    SetEvent(hSetupEvent);
}

void QtPlugin::WaitForSetup()
{
    WaitForSingleObject(hSetupEvent, INFINITE);
}

void QtPlugin::Stop()
{
    GuiCloseQWidgetTab(pluginTabWidget);
    pluginTabWidget->close();
    delete pluginTabWidget;

    SetEvent(hStopEvent);
}

void QtPlugin::WaitForStop()
{
    WaitForSingleObject(hStopEvent, INFINITE);
}

void QtPlugin::ShowTab()
{
    GuiShowQWidgetTab(pluginTabWidget);
}

void QtPlugin::Event(EventType event, const QVariant& data)
{
    auto fn = new std::function<void()>([=]()
    {
        pluginTabWidget->pluginEvent(event, data);
    });
    GuiExecuteOnGuiThreadEx([](void* userdata)
    {
        auto fn = (std::function<void()>*)userdata;
        (*fn)();
        delete fn;
    }, fn);
}
