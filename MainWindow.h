#pragma once

#include <QMainWindow>
#include <QThread>

// Ïğåäâàğèòåëüíîå îáúÿâëåíèå êëàññîâ
class BusinessLogic;
namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    // ==================== ÑËÎÒÛ ÄËß ÂÊËÀÄÊÈ "ÑÎÊÅÒ" ====================
    void onConnectClicked();
    void onDisconnectClicked();
    void onClearLogClicked();
    void onSendMessageClicked();
    void onMessageTextChanged(const QString& text);

    // ==================== ÑËÎÒÛ ÄËß ÂÊËÀÄÊÈ "ĞÀÇÂ¨ĞÒÊÀ" ====================
    void onLoadImageClicked();
    void onProcessImageClicked();
    void onClearImageClicked();

    // ==================== ÑËÎÒÛ ÄËß ÎÁĞÀÁÎÒÊÈ ÑÈÃÍÀËÎÂ ÎÒ ÁÈÇÍÅÑ-ËÎÃÈÊÈ ====================
    void onLogMessage(const QString& message);
    void onConnectionStateChanged(bool connected);
    void onImageLoaded(const QPixmap& preview, const QString& fileName);
    void onImageProcessed();
    void onImageCleared();
    void onSocketError(const QString& error);

private:
    Ui::MainWindow* ui;

    // Áèçíåñ-ëîãèêà è å¸ ïîòîê
    BusinessLogic* m_businessLogic;
    QThread* m_businessThread;

    // ==================== ÂÑÏÎÌÎÃÀÒÅËÜÍÛÅ ÌÅÒÎÄÛ ====================
    void setupConnections();
    void updateSendButtonState();
    void updateImageButtonsState();
};