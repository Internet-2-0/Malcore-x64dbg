#pragma once

#include <QDialog>
#include <QNetworkAccessManager>

namespace Ui
{
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget* parent);
    ~LoginDialog();
    void startLogin(bool uploadAfter);
    QString apiKey() const { return mApiKey; }
    bool uploadAfter() const { return mUploadAfter; }

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void checkApiKey(const QString& apiKey);

private slots:
    void on_checkBoxApiKey_toggled(bool checked);
    void on_buttonBox_accepted();

private:
    Ui::LoginDialog *ui;
    QNetworkAccessManager* mHttp = nullptr;
    QString mApiKey;
    bool mUploadAfter = false;
};
