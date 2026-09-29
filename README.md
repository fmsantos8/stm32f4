# Port STM32F4

Implementa `hal/interface` usando os drivers oficiais
[STM32CubeF4 HAL](https://github.com/STMicroelectronics/stm32f4xx-hal-driver).
Os componentes usam os headers públicos da interface; somente BSP e integração
incluem os tipos concretos deste port.

| Módulo | Header concreto | Comportamento |
| --- | --- | --- |
| GPIO | `gpio/hal_gpio_stm32f4.h` | Entrada ou saída push-pull, pull-up/down; configuração de saída sempre começa em nível baixo. |
| I2C | `i2c/hal_i2c_stm32f4.h` | Controlador de 7 bits, até 400 kHz, leitura/escrita bloqueante. |
| UART | `uart/hal_uart_stm32f4.h` | Oito bits de dados, paridade nenhuma/par/ímpar, um ou dois stop bits; transmissão bloqueante. |
| Timer | `timer/hal_timer_stm32f4.h` | TIM2/TIM5 dedicado a 1 MHz, convertido para milissegundos, com extensão do contador por IRQ. |

## Inicialização e responsabilidades

1. A integração chama `HAL_Init()` e configura os clocks antes de usar o port.
   O tick da HAL precisa avançar durante as chamadas bloqueantes de I2C/UART.
   Em bare metal, `SysTick_Handler()` deve chamar `HAL_IncTick()`.
2. O BSP cria handles com duração de vida suficiente para todo o uso do recurso.
   Inicialize apenas os campos físicos indicados abaixo; os demais começam em
   zero e pertencem ao port. Use um único handle por periférico/pino.
3. Para GPIO, o BSP habilita o clock de GPIOx antes de configurar o pino.
   `pin` é uma máscara `GPIO_PIN_x`, com um único bit, e não um número de pino.
4. Para I2C/UART, o BSP implementa `HAL_I2C_MspInit()` e `HAL_UART_MspInit()`:
   habilitação dos clocks, seleção dos pinos e das funções alternativas corretas
   para o encapsulamento. I2C requer saída open-drain e pull-ups adequados na placa.
5. Para TIM, o BSP implementa `HAL_TIM_Base_MspInit()` e `HAL_TIM_Base_MspDeInit()`:
   clock do timer e habilitação/desabilitação da sua IRQ no NVIC. O handler
   chama `hal_timer_stm32_irq()` diretamente, conforme o exemplo abaixo.

Mantenha os clocks estáveis após a configuração. Os handles não devem ser
copiados ou alterados depois de inicializados. Chamadas são serializadas pela
integração; as operações bloqueantes não devem ser feitas em ISR. `HAL_BUSY`
vira `HAL_STATUS_ERROR`, pois a interface comum não tem um estado BUSY.

Exemplo de declaração no BSP (os recursos são ilustrativos; selecione os da placa):

```c
#include "gpio/hal_gpio_stm32f4.h"
#include "i2c/hal_i2c_stm32f4.h"
#include "uart/hal_uart_stm32f4.h"
#include "timer/hal_timer_stm32f4.h"

static hal_gpio_t led = {.port = GPIOC, .pin = GPIO_PIN_13};
static hal_i2c_t bus = {.instance = I2C1};
static hal_uart_t serial = {.instance = USART2};
static hal_timer_t timer = {.instance = TIM2, .clock_hz = 16000000U};
```

Depois de preparar os clocks e hooks MSP, a integração usa os contratos comuns:

```c
hal_gpio_config_t gpio_config = {HAL_GPIO_DIRECTION_OUTPUT, HAL_GPIO_PULL_NONE};
hal_i2c_config_t i2c_config = {.clock_speed = 100000U};
hal_uart_config_t uart_config = {115200U, HAL_UART_PARITY_NONE, HAL_UART_STOP_BITS_1};

/* Verifique cada retorno antes de prosseguir ou usar o recurso. */
hal_status_t status = hal_gpio_config(&led, &gpio_config);
/* ... hal_i2c_config(&bus, &i2c_config), hal_uart_config(&serial, &uart_config),
 *     hal_timer_start(&timer) ... */
```

## Timer e overflow

`clock_hz` é a frequência real de entrada do timer, incluindo multiplicadores
de APB/TIMPRE, e não necessariamente PCLK1. O port aceita clocks múltiplos
inteiros de 1 MHz e TIM2/TIM5 de 32 bits. Recursos incompatíveis retornam
`HAL_STATUS_NOT_SUPPORTED`. O prescaler produz ticks de 1 us; a leitura pública
retorna milissegundos truncados.

Exemplo de ligação da IRQ para o handle `timer` acima, usando HSI de 16 MHz e
APB1 sem divisão:

```c
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *driver) {
    if (driver->Instance == TIM2) {
        __HAL_RCC_TIM2_CLK_ENABLE();
        HAL_NVIC_SetPriority(TIM2_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(TIM2_IRQn);
    }
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *driver) {
    if (driver->Instance == TIM2) {
        HAL_NVIC_DisableIRQ(TIM2_IRQn);
        __HAL_RCC_TIM2_CLK_DISABLE();
    }
}

void TIM2_IRQHandler(void) {
    hal_timer_stm32_irq(&timer);
}
```

Não chame também `HAL_TIM_IRQHandler()` para esse timer: ele consumiria a mesma
flag de overflow. O timer é exclusivo deste handle, sem PWM, captura ou uso como
tick da HAL. A IRQ precisa ser atendida antes de ocorrer outro overflow físico
(aproximadamente 71,6 minutos). Não há requisito de frequência mínima de leitura.

`hal_timer_get_millis()` faz uma leitura atômica, inclui um overflow pendente e
preserva o estado anterior de interrupções. O resultado só dá a volta após
2^32 milissegundos (aproximadamente 49,7 dias). Calcule intervalos por subtração
sem sinal. Repetir `hal_timer_start()` não reinicia a contagem.

## Limites e erros

- I2C passa o endereço de 7 bits deslocado ao SDK. Cada chamada representa uma
  transação e aceita de 1 a 65535 bytes; tamanhos maiores retornam
  `HAL_STATUS_NOT_SUPPORTED` sem iniciar a transferência. O timeout passado
  ao driver é de 1000 ms; a HAL também tem esperas internas mais curtas para
  condições como barramento ocupado. Timeout sinalizado no `ErrorCode` é
  traduzido mesmo quando o driver retorna `HAL_ERROR` ou `HAL_BUSY`.
- Frequências I2C que excedem os limites dos divisores/clock periférico são
  rejeitadas. Um erro de leitura pode deixar dados parciais no buffer.
- UART preserva oito bits de dados usando word length de nove bits quando há
  paridade. A taxa precisa caber no divisor do periférico, com oversampling
  de 16 ou 8; a taxa efetiva está sujeita ao arredondamento do SDK.
- UART divide buffers grandes em blocos de até 65535 bytes. Cada bloco espera
  a conclusão física do envio, com timeout calculado pelo tamanho e baud rate,
  acrescido de 1000 ms. Pode haver pausas entre blocos. Após erro, um prefixo
  do buffer já pode ter sido transmitido.
- Uma falha durante reconfiguração do driver invalida o handle de I2C/UART até
  uma configuração bem-sucedida. Parâmetros rejeitados antes de acessar o SDK
  preservam a configuração anterior.

## CMake

O `CMakeLists.txt` exporta a biblioteca `stm32f4_port`, que inclui os quatro
módulos e os diretórios da interface/port. O projeto pai fornece o target
`stm32cube_f4_hal`, com fontes, headers, flags Cortex-M4F, definição do dispositivo
(`STM32F411xE`, por exemplo) e `stm32f4xx_hal_conf.h`. Habilite os módulos GPIO,
I2C, UART, TIM e suas dependências na configuração da HAL.

O projeto pai mantém o SDK, startup, tick e linker script. O port não escolhe
pinos da placa nem altera o contrato público da HAL.
