# Projeto Final - STUDY BUDDY
**Atividade Prática Individual**


─────────────────────────────────────────────────────────


*Desenvolvido por: Amanda Cardoso Lopes*

─────────────────────────────────────────────────────────

## Descrição do Projeto

Este projeto implementa um sistema interativo com display OLED SSD1306, LEDs RGB, joystick analógico e botões para navegação. Ele permite ao usuário visualizar informações na tela, controlar LEDs via PWM e interagir por meio de botões e joystick. Além disso, possui um buzzer para emitir sinais sonoros e funcionalidades como alarme e modo Pomodoro.


─────────────────────────────────────────────────────────

## Funcionalidades Implementadas

- Exibição de menus interativos no display OLED SSD1306
- Controle de LEDs RGB via GPIO e PWM
- Leitura analógica do joystick para navegação
- Detecção de botões com debounce
- Sistema de alarme e ajuste de horário
- Modo Pomodoro com diferentes intervalos
- Reset via botão BOOTSEL
- Emissão de sons via buzzer


─────────────────────────────────────────────────────────

## Componentes Utilizados

- **Microcontrolador:** RP2040 (Placa Raspberry Pi Pico)
- **Display SSD1306 (128x64):**  
  - Conectado via I2C (SDA no GPIO 14 e SCL no GPIO 15)
- **LED RGB:**  
  - **LED Vermelho:** GPIO 13 (PWM)  
  - **LED Azul:** GPIO 12 (PWM)  
  - **LED Verde:** GPIO 11 (Liga/Desliga)
- **Joystick:**  
  - Eixos X e Y conectados aos GPIOs 26 e 27 (leitura via ADC)  
  - Botão integrado ao joystick: GPIO 22
- **Botões:**  
  - **Botão A:** GPIO 5  
  - **Botão B:** GPIO 6  
- **Buzzer:**  
  - Conectado ao GPIO 10


─────────────────────────────────────────────────────────

## Como Executar o Projeto

1. **Clonagem do Repositório:**  
   Abra o VS Code (ou seu editor de preferência) e clone o repositório:
   ```sh
   git init
   git clone https://github.com/4mandaCardoso/Projetofinal_Embarcatech.git
   ```  

2. **Carregamento e Compilação:**  
   - Compile o código e gere o arquivo .UF2.

3. **Rodando na plaquinha:**  
   - Arraste o arquivo .UF2 para o diretório da plaquinha.


─────────────────────────────────────────────────────────

## VÍDEO DE DEMONSTRAÇÃO

### Link do vídeo:
🔗 https://drive.google.com/drive/folders/1jEKdSOxEIoAmqsCPybZ3ZO7VkRa051Pj?usp=sharing


