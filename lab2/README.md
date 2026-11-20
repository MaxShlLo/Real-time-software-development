# Лабораторна робота 2

Основа перенесена з першої лабораторної. Реалізовано пункти 1–7: перетворення
тексту, ледачу підписку, QoS, асинхронний сервіс з інтроспекцією, Action
та адаптацію PCL PointCloud до PointCloud2.
`lab2_pkg` містить вузли, `lab2_interfaces` — власні типи сервісу й Action.
Сьомий пункт виконується двома компонентами в одному процесі.

Перевірено 28 жовтня 2026 року в Docker. Образ готовий, обидва C++-пакети
зібрано без попереджень. Пройдено чотири набори перевірок: текст і QoS,
сервіс та Action, повний launch із CLI-командами, адаптер PCL/PointCloud2.

## Навіщо три термінали

Це три вкладки термінала, підключені до одного Docker-контейнера:

1. Фільтр `data_filter` працює й показує журнал.
2. `ros2 topic echo` слухає результат на `/uppercase_topic`.
3. `ros2 topic pub` надсилає текст на `/input_topic`.

Така схема потрібна вже для пунктів 1–4 і не залежить від сьомого завдання.

## Запуск

Запустіть Docker Desktop. У PowerShell відкрийте папку цієї лабораторної:

```powershell
Set-Location 'C:\Study\Розробки ПЗ СРЧ\lab2'
docker build -t ros2_lab2_image .
docker run -it --rm --name ros2_lab2 -v "${PWD}/ros2_ws:/ros2_ws" ros2_lab2_image
```

Далі всередині контейнера:

```bash
colcon build --symlink-install
source install/setup.bash
ros2 launch lab2_pkg lab2.launch.py
```

На початку вузол чекає слухачів `/uppercase_topic` і не підписується на вхід.
Одночасно запускається `array_processor` з Action `/process_array`.
Також запускається `cloud_container` із компонентами `CloudPublisher` і
`CloudSubscriber`. За замовчуванням внутрішньопроцесний обмін увімкнено.
Для окремого запуску вузлів використовуйте `ros2 run lab2_pkg data_filter`
та `ros2 run lab2_pkg array_processor`.

## Перевірка тексту та ледачої підписки

У другому вікні PowerShell відкрийте оболонку того самого контейнера:

```powershell
docker exec -it ros2_lab2 bash
```

У цій оболонці підключіть середовище і запустіть слухача:

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
source /ros2_ws/install/setup.bash
ros2 topic echo /uppercase_topic std_msgs/msg/String --qos-reliability reliable --qos-durability transient_local --qos-depth 1
```

У першому вікні з'явиться `Input subscription enabled`.
Відкрийте третю оболонку через `docker exec -it ros2_lab2 bash`, підключіть
середовище такими самими двома командами `source` та надішліть повідомлення:

```bash
ros2 topic pub --once /input_topic std_msgs/msg/String "{data: 'Hello ROS 2'}"
ros2 topic pub --once /input_topic std_msgs/msg/String '{data: "Привіт, Україно!"}'
```

Очікуваний результат: `HELLO ROS 2` та `ПРИВІТ, УКРАЇНО!`.
Зупиніть усіх слухачів вихідної теми через Ctrl+C.
Вузол має вивести `Input subscription disabled`.

## Перевірка збереження останнього повідомлення

Після попередньої перевірки повторно запустіть `ros2 topic echo` з тими самими
QoS-параметрами, нічого не публікуючи на вході. Новий слухач має отримати
останній результат `ПРИВІТ, УКРАЇНО!`. Сам вузол у цей час має залишатися
запущеним: Transient Local зберігає історію видавця, а не дані на диску.

## Перевизначення QoS

Зупиніть вузол та слухачів через Ctrl+C. Замість звичайного запуску виконайте:

```bash
ros2 run lab2_pkg data_filter --ros-args --params-file /ros2_ws/install/lab2_pkg/share/lab2_pkg/config/qos_overrides.yaml
```

Файл задає depth=3, reliability=best_effort, durability=volatile. Перевірка
з іншої оболонки контейнера:

```bash
ros2 topic info /uppercase_topic --verbose
```

Для цього профілю слухача запускайте із сумісними параметрами:

```bash
ros2 topic echo /uppercase_topic std_msgs/msg/String --qos-reliability best_effort --qos-durability volatile
```

Перевизначення застосовуються під час створення видавця при запуску вузла.
Стандартний запуск без YAML повертає Reliable, Transient Local, depth=1.

## Файли

- `Dockerfile`, `entrypoint.sh`: середовище на основі першої лабораторної.
- `ros2_ws/src/lab2_pkg/src/data_filter.cpp`: логіка вузла-компонента.
- `ros2_ws/src/lab2_pkg/CMakeLists.txt`: бібліотека та виконувана програма `data_filter`.
- `ros2_ws/src/lab2_pkg/package.xml`: назва та залежності пакета.
- `ros2_ws/src/lab2_pkg/config/qos_overrides.yaml`: приклад перевизначення QoS.
- `ros2_ws/src/lab2_pkg/include/lab2_pkg/point_cloud_adapter.hpp`: адаптер типів.
- `ros2_ws/src/lab2_pkg/src/point_cloud_components.cpp`: видавець і підписник PCL.
- `ros2_ws/src/lab2_pkg/launch/point_cloud.launch.py`: два режими сьомого пункту.

Вхідну та вихідну теми можна задати параметрами `input_topic` і `output_topic`.
Для стандартної вихідної теми в YAML використаний повний шлях `/uppercase_topic`.
Якщо перейменувати тему або додати namespace, відповідно змініть і цей шлях.

## Автоматична перевірка пунктів 1–4

Зупиніть фільтр та ручних слухачів, щоб вони не впливали на перевірку.
У контейнері після збирання й підключення `install/setup.bash` виконайте:

```bash
ROS_DOMAIN_ID=72 python3 /ros2_ws/src/lab2_pkg/test/check_filter.py
```

Скрипт сам запускає та зупиняє фільтр. Перевіряє латинський, український і
порожній текст, створення/видалення вхідної підписки, доставку лише останнього
повідомлення пізньому слухачеві, перевизначення трьох політик QoS та відсутність
активації від слухача з несумісним QoS. Наприкінці має бути `ALL CHECKS PASSED`.

## Пункт 5 Сервіс ResetFilter

Сервіс `/reset_filter` скидає лічильник успішно оброблених рядків. Запит порожній,
відповідь містить `success`, `previous_count`, `message`. Це не видалення кешу
Transient Local: останній результат видавця зберігається до його знищення.
Кількість рядків і скидання обробляються в одній взаємовиключній callback-групі.

У додатковій оболонці того самого контейнера після двох команд `source`:

```bash
ros2 run lab2_pkg reset_filter_client
ros2 service call /reset_filter lab2_interfaces/srv/ResetFilter '{}'
```

Власний C++-клієнт викликає `async_send_request` та очікує через `future.wait_for`.
Окремий потік обслуговує Executor, тому очікування не блокує отримання відповіді.
При таймауті видаляється pending request. Код завершення: 0 — успіх, 1 — помилка,
2 — сервіс не знайдено вчасно, 3 — відповідь не отримано вчасно.

```bash
ros2 run lab2_pkg reset_filter_client --ros-args -p timeout_ms:=1000 -p service_wait_ms:=2000
```

Для відтворення таймауту відповіді зупиніть звичайний фільтр, запустіть його з
навмисною демонстраційною затримкою (у першому терміналі), а клієнта — у другому:

```bash
ros2 run lab2_pkg data_filter --ros-args -p reset_delay_ms:=350
# В іншому терміналі:
ros2 run lab2_pkg reset_filter_client --ros-args -p timeout_ms:=50
```

За замовчуванням затримка дорівнює нулю; значення 0–5000 мс потрібне лише для
демонстрації. Під час неї callback фільтра зайнятий. Таймаут клієнта не скасовує
операцію на сервері: вона може завершитися пізніше.

### Інтроспекція

Сервер і C++-клієнт вмикають `RCL_SERVICE_INTROSPECTION_CONTENTS`.
В окремому терміналі перед викликом сервісу:

```bash
ros2 topic list --include-hidden-topics
ros2 topic echo /reset_filter/_service_event
```

Прихована тема містить метадані та вміст запиту/відповіді. Події: 0 — запит
надіслано, 1 — запит отримано, 2 — відповідь надіслано, 3 — відповідь отримано.
Сервер створює події 1 і 2, інструментований клієнт — 0 і 3.
У методичці опція записана неточно; правильна форма для списку тем:
`--include-hidden-topics`.

## Пункт 6 Action ProcessArray

Action `/process_array` працює у вузлі `array_processor`.
Ціль: `float64[] data`, `uint32 delay_ms`; результат: `processed_data`,
`processed_count`; feedback: `processed_count`, `total_count`, `progress` у %.
Кожне число підноситься до квадрата в окремому робочому потоці.

```bash
ros2 action send_goal /process_array lab2_interfaces/action/ProcessArray \
  "{data: [1.0, -2.0, 3.5], delay_ms: 300}" --feedback
```

Очікується прогрес приблизно 33.3%, 66.7%, 100%, результат `[1.0, 4.0, 12.25]`
і фінальний стан `SUCCEEDED`. `delay_ms` задає затримку перед кожним елементом
для наочної демонстрації. Допускається 1–10000 чисел та затримка 0–1000 мс.
Відхиляються NaN, нескінченність і числа, квадрат яких переповнює float64.

Одночасно виконується одна ціль; інша отримує відмову, доки попередня зайнята.
При скасуванні повертається частково оброблений масив зі станом `CANCELED`.
Після цього можна надіслати нову ціль. Робочий потік зберігається у вузлі та
приєднується при завершенні, а не залишається відокремленим.

Приклад скасування довгої цілі з іншого термінала:

```bash
ros2 service call /process_array/_action/cancel_goal action_msgs/srv/CancelGoal \
  "{goal_info: {goal_id: {uuid: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]}, stamp: {sec: 0, nanosec: 0}}}"
```

Нульовий UUID і нульовий час означають запит скасування всіх поточних цілей
цього Action-сервера; тут одночасно активна лише одна.

## Автоматична перевірка пунктів 5–6

```bash
ROS_DOMAIN_ID=72 python3 /ros2_ws/src/lab2_pkg/test/check_services_actions.py
```

Тест сам запускає сервери, перевіряє C++-клієнта, реальний таймаут відповіді,
відсутній сервіс, скидання лічильника, чотири типи подій CONTENTS і CLI.
Для Action перевіряються результат, feedback, відхилення некоректних/зайвих
цілей, скасування з частковим результатом, нова ціль після скасування та
завершення вузла під час активної роботи.

Перевірка спільного запуску та CLI-команд:

```bash
ROS_DOMAIN_ID=72 python3 /ros2_ws/src/lab2_pkg/test/check_launch.py
```

Фінальні повідомлення: `ALL SERVICE AND ACTION CHECKS PASSED` та
`ALL LAUNCH AND CLI CHECKS PASSED`. Логи поточної перевірки збережені в
`ros2_ws/log/verification/`.

## Пункт 7 Type Adapter для PCL та PointCloud2

Тип у C++: справжній `pcl::PointCloud<pcl::PointXYZ>` із бібліотеки PCL.
Тип ROS: `sensor_msgs::msg::PointCloud2`. Спеціалізація `rclcpp::TypeAdapter`
реалізована в `include/lab2_pkg/point_cloud_adapter.hpp`.
Підтримуються поля x, y, z типу FLOAT32, розміри та заголовок хмари;
під час читання враховуються порядок байтів і відступи між рядками.

Dockerfile використовує офіційні заголовки пакета `libpcl-dev`, Eigen та Boost.
Для цієї демонстрації встановлюються лише заголовки PCL, потрібні для типів
даних; графічні модулі, VTK та алгоритмічні бібліотеки PCL не використовуються.

`CloudPublisher` генерує 1000 точок кожні 500 мс. Для хмари з номером N точки
мають x=N, y=i, z=N+i/2. `CloudSubscriber` перевіряє всі координати й показує
`valid=true`. Видавець передає `unique_ptr` через `std::move`, підписник
приймає `unique_ptr` того самого типу.

### Чистий внутрішньопроцесний обмін

У повному `lab2.launch.py` ці повідомлення вже видно в терміналі запуску.
Для окремої демонстрації зупиніть повний запуск через Ctrl+C і виконайте:

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
source /ros2_ws/install/setup.bash
ros2 launch lab2_pkg point_cloud.launch.py intra_process:=true
```

Порівнюйте `CLOUD_PUB` і `CLOUD_SUB` з однаковим `seq`:

- `object` — адреса об’єкта хмари, має збігатися;
- `data` — адреса буфера точок, має збігатися;
- `points=1000`, `valid=true`;
- `to_ros=0 from_ros=0` — перетворення між PCL і ROS не викликалися.

Конкретні адреси залежать від запуску. Повторне використання адреси для іншого
seq допустиме; порівнюйте саме пару PUB/SUB одного повідомлення.

Під час цього вимірювання не запускайте `ros2 topic echo /point_cloud` і не
записуйте тему через rosbag. Зовнішній слухач потребує ROS-повідомлення, тому
спричиняє перетворення. Нульові лічильники перевіряються для чистої пари
компонентів з однаковим адаптованим типом в одному процесі.

### Контрольний запуск без intra-process

Зупиніть попередній запуск через Ctrl+C. У тому самому терміналі:

```bash
ros2 launch lab2_pkg point_cloud.launch.py intra_process:=false
```

Компоненти залишаються в одному процесі, але обмін іде через ROS/DDS.
З’являються рядки `ADAPTER`; лічильники `to_ros` і `from_ros` зростають,
координати залишаються правильними (`valid=true`). Адреси між різними режимами
не є доказом копіювання: контрольний показник тут — реальні виклики адаптера.

### Перевірка звичайним слухачем PointCloud2

Залиште контрольний режим `intra_process:=false` запущеним. У другій оболонці
того самого контейнера після двох команд `source` виконайте:

```bash
ros2 topic echo /point_cloud sensor_msgs/msg/PointCloud2 --field width --once
```

Має надійти `1000`. Це перевіряє сумісність адаптера зі звичайними інструментами
ROS 2. Якщо підключати зовнішнього слухача в режимі intra-process, видавець
також мусить створювати ROS-повідомлення для нього; для чистого вимірювання
без перетворень зовнішніх слухачів бути не повинно.

### Автоматична перевірка сьомого пункту

Зупиніть ручні демонстрації. Скрипт сам виконає три незалежні запуски:

```bash
ROS_DOMAIN_ID=72 python3 /ros2_ws/src/lab2_pkg/test/check_type_adapter.py
```

Перевіряються спільні адреси та нульові перетворення, обидва перетворення при
вимкненому intra-process, а також зовнішній слухач PointCloud2 з перевіркою
координат. Фінальне повідомлення: `ALL TYPE ADAPTER CHECKS PASSED`.
Журнали: `ros2_ws/log/verification/cloud_intra.log`, `cloud_dds.log`,
`cloud_external.log`; загальний результат — `type_adapter.log`.
