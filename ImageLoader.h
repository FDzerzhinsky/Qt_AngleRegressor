#pragma once

#include <QPixmap>
#include <QString>
#include <QFileInfo>
#include <memory>
#include <stdexcept>

// Базовый класс для всех загрузчиков изображений
class ImageLoader {
public:
    virtual ~ImageLoader() = default;
    virtual QPixmap load(const QString& filePath) = 0;
    virtual bool canLoad(const QString& filePath) = 0;
};

// Загрузчик для стандартных форматов изображений
class StandardImageLoader : public ImageLoader {
public:
    QPixmap load(const QString& filePath) override;
    bool canLoad(const QString& filePath) override;

private:
    QStringList m_supportedFormats = { "png", "jpg", "jpeg", "bmp", "tiff" };
};

// Загрузчик для PDF файлов
class PdfImageLoader : public ImageLoader {
public:
    PdfImageLoader(int dpi = 300);
    QPixmap load(const QString& filePath) override;
    bool canLoad(const QString& filePath) override;  // УБЕДИТЕСЬ, ЧТО ЭТА СТРОКА ЕСТЬ!
private:
    int m_dpi;  // ЧЛЕН ДЛЯ ХРАНЕНИЯ DPI
};

// Фабрика для создания загрузчиков
class ImageLoaderFactory {
public:
    static std::unique_ptr<ImageLoader> createLoader(const QString& filePath, int pdfDpi = 300);
};