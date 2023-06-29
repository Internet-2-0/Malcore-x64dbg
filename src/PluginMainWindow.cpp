#include "PluginMainWindow.h"
#include "ui_PluginMainWindow.h"

#include <QDebug>
#include <QFile>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHttpPart>
#include <QFileInfo>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QAbstractListModel>
#include <QDir>
#include <QCryptographicHash>
#include <QKeyEvent>
#include <QDesktopServices>
#include <QClipboard>

#include "pluginmain.h"
#include "LoginDialog.h"
#include "MalcoreReport.h"

PluginMainWindow::PluginMainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::PluginMainWindow)
{
    ui->setupUi(this);

    enableUi(false);
    ui->buttonOptions->setEnabled(true);

    // Don't allow the user to enter data
    ui->editReport->setUndoRedoEnabled(false);
    ui->editReport->setContextMenuPolicy(Qt::NoContextMenu);
    ui->editReport->installEventFilter(this);

    // Hide the menu bar
    ui->menubar->setVisible(false);

    mUserDir = QString::fromUtf16((const ushort*)BridgeUserDirectory());
    mUserDir += "\\Malcore";
    QDir(mUserDir).mkpath(".");

    char setting[MAX_SETTING_SIZE]="";
    if(BridgeSettingGet("Malcore", "ApiKey", setting))
        mApiKey = QString::fromUtf8(setting);

    mHttp = new QNetworkAccessManager(this);
    mLogFile = new QFile(QString("%1\\debug.log").arg(mUserDir), this);
    if(!mLogFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        dputs("Failed to open 'malcore.log'...");
        delete mLogFile;
        mLogFile = nullptr;
    }

    mPollTimer = new QTimer(this);
    mPollTimer->setInterval(300);
    mPollTimer->setSingleShot(true);
    connect(mPollTimer, &QTimer::timeout, this, &PluginMainWindow::pollTimerSlot);

    mLoginDialog = new LoginDialog(this);
    connect(mLoginDialog, &LoginDialog::accepted, this, &PluginMainWindow::loginAcceptedSlot);
}

PluginMainWindow::~PluginMainWindow()
{
    delete ui;
}

static QString getModulePath(duint base)
{
    char pathUtf8[MAX_PATH] = "";
    if(!Script::Module::PathFromAddr(base, pathUtf8))
        return QString();
    return QString::fromUtf8(pathUtf8);
}

void PluginMainWindow::pluginEvent(QtPlugin::EventType event, const QVariant& data)
{
    switch(event)
    {
    case QtPlugin::LoadModule:
    {
        if(!mIsDebugging)
        {
            mIsDebugging = true;
            enableUi(true);
            ui->labelStatus->setText("Ready!");
        }
        auto base = data.toULongLong();
        auto party = DbgFunctions()->ModGetParty(base);
        auto path = getModulePath(base);
        ui->comboModules->addItem(QString("[%1] %2").arg(party == mod_system ? "system" : "user", path), QVariant(base));
        if(ui->comboModules->count() == 1)
        {
            ui->comboModules->setCurrentIndex(0);
        }
    }
    break;

    case QtPlugin::UnloadModule:
    {
        duint base = data.toULongLong();
        for(int i = 0; i < ui->comboModules->count(); i++)
        {
            duint entryBase = ui->comboModules->itemData(i).toULongLong();
            if(base == entryBase)
            {
                ui->comboModules->removeItem(i);
                break;
            }
        }
    }
    break;

    case QtPlugin::StopDebug:
    {
        mIsDebugging = false;
        ui->comboModules->clear();
        ui->labelStatus->setText("Start debugging to analyze a module...");
        ui->editReport->clear();
        enableUi(false);
        ui->buttonOptions->setEnabled(true);
    }
    break;
    }
}

bool PluginMainWindow::eventFilter(QObject* watched, QEvent* event)
{
    // Only filter key press events for the report
    if(watched != ui->editReport || event->type() != QEvent::KeyPress)
        return false;

    // https://doc.qt.io/qt-6/qplaintextedit.html#read-only-key-bindings
    auto keyEvent = (QKeyEvent*)event;
    switch(keyEvent->key())
    {
    case Qt::Key_Copy:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_Shift:
    case Qt::Key_Alt:
    case Qt::Key_Control:
    case Qt::Key_Meta:
        return false;
    case Qt::Key_C:
    case Qt::Key_A:
        // Select all
        if(keyEvent->modifiers() & (Qt::ControlModifier | Qt::MetaModifier))
            return false;
    default:
        return true;
    }
}

void PluginMainWindow::enableUi(bool enabled)
{
    ui->buttonUpload->setEnabled(enabled);
    ui->buttonOptions->setEnabled(enabled);
    ui->comboModules->setEnabled(enabled);
}

void PluginMainWindow::logInfo(const QString& message)
{
    qDebug().noquote() << message;
    if(mLogFile != nullptr)
    {
        auto date = QString("[%1] ").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
        mLogFile->write(date.toUtf8());
        mLogFile->write(message.toUtf8());
        mLogFile->write("\n");
        mLogFile->flush();
    }
}

void PluginMainWindow::setStatus(const QString& status)
{
    ui->labelStatus->setText(status);
    if(ui->labelStatus->text() != status)
    {
        logInfo("[status] " + status);
    }
}

void PluginMainWindow::pollTimerSlot()
{
    // Reference: https://malcore.readme.io/reference/status-check
    logInfo(QString("[poll] %1").arg(mPollUuid));

    // Create the request
    QNetworkRequest request(QUrl("https://api.malcore.io/api/status"));
    request.setRawHeader("apiKey", mApiKey.toUtf8());
    request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
    request.setHeader(QNetworkRequest::UserAgentHeader, "x64dbg");

    // Perform the POST
    QUrlQuery query;
    query.addQueryItem("uuid", mPollUuid);
    QNetworkReply* reply = mHttp->post(request, query.toString().toUtf8());

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        // Handle the response here
        if (reply->error() == QNetworkReply::NoError)
        {
            QByteArray responseData = reply->readAll();

            logInfo("[poll] response: " + QString::fromUtf8(responseData));

            auto json = QJsonDocument::fromJson(responseData).object();
            auto success = json["success"].toBool();
            if(!success)
            {
                setStatus("Failed to get report!");
                enableUi(true);
                ui->progressBar->setMaximum(100);
                ui->progressBar->setValue(0);
                QMessageBox::critical(this, "Poll Error", reply->errorString());
            }
            else
            {
                QJsonObject data = json["data"].toObject();
                auto status = data["status"].toString();
                if(status == "pending")
                {
                    // Poll again
                    mPollTimer->start();
                }
                else
                {
                    // Finish processing
                    enableUi(true);
                    setStatus("Ready!");
                    ui->progressBar->setMaximum(100);
                    ui->progressBar->setValue(0);

                    // Cache the report
                    auto jsonPath = getReportJsonPath(mPollModule);
                    {
                        QFile f(jsonPath);
                        if(f.open(QIODevice::WriteOnly))
                            f.write(responseData);
                    }
                    mPollUuid.clear();
                    mPollModule = 0;
                    displayReport(std::move(data), jsonPath);
                }
            }
        }
        else
        {
            logInfo("[poll] error: " + reply->errorString());
            setStatus(reply->errorString());
            enableUi(true);
            ui->progressBar->setMaximum(100);
            ui->progressBar->setValue(0);
            QMessageBox::critical(this, "Poll Error", reply->errorString());
        }

        // Clean up
        reply->deleteLater();
    });
    connect(reply, &QNetworkReply::uploadProgress, this, [this](qint64 bytesSent, qint64 bytesTotal)
    {
        logInfo(QString("[poll] upload %1/%2").arg(bytesSent).arg(bytesTotal));
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal)
    {
        logInfo(QString("[poll] download %1/%2").arg(bytesReceived).arg(bytesTotal));
        if(bytesReceived == bytesTotal)
        {
            ui->progressBar->setMaximum(0);
            ui->progressBar->setValue(0);
        }
        else
        {
            ui->progressBar->setMaximum(bytesTotal);
            ui->progressBar->setValue(bytesReceived);
            setStatus("Downloading report...");
        }
    });
}

void PluginMainWindow::loginAcceptedSlot()
{
    mApiKey = mLoginDialog->apiKey();
    BridgeSettingSet("Malcore", "ApiKey", mApiKey.toUtf8().constData());
    if(mLoginDialog->uploadAfter())
    {
        ui->buttonUpload->click();
    }
}

void PluginMainWindow::uploadFile(uintptr_t moduleBase, const QString& path)
{
    // Reference: https://malcore.readme.io/reference/upload
    logInfo("[upload] file: " + path);

    // Open the file for the form data
    auto fileName = QFileInfo(path).fileName();
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QString("form-data; name=\"filename1\"; filename=\"%1\"").arg(fileName));
    QFile* file = new QFile(path);
    if(!file->open(QIODevice::ReadOnly))
    {
        QMessageBox::critical(this, "Error", QString("Failed to open file: %1").arg(path));
        return;
    }

    // Create a multi-part form data object
    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    filePart.setBodyDevice(file);
    file->setParent(multiPart); // Ownership of the file is transferred to the multi-part object
    multiPart->append(filePart);

    // Create the request and set the necessary headers
    QNetworkRequest request(QUrl("https://api.malcore.io/api/upload"));
    request.setRawHeader("apiKey", mApiKey.toUtf8());
    request.setRawHeader("X-No-Poll", "true");
    request.setHeader(QNetworkRequest::UserAgentHeader, "x64dbg");

    // Send the POST request
    QNetworkReply* reply = mHttp->post(request, multiPart);
    multiPart->setParent(reply); // Ownership of the multi-part object is transferred to the reply

    // Connect signals for handling the response
    connect(reply, &QNetworkReply::finished, this, [this, reply, moduleBase]()
    {
        if (reply->error() == QNetworkReply::NoError)
        {
            QByteArray responseData = reply->readAll();

            // Process the response data as needed
            logInfo("[upload] response: " + QString::fromUtf8(responseData));

            auto root = QJsonDocument::fromJson(responseData).object();
            QJsonObject data = root["data"].toObject();
            QJsonObject data2 = data["data"].toObject();

            setStatus("Waiting for report...");

            ui->progressBar->setMaximum(0);
            ui->progressBar->setValue(0);

            // Start polling
            auto uuid = data2["uuid"].toString();
            mPollUuid = uuid;
            mPollModule = moduleBase;
            mPollTimer->start();
        }
        else
        {
            logInfo("[upload] error: " + reply->errorString());
            setStatus(reply->errorString());
            enableUi(true);
            ui->progressBar->setMaximum(100);
            ui->progressBar->setValue(0);

            auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if(status == 403)
            {
                mLoginDialog->startLogin(true);
            }
            else
            {
                QMessageBox::critical(this, "Upload Error", reply->errorString());
            }
        }

        // Clean up
        reply->deleteLater();
    });
    connect(reply, &QNetworkReply::uploadProgress, this, [this](qint64 bytesSent, qint64 bytesTotal)
    {
        logInfo(QString("[upload] upload %1/%2").arg(bytesSent).arg(bytesTotal));
        if(bytesSent == bytesTotal)
        {
            ui->progressBar->setMaximum(0);
            ui->progressBar->setValue(0);
        }
        else
        {
            ui->progressBar->setMaximum(bytesTotal);
            ui->progressBar->setValue(bytesSent);
            setStatus("Uploading executable...");
        }
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal)
    {
        logInfo(QString("[upload] download %1/%2").arg(bytesReceived).arg(bytesTotal));
    });

    ui->editReport->clear();
    ui->progressBar->setMaximum(0);
    ui->progressBar->setValue(0);
    setStatus("Upload started!");
}

void PluginMainWindow::displayReport(QJsonObject data, const QString& jsonPath)
{
    MalcoreAnalysis analysis(std::move(data));
    auto html = analysis.getReportHtml();
    if(!jsonPath.isEmpty())
    {
        auto htmlPath = jsonPath;
        auto periodIdx = htmlPath.lastIndexOf('.');
        if(periodIdx != -1)
            htmlPath.resize(periodIdx);
        htmlPath += ".html";
        QFile f(htmlPath);
        if(f.open(QIODevice::WriteOnly))
            f.write(html.toUtf8());
    }
    ui->editReport->setHtml(html);
}

QString PluginMainWindow::getReportJsonPath(uintptr_t base)
{
    auto modulePath = getModulePath(base);
    if(modulePath.isEmpty())
        return QString();

    auto itr = mReportCache.find(modulePath);
    if(itr != mReportCache.end())
        return itr.value();

    QFile f(modulePath);
    if(!f.open(QIODevice::ReadOnly))
        return QString();

    QCryptographicHash hash(QCryptographicHash::Sha1);
    if(!hash.addData(&f))
        return QString();

    auto sha1 = QString::fromUtf8(hash.result().toHex());
    auto moduleName = QFileInfo(modulePath).baseName();
    auto jsonPath = QString("%1\\report-%2-%3.json").arg(mUserDir, moduleName, sha1);
    mReportCache[modulePath] = jsonPath;
    return jsonPath;
}

void PluginMainWindow::on_buttonUpload_clicked()
{
    if(mApiKey.isEmpty())
    {
        mLoginDialog->startLogin(true);
        return;
    }

    ui->editReport->setFocus();

    auto index = ui->comboModules->currentIndex();
    if(index == -1)
        return;
    duint base = ui->comboModules->itemData(index).toULongLong();
    auto path = getModulePath(base);
    if(path.isEmpty())
        return;

    if(DbgFunctions()->ModGetParty(base) == mod_system)
    {
        if(QMessageBox::question(
                this,
                "System module",
                "The selected module is a system module. Analyzing it would likely not yield any interesting results.\n\nDo you want to upload it anyway?",
                QMessageBox::Yes|QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
    }

    enableUi(false);
    uploadFile(base, path);
}

void PluginMainWindow::on_actionExampleReport_triggered()
{
    QFile f(":/example-report.json");
    if(!f.open(QIODevice::ReadOnly))
        return;

    auto root = QJsonDocument::fromJson(f.readAll()).object();
    displayReport(std::move(root["data"].toObject()), QString());
}

void PluginMainWindow::on_buttonOptions_clicked()
{
    ui->editReport->setFocus();
    ui->menuOptions->exec(QCursor::pos());
}

void PluginMainWindow::on_actionLogin_triggered()
{
    mLoginDialog->startLogin(false);
}

void PluginMainWindow::on_comboModules_currentIndexChanged(int index)
{
    // Clear the current report
    ui->editReport->clear();

    if(index < 0 || index >= ui->comboModules->count())
        return;

    duint base = ui->comboModules->itemData(index).toULongLong();
    auto jsonPath = getReportJsonPath(base);
    if(jsonPath.isEmpty())
        return;

    QFile f(jsonPath);
    if(!f.open(QIODevice::ReadOnly))
        return;

    auto root = QJsonDocument::fromJson(f.readAll()).object();
    displayReport(std::move(root["data"].toObject()), jsonPath);
}

void PluginMainWindow::on_editReport_anchorClicked(const QUrl& url)
{
    if(url.scheme() == "address")
    {
        auto addr = url.host().toULongLong(nullptr, 0);
        if(DbgMemIsValidReadPtr(addr))
        {
            char cmd[256] = "";
            if(DbgFunctions()->MemIsCodePage(addr, true))
            {
                sprintf_s(cmd, "disasm 0x%llX", addr);
            }
            else
            {
                sprintf_s(cmd, "dump 0x%llX", addr);
            }
            DbgCmdExecDirect(cmd);
        }
        else
        {
            QClipboard* clipboard = QApplication::clipboard();
            clipboard->setText(url.host());
            GuiAddStatusBarMessage(tr("The value has been copied to the clipboard.\n").toUtf8().constData());
        }
    }
    else
    {
        QDesktopServices::openUrl(url);
    }
}

