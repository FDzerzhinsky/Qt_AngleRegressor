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

    // Установка изображения для кадрирования
    void setImage(const QPixmap& image);
    // Получение кадрированного изображения
    QPixmap getCroppedImage() const;

protected:
    // Обработчики событий мыши для интерактивного кадрирования
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    // Обработчики событий отрисовки и изменения размера
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onResetClicked();    // Сброс кадрирования
    void onAcceptClicked();   // Применение кадрирования

private:
    Ui::CropDialog* ui;
    QPixmap m_originalImage;      // Исходное изображение
    QPixmap m_displayImage;       // Масштабированное изображение для отображения

    // Параметры кадрирования в пикселях исходного изображения
    int m_cropTop;
    int m_cropBottom;

    // Для интерактивного перемещения линий кадрирования
    bool m_draggingTop;
    bool m_draggingBottom;
    int m_dragStartY;

    // Геометрия отображения
    QRect m_imageDisplayRect;     // Прямоугольник отображения изображения на экране

    void updatePreview();                          // Обновление превью
    void updateImageDisplayRect();                 // Пересчет прямоугольника отображения
    void drawCropAreas(QPainter& painter);         // Отрисовка областей кадрирования
    int displayToImageY(int displayY) const;       // Конвертация координат
    int imageToDisplayY(int imageY) const;         // Конвертация координат
};