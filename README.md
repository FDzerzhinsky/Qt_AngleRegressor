# Qt_AngleRegressor — Настройка и перенос проекта

Этот README описывает шаги, необходимые для переноса и сборки проекта `Qt_AngleRegressor` на другой машине под Windows. Включены версии инструментов, зависимости, команды и рекомендации по отладке.

---

## Краткое описание
Проект написан на C++ (C++17) с использованием фреймворка `Qt`. В проекте используются компоненты компьютерного зрения (OpenCV) и нейронная модель в формате ONNX (через `onnxruntime` или внешний загрузчик). Visual Studio с MSVC используется как основная среда сборки (файлы проекта `.vcxproj` и `.sln`).

---

## Требования (рекомендуемые версии)
- Операционная система: Windows10 /11 (x64)
- Visual Studio2019 (16.x) или Visual Studio2022 (17.x) с поддержкой MSVC (v142 / v143)
- Qt5.15 LTS (рекомендовано `5.15.2` или `5.15.6`) — сборка для вашей версии MSVC (например `msvc2019_64` / `msvc2022_64`)
- OpenCV4.5.x или4.6.x (рекомендовано `4.5.5`)
- ONNX Runtime1.13.x или новее (рекомендовано `1.13.1`)
- C++ стандарта: C++17 (включено в проект)
- (Опционально) vcpkg для управления пакетами и упрощения интеграции
- Git

---

## Рекомендованный способ установки зависимостей — vcpkg (быстро и удобно)
1. Установите `vcpkg`:

```powershell
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

2. Интегрируйте `vcpkg` с Visual Studio:

```powershell
.\vcpkg.exe integrate install
```

3. Установите нужные пакеты (x64):

```powershell
.\vcpkg.exe install opencv4:x64-windows onnxruntime:x64-windows
```

После интеграции `vcpkg` автоматически подставит include/lib в проекты MSBuild/Visual Studio.

---

## Альтернативная ручная установка (если не используете vcpkg)
1. Установите Qt:
 - Скачайте Qt Online Installer с https://www.qt.io/download
 - Во время установки выберите версию Qt `5.15.x` для вашей MSVC (например `msvc2019_64`)
 - Установите `Qt Visual Studio Tools` из Marketplace, чтобы интегрировать Qt в Visual Studio.

2. Установите OpenCV:
 - Скачайте предварительно собранный пакет OpenCV для Windows (например `opencv-4.5.5-vc14_vc15.exe`) или соберите из исходников.
 - Распакуйте и запомните путь (например `C:\opencv\build`).
 - В Visual Studio: Project -> Properties -> C/C++ -> Additional Include Directories -> добавьте `C:\opencv\build\include`.
 - Linker -> Additional Library Directories -> добавьте `C:\opencv\build\x64\vc15\lib`.
 - В Linker -> Input -> Additional Dependencies добавьте `opencv_world455.lib` (или соответствующую вашему пакету библиотеку).
 - Скопируйте `opencv_world455.dll` (или аналогичный) в папку с исполняемым файлом либо добавьте путь к `bin` в PATH.

3. Установите ONNX Runtime:
 - Скачайте соответствующую предварительно собранную версию ONNX Runtime (Windows x64) с https://onnxruntime.ai или установите через vcpkg.
 - Добавьте include и lib пути в Project -> Properties как для OpenCV.
 - Скопируйте `onnxruntime.dll` в папку с исполняемым файлом или добавьте путь в PATH.

---

## Настройка Visual Studio проекта
1. Откройте решение `Qt_AngleRegressor.sln` (если файл решения лежит в корне репозитория) или файл проекта `.vcxproj` через Visual Studio.
2. Убедитесь, что платформа установлена в `x64` (правка в Configuration Manager).
3. В `Project -> Properties`:
 - Configuration: `Debug` / `Release` и Platform: `x64`.
 - C/C++ -> Language -> C++ Language Standard -> `ISO C++17 Standard (/std:c++17)`.
 - Добавьте include-пути для OpenCV и ONNX (если не используете `vcpkg`).
 - Добавьте библиотеки (opencv_worldXXX.lib, onnxruntime.lib) в Linker -> Input.
 - Убедитесь, что Qt подключён: если используете Qt VS Tools — в проекте появится подключение; если нет — добавьте пути к `Qt\include` и либам вручную.

4. Запишите или проверьте рабочую директорию приложения (`Debugging -> Working Directory`) — проект в `main.cpp` делает `QDir::setCurrent(QApplication::applicationDirPath())`, поэтому обычно достаточно не менять, но при запуске из Visual Studio убедитесь, что копируемые ресурсы (`models`, `snaps`, `config.ini`) находятся рядом с exe.

---

## Файловая структура и ресурсы
- `models/` — каталог с файлами `.onnx`. Убедитесь, что в нём есть желаемые модели. Кнопка выбора модели в приложении сохранит `NeuralNetwork/model_path` в `config.ini`.
- `snaps/` — каталог для сохранения снимков (если включено `save_snapshots`). Создаётся автоматически, при необходимости создайте вручную.
- `config.ini` — создаётся автоматически при первом запуске в каталоге с exe. При переносе можно положить готовый `config.ini` рядом с exe.
- `values.txt` — файл для сопоставления снимков и значений из сокета (создаётся при включении опции).

---

## Запуск и упаковка (развёртывание)
1. Запустите сборку (Debug/Release) из Visual Studio.
2. Развёртывание Qt-библиотек: используйте `windeployqt` (входит в дистрибутив Qt) чтобы скопировать все нужные Qt DLL в папку с exe:

```powershell
"C:\Qt\<version>\msvc<ver>_64\bin\windeployqt.exe" "path\to\Qt_AngleRegressor.exe"
```

3. Скопируйте дополнительные runtime DLL для OpenCV и ONNX Runtime (`opencv_world*.dll`, `onnxruntime.dll`) в ту же папку.
4. Проверьте, что рядом с exe есть каталоги `models/` и (по необходимости) `snaps/`, а также `config.ini` или разрешена генерация файла на первом запуске.

---

## Частые проблемы и их решения
- Ошибка компоновки (unresolved external symbol): проверьте, что добавили правильные `.lib` и что архитектура (x64) совпадает.
- Отсутствие DLL при запуске: используйте `windeployqt` и/или скопируйте `opencv`/`onnxruntime` DLL в папку с исполняемым файлом.
- Несовпадение версий Qt: используйте сборку Qt, соответствующую MSVC версии (например, `msvc2019_64` для VS2019).
- Программа не находит модели или `snaps`: проверьте наличие папок `models` и `snaps` в каталоге с exe.

---

## Примеры команд (vcpkg + сборка)
```powershell
# Клонируем vcpkg и устанавливаем зависимости
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg.exe integrate install
.\vcpkg.exe install opencv4:x64-windows onnxruntime:x64-windows

# Открываем решение в Visual Studio и собираем (x64)
# После сборки запускаем windeployqt и копируем дополнительные dll при необходимости
```

---

## Контакты / дальнейшие заметки
- Конфигурации параметров нейронной сети и обработки изображений хранятся в `config.ini` в секциях `[ImageProcessing]` и `[NeuralNetwork]`.
- Если требуется GPU-ускорение для `onnxruntime`, установите и подключите соответствующие провайдеры (CUDA/DirectML) и используйте совместимые версии `onnxruntime` и драйверов.

---

Если нужна конкретная инструкция под Linux или macOS — укажите платформу, и будет подготовлен отдельный раздел.
