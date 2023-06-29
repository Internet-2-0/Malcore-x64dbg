#include "LoginDialog.h"
#include "ui_LoginDialog.h"
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCloseEvent>
#include "QtPlugin.h"
#include "pluginmain.h"

LoginDialog::LoginDialog(QWidget* parent) : QDialog(parent), ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setFixedSize(size());
    mHttp = new QNetworkAccessManager(this);
    on_checkBoxApiKey_toggled(false);
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::startLogin(bool uploadAfter)
{
    mUploadAfter = uploadAfter;
    ui->checkBoxApiKey->setChecked(false);
    ui->editEmail->clear();
    ui->editPassword->clear();
    open();
}

void LoginDialog::closeEvent(QCloseEvent* event)
{
    if(isEnabled())
        QDialog::closeEvent(event);
    else
        event->setAccepted(false);
}

void LoginDialog::checkApiKey(const QString& apiKey)
{
    // Disable the UI
    setEnabled(false);
    qApp->processEvents();

    // Create the request
    QNetworkRequest request(QUrl("https://api.malcore.io/api/status"));
    request.setRawHeader("apiKey", apiKey.toUtf8());
    request.setHeader(QNetworkRequest::UserAgentHeader, "x64dbg");

    // Perform the POST
    QUrlQuery query;
    query.addQueryItem("uuid", "");
    QNetworkReply* reply = mHttp->post(request, query.toString().toUtf8());

    connect(reply, &QNetworkReply::finished, this, [this, reply, apiKey]()
    {
        // Enable the UI
        setEnabled(true);

        // Handle the response here
        if (reply->error() == QNetworkReply::NoError)
        {
            if(!mUploadAfter)
            {
                QMessageBox::information(this, "Success", "User logged in successfully!");
            }
            mApiKey = apiKey;
            accept();
        }
        else
        {
            QMessageBox::critical(this, "Error", "Failed to verify API Key");
        }

        // Clean up
        reply->deleteLater();
    });
}

void LoginDialog::on_checkBoxApiKey_toggled(bool checked)
{
    if(checked)
    {
        ui->labelEmail->setVisible(false);
        ui->editEmail->setVisible(false);
        ui->labelPassword->setText("API Key:");
        ui->editPassword->setFocus();
        ui->editPassword->clear();
        ui->labelRegister->setText("<a href=\"https://malcore.io/settings\">Settings</a>");
    }
    else
    {
        ui->labelEmail->setVisible(true);
        ui->editEmail->setVisible(true);
        ui->labelPassword->setText("Password:");
        ui->editEmail->setFocus();
        ui->labelRegister->setText("<a href=\"https://link.malcore.io/ref/x64dbg\">Register</a>");
        ui->editPassword->clear();
    }
}

void LoginDialog::on_buttonBox_accepted()
{
    // HACK: attempt to prevent the button from glitching
    qApp->focusWidget()->clearFocus();
    ui->buttonBox->repaint();
    qApp->processEvents();

    if(!ui->checkBoxApiKey->isChecked())
    {
        auto email = ui->editEmail->text();
        auto password = ui->editPassword->text();

        if(email.isEmpty() || password.isEmpty())
        {
            QMessageBox::critical(this, "Error", "Please enter both an email and a password.");
            return;
        }

        // Disable the UI
        setEnabled(false);
        qApp->processEvents();

        // Create the request
        QNetworkRequest request(QUrl("https://api.malcore.io/auth/login"));
        request.setRawHeader("Content-Type", "application/json");
        request.setHeader(QNetworkRequest::UserAgentHeader, "x64dbg");

        // Perform the POST
        QJsonObject body;
        body["email"] = ui->editEmail->text();
        body["password"] = ui->editPassword->text();
        QNetworkReply* reply = mHttp->post(request, QJsonDocument(body).toJson());

        connect(reply, &QNetworkReply::finished, this, [this, reply]()
        {
            // Enable the UI
            setEnabled(true);

            // Handle the response here
            if (reply->error() == QNetworkReply::NoError)
            {
                QByteArray responseData = reply->readAll();
                auto root = QJsonDocument::fromJson(responseData).object();
                auto success = root["success"].toBool();
                if(!success)
                {
                    QJsonArray messages = root["messages"].toArray();
                    if(!messages.isEmpty())
                    {
                        auto entry = messages[0].toObject();
                        auto message = entry["message"].toString();
                        QMessageBox::critical(this, "Error", message);
                    }
                    else
                    {
                        QMessageBox::critical(this, "Error", "Login failed (no message provided by server)");
                    }
                }
                else
                {
                    QJsonObject data = root["data"].toObject();
                    QJsonObject user = data["user"].toObject();
                    auto apiKey = user["apiKey"].toString();
                    if(apiKey == "...")
                    {
                        QMessageBox::critical(this, "Error", "Login successful, but the user doesn't have an API key");
                    }
                    else
                    {
                        checkApiKey(apiKey);
                    }
                }
            }
            else
            {
                QMessageBox::critical(this, "Error", reply->errorString());
            }

            // Clean up
            reply->deleteLater();
        });
    }
    else
    {
        auto apiKey = ui->editPassword->text();
        if(apiKey.isEmpty())
        {
            QMessageBox::critical(this, "Error", "Please enter an API Key");
            return;
        }
        checkApiKey(apiKey);
    }
}

