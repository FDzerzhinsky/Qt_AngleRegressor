// =============================================================================
// ВКЛЮЧЕНИЕ БИБЛИОТЕК
// =============================================================================
#include "angle.h"
#include <QDebug>  // ДОБАВЛЕНО ДЛЯ ДИАГНОСТИЧЕСКИХ СООБЩЕНИЙ

// Используем пространства имен для удобства
using namespace cv;
using namespace std;
using namespace chrono;


// =============================================================================
// ЧТЕНИЕ КОНФИГУРАЦИИ ОБРАБОТКИ ИЗОБРАЖЕНИЙ ИЗ ФАЙЛА
// =============================================================================
// Эта функция читает параметры обработки изображений из конфигурационного файла
ImageProcessingConfig read_image_processing_config(const std::string& filename) {
    ImageProcessingConfig config;

    // Открываем файл конфигурации для чтения
    ifstream file(filename);
    if (file.is_open()) {
        string line;
        string current_section = "";
        // Читаем файл построчно
        while (getline(file, line)) {
            // Пропускаем пустые строки и комментарии
            if (line.empty() || line[0] == '#') continue;

            // Проверяем, является ли строка секцией
            if (line[0] == '[' && line[line.length() - 1] == ']') {
                current_section = line.substr(1, line.length() - 2);
                continue;
            }

            // Обрабатываем только секцию ImageProcessing
            if (current_section != "ImageProcessing") continue;

            // Используем stringstream для разбора строки
            istringstream iss(line);
            string key, value;
            // Разбираем строку на ключ и значение по знаку '='
            if (getline(iss, key, '=') && getline(iss, value)) {
                // Удаляем пробелы в начале и конце ключа и значения
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);

                // Записываем значения в конфигурацию в зависимости от ключа
                try {
                    if (key == "ref_image_path") config.ref_image_path = value;
                    else if (key == "ref_image_scale") config.ref_image_scale = stod(value);
                    else if (key == "orb_max_features") config.orb_max_features = stoi(value);
                    else if (key == "orb_scale_factor") config.orb_scale_factor = stod(value);
                    else if (key == "orb_n_levels") config.orb_n_levels = stoi(value);
                    else if (key == "orb_edge_threshold") config.orb_edge_threshold = stoi(value);
                    else if (key == "orb_first_level") config.orb_first_level = stoi(value);
                    else if (key == "orb_wta_k") config.orb_wta_k = stoi(value);
                    else if (key == "orb_score_type") config.orb_score_type = stoi(value);
                    else if (key == "orb_patch_size") config.orb_patch_size = stoi(value);
                    else if (key == "orb_fast_threshold") config.orb_fast_threshold = stoi(value);
                    else if (key == "flann_search_params") config.flann_search_params = stoi(value);
                    else if (key == "good_match_ratio") config.good_match_ratio = stod(value);
                    else if (key == "min_good_matches") config.min_good_matches = stoi(value);
                    else if (key == "ransac_threshold") config.ransac_threshold = stod(value);
                    else if (key == "save_snapshots") config.save_snapshots = (value == "true" || value == "1");
                }
                catch (const exception& e) {
                    // Выводим ошибку, но продолжаем работу с остальными параметрами
                    cerr << "Error parsing config value: " << key << " = " << value << endl;
                    cerr << "Error message: " << e.what() << endl;
                }
            }
        }
        file.close();
    }
    else {
        // Если файл не найден, используем значения по умолчанию
        cout << "Config file not found, using default values for image processing" << endl;
    }

    return config;
}

// =============================================================================
// ИНИЦИАЛИЗАЦИЯ ОБРАБОТКИ ИЗОБРАЖЕНИЙ
// =============================================================================
// Эта функция инициализирует все компоненты системы обработки изображений:
// - Загружает референсное изображение
// - Настраивает детектор и дескриптор ORB
// - Настраивает FLANN матчер для сравнения дескрипторов
void initialize_angle_processing(AngleContext& context) {
    // Загрузка конфигурации обработки изображений из файла
    context.config = read_image_processing_config();

    // Вывод загруженных параметров для отладки
    cout << "Image processing configuration loaded:" << endl;
    cout << "  ref_image_path: " << context.config.ref_image_path << endl;
    cout << "  ref_image_scale: " << context.config.ref_image_scale << endl;
    cout << "  orb_max_features: " << context.config.orb_max_features << endl;
    cout << "  orb_scale_factor: " << context.config.orb_scale_factor << endl;
    cout << "  orb_n_levels: " << context.config.orb_n_levels << endl;
    cout << "  orb_edge_threshold: " << context.config.orb_edge_threshold << endl;
    cout << "  orb_patch_size: " << context.config.orb_patch_size << endl;
    cout << "  orb_fast_threshold: " << context.config.orb_fast_threshold << endl;
    cout << "  good_match_ratio: " << context.config.good_match_ratio << endl;
    cout << "  min_good_matches: " << context.config.min_good_matches << endl;
    cout << "  ransac_threshold: " << context.config.ransac_threshold << endl;
    cout << "  save_snapshots: " << context.config.save_snapshots << endl;

    // Загрузка референсного изображения из указанного пути
    context.ref_image = imread(context.config.ref_image_path, IMREAD_COLOR);
    if (context.ref_image.empty()) {
        throw runtime_error("Error: Reference image not found at path: " + context.config.ref_image_path);
    }

    // Уменьшение размера референсного изображения для ускорения обработки
    double scale = context.config.ref_image_scale;
    if (scale > 0 && scale != 1.0) {
        // Вычисляем новые размеры с учетом масштаба
        int new_width = static_cast<int>(context.ref_image.cols * scale);
        int new_height = static_cast<int>(context.ref_image.rows * scale);
        resize(context.ref_image, context.ref_image, Size(new_width, new_height));
    }

    // Преобразование в оттенки серого (большинство алгоритмов CV работают с灰度图像)
    cvtColor(context.ref_image, context.ref_gray, COLOR_BGR2GRAY);

    // Инициализация ORB детектора с параметрами из конфигурации
    // Преобразуем числовое значение типа оценки в соответствующий enum
    cv::ORB::ScoreType score_type = context.config.orb_score_type == 0 ?
        cv::ORB::FAST_SCORE : cv::ORB::HARRIS_SCORE;

    context.orb = ORB::create(
        context.config.orb_max_features,        // Максимальное количество фич
        context.config.orb_scale_factor,        // Масштабный фактор пирамиды
        context.config.orb_n_levels,            // Количество уровней пирамиды
        context.config.orb_edge_threshold,      // Размер бордюра
        context.config.orb_first_level,         // Первый уровень пирамиды
        context.config.orb_wta_k,               // Параметр WTA_K
        score_type,                             // Тип оценки (HARRIS/FAST)
        context.config.orb_patch_size,          // Размер патча
        context.config.orb_fast_threshold       // Порог для детектора
    );

    // Поиск ключевых точек и дескрипторов на референсном изображении
    context.orb->detectAndCompute(context.ref_gray, noArray(), context.kp_ref, context.des_ref);

    // Настройка FLANN матчера в зависимости от типа дескрипторов
    if (context.des_ref.type() == CV_8U) {
        // Для бинарных дескрипторов (ORB) используем LSH индекс
        cv::Ptr<cv::flann::IndexParams> indexParams = cv::makePtr<cv::flann::LshIndexParams>(5, 10, 1);
        cv::Ptr<cv::flann::SearchParams> searchParams = cv::makePtr<cv::flann::SearchParams>(context.config.flann_search_params);
        context.flann = cv::makePtr<cv::FlannBasedMatcher>(indexParams, searchParams);
    }
    else {
        // Для небинарных дескрипторов используем стандартный матчер
        context.flann = cv::FlannBasedMatcher::create();
    }

    // Установка флага сохранения снэпшотов из конфигурации
    context.save_snapshots = context.config.save_snapshots;
}

// =============================================================================
// ОБРАБОТКА ОДНОГО КАДРА
// =============================================================================
// Эта функция обрабатывает один кадр изображения, вычисляет позицию объекта
// и возвращает true при успешной обработке
bool process_frame(AngleContext& context, const cv::Mat& query_image) {
    // Записываем время начала обработки
    context.processing_start_time = high_resolution_clock::now();

    // =========================================================================
    // ПРОВЕРКА ВХОДНЫХ ДАННЫХ
    // =========================================================================
    // Убеждаемся, что полученное изображение не пустое
    if (query_image.empty()) {
        qDebug() << "ERROR: Empty query image received!";
        return false;
    }

    // =========================================================================
    // ПОИСК КЛЮЧЕВЫХ ТОЧЕК И ДЕСКРИПТОРОВ НА ТЕКУЩЕМ КАДРЕ
    // =========================================================================
    vector<KeyPoint> kp_query;    // Здесь будут храниться найденные ключевые точки
    Mat des_query;                // Здесь будут дескрипторы этих точек

    // Детектируем ключевые точки и вычисляем их дескрипторы с помощью ORB
    context.orb->detectAndCompute(query_image, noArray(), kp_query, des_query);

    // Проверяем, найдены ли ключевые точки на изображении
    if (kp_query.empty()) {
        qDebug() << "ERROR: No keypoints found in query image!";
        return false;
    }

    // Проверяем, что дескрипторы были успешно вычислены
    if (des_query.empty()) {
        qDebug() << "ERROR: No descriptors generated for query image!";
        return false;
    }

    // =========================================================================
    // ПОИСК СОВПАДЕНИЙ МЕЖДУ ДЕСКРИПТОРАМИ
    // =========================================================================
    vector<vector<DMatch>> matches; // Вектор пар совпадений (k=2 ближайших соседа)

    // Ищем два ближайших соседа для каждого дескриптора
    context.flann->knnMatch(des_query, context.des_ref, matches, 2);

    // =========================================================================
    // ФИЛЬТРАЦИЯ СОВПАДЕНИЙ ПО МЕТОДу LOWE'S RATIO TEST
    // =========================================================================
    // Этот метод отфильтровывает ложные совпадения, оставляя только качественные
    vector<DMatch> good_matches; // Вектор для хранения хороших совпадений

    for (size_t i = 0; i < matches.size(); ++i) {
        // Пропускаем совпадения, у которых нет двух ближайших соседей
        if (matches[i].size() < 2) {
            continue;
        }

        // Lowe's ratio test: хорошим считается совпадение, где расстояние до
        // первого соседа значительно меньше расстояния до второго соседа
        if (matches[i][0].distance < context.config.good_match_ratio * matches[i][1].distance) {
            good_matches.push_back(matches[i][0]);
        }
    }

    // =========================================================================
    // ПРОВЕРКА ДОСТАТОЧНОСТИ СОВПАДЕНИЙ ДЛЯ ВЫЧИСЛЕНИЯ ГОМОГРАФИИ
    // =========================================================================
    // Для надежного вычисления гомографии нужно достаточно хороших совпадений
    if (good_matches.size() > context.config.min_good_matches) {
        // Подготавливаем точки для вычисления гомографии
        vector<Point2f> src_pts, dst_pts;

        // Заполняем векторы точками из совпадений
        for (const auto& m : good_matches) {
            // Точки из текущего кадра (source)
            src_pts.push_back(kp_query[m.queryIdx].pt);
            // Соответствующие точки из референсного изображения (destination)
            dst_pts.push_back(context.kp_ref[m.trainIdx].pt);
        }

        // =====================================================================
        // ВЫЧИСЛЕНИЕ МАТРИЦЫ ГОМОГРАФИИ С ИСПОЛЬЗОВАНИЕМ RANSAC
        // =====================================================================
        // Гомография - это преобразование, которое описывает, как точки одного
        // изображения проецируются на точки другого изображения
        // RANSAC - алгоритм, устойчивый к выбросам (ложным совпадениям)
        // Используем параметр из конфигурации вместо жестко заданного значения
        Mat H = findHomography(src_pts, dst_pts, RANSAC, context.config.ransac_threshold);

        // Проверяем, что матрица гомографии успешно вычислена
        if (!H.empty()) {
            // =================================================================
            // ПРЕОБРАЗОВАНИЕ ЦЕНТРА КАДРА С ПОМОЩЬЮ МАТРИЦЫ ГОМОГРАФИИ
            // =================================================================
            // Определяем центр текущего кадра
            vector<Point2f> x_points(1);
            x_points[0] = Point2f(static_cast<float>(query_image.cols / 2),
                static_cast<float>(query_image.rows / 2));

            // Применяем матрицу гомографии к центру кадра
            vector<Point2f> transformed_x_points(1);
            perspectiveTransform(x_points, transformed_x_points, H);

            // Извлекаем X-координату преобразованной точки
            float center_x = transformed_x_points[0].x;

            // =================================================================
            // РАСЧЕТ ВРЕМЕНИ ВЫПОЛНЕНИЯ ОБРАБОТКИ
            // =================================================================
            // Записываем время окончания обработки
            context.processing_end_time = high_resolution_clock::now();

            // Вычисляем все временные характеристики
            auto processing_duration = duration_cast<microseconds>(
                context.processing_end_time - context.processing_start_time
            );
            auto total_duration = duration_cast<microseconds>(
                context.processing_end_time - context.capture_start_time
            );

            // Сохраняем результаты
            std::lock_guard<std::mutex> lock(context.data_mutex);
            context.execution_time = processing_duration.count() / 1000.0;
            context.total_time = total_duration.count() / 1000.0;
            context.x_position = static_cast<int>(center_x);
            context.processing_complete = true;

            qDebug() << "Frame processed successfully: X =" << center_x << "Time =" << total_duration.count() / 1000.0 << "ms";
            return true; // Успешное завершение обработки
        }
        else {
            qDebug() << "ERROR: Homography matrix computation failed!";
        }
    }
    else {
        // Выводим подробную информацию о неудачном сопоставлении
        qDebug() << "ERROR: Not enough good matches found (" << good_matches.size()
            << "<" << context.config.min_good_matches << ")";

        // Дополнительная диагностика: проверяем, есть ли вообще какие-либо совпадения
        if (matches.empty()) {
            qDebug() << "ERROR: No matches found at all!";
        }
        else {
            qDebug() << "Total matches found:" << matches.size() << "Good matches:" << good_matches.size();
        }
    }

    return false; // Обработка завершилась неудачно
}

// =============================================================================
// СОЗДАНИЕ И ИНИЦИАЛИЗАЦИЯ КОНТЕКСТА ОБРАБОТКИ ИЗОБРАЖЕНИЙ
// =============================================================================
// Эта функция создает и возвращает умный указатель на инициализированный контекст

std::unique_ptr<AngleContext> create_angle_context() {
    // Создаем умный указатель на новый объект AngleContext
    // make_unique - современный и безопасный способ создания unique_ptr
    auto context = std::make_unique<AngleContext>();

    // Инициализируем контекст обработки изображений
    // Эта функции загружает референсное изображение и настраивает алгоритмы
    initialize_angle_processing(*context);

    // Возвращаем умный указатель на инициализированный контекст
    return context;
}