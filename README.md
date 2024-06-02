# AD5933_bioimpedance_meter
Software carregado para o ESP32 usado no projeto de medidor de bioimpedância vestível apresentado como TCC.

## Disclaimer
O código apresentado foi altamente baseado [nesse repositório](https://github.com/mjmeli/arduino-ad5933) cujo exemplo foi modificado para ser compatível com o hardware externo e adicionar o envio do resultado das medições via BLE. Além disso a biblioteca disponibilizada também foi levemente modificada e incrementada.

## Trabalhos futuros
Como discorrido durante o TCC, o potenciômetro digital apresentou problemas, por isso o ajuste do ganho do amplificador de instrumentação via software não pode ser testado, logo a presente versão não possui o ajuste de ganho implementado. 
