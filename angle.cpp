// =============================================================================
// ВКЛЮЧЕНИЕ БИБЛИОТЕК
// =============================================================================
#include "angle.h"
#include <QDebug>

using namespace cv;
using namespace std;
using namespace chrono;

// =============================================================================
// ЧТЕНИЕ КОНФИГУРАЦИИ НЕЙРОСЕТЕВОЙ МОДЕЛИ ИЗ ФАЙЛА
// =============================================================================
NeuralNetworkConfig read_neural_network_config(const std::string& filename) {
    NeuralNetworkConfig config;

    ifstream file(filename);
    if (file.is_open()) {
        string line;
        string current_section = "";
        while (getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;

            if (line[0] == '[' && line[line.length() - 1] == ']') {
                current_section = line.substr(1, line.length() - 2);
                continue;
            }

            if (current_section != "NeuralNetwork") continue;

            istringstream iss(line);
            string key, value;
            if (getline(iss, key, '=') && getline(iss, value)) {
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);

                try {
                    if (key == "model_path") config.model_path = value;
                    else if (key == "input_width") config.input_size.width = stoi(value);
                    else if (key == "input_height") config.input_size.height = stoi(value);
                    else if (key == "input_channels") config.input_channels = stoi(value);
                    else if (key == "mean") config.mean = stof(value);
                    else if (key == "std") config.std = stof(value);
                    else if (key == "num_classes") config.num_classes = stoi(value);
                    else if (key == "use_gpu") config.use_gpu = (value == "true" || value == "1");
                }
                catch (const exception& e) {
                    cerr << "Error parsing config value: " << key << " = " << value << endl;
                    cerr << "Error message: " << e.what() << endl;
                }
            }
        }
        file.close();
    }
    else {
        cout << "Config file not found, using default neural network values" << endl;
    }

    return config;
}

// =============================================================================
// ПРЕДОБРАБОТКА ИЗОБРАЖЕНИЯ ДЛЯ НЕЙРОСЕТИ (БЕЗ РЕСАЙЗА)
// =============================================================================
cv::Mat preprocess_frame_for_nn(const cv::Mat& input_frame, const NeuralNetworkConfig& config) {
    cv::Mat processed_frame;

    try {
        qDebug() << "=== НАЧАЛО ПРЕДОБРАБОТКИ ИЗОБРАЖЕНИЯ ===";
        qDebug() << "Входное изображение - Тип:" << input_frame.type() << "Каналы:" << input_frame.channels()
            << "Размер:" << input_frame.cols << "x" << input_frame.rows;

        // Проверяем размер изображения
        if (input_frame.cols != config.input_size.width || input_frame.rows != config.input_size.height) {
            qDebug() << "ОШИБКА: Размер изображения" << input_frame.cols << "x" << input_frame.rows
                << "не соответствует ожидаемому" << config.input_size.width << "x" << config.input_size.height;
            return cv::Mat();
        }

        // Конвертация BGR в Grayscale
        if (input_frame.channels() == 3) {
            cv::cvtColor(input_frame, processed_frame, cv::COLOR_BGR2GRAY);
            qDebug() << "Конвертировано BGR -> Grayscale";
        }
        else if (input_frame.channels() == 1) {
            processed_frame = input_frame.clone();
            qDebug() << "Изображение уже в Grayscale, скопировано";
        }
        else {
            qDebug() << "ОШИБКА: Неподдерживаемое количество каналов:" << input_frame.channels();
            return cv::Mat();
        }

        qDebug() << "Размер изображения корректен, ресайз не требуется";

        // Конвертация в float32 и масштабирование в [0, 1]
        cv::Mat float_frame;
        processed_frame.convertTo(float_frame, CV_32FC1, 1.0 / 255.0);
        qDebug() << "Конвертировано в float32 и масштабировано в [0, 1]";

        // Ручная нормализация
        cv::Mat normalized_frame = float_frame.clone();
        normalized_frame -= config.mean;
        normalized_frame /= config.std;

        qDebug() << "Ручная нормализация применена: mean =" << config.mean << "std =" << config.std;

        // Создание blob
        cv::Mat input_blob = cv::dnn::blobFromImage(
            normalized_frame,
            1.0,
            cv::Size(),
            cv::Scalar(0),
            false,
            false,
            CV_32F
        );

        qDebug() << "Blob создан - Размеры:" << input_blob.size[0] << "x"
            << input_blob.size[1] << "x" << input_blob.size[2] << "x" << input_blob.size[3];

        // Проверяем статистику blob
        cv::Scalar blob_mean, blob_stddev;
        cv::meanStdDev(input_blob, blob_mean, blob_stddev);
        qDebug() << "Статистика blob - Среднее:" << blob_mean[0] << "Стд. отклонение:" << blob_stddev[0];

        qDebug() << "=== ПРЕДОБРАБОТКА ЗАВЕРШЕНА УСПЕШНО ===";

        return input_blob;
    }
    catch (const std::exception& e) {
        qDebug() << "ОШИБКА в предобработке:" << e.what();
        return cv::Mat();
    }
}

// =============================================================================
// ИНИЦИАЛИЗАЦИЯ НЕЙРОСЕТЕВОЙ ОБРАБОТКИ
// =============================================================================
void initialize_angle_processing(AngleContext& context) {
    context.nn_config = read_neural_network_config();

    qDebug() << "Конфигурация нейросети загружена:";
    qDebug() << "  model_path:" << QString::fromStdString(context.nn_config.model_path);
    qDebug() << "  input_size:" << context.nn_config.input_size.width << "x" << context.nn_config.input_size.height;
    qDebug() << "  input_channels:" << context.nn_config.input_channels;
    qDebug() << "  mean:" << context.nn_config.mean;
    qDebug() << "  std:" << context.nn_config.std;
    qDebug() << "  num_classes:" << context.nn_config.num_classes;
    qDebug() << "  use_gpu:" << context.nn_config.use_gpu;

    try {
        qDebug() << "Загрузка нейросетевой модели из:" << QString::fromStdString(context.nn_config.model_path);

        context.neural_net = cv::dnn::readNetFromONNX(context.nn_config.model_path);

        if (context.neural_net.empty()) {
            throw runtime_error("Не удалось загрузить нейросетевую модель из: " + context.nn_config.model_path);
        }

        context.neural_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        context.neural_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        qDebug() << "Используется CPU бэкенд для нейросети";

        context.model_loaded = true;
        qDebug() << "Нейросетевая модель загружена успешно";

    }
    catch (const exception& e) {
        QString error_msg = "Ошибка инициализации нейросети: " + QString(e.what());
        qDebug() << error_msg;
        throw runtime_error(error_msg.toStdString());
    }
}

// =============================================================================
// ОБРАБОТКА ОДНОГО КАДРА НЕЙРОСЕТЕВОЙ МОДЕЛЬЮ
// =============================================================================
bool process_frame(AngleContext& context, const cv::Mat& query_image) {
    context.processing_start_time = high_resolution_clock::now();

    if (query_image.empty()) {
        qDebug() << "ОШИБКА: Получено пустое изображение!";
        return false;
    }

    if (!context.model_loaded || context.neural_net.empty()) {
        qDebug() << "ОШИБКА: Нейросетевая модель не загружена!";
        return false;
    }

    qDebug() << "Запуск нейросетевого инференса...";

    try {
        cv::Mat input_blob = preprocess_frame_for_nn(query_image, context.nn_config);

        if (input_blob.empty()) {
            qDebug() << "ОШИБКА: Предобработка кадра не удалась!";
            return false;
        }

        context.neural_net.setInput(input_blob);
        cv::Mat network_output = context.neural_net.forward();

        qDebug() << "Инференс нейросети завершен";
        qDebug() << "Выходной тензор - Размеры:" << network_output.size[0] << "x" << network_output.size[1];

        if (network_output.empty() || network_output.cols != context.nn_config.num_classes) {
            qDebug() << "ОШИБКА: Некорректный выход сети!";
            qDebug() << "Ожидалось:" << context.nn_config.num_classes << "классов, получено:" << network_output.cols;
            return false;
        }

        // Softmax
        cv::Mat probabilities;
        cv::exp(network_output, probabilities);
        cv::Scalar sum = cv::sum(probabilities);
        probabilities /= sum[0];

        // Находим класс с максимальной вероятностью
        cv::Point class_id_point;
        double max_prob;
        cv::minMaxLoc(probabilities, nullptr, &max_prob, nullptr, &class_id_point);

        int predicted_class = class_id_point.x;
        float confidence = static_cast<float>(max_prob);

        if (predicted_class < 0 || predicted_class >= context.nn_config.num_classes) {
            qDebug() << "ОШИБКА: Некорректный предсказанный класс:" << predicted_class;
            return false;
        }

        qDebug() << "Предсказание - Класс:" << predicted_class << "Уверенность:" << confidence;

        context.processing_end_time = high_resolution_clock::now();

        auto processing_duration = duration_cast<microseconds>(
            context.processing_end_time - context.processing_start_time
        );
        auto total_duration = duration_cast<microseconds>(
            context.processing_end_time - context.capture_start_time
        );

        std::lock_guard<std::mutex> lock(context.data_mutex);
        context.execution_time = processing_duration.count() / 1000.0;
        context.total_time = total_duration.count() / 1000.0;
        context.predicted_angle = predicted_class;
        context.confidence = confidence;
        context.processing_complete = true;

        qDebug() << "Кадр обработан успешно: Угол =" << predicted_class
            << "°, Уверенность =" << confidence << "Время =" << total_duration.count() / 1000.0 << "мс";
        return true;

    }
    catch (const std::exception& e) {
        qDebug() << "ОШИБКА во время нейросетевого инференса:" << e.what();
        return false;
    }
}

// =============================================================================
// СОЗДАНИЕ И ИНИЦИАЛИЗАЦИЯ КОНТЕКСТА НЕЙРОСЕТЕВОЙ ОБРАБОТКИ
// =============================================================================
std::unique_ptr<AngleContext> create_angle_context() {
    auto context = std::make_unique<AngleContext>();

    try {
        initialize_angle_processing(*context);
        qDebug() << "Контекст нейросетевой обработки создан успешно";
    }
    catch (const std::exception& e) {
        qDebug() << "ОШИБКА: Не удалось создать контекст обработки:" << e.what();
        context->model_loaded = false;
    }

    return context;
}