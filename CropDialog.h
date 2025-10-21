#pragma once

#include <QDialog>
#include <QPixmap>
#include <QMouseEvent>

namespace Ui {
    class CropDialog;
}

class CropDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CropDialog(QWidget* parent = nullptr);
    ~CropDialog();

    void setImage(const QPixmap& image);
    QPixmap getCroppedImage() const;

private slots:
    void onCropTopChanged(int value);
    void onCropBottomChanged(int value);
    void onResetClicked();
    void onAcceptClicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    Ui::CropDialog* ui;
    QPixmap m_originalImage;
    QPixmap m_displayImage;

    // Параметры кадрирования
    int m_cropTop;
    int m_cropBottom;

    // Для интерактивного выбора зон кадрирования
    bool m_draggingTop;
    bool m_draggingBottom;
    int m_dragStartY;

    void updatePreview();
    void drawCropAreas();
    int getImageDisplayY() const;
    int getImageDisplayHeight() const;
    int displayToImageY(int displayY) const;
    int imageToDisplayY(int imageY) const;
};