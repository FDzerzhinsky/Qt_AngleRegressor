#include "CropDialog.h"
#include "ui_CropDialog.h"
#include <QPainter>
#include <QMouseEvent>

CropDialog::CropDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::CropDialog),
    m_cropTop(0),
    m_cropBottom(0),
    m_draggingTop(false),
    m_draggingBottom(false),
    m_dragStartY(0)
{
    ui->setupUi(this);

    // Настройка диапазонов слайдеров (будут установлены после загрузки изображения)
    ui->topSlider->setRange(0, 0);
    ui->bottomSlider->setRange(0, 0);

    // Соединения сигналов
    connect(ui->topSlider, &QSlider::valueChanged, this, &CropDialog::onCropTopChanged);
    connect(ui->bottomSlider, &QSlider::valueChanged, this, &CropDialog::onCropBottomChanged);
    connect(ui->resetButton, &QPushButton::clicked, this, &CropDialog::onResetClicked);
    connect(ui->acceptButton, &QPushButton::clicked, this, &CropDialog::onAcceptClicked);
}

CropDialog::~CropDialog()
{
    delete ui;
}

void CropDialog::setImage(const QPixmap& image)
{
    m_originalImage = image;
    m_displayImage = image;

    // Установка диапазонов слайдеров на основе размеров изображения
    int maxCrop = m_originalImage.height() / 2; // Максимальная обрезка - половина высоты
    ui->topSlider->setRange(0, maxCrop);
    ui->bottomSlider->setRange(0, maxCrop);

    // Сброс значений кадрирования
    m_cropTop = 0;
    m_cropBottom = 0;
    ui->topSlider->setValue(0);
    ui->bottomSlider->setValue(0);

    updatePreview();
}

QPixmap CropDialog::getCroppedImage() const
{
    if (m_cropTop == 0 && m_cropBottom == 0) {
        return m_originalImage; // Возвращаем оригинал если кадрирование не применялось
    }

    // Создаем обрезанное изображение
    int cropHeight = m_originalImage.height() - m_cropTop - m_cropBottom;
    if (cropHeight <= 0) {
        return m_originalImage; // Защита от некорректных значений
    }

    QRect cropRect(0, m_cropTop, m_originalImage.width(), cropHeight);
    return m_originalImage.copy(cropRect);
}

void CropDialog::onCropTopChanged(int value)
{
    m_cropTop = value;
    updatePreview();
}

void CropDialog::onCropBottomChanged(int value)
{
    m_cropBottom = value;
    updatePreview();
}

void CropDialog::onResetClicked()
{
    m_cropTop = 0;
    m_cropBottom = 0;
    ui->topSlider->setValue(0);
    ui->bottomSlider->setValue(0);
    updatePreview();
}

void CropDialog::onAcceptClicked()
{
    accept(); // Закрываем диалог с результатом Accepted
}

void CropDialog::updatePreview()
{
    // Обновляем текст с информацией о кадрировании
    QString info = QString("Обрезка: сверху %1px, снизу %2px. Итоговый размер: %3x%4")
        .arg(m_cropTop)
        .arg(m_cropBottom)
        .arg(m_originalImage.width())
        .arg(m_originalImage.height() - m_cropTop - m_cropBottom);
    ui->infoLabel->setText(info);

    // Перерисовываем preview
    update();
}

void CropDialog::drawCropAreas()
{
    if (m_originalImage.isNull()) return;

    // Получаем координаты отображения изображения
    int imgY = getImageDisplayY();
    int imgHeight = getImageDisplayHeight();

    // Вычисляем позиции линий кадрирования в координатах отображения
    int topLine = imgY + imageToDisplayY(m_cropTop);
    int bottomLine = imgY + imageToDisplayY(m_originalImage.height() - m_cropBottom);

    QPainter painter(this);

    // Рисуем затемненные области сверху и снизу
    QColor darkArea(0, 0, 0, 128); // Полупрозрачный черный

    // Верхняя область кадрирования
    if (m_cropTop > 0) {
        painter.fillRect(0, imgY, width(), topLine - imgY, darkArea);
    }

    // Нижняя область кадрирования
    if (m_cropBottom > 0) {
        painter.fillRect(0, bottomLine, width(), imgY + imgHeight - bottomLine, darkArea);
    }

    // Рисуем линии кадрирования
    QPen linePen(Qt::red, 2);
    painter.setPen(linePen);

    // Верхняя линия
    if (m_cropTop > 0) {
        painter.drawLine(0, topLine, width(), topLine);
    }

    // Нижняя линия
    if (m_cropBottom > 0) {
        painter.drawLine(0, bottomLine, width(), bottomLine);
    }

    // Подписи линий
    painter.setPen(Qt::white);
    if (m_cropTop > 0) {
        painter.drawText(10, topLine - 5, QString("Сверху: %1px").arg(m_cropTop));
    }
    if (m_cropBottom > 0) {
        painter.drawText(10, bottomLine + 15, QString("Снизу: %1px").arg(m_cropBottom));
    }
}

void CropDialog::paintEvent(QPaintEvent* event)
{
    QDialog::paintEvent(event);

    if (m_originalImage.isNull()) return;

    QPainter painter(this);

    // Вычисляем размеры для отображения изображения с сохранением пропорций
    QSize labelSize = ui->previewLabel->size();
    QPixmap scaledImage = m_originalImage.scaled(
        labelSize.width() - 4,    // -4 для отступов
        labelSize.height() - 4,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );

    // Центрируем изображение в label
    int x = (labelSize.width() - scaledImage.width()) / 2;
    int y = (labelSize.height() - scaledImage.height()) / 2;

    // Сохраняем параметры отображения для обработки мыши
    m_displayImage = scaledImage;

    // Рисуем изображение
    painter.drawPixmap(x, y, scaledImage);

    // Рисуем области кадрирования
    drawCropAreas();
}

void CropDialog::mousePressEvent(QMouseEvent* event)
{
    if (m_originalImage.isNull()) {
        QDialog::mousePressEvent(event);
        return;
    }

    int imgY = getImageDisplayY();
    int imgHeight = getImageDisplayHeight();
    int mouseY = event->pos().y();

    // Проверяем, близко ли мышь к линиям кадрирования
    int topLine = imgY + imageToDisplayY(m_cropTop);
    int bottomLine = imgY + imageToDisplayY(m_originalImage.height() - m_cropBottom);

    const int grabMargin = 10; // Радиус захвата линии

    if (abs(mouseY - topLine) <= grabMargin) {
        m_draggingTop = true;
        m_dragStartY = mouseY;
    }
    else if (abs(mouseY - bottomLine) <= grabMargin) {
        m_draggingBottom = true;
        m_dragStartY = mouseY;
    }

    QDialog::mousePressEvent(event);
}

void CropDialog::mouseMoveEvent(QMouseEvent* event)
{
    if ((m_draggingTop || m_draggingBottom) && !m_originalImage.isNull()) {
        int mouseY = event->pos().y();
        int deltaY = mouseY - m_dragStartY;

        if (deltaY != 0) {
            int imgHeight = getImageDisplayHeight();
            int originalHeight = m_originalImage.height();

            // Преобразуем смещение в координаты изображения
            int deltaImageY = (deltaY * originalHeight) / imgHeight;

            if (m_draggingTop) {
                int newTop = qMax(0, qMin(m_cropTop + deltaImageY, originalHeight - m_cropBottom - 1));
                m_cropTop = newTop;
                ui->topSlider->setValue(newTop);
            }
            else if (m_draggingBottom) {
                int newBottom = qMax(0, qMin(m_cropBottom - deltaImageY, originalHeight - m_cropTop - 1));
                m_cropBottom = newBottom;
                ui->bottomSlider->setValue(newBottom);
            }

            m_dragStartY = mouseY;
            updatePreview();
        }
    }

    QDialog::mouseMoveEvent(event);
}

void CropDialog::mouseReleaseEvent(QMouseEvent* event)
{
    m_draggingTop = false;
    m_draggingBottom = false;
    QDialog::mouseReleaseEvent(event);
}

int CropDialog::getImageDisplayY() const
{
    QSize labelSize = ui->previewLabel->size();
    int displayHeight = m_displayImage.height();
    return (labelSize.height() - displayHeight) / 2;
}

int CropDialog::getImageDisplayHeight() const
{
    return m_displayImage.height();
}

int CropDialog::displayToImageY(int displayY) const
{
    if (m_displayImage.isNull()) return 0;
    return (displayY * m_originalImage.height()) / m_displayImage.height();
}

int CropDialog::imageToDisplayY(int imageY) const
{
    if (m_originalImage.isNull()) return 0;
    return (imageY * m_displayImage.height()) / m_originalImage.height();
}