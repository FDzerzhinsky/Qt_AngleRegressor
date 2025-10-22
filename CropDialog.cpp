#include "CropDialog.h"
#include "ui_CropDialog.h"
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QScreen>
#include <QApplication>

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

    // Настройка окна - разрешаем разворачивание на весь экран
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);

    // Установка подсказки для пользователя
    ui->instructionLabel->setText("Перетащи верхнюю и/или нижнюю линию для обрезки изображения");

    // Соединения сигналов кнопок
    connect(ui->resetButton, &QPushButton::clicked, this, &CropDialog::onResetClicked);
    connect(ui->acceptButton, &QPushButton::clicked, this, &CropDialog::onAcceptClicked);

    // Устанавливаем фокус на кнопку "Далее" для удобства
    ui->acceptButton->setFocus();
}

CropDialog::~CropDialog()
{
    delete ui;
}

void CropDialog::setImage(const QPixmap& image)
{
    m_originalImage = image;

    // Сброс значений кадрирования
    m_cropTop = 0;
    m_cropBottom = 0;

    // Обновляем геометрию отображения и превью
    updateImageDisplayRect();
    updatePreview();
}

QPixmap CropDialog::getCroppedImage() const
{
    // Если кадрирование не применялось, возвращаем оригинальное изображение
    if (m_cropTop == 0 && m_cropBottom == 0) {
        return m_originalImage;
    }

    // Вычисляем высоту обрезанного изображения
    int cropHeight = m_originalImage.height() - m_cropTop - m_cropBottom;

    // Защита от некорректных значений
    if (cropHeight <= 0) {
        return m_originalImage;
    }

    // Создаем прямоугольник кадрирования и возвращаем обрезанное изображение
    QRect cropRect(0, m_cropTop, m_originalImage.width(), cropHeight);
    return m_originalImage.copy(cropRect);
}

void CropDialog::onResetClicked()
{
    // Сброс кадрирования к исходному состоянию
    m_cropTop = 0;
    m_cropBottom = 0;
    updatePreview();
}

void CropDialog::onAcceptClicked()
{
    accept();
}

void CropDialog::updatePreview()
{
    // Обновляем информационную метку с данными о кадрировании
    QString info = QString("Кадрировано: верх %1px, низ %2px. финальный размер: %3x%4")
        .arg(m_cropTop)
        .arg(m_cropBottom)
        .arg(m_originalImage.width())
        .arg(m_originalImage.height() - m_cropTop - m_cropBottom);
    ui->infoLabel->setText(info);

    update();
}

void CropDialog::updateImageDisplayRect()
{
    if (m_originalImage.isNull()) return;

    // Получаем размеры области preview (QLabel)
    QSize previewSize = ui->previewLabel->size();

    // Вычисляем максимальный размер для отображения с сохранением пропорций
    int maxWidth = previewSize.width() - 20;  // Отступы по бокам
    int maxHeight = previewSize.height() - 60; // Увеличиваем отступы сверху и снизу для текста

    // Масштабируем изображение с сохранением пропорций
    m_displayImage = m_originalImage.scaled(
        maxWidth,
        maxHeight,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );

    // Вычисляем прямоугольник для отображения изображения (центрированный в previewLabel)
    int x = (previewSize.width() - m_displayImage.width()) / 2;
    int y = (previewSize.height() - m_displayImage.height()) / 2;

    // Получаем глобальную позицию previewLabel и вычисляем прямоугольник отображения
    QPoint previewPos = ui->previewLabel->pos();
    m_imageDisplayRect = QRect(previewPos.x() + x, previewPos.y() + y,
        m_displayImage.width(), m_displayImage.height());
}

void CropDialog::drawCropAreas(QPainter& painter)
{
    if (m_originalImage.isNull() || m_displayImage.isNull()) return;

    // Вычисляем позиции линий кадрирования в координатах отображения
    int topLine = m_imageDisplayRect.top() + imageToDisplayY(m_cropTop);
    int bottomLine = m_imageDisplayRect.top() + imageToDisplayY(m_originalImage.height() - m_cropBottom);

    // Рисуем затемненные области кадрирования
    QColor darkArea(0, 0, 0, 150);

    // Верхняя область кадрирования
    if (m_cropTop > 0) {
        QRect topCropRect(m_imageDisplayRect.left(), m_imageDisplayRect.top(),
            m_imageDisplayRect.width(), topLine - m_imageDisplayRect.top());
        painter.fillRect(topCropRect, darkArea);
    }

    // Нижняя область кадрирования
    if (m_cropBottom > 0) {
        QRect bottomCropRect(m_imageDisplayRect.left(), bottomLine,
            m_imageDisplayRect.width(), m_imageDisplayRect.bottom() - bottomLine);
        painter.fillRect(bottomCropRect, darkArea);
    }

    // Настройка пера для линий кадрирования
    QPen linePen(QColor(255, 50, 50), 3);
    painter.setPen(linePen);

    // Рисуем верхнюю линию кадрирования
    if (m_cropTop > 0) {
        painter.drawLine(m_imageDisplayRect.left(), topLine, m_imageDisplayRect.right(), topLine);

        // Добавляем индикатор размера обрезки сверху
        painter.setPen(Qt::white);
        painter.drawText(m_imageDisplayRect.left() + 10, topLine - 8, QString("↑ %1px").arg(m_cropTop));
        painter.setPen(linePen);
    }

    // Рисуем нижнюю линию кадрирования
    if (m_cropBottom > 0) {
        painter.drawLine(m_imageDisplayRect.left(), bottomLine, m_imageDisplayRect.right(), bottomLine);

        // Добавляем индикатор размера обрезки снизу
        painter.setPen(Qt::white);
        painter.drawText(m_imageDisplayRect.left() + 10, bottomLine + 18, QString("↓ %1px").arg(m_cropBottom));
    }

    // Рисуем подсказочные линии независимо для верхнего и нижнего края
    QPen hintPen(QColor(100, 100, 255), 2, Qt::DashLine);

    // Получаем геометрию previewLabel для ограничения области рисования текста
    QRect previewRect = ui->previewLabel->geometry();
    int textVerticalOffset = 25; // Отступ для текста от границ изображения

    // Верхняя подсказочная линия (только если не сдвинута)
    if (m_cropTop == 0) {
        painter.setPen(hintPen);
        painter.drawLine(m_imageDisplayRect.left(), m_imageDisplayRect.top(),
            m_imageDisplayRect.right(), m_imageDisplayRect.top());

        // Текст подсказки для верхней линии - внутри previewLabel
        painter.setPen(Qt::white);
        QRect textRect(previewRect.left() + 10, previewRect.top() - 4,
            previewRect.width() - 20, 30);
        painter.drawText(textRect, Qt::AlignCenter, "Перетащи для обрезки сверху");
    }

    // Нижняя подсказочная линия (только если не сдвинута)
    if (m_cropBottom == 0) {
        painter.setPen(hintPen);
        painter.drawLine(m_imageDisplayRect.left(), m_imageDisplayRect.bottom(),
            m_imageDisplayRect.right(), m_imageDisplayRect.bottom());

        // Текст подсказки для нижней линии - внутри previewLabel
        painter.setPen(Qt::white);
        QRect textRect(previewRect.left() + 10, previewRect.bottom() - 35,
            previewRect.width() - 20, 30);
        painter.drawText(textRect, Qt::AlignCenter, "Перетащи для обрезки снизу");
    }
}

void CropDialog::paintEvent(QPaintEvent* event)
{
    QDialog::paintEvent(event);

    if (m_originalImage.isNull()) return;

    QPainter painter(this);

    // Рисуем рамку вокруг области preview
    QRect previewFrame = ui->previewLabel->geometry();
    painter.setPen(QPen(Qt::darkGray, 2));
    painter.drawRect(previewFrame);

    // Рисуем масштабированное изображение в вычисленном прямоугольнике
    painter.drawPixmap(m_imageDisplayRect, m_displayImage);

    // Рисуем области и линии кадрирования поверх изображения
    drawCropAreas(painter);
}

void CropDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    updateImageDisplayRect();
    updatePreview();
}

void CropDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    // При показе окна устанавливаем оптимальный размер
    QScreen* screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();

    // Устанавливаем начальный размер окна (80% от экрана)
    QSize initialSize = screenGeometry.size() * 0.8;
    resize(initialSize);

    // Центрируем окно на экране
    move(screenGeometry.center() - rect().center());

    updateImageDisplayRect();
    updatePreview();
}

void CropDialog::mousePressEvent(QMouseEvent* event)
{
    if (m_originalImage.isNull()) {
        QDialog::mousePressEvent(event);
        return;
    }

    QPoint mousePos = event->pos();

    // Проверяем, находится ли мышь в области отображения изображения
    if (!m_imageDisplayRect.contains(mousePos)) {
        QDialog::mousePressEvent(event);
        return;
    }

    int mouseY = mousePos.y();

    // Вычисляем позиции линий кадрирования
    int topLine = m_imageDisplayRect.top() + imageToDisplayY(m_cropTop);
    int bottomLine = m_imageDisplayRect.top() + imageToDisplayY(m_originalImage.height() - m_cropBottom);

    const int grabMargin = 25; // Увеличенный радиус захвата линии

    // Проверяем близость к верхней линии кадрирования
    if (abs(mouseY - topLine) <= grabMargin) {
        m_draggingTop = true;
        m_dragStartY = mouseY;
        return;
    }

    // Проверяем близость к нижней линии кадрирования
    if (abs(mouseY - bottomLine) <= grabMargin) {
        m_draggingBottom = true;
        m_dragStartY = mouseY;
        return;
    }

    QDialog::mousePressEvent(event);
}

void CropDialog::mouseMoveEvent(QMouseEvent* event)
{
    if ((m_draggingTop || m_draggingBottom) && !m_originalImage.isNull()) {
        int mouseY = event->pos().y();
        int deltaY = mouseY - m_dragStartY;

        if (deltaY != 0) {
            int deltaImageY = displayToImageY(deltaY);

            if (m_draggingTop) {
                int newTop = qMax(0, qMin(m_cropTop + deltaImageY, m_originalImage.height() - m_cropBottom - 10));
                m_cropTop = newTop;
            }
            else if (m_draggingBottom) {
                int newBottom = qMax(0, qMin(m_cropBottom - deltaImageY, m_originalImage.height() - m_cropTop - 10));
                m_cropBottom = newBottom;
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

int CropDialog::displayToImageY(int displayY) const
{
    if (m_displayImage.isNull() || m_imageDisplayRect.height() == 0) return 0;
    return (displayY * m_originalImage.height()) / m_imageDisplayRect.height();
}

int CropDialog::imageToDisplayY(int imageY) const
{
    if (m_originalImage.isNull() || m_originalImage.height() == 0) return 0;
    return (imageY * m_imageDisplayRect.height()) / m_originalImage.height();
}