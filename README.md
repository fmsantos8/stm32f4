# Porte STM32F4

Implementa os contratos públicos de GPIO, I2C, UART e temporizador usando a
STM32CubeF4 HAL. Os mesmos fontes atendem ao STM32F411 e ao STM32F407. As
condicionais verificam recursos definidos pelo SDK; não selecionam placas,
pinos de produto ou configurações de clock global.

## Responsabilidades e pré-requisitos

O BSP/integração deve:

- Fornecer os recursos físicos nos handles: instâncias, pinos, funções
  alternativas, pulls, velocidades elétricas, timeouts e prioridades de IRQ.
- Confirmar o mapeamento de funções alternativas e os pinos efetivamente
  disponíveis no encapsulamento. O porte valida instâncias, máscaras de um
  único pino e valores do SDK; não contém tabelas de pinagem de placas.
- Chamar `HAL_Init()`, configurar osciladores, PLL, clocks globais e manter
  `SystemCoreClock` atualizado antes de configurar os recursos do porte.
- Manter a base de tempo do SDK funcionando para os timeouts. Com o SysTick
  padrão, `SysTick_Handler()` deve chamar `HAL_IncTick()`.
- Providenciar as condições elétricas externas, incluindo os pull-ups do I2C.
- Encaminhar a interrupção do TIM dedicado para `hal_timer_stm32f4_irq()`.

O porte habilita os clocks dos GPIOs e periféricos recebidos, configura os pinos,
reinicializa os controladores I2C/UART/TIM e inicializa seus drivers. No TIM,
também configura e habilita o NVIC. Não é necessário chamar funções externas
`MX_GPIO_Init()`, `MX_I2Cx_Init()`, `MX_USARTx_UART_Init()` ou `MX_TIMx_Init()`
para recursos controlados por este porte.

Os handles devem ser inicializados por agregação, com os campos de estado e
driver inicialmente zerados. Sua memória deve permanecer válida durante o uso;
cada recurso físico deve ter um único handle proprietário. Os campos físicos
são constantes. Não copie handles ativos nem manipule seus drivers diretamente.
O acesso e a reconfiguração de recursos compartilhados devem ser serializados
pela integração; as funções públicas não oferecem sincronização nem uso em ISR.
O encaminhamento de interrupção do temporizador é uma API específica do porte.

Os hooks `HAL_*_MspInit()` padrão do SDK podem permanecer vazios. Se o projeto
os sobrescrever para outros recursos, eles devem preservar a configuração dos
recursos deste porte. Não despache também `HAL_TIM_IRQHandler()` para o mesmo
TIM: o encaminhamento específico já consome seu evento de atualização.

## Capacidades

| Contrato | Implementação e limites |
| --- | --- |
| GPIO | Entrada ou saída push-pull, três opções de pull. A saída sempre começa em nível baixo, inclusive na reconfiguração; o latch é escrito antes da mudança de modo. |
| I2C | Controlador mestre I2C1/2/3, quando disponíveis; endereços de 7 bits; até 400 kHz, conforme a representabilidade do clock. Leitura/escrita síncronas, de 1 a 65535 bytes por transação. Tamanhos maiores retornam `HAL_STATUS_NOT_SUPPORTED`, sem dividir a transação. |
| UART | Instâncias USART/UART disponíveis entre USART1/2/3/6 e UART4/5/7/8. Transmissão síncrona, oito bits de dados, paridade opcional, um ou dois stop bits, sem controle de fluxo. Somente TX; oversampling 16. Baud rates fora da capacidade do divisor retornam `HAL_STATUS_NOT_SUPPORTED`. |
| Temporizador | TIM2/3/4/5 dedicado, quando disponível. Atualização a cada 1 ms, contagem em software de 32 bits, módulo 2³². Funciona com contadores de 16 ou 32 bits e considera o prescaler APB1 e TIMPRE, quando disponível. |

I2C e UART recebem `timeout_ms` no handle específico do porte, com valor
obrigatório entre 1 e `HAL_MAX_DELAY - 1`. A interface pública atual não recebe
timeout por chamada. Na UART, um mesmo orçamento cobre todos os blocos de até
65535 bytes de uma escrita. Uma falha pode ocorrer após transmissão parcial;
o contrato não informa a quantidade transmitida. No I2C, o limite é passado ao
driver do SDK, que também utiliza limites internos para algumas fases do
barramento. Os retornos de timeout do SDK, incluindo seu código de erro I2C,
são traduzidos para `HAL_STATUS_TIMEOUT`; busy e demais falhas são erros gerais.

Configurações inválidas são rejeitadas antes de alterar o periférico. Se a
inicialização de uma configuração válida falhar, I2C/UART ficam indisponíveis
até uma configuração bem-sucedida. O porte não promete restaurar a configuração
anterior após falha de hardware.

O temporizador exige que o clock permita um período exato de 1 ms. A primeira
inicialização começa em zero; chamadas seguintes ao mesmo handle em execução
preservam a contagem. O período é atendido por IRQ: a integração deve permitir
seu atendimento a cada milissegundo. Bloqueios de interrupção que acumulem
múltiplas atualizações perdem eventos e comprometem a contagem. Clocks devem
permanecer estáveis enquanto os recursos estiverem ativos.

## Exemplo de composição

Os nomes `BOARD_*` abaixo representam constantes/descritores definidos pelo
BSP do projeto consumidor. Nenhum desses recursos é fixado pelo porte.

```c
#include "gpio/hal_gpio_stm32f4.h"
#include "i2c/hal_i2c_stm32f4.h"
#include "timer/hal_timer_stm32f4.h"

static hal_gpio_t led = {
    .port = BOARD_LED_PORT,
    .pin = BOARD_LED_PIN,
    .speed = BOARD_LED_SPEED,
};

static hal_i2c_t bus = {
    .instance = BOARD_I2C_INSTANCE,
    .scl = BOARD_I2C_SCL, /* hal_stm32f4_pin_t: port, pin, alternate, pull, speed */
    .sda = BOARD_I2C_SDA,
    .timeout_ms = BOARD_I2C_TIMEOUT_MS,
};

static hal_timer_t timebase = {
    .instance = BOARD_TIMER_INSTANCE,
    .irq_priority = BOARD_TIMER_IRQ_PRIORITY,
    .irq_subpriority = BOARD_TIMER_IRQ_SUBPRIORITY,
};

/* Após HAL_Init() e a configuração global da placa, verificando cada retorno: */
/* hal_gpio_config(&led, &gpio_config); */
/* hal_i2c_config(&bus, &i2c_config); */
/* hal_timer_start(&timebase); */
```

Por exemplo, **se o BSP escolher TIM3**, a integração fornece:

```c
void TIM3_IRQHandler(void)
{
    hal_timer_stm32f4_irq(&timebase);
}
```

Componentes incluem somente os cabeçalhos de `hal/interface/` e recebem os
handles já válidos. Cabeçalhos `*_stm32f4.h` ficam no BSP/integração.

## Compilação com SDK externo

O consumidor seleciona a toolchain, as opções de CPU/ABI, o MCU, o startup, o
linker script e seu próprio `stm32f4xx_hal_conf.h`. Este diretório não chama
`project()` nem configura compiladores, sendo reutilizável por `add_subdirectory`.

Com um pacote completo [STM32CubeF4](https://github.com/STMicroelectronics/STM32CubeF4)
externo, configure:

```cmake
set(STM32CUBE_F4_PATH "/caminho/local/STM32Cube_FW_F4")
set(STM32F4_HAL_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bsp/include")
set(STM32F4_DEVICE STM32F411xE) # Ou STM32F407xx no projeto correspondente.
set(STM32F4_HAL_INTERFACE_DIR "/caminho/local/hal/interface")
add_subdirectory("/caminho/local/hal/ports/stm32f4" stm32f4_port)
target_link_libraries(firmware PRIVATE hal::stm32f4)
```

O porte cria a biblioteca `stm32f4_sdk` com os módulos usados e exporta seus
includes e definições. A configuração da HAL deve habilitar HAL, Cortex, DMA,
Flash, GPIO, I2C, PWR, RCC, TIM e UART, incluindo seus cabeçalhos como no template
do SDK. DMA é uma dependência dos fontes dos drivers; o porte usa operações
bloqueantes. Configure os valores de oscilador, alimentação e tick conforme o
projeto. O arquivo em `tests/fixtures/` é exclusivo dos testes.

Esse modo não adiciona `system_stm32f4xx.c` nem startup à biblioteca. O consumidor
fornece esses arquivos. Os valores de oscilador usados pelo arquivo de sistema
devem coincidir com a configuração HAL do BSP.

Se a integração existente já compila o SDK, forneça um target com seus includes,
definições de MCU/configuração e objetos/bibliotecas:

```cmake
set(STM32F4_HAL_SDK_TARGET sdk_do_projeto)
add_subdirectory("/caminho/local/hal/ports/stm32f4" stm32f4_port)
target_link_libraries(firmware PRIVATE hal::stm32f4)
```

Use um target de drivers, com dependências transitivas completas, evitando
targets que carreguem fontes da aplicação/BSP via `INTERFACE_SOURCES`. Assim
a integração pode aproveitar o SDK já configurado pelo ambiente STM32Cube.

O CMake da raiz deste checkout continua sendo a integração STM32F411 existente.
Recebe `STM32CUBE_F4_PATH` e `STM32F4_HAL_CONFIG_DIR`, adiciona o porte e o arquivo
de sistema do SDK, e preserva as opções de CPU/ABI da toolchain. A extensão
STM32Cube para VS Code pode usar o preset Debug/Release existente. Caminhos
pessoais podem ser informados em `CMakeUserPresets.json` (ignorado pelo Git):

```json
{
  "version": 3,
  "configurePresets": [{
    "name": "Debug-local",
    "inherits": "Debug",
    "cacheVariables": {
      "STM32CUBE_F4_PATH": "/caminho/local/STM32Cube_FW_F4",
      "STM32F4_HAL_CONFIG_DIR": "/caminho/local/configuracao-do-bsp"
    }
  }]
}
```

O executável da raiz conserva seu `main` vazio; a composição concreta da placa
é fornecida pelo projeto que utilizar o framework.

## Verificação

Os testes Linux usam os cabeçalhos reais do SDK, funções de driver simuladas e
memória de registradores mapeada nos endereços STM32. Verificam clocks e sua
ordem em relação à configuração, argumentos, reconfiguração, erros, limites de
transferência, paridade, timeout total, interrupção e wrap do temporizador.
Incluem compilação dos cabeçalhos em C++.

```sh
cmake -S hal/ports/stm32f4/tests -B /tmp/stm32f4-tests \
  -DSTM32CUBE_F4_PATH=/caminho/local/STM32Cube_FW_F4 \
  -DSTM32F4_DEVICE=STM32F411xE
cmake --build /tmp/stm32f4-tests
ctest --test-dir /tmp/stm32f4-tests --output-on-failure
```

Repita com `STM32F4_DEVICE=STM32F407xx` e, opcionalmente,
`STM32F4_TEST_CALLBACKS=1`. Os testes não verificam condições elétricas,
precisão física do clock nem execução nas placas.

A implementação foi compilada para ARM Cortex-M4 com Clang 21.1.8, para ambos
os MCUs, usando os drivers reais, e testada no host com GCC 15.2.0. Referências
utilizadas na verificação:

- STM32F4 HAL v1.8.5: `1f6451c3e07728b4c830744de380e56bf5bc0026`.
- CMSIS Device F4: `9192c7b9df75a142f2027ab266601fe061fc00b3`.
- CMSIS 5: `2b7495b8535bdcb306dac29b9ded4cfb679d7e5c`.

Os contratos públicos vigentes foram preservados. A proposta textual também
menciona timeout por chamada e transação I2C combinada, que não estão expostos
na interface atual e não foram acrescentados por este porte.
