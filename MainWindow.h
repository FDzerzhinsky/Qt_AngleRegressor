#pragma once

#include <QMainWindow>
#include <QTcpSocket>
#include <QTimer>
#include <QDateTime>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSocketReadyRead();
    void onClearLogClicked();
    void onSendMessageClicked();
    void onMessageTextChanged(const QString& text);

private:
    Ui::MainWindow* ui;
    QTcpSocket* tcpSocket;
    void logMessage(const QString& message);
    void updateSendButtonState();
};