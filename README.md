# Port STM32F4

Implementa os contratos de `hal/interface` para `STM32F411xE` e `STM32F407xx`
usando STM32Cube HAL e CMSIS. A integração seleciona MCU, startup, linker script
e BSP; outros membros da família STM32F4 ainda não são suportados.

## Inicialização e memória

O startup CMSIS deve executar `SystemInit()` antes de `main`, habilitando a FPU.
`hal_system_init()` valida o buffer externo alinhado a `max_align_t`, inicializa
a HAL ST, habilita PWR/SYSCFG e configura o maior SYSCLK nominal do dispositivo
usando HSI de 16 MHz e PLL. A integração assume alimentação de 3,3 V (latências
de Flash configuradas para VDD entre 2,7 e 3,6 V).

| MCU | PLL M/N/P/Q | SYSCLK/HCLK | PCLK1 | PCLK2 | Flash |
| --- | --- | --- | --- | --- | --- |
| F411 | 16/200/2/5 | 100 MHz | 50 MHz | 100 MHz | 3 wait states |
| F407 | 16/336/2/7 | 168 MHz | 42 MHz | 84 MHz | 5 wait states |

A escala de tensão é 1. A inicialização muda temporariamente para HSI, desliga
o PLL antes de alterar a escala, aguarda PLL/regulador e atualiza o SysTick
através da HAL ST. Não habilita HSE nem LSE. No F411, PLLQ fornece 40 MHz:
esta configuração não fornece os 48 MHz exigidos pelo USB.

O port fornece `SysTick_Handler()` para incrementar o tick ST a cada milissegundo,
com prioridade 0 na configuração incluída. Não defina outro handler com o mesmo
nome. Operações bloqueantes exigem interrupções habilitadas e tick funcionando;
não suspenda o tick nem altere os clocks após construir recursos. Não há RTOS.

Os construtores reservam objetos opacos na arena singleton de `hal/memory`,
sem `malloc`. Inicialize o sistema uma vez, construa os recursos, chame
`hal_system_seal()` e então inicie os temporizadores. Selar é idempotente e
preserva os handles. Não há destruição nem reconfiguração. Falhas anteriores à
reserva não consomem memória; depois de tocar o hardware, falhas conservam as
reservas até o reinício. Saídas válidas de construtor recebem `NULL` em falhas.

## Clocks e recursos

Cada `hal_*_config()` configura tudo que seu recurso necessita, sem exigir
`MX_*_Init`, `HAL_*_MspInit` específicos ou construção prévia de GPIOs:

- GPIO: clock AHB1 do banco, latch inicial antes de habilitar saída, modo e pull.
- UART: clock/reset APB, formato, baud rate, GPIOs e funções alternativas;
  controle de fluxo e interrupções de UART desabilitados.
- I2C: clock/reset APB1, GPIOs em função alternativa open drain, temporização
  de mestre e filtros programáveis quando disponíveis no dispositivo.
- Timer: clock/reset APB1, fonte interna, PSC/ARR, limpeza de flags e NVIC.
  A construção deixa contador e interrupção de update parados.

Pinos e controladores são exclusivos durante a vida do firmware. O port detecta
reservas de seus construtores e controladores já habilitados pelo SDK. O BSP
deve evitar recursos usados diretamente por outro código, pinos de depuração,
osciladores ativos e rotas ausentes no encapsulamento da placa. Uma reserva de
I2C/UART já inclui seus GPIOs; não construa handles GPIO para eles.

## Identificadores e rotas

`hal_pin_t.port` usa A=0, B=1, ..., I=8; `number` usa o índice 0..15.
F411 possui A..E e H (PH0/PH1); F407 possui A..I (PI0..PI11). A validação cobre
o dispositivo e a função alternativa; disponibilidade no encapsulamento e
ligação elétrica pertencem ao BSP.

UART usa identificadores estáveis a partir de zero. TX e RX podem escolher
individualmente qualquer rota listada para o mesmo controlador.

| `instance` | Controlador | TX | RX | AF |
| --- | --- | --- | --- | --- |
| 0 | USART1 | PA9, PB6 | PA10, PB7; PB3 somente F411 | 7 |
| 1 | USART2 | PA2, PD5 | PA3, PD6 | 7 |
| 2 | USART3 (F407) | PB10, PC10, PD8 | PB11, PC11, PD9 | 7 |
| 3 | UART4 (F407) | PA0, PC10 | PA1, PC11 | 8 |
| 4 | UART5 (F407) | PC12 | PD2 | 8 |
| 5 | USART6 | PC6; PG14 somente F407 | PC7; PG9 somente F407 | 8 |

I2C também usa identificadores a partir de zero. Cada sinal pode selecionar
uma rota de seu controlador; ambos precisam de pull-ups externos adequados.

| `instance` | Controlador | SCL | SDA |
| --- | --- | --- | --- |
| 0 | I2C1 | PB6/PB8 (AF4) | PB7/PB9 (AF4) |
| 1 | I2C2 | PB10 (AF4); PF1/PH4 no F407 (AF4) | PB11 (AF4); PB3/PB9 no F411 (AF9); PF0/PH5 no F407 (AF4) |
| 2 | I2C3 | PA8 (AF4); PH7 no F407 (AF4) | PC9 (AF4); PB4/PB8 no F411 (AF9); PH8 no F407 (AF4) |

Rotas e limites de clock seguem os datasheets
[STM32F411](https://www.st.com/resource/en/datasheet/stm32f411ce.pdf) e
[STM32F407](https://www.st.com/resource/en/datasheet/stm32f407vg.pdf).

## Transferências e temporizadores

UART suporta 8 bits sem paridade, 7 ou 8 bits com paridade par/ímpar e 1 ou 2
stop bits. Payload é traduzido para o word length ST, que inclui paridade.
Sete bits sem paridade não são suportados. O divisor de oversampling 16 deve
ser representável com erro nominal de baud rate de até 2%, sem incluir a
tolerância do HSI. Toda escrita de 7 bits é validada antes de transmitir.
A contagem indica bytes entregues ao registrador de dados, inclusive em
timeout na espera da conclusão (TC).

I2C aceita 10 kHz a 400 kHz e endereços de sete bits não reservados
(`0x08..0x77`). Cada chamada conclui uma transação com STOP em sucesso, sem
retries. Falhas podem deixar dados parciais ou alterar o dispositivo.
UART e I2C aceitam até 65535 bytes por transferência; UART aceita tamanho zero
como no-op e I2C exige tamanho positivo.

Timeouts são positivos, em milissegundos, de 1 a `UINT32_MAX - 1`;
`UINT32_MAX` é rejeitado porque significa espera infinita na HAL ST. A resolução
é de 1 ms. A ST verifica ticks com overhead próprio; I2C possui também espera
interna de BUSY de 25 ms. O timeout não é prazo estrito para toda a chamada.
NACK e erros de barramento retornam `HAL_STATUS_ERROR`; os caminhos de timeout
da ST são traduzidos para `HAL_STATUS_TIMEOUT`.

| Timer `instance` | Hardware | ARR |
| --- | --- | --- |
| 0 | TIM2 | 32 bits |
| 1 | TIM3 | 16 bits |
| 2 | TIM4 | 16 bits |
| 3 | TIM5 | 32 bits |

O período usa milissegundos inteiros positivos. O cálculo usa o clock real do
timer, incluindo multiplicação APB e TIMPRE quando disponível, e aceita somente
pares PSC/ARR que representem exatamente o período dentro dos limites físicos.
Períodos não representáveis retornam `HAL_STATUS_NOT_SUPPORTED` antes da
reserva. TIM1 e timers adicionais não são expostos; SysTick é dedicado à HAL ST.

O port fornece `TIM2_IRQHandler` até `TIM5_IRQHandler`, com prioridade 5.
`hal_timer_start()` não aloca, publica o estado para a ISR, recarrega o prescaler
e começa um período completo. Uma segunda chamada retorna estado inválido.
Callbacks sem argumentos executam em interrupção e devem ser curtos e não
bloqueantes. UIF acumula somente uma ocorrência: múltiplas expirações durante
bloqueio/latência podem gerar uma única notificação. Não há suporte a chamadas
concorrentes dos construtores ou das transferências.

## Integração

Inclua `hal/interface`, `hal/memory`, a raiz do port, a configuração
`stm32f4xx_hal_conf.h`, CMSIS e os headers ST. Compile todos os `.c` deste port,
`hal_memory.c`, o system CMSIS e os módulos ST habilitados na configuração.
O projeto F411 já faz isso em `cmake/stm32f4.cmake`.

Para F407, selecione `STM32F407xx` em lugar de `STM32F411xE` e forneça startup,
linker script e recursos da placa correspondente. O firmware raiz continua
selecionando STM32F411CEU6; os testes verificam ambos os caminhos do port.
