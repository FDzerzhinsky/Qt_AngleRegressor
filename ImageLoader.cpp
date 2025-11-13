#include "ImageLoader.h"
#include <QPdfDocument>
#include <QImage>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>

// Реализация StandardImageLoader
QPixmap StandardImageLoader::load(const QString& filePath) {
    QPixmap image(filePath);
    if (image.isNull()) {
        throw std::runtime_error("Failed to load image file");
    }
    return image;
}

bool StandardImageLoader::canLoad(const QString& filePath) {
    QString extension = QFileInfo(filePath).suffix().toLower();
    return m_supportedFormats.contains(extension);
}

PdfImageLoader::PdfImageLoader(int dpi)
    : m_dpi(dpi)
{
}

// Реализация PdfImageLoader
QPixmap PdfImageLoader::load(const QString& filePath) {
    // Создаем документ на стеке (без умного указателя)
    QPdfDocument pdfDocument;

    // Загружаем PDF документ
    if (pdfDocument.load(filePath) != QPdfDocument::Error::None) {
        throw std::runtime_error("Failed to load PDF document");
    }

    // Проверяем количество страниц
    if (pdfDocument.pageCount() != 1) {
        throw std::runtime_error("PDF must contain exactly one page");
    }

    // Получаем реальный DPI системы
    //  TODO:   Задепрекейтить
    QScreen* screen = QGuiApplication::primaryScreen();
    qreal realDpi = screen ? screen->logicalDotsPerInch() : 96.0;

    // Устанавливаем нужный DPI
    qreal renderDpi = static_cast<qreal>(m_dpi);
    


    // Получаем размер страницы в пунктах (1/72 дюйма)
    QSizeF pageSizeInPoints = pdfDocument.pagePointSize(0);
    if (pageSizeInPoints.isEmpty()) {
        throw std::runtime_error("Failed to get page size");
    }

    // Конвертируем в пиксели с использованием реального DPI
    int renderWidth = static_cast<int>(pageSizeInPoints.width() * renderDpi / 72.0);
    int renderHeight = static_cast<int>(pageSizeInPoints.height() * renderDpi / 72.0);

    qDebug() << "PDF Page size in points:" << pageSizeInPoints;
    qDebug() << "Real DPI:" << realDpi;
    qDebug() << "Render size:" << renderWidth << "x" << renderHeight;

    // Рендерим с высоким качеством - ВАЖНО: вызываем render у объекта, а не указателя!
    QImage image = pdfDocument.render(0, QSize(renderWidth, renderHeight));

    // Явно завершаем работу с документом
    pdfDocument.close();

    if (image.isNull()) {
        throw std::runtime_error("Failed to render PDF page");
    }

    // Создаем QPixmap напрямую из QImage
    QPixmap pixmap = QPixmap::fromImage(image);

    if (pixmap.isNull()) {
        throw std::runtime_error("Failed to convert image to pixmap");
    }

    return pixmap;
}

// УБЕДИТЕСЬ, ЧТО ЭТА ФУНКЦИЯ ПРИСУТСТВУЕТ!
bool PdfImageLoader::canLoad(const QString& filePath) {
    QString extension = QFileInfo(filePath).suffix().toLower();
    return extension == "pdf";
}

// Реализация ImageLoaderFactory
std::unique_ptr<ImageLoader> ImageLoaderFactory::createLoader(const QString& filePath, int pdfDpi) {
    // Создаем загрузчики и проверяем, какой может обработать файл
    auto pdfLoader = std::make_unique<PdfImageLoader>();
    auto standardLoader = std::make_unique<StandardImageLoader>();

    if (pdfLoader->canLoad(filePath)) {
        // ПЕРЕДАЕМ DPI В PDF ЗАГРУЗЧИК
        return std::make_unique<PdfImageLoader>(pdfDpi);
    }
    else if (standardLoader->canLoad(filePath)) {
        return standardLoader;
    }

    // Если формат не поддерживается, возвращаем стандартный загрузчик
    // и позволим ему выбросить исключение при загрузке
    return standardLoader;
}
