#pragma once

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QAbstractListModel>
#include <QFile>

#include "LoginDialog.h"
#include "QtPlugin.h"

namespace Ui {
class PluginMainWindow;
}

class PluginMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit PluginMainWindow(QWidget* parent = nullptr);
    ~PluginMainWindow();
    void pluginEvent(QtPlugin::EventType event, const QVariant& data);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void enableUi(bool enabled);
    void logInfo(const QString& message);
    void setStatus(const QString& status);
    void uploadFile(uintptr_t moduleBase, const QString& path);
    void displayReport(QJsonObject data, const QString& jsonPath, uintptr_t loadedBase);
    QString getReportJsonPath(uintptr_t base);

private slots:
    void pollTimerSlot();
    void loginAcceptedSlot();
    void on_buttonUpload_clicked();
    void on_actionExampleReport_triggered();
    void on_buttonOptions_clicked();
    void on_actionLogin_triggered();
    void on_comboModules_currentIndexChanged(int index);
    void on_editReport_anchorClicked(const QUrl& url);

private:
    Ui::PluginMainWindow* ui = nullptr;
    QString mUserDir;
    QNetworkAccessManager* mHttp = nullptr;
    QString mApiKey;
    QTimer* mPollTimer = nullptr;
    QString mPollUuid;
    uintptr_t mPollModule = 0;
    bool mIsDebugging = false;
    QFile* mLogFile = nullptr;
    LoginDialog* mLoginDialog = nullptr;
    QMap<QString, QString> mReportCache;
};
