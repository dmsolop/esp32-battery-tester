# ADC Driver Component (`components/adc_driver/`)

## Огляд
Драйвер низькорівневої аналого-цифрової телеметрії на базі 16-бітного АЦП **ADS1115** (шина I2C, адреси `0x48..0x4B` для 4 каналів) з підтримкою апаратного переривання аварійних меж `ADC_ALERT` на піні `IO47`.

## Архітектура та канали
Підлегла плата (`slave_board`) підключає ADS1115 наступним чином:
- **AIN0 (`0xCB83`):** Вимір струму плеча MOSFET Q1 (`I_SENSE_Q1`) відносно GND. Діапазон PGA: $\pm 0.256\,\text{V}$.
- **AIN1 (`0xDB83`):** Вимір струму плеча MOSFET Q2 (`I_SENSE_Q2`) відносно GND. Діапазон PGA: $\pm 0.256\,\text{V}$.
- **AIN2 / AIN3 (`0xB383`):** Диференційний Кельвінівський замір напруги на клемах акумулятора (`LOAD_SENSE_+` / `LOAD_SENSE_-`) через резистивний подільник $21.5:1$ (205 кОм / 10 кОм). Діапазон PGA: $\pm 4.096\,\text{V}$.

## Математичні моделі та точність

### 1. Прецизійний цілочисельний розрахунок струму (без втрати мікровольт)
При коефіцієнті підсилення PGA $\pm 0.256\,\text{V}$ вага молодшого розряду (LSB) становить:
$$V_{LSB} = \frac{256000\,\mu\text{V}}{32768} = 7.8125\,\mu\text{V} = \frac{125}{16}\,\mu\text{V}$$

За законом Ома струм через вимірювальний шунт $R_{shunt}$ (номінал у міліомах `CONFIG_SHUNT_RESISTOR_MOHM`):
$$I = \frac{V_{shunt}}{R_{shunt}} = \frac{raw \cdot \frac{125}{16}\,\mu\text{V} \cdot 1000}{R_{shunt\_mohm} \cdot 1000\,\mu\Omega / \text{m}\Omega}$$
$$I_{\mu\text{A}} = \frac{raw \cdot 125 \cdot 1000}{16 \cdot R_{shunt\_mohm}} = \frac{raw \cdot 125000}{16 \cdot R_{shunt\_mohm}} = \frac{raw \cdot 15625}{2 \cdot R_{shunt\_mohm}}$$

Ця формула виконується в 64-бітному цілочисельному просторі (`uint64_t` / `int64_t`), що повністю виключає проміжне цілочисельне округлення в мікровольтах та запобігає переповненню.

### 2. Режими роботи: Одноплечовий vs Двоплечовий
- **Одноплечовий режим (`dual_branch_enabled = false`, за замовчуванням):**
  Сумісність із поточним лабораторним макетом на 1 транзисторі з шунтом 0.27 Ом (270 мОм). Опитується канал AIN0.
- **Двоплечовий режим (`dual_branch_enabled = true`):**
  Для фінальної плати `slave_board` із шунтами 5 мОм на обох транзисторах Q1 та Q2. Опитуються канали AIN0 та AIN1, сумарний струм та струми окремих плечей доступні як у загальній функції `adc_driver_read_current()`, так і через `adc_driver_read_current_dual()`.

## API драйвера
- `adc_driver_init()`: Ініціалізація I2C-шини та реєстрація обробника ALERT ISR.
- `adc_driver_read_voltage(channel, voltage_uv)`: Диференційне зчитування напруги Кельвіна (AIN2-AIN3).
- `adc_driver_read_current(channel, current_ua, pid_current_ua)`: Зчитування струму (підтримує як 1 плече, так і 2 плеча згідно з налаштуванням).
- `adc_driver_read_current_branch(channel, branch, current_ua, pid_current_ua)`: Пряме зчитування конкретного плеча (`ADC_CURRENT_SENSE_Q1` або `ADC_CURRENT_SENSE_Q2`).
- `adc_driver_read_current_dual(channel, i_q1_ua, i_q2_ua, total_ua, pid_total_ua)`: Одночасне зчитування обох плечей і їх алгебраїчної суми.
- `adc_driver_set_dual_branch_mode(channel, enable_dual)`: Перемикання між макетом (1 плече) та фінальною платою (2 плеча).
- `adc_driver_set_zero_offset(channel, v_off_uv, i_off_ua)` / `adc_driver_set_zero_offset_branch(channel, branch, i_off_ua)`: Калібрування нульового зміщення.
- `adc_driver_register_safety_task(task)`: Реєстрація задачі моніторингу безпеки для сповіщень про апаратний ALERT.
