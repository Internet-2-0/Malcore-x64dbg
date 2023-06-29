#include "pluginmain.h"
#include "QtPlugin.h"
#include <QDebug>

int Plugin::handle;
HWND Plugin::hwndDlg;
int Plugin::hMenu;
int Plugin::hMenuDisasm;
int Plugin::hMenuDump;
int Plugin::hMenuStack;
int Plugin::hMenuGraph;
int Plugin::hMenuMemmap;
int Plugin::hMenuSymmod;

PLUG_EXPORT void CBCREATEPROCESS(CBTYPE, PLUG_CB_CREATEPROCESS* info)
{
    QtPlugin::Event(QtPlugin::LoadModule, (uintptr_t)info->CreateProcessInfo->lpBaseOfImage);
}

PLUG_EXPORT void CBLOADDLL(CBTYPE, PLUG_CB_LOADDLL* info)
{
    QtPlugin::Event(QtPlugin::LoadModule, (uintptr_t)info->LoadDll->lpBaseOfDll);
}

PLUG_EXPORT void CBUNLOADDLL(CBTYPE, PLUG_CB_UNLOADDLL* info)
{
    QtPlugin::Event(QtPlugin::UnloadModule, (uintptr_t)info->UnloadDll->lpBaseOfDll);
}

PLUG_EXPORT void CBSTOPDEBUG(CBTYPE, PLUG_CB_STOPDEBUG*)
{
    QtPlugin::Event(QtPlugin::StopDebug, {});
}

PLUG_EXPORT bool pluginit(PLUG_INITSTRUCT* initStruct)
{
    initStruct->pluginVersion = PLUGIN_VERSION;
    initStruct->sdkVersion = PLUG_SDKVERSION;
    strcpy_s(initStruct->pluginName, PLUGIN_NAME);

    Plugin::handle = initStruct->pluginHandle;
    QtPlugin::Init();
    return true;
}

PLUG_EXPORT void plugsetup(PLUG_SETUPSTRUCT* setupStruct)
{
    Plugin::hwndDlg = setupStruct->hwndDlg;
    Plugin::hMenu = setupStruct->hMenu;
    Plugin::hMenuDisasm = setupStruct->hMenuDisasm;
    Plugin::hMenuDump = setupStruct->hMenuDump;
    Plugin::hMenuStack = setupStruct->hMenuStack;
    Plugin::hMenuGraph = setupStruct->hMenuGraph;
    Plugin::hMenuMemmap = setupStruct->hMenuMemmap;
    Plugin::hMenuSymmod = setupStruct->hMenuSymmod;
    // NOTE: This is redundant, since this function already executes on the GUI thread
    GuiExecuteOnGuiThread(QtPlugin::Setup);
    QtPlugin::WaitForSetup();
}

PLUG_EXPORT bool plugstop()
{
    GuiExecuteOnGuiThread(QtPlugin::Stop);
    QtPlugin::WaitForStop();
    return true;
}
